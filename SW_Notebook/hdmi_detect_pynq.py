#!/usr/bin/env python3
"""
hdmi_detect_pynq.py — Script exécuté sur la PYNQ-Z2.

1. Charge VivadoHW.bit (active hdmi_in_hpd pour que le PC détecte le port HDMI).
2. Précharge une liste d'images de référence via VDMA (PS_TO_PL) et mémorise leurs descripteurs.
3. Bascule proprement l'entrée vidéo sur le port HDMI IN (1280x720) sans bloquer dma.recvchannel.
4. Maintient vdma.readchannel actif pour éviter la contre-pression sur axis_broadcaster_0,
   draine axi_dma_0 dans un thread C++ dédié et affiche en direct les objets détectés.
"""

import argparse
import ctypes
import hashlib
import os
import signal
import subprocess
import sys
import time
from enum import Enum

import cv2
import numpy as np
from pynq import Overlay, allocate
from pynq.lib.video import VideoMode

PROJECT_DIR = "/home/xilinx/jupyter_notebooks/PynqZ2-ORB-Accelerator"
IN_WIDTH, IN_HEIGHT = 1280, 720
CPP_PATH = "/tmp/hdmi_matcher.cpp"
SO_PATH = "/tmp/libhdmi_matcher.so"

HDMI_MATCHER_CPP = r"""
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <mutex>
#include <omp.h>
#include <sys/mman.h>
#include <thread>
#include <unistd.h>
#include <vector>

#define AXI_DMA_BASE_ADDR 0x40400000
#define AXI_DMA_MAP_SIZE  0x10000

#define S2MM_DMACR  (0x30 >> 2)
#define S2MM_DMASR  (0x34 >> 2)
#define S2MM_DA     (0x48 >> 2)
#define S2MM_LENGTH (0x58 >> 2)

#define DMA_STATUS_HALTED 0x00000001
#define DMA_STATUS_IDLE   0x00000002
#define DMA_STATUS_IOC    0x00001000
#define DMA_STATUS_ERR    0x00000770

struct __attribute__((packed, aligned(8))) RawOrbPacket {
    uint64_t desc[4];
    uint16_t x;
    uint16_t y;
    uint8_t  padding[28];
};

struct CachedDescriptor {
    uint64_t desc[4];
};

struct SceneFeature {
    uint64_t desc[4];
    uint16_t x;
    uint16_t y;
};

static std::vector<std::vector<CachedDescriptor>> g_refs;

static int g_mem_fd = -1;
static void* g_map_base = MAP_FAILED;
static volatile uint32_t* g_dma_regs = nullptr;

static std::thread g_dma_thread;
static std::atomic<bool> g_worker_running{false};
static std::atomic<bool> g_signal_lost{true};
static std::atomic<uint64_t> g_frame_seq{0};
static std::atomic<uint64_t> g_dma_frames_total{0};

static std::mutex g_scene_mutex;
static std::vector<SceneFeature> g_latest_scene;

inline void rearm_dma(volatile uint32_t* dma_regs, uint32_t phys_addr, uint32_t max_bytes) {
    dma_regs[S2MM_DMASR] = DMA_STATUS_IOC | DMA_STATUS_ERR;
    dma_regs[S2MM_DMACR] |= 0x1;
    dma_regs[S2MM_DA] = phys_addr;
    dma_regs[S2MM_LENGTH] = max_bytes;
    (void)dma_regs[S2MM_DMACR];
}

inline void reset_and_rearm_dma(volatile uint32_t* dma_regs, uint32_t phys_addr, uint32_t max_bytes) {
    dma_regs[S2MM_DMACR] = 0x4;
    auto t0 = std::chrono::steady_clock::now();
    while (dma_regs[S2MM_DMACR] & 0x4) {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - t0).count() > 500) {
            break;
        }
    }
    rearm_dma(dma_regs, phys_addr, max_bytes);
}

static void dma_worker_loop(
    const void* virt_dma_buf,
    uint32_t phys_dma_addr,
    uint32_t max_dma_bytes,
    int warmup_frames
) {
    const RawOrbPacket* dma_packets = reinterpret_cast<const RawOrbPacket*>(virt_dma_buf);
    const size_t max_packets = max_dma_bytes / sizeof(RawOrbPacket);

    std::vector<RawOrbPacket> local_raw(max_packets);
    std::vector<SceneFeature> local_scene;
    local_scene.reserve(max_packets);

    reset_and_rearm_dma(g_dma_regs, phys_dma_addr, max_dma_bytes);
    int frames_seen = 0;

    while (g_worker_running.load(std::memory_order_acquire)) {
        bool saw_busy = false;
        bool got_frame = false;
        auto wait_start = std::chrono::steady_clock::now();

        while (g_worker_running.load(std::memory_order_acquire)) {
            uint32_t status = g_dma_regs[S2MM_DMASR];
            if ((status & DMA_STATUS_ERR) || (status & DMA_STATUS_HALTED)) {
                reset_and_rearm_dma(g_dma_regs, phys_dma_addr, max_dma_bytes);
                saw_busy = false;
            } else {
                if ((status & DMA_STATUS_IDLE) == 0) {
                    saw_busy = true;
                }
                if ((status & DMA_STATUS_IOC) || (saw_busy && (status & DMA_STATUS_IDLE))) {
                    got_frame = true;
                    break;
                }
            }

            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - wait_start).count();
            if (elapsed_ms >= 500) {
                g_signal_lost.store(true, std::memory_order_release);
                reset_and_rearm_dma(g_dma_regs, phys_dma_addr, max_dma_bytes);
                break;
            }
            if (elapsed_ms >= 2) {
                usleep(250);
            }
        }

        if (!got_frame) {
            continue;
        }

        uint32_t bytes_transferred = g_dma_regs[S2MM_LENGTH];
        size_t num_packets = bytes_transferred / sizeof(RawOrbPacket);
        if (num_packets > max_packets) num_packets = max_packets;

        local_scene.clear();
        if (num_packets > 0) {
            std::memcpy(local_raw.data(), dma_packets, num_packets * sizeof(RawOrbPacket));
            for (size_t i = 0; i < num_packets; ++i) {
                if (local_raw[i].x != 0 || local_raw[i].y != 0) {
                    SceneFeature sf;
                    sf.desc[0] = local_raw[i].desc[0];
                    sf.desc[1] = local_raw[i].desc[1];
                    sf.desc[2] = local_raw[i].desc[2];
                    sf.desc[3] = local_raw[i].desc[3];
                    sf.x = local_raw[i].x;
                    sf.y = local_raw[i].y;
                    local_scene.push_back(sf);
                }
            }
        }

        rearm_dma(g_dma_regs, phys_dma_addr, max_dma_bytes);

        frames_seen++;
        g_dma_frames_total.fetch_add(1, std::memory_order_relaxed);

        if (frames_seen > warmup_frames) {
            {
                std::lock_guard<std::mutex> lock(g_scene_mutex);
                g_latest_scene.swap(local_scene);
            }
            g_signal_lost.store(false, std::memory_order_release);
            g_frame_seq.fetch_add(1, std::memory_order_release);
        }
    }
}

extern "C" {

void clear_references() {
    g_refs.clear();
}

int add_reference(const void* ref_descs_ptr, int num_ref) {
    if (!ref_descs_ptr || num_ref <= 0) return -1;
    std::vector<CachedDescriptor> vec(num_ref);
    std::memcpy(vec.data(), ref_descs_ptr, num_ref * sizeof(CachedDescriptor));
    g_refs.push_back(std::move(vec));
    return static_cast<int>(g_refs.size()) - 1;
}

int start_dma_worker(
    const void* virt_dma_buf,
    uint32_t phys_dma_addr,
    uint32_t max_dma_bytes,
    int warmup_frames
) {
    if (g_worker_running.load()) return 0;

    g_mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (g_mem_fd < 0) {
        std::cerr << "[Erreur C++] Impossible d'ouvrir /dev/mem (sudo requis)." << std::endl;
        return -1;
    }

    g_map_base = mmap(NULL, AXI_DMA_MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, g_mem_fd, AXI_DMA_BASE_ADDR);
    if (g_map_base == MAP_FAILED) {
        std::cerr << "[Erreur C++] Echec mmap sur axi_dma_0 (0x40400000)." << std::endl;
        close(g_mem_fd);
        g_mem_fd = -1;
        return -1;
    }

    g_dma_regs = reinterpret_cast<volatile uint32_t*>(g_map_base);
    g_frame_seq.store(0);
    g_dma_frames_total.store(0);
    g_signal_lost.store(true);
    g_worker_running.store(true);

    g_dma_thread = std::thread(dma_worker_loop, virt_dma_buf, phys_dma_addr, max_dma_bytes, warmup_frames);
    return 0;
}

void stop_dma_worker() {
    if (g_worker_running.load()) {
        g_worker_running.store(false);
        if (g_dma_thread.joinable()) {
            g_dma_thread.join();
        }
    }
    if (g_map_base != MAP_FAILED) {
        munmap(g_map_base, AXI_DMA_MAP_SIZE);
        g_map_base = MAP_FAILED;
        g_dma_regs = nullptr;
    }
    if (g_mem_fd >= 0) {
        close(g_mem_fd);
        g_mem_fd = -1;
    }
}

int match_latest_frame(
    int hamming_thresh,
    int timeout_ms,
    uint64_t* last_seq_io,
    int* out_scene_count,
    uint64_t* out_dma_frames,
    int* out_matches_per_ref,
    int max_refs
) {
    if (!last_seq_io || !out_scene_count || !out_matches_per_ref) return -2;

    auto wait_start = std::chrono::steady_clock::now();
    uint64_t cur_seq = g_frame_seq.load(std::memory_order_acquire);
    while (cur_seq == *last_seq_io) {
        if (!g_worker_running.load(std::memory_order_acquire)) return -3;
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - wait_start).count();
        if (elapsed_ms >= timeout_ms) {
            if (out_dma_frames) *out_dma_frames = g_dma_frames_total.load(std::memory_order_relaxed);
            return g_signal_lost.load(std::memory_order_acquire) ? -1 : -4;
        }
        usleep(300);
        cur_seq = g_frame_seq.load(std::memory_order_acquire);
    }

    std::vector<SceneFeature> scene_copy;
    {
        std::lock_guard<std::mutex> lock(g_scene_mutex);
        scene_copy = g_latest_scene;
        *last_seq_io = g_frame_seq.load(std::memory_order_acquire);
    }

    const size_t n_scene = scene_copy.size();
    *out_scene_count = static_cast<int>(n_scene);
    if (out_dma_frames) *out_dma_frames = g_dma_frames_total.load(std::memory_order_relaxed);

    const int n_refs = std::min<int>(static_cast<int>(g_refs.size()), max_refs);
    for (int idx = 0; idx < n_refs; ++idx) {
        const auto& ref = g_refs[idx];
        const int num_ref = static_cast<int>(ref.size());
        int best_cluster = 0;

        if (num_ref > 0 && n_scene > 0) {
            std::vector<uint8_t> r_passed(num_ref, 0);
            std::vector<uint16_t> r_match_x(num_ref, 0);
            std::vector<uint16_t> r_match_y(num_ref, 0);

            #pragma omp parallel for num_threads(2) schedule(static)
            for (int r = 0; r < num_ref; ++r) {
                const uint64_t r0 = ref[r].desc[0];
                const uint64_t r1 = ref[r].desc[1];
                const uint64_t r2 = ref[r].desc[2];
                const uint64_t r3 = ref[r].desc[3];

                int best_dist = 256;
                int best_s = -1;

                for (size_t s = 0; s < n_scene; ++s) {
                    int dist = __builtin_popcountll(r0 ^ scene_copy[s].desc[0]) +
                               __builtin_popcountll(r1 ^ scene_copy[s].desc[1]) +
                               __builtin_popcountll(r2 ^ scene_copy[s].desc[2]) +
                               __builtin_popcountll(r3 ^ scene_copy[s].desc[3]);
                    if (dist < best_dist) {
                        best_dist = dist;
                        best_s = static_cast<int>(s);
                    }
                }

                if (best_s >= 0 && best_dist < hamming_thresh) {
                    const uint64_t s0 = scene_copy[best_s].desc[0];
                    const uint64_t s1 = scene_copy[best_s].desc[1];
                    const uint64_t s2 = scene_copy[best_s].desc[2];
                    const uint64_t s3 = scene_copy[best_s].desc[3];
                    const int bx = static_cast<int>(scene_copy[best_s].x);
                    const int by = static_cast<int>(scene_copy[best_s].y);

                    bool is_mutual_best = true;
                    for (int r2_idx = 0; r2_idx < num_ref; ++r2_idx) {
                        if (r2_idx == r) continue;
                        int dist_back = __builtin_popcountll(s0 ^ ref[r2_idx].desc[0]) +
                                        __builtin_popcountll(s1 ^ ref[r2_idx].desc[1]) +
                                        __builtin_popcountll(s2 ^ ref[r2_idx].desc[2]) +
                                        __builtin_popcountll(s3 ^ ref[r2_idx].desc[3]);
                        if (dist_back < best_dist) {
                            is_mutual_best = false;
                            break;
                        }
                    }

                    if (is_mutual_best) {
                        // Test de ratio de Lowe spatial (best_dist <= 0.85 * dist2)
                        // Exclusion de +/-16 px pour ne pas penaliser le meme coin sur un autre etage
                        bool lowe_ok = true;
                        if (best_dist > 0) {
                            for (size_t s = 0; s < n_scene; ++s) {
                                int dx = static_cast<int>(scene_copy[s].x) - bx;
                                int dy = static_cast<int>(scene_copy[s].y) - by;
                                if (dx >= -16 && dx <= 16 && dy >= -16 && dy <= 16) {
                                    continue;
                                }
                                int dist2 = __builtin_popcountll(r0 ^ scene_copy[s].desc[0]) +
                                            __builtin_popcountll(r1 ^ scene_copy[s].desc[1]) +
                                            __builtin_popcountll(r2 ^ scene_copy[s].desc[2]) +
                                            __builtin_popcountll(r3 ^ scene_copy[s].desc[3]);
                                if (best_dist * 20 > dist2 * 17) {
                                    lowe_ok = false;
                                    break;
                                }
                            }
                        }

                        if (lowe_ok) {
                            r_passed[r] = 1;
                            r_match_x[r] = scene_copy[best_s].x;
                            r_match_y[r] = scene_copy[best_s].y;
                        }
                    }
                }
            }

            // Filtrage par cluster spatial (fenetre 440x640 px autour de l'objet)
            std::vector<int> pts_x, pts_y;
            pts_x.reserve(num_ref);
            pts_y.reserve(num_ref);
            for (int r = 0; r < num_ref; ++r) {
                if (r_passed[r]) {
                    pts_x.push_back(static_cast<int>(r_match_x[r]));
                    pts_y.push_back(static_cast<int>(r_match_y[r]));
                }
            }

            const size_t n_pts = pts_x.size();
            for (size_t i = 0; i < n_pts; ++i) {
                const int cx = pts_x[i];
                const int cy = pts_y[i];
                int cnt = 0;
                for (size_t j = 0; j < n_pts; ++j) {
                    int dx = pts_x[j] - cx;
                    int dy = pts_y[j] - cy;
                    if (dx >= -220 && dx <= 220 && dy >= -320 && dy <= 320) {
                        cnt++;
                    }
                }
                if (cnt > best_cluster) {
                    best_cluster = cnt;
                }
            }
        }
        out_matches_per_ref[idx] = best_cluster;
    }

    return 0;
}

} // extern "C"
"""


class VideoSource(Enum):
    HDMI = (0, 0)
    TPG_COLOR_BAR_BOX = (1, 1)
    TPG_GRID_BOX = (2, 1)
    PS_TO_PL = (3, 2)


def ensure_matcher_lib():
    src_hash = hashlib.sha256(HDMI_MATCHER_CPP.encode("utf-8")).hexdigest()[:16]
    hash_file = CPP_PATH + ".sha256"
    need_compile = not os.path.exists(SO_PATH) or not os.path.exists(hash_file)
    if not need_compile:
        with open(hash_file, "r", encoding="utf-8") as f:
            if f.read().strip() != src_hash:
                need_compile = True

    if need_compile:
        print("[PYNQ] Compilation de libhdmi_matcher.so...", flush=True)
        with open(CPP_PATH, "w", encoding="utf-8") as f:
            f.write(HDMI_MATCHER_CPP)
        subprocess.check_call([
            "g++", "-O3", "-shared", "-fPIC", "-Wno-psabi",
            "-mcpu=cortex-a9", "-mfpu=neon", "-fopenmp", "-pthread",
            CPP_PATH, "-o", SO_PATH,
        ])
        with open(hash_file, "w", encoding="utf-8") as f:
            f.write(src_hash)
        print("[PYNQ] libhdmi_matcher.so pret.", flush=True)

    lib = ctypes.CDLL(SO_PATH)
    lib.clear_references.argtypes = []
    lib.clear_references.restype = None

    lib.add_reference.argtypes = [ctypes.c_void_p, ctypes.c_int]
    lib.add_reference.restype = ctypes.c_int

    lib.start_dma_worker.argtypes = [
        ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_int
    ]
    lib.start_dma_worker.restype = ctypes.c_int

    lib.stop_dma_worker.argtypes = []
    lib.stop_dma_worker.restype = None

    lib.match_latest_frame.argtypes = [
        ctypes.c_int,
        ctypes.c_int,
        ctypes.POINTER(ctypes.c_uint64),
        ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_uint64),
        ctypes.POINTER(ctypes.c_int),
        ctypes.c_int,
    ]
    lib.match_latest_frame.restype = ctypes.c_int
    return lib


def letterbox(img, target_w=1280, target_h=720, pad_color=(0, 0, 0)):
    h, w = img.shape[:2]
    scale = min(target_w / w, target_h / h)
    new_w, new_h = int(w * scale), int(h * scale)
    resized = cv2.resize(img, (new_w, new_h), interpolation=cv2.INTER_LINEAR)
    canvas = np.full((target_h, target_w, 3), pad_color, dtype=np.uint8)
    x_offset = (target_w - new_w) // 2
    y_offset = (target_h - new_h) // 2
    canvas[y_offset:y_offset + new_h, x_offset:x_offset + new_w] = resized
    return canvas


def parse_descriptors(buf, count):
    if count <= 0:
        return None
    raw = np.frombuffer(buf, dtype=np.uint8, count=count * 64).reshape(count, 64).copy()
    descs = np.ascontiguousarray(raw[:, :32])
    pts = raw[:, 32:36].copy().view(np.uint16)
    valid = (pts[:, 0] != 0) | (pts[:, 1] != 0)
    valid_descs = np.ascontiguousarray(descs[valid])
    return valid_descs if len(valid_descs) > 0 else None


def resolve_ref_paths(user_refs):
    images_dir = os.path.join(PROJECT_DIR, "images", "refs")
    valid_ext = (".jpg", ".jpeg", ".png", ".bmp")
    if not user_refs:
        if not os.path.isdir(images_dir):
            raise RuntimeError(f"Dossier images introuvable : {images_dir}")
        refs = sorted([
            os.path.join(images_dir, f)
            for f in os.listdir(images_dir)
            if f.lower().endswith(valid_ext) and "_ref" in f.lower()
        ])
        if not refs:
            raise RuntimeError(f"Aucune image '*_ref.*' trouvée dans {images_dir}")
        return refs

    resolved = []
    for item in user_refs:
        candidates = [
            item,
            os.path.join(PROJECT_DIR, item),
            os.path.join(images_dir, item),
            os.path.join(images_dir, os.path.basename(item)),
        ]
        found = next((c for c in candidates if os.path.isfile(c)), None)
        if not found:
            raise FileNotFoundError(f"Image de reference introuvable sur la PYNQ : {item}")
        resolved.append(found)
    return resolved


def main():
    parser = argparse.ArgumentParser(description="Détection d'objets temps réel via HDMI IN sur PYNQ-Z2")
    parser.add_argument("--refs", nargs="*", default=[], help="Images de référence à précharger")
    parser.add_argument("--fast-thresh", type=int, default=60, help="Seuil FAST pour les références (défaut: 60)")
    parser.add_argument("--hdmi-fast-thresh", type=int, default=35, help="Seuil FAST pour le flux webcam HDMI (défaut: 35)")
    parser.add_argument("--hamming-thresh", type=int, default=56, help="Seuil distance de Hamming (défaut: 56)")
    parser.add_argument("--min-matches", type=int, default=20, help="Nombre minimal de correspondances clusterisées (défaut: 20)")
    parser.add_argument("--min-ratio", type=float, default=0.07, help="Ratio minimal matches/ref (défaut: 0.07)")
    parser.add_argument("--min-margin", type=float, default=1.45, help="Facteur de dominance sur le 2e objet (défaut: 1.45)")
    parser.add_argument("--report-hz", type=float, default=4.0, help="Fréquence maximale d'affichage console (Hz)")
    args = parser.parse_args()

    stop_requested = False

    def handle_sig(signum, frame):
        nonlocal stop_requested
        stop_requested = True

    signal.signal(signal.SIGINT, handle_sig)
    signal.signal(signal.SIGTERM, handle_sig)

    os.chdir(PROJECT_DIR)

    # Charger le bitstream EN PREMIER pour activer hdmi_in_hpd immédiatement
    print("[PYNQ] Chargement de VivadoHW.bit...", flush=True)
    overlay = Overlay(PROJECT_DIR + "/VivadoHW.bit")
    overlay.download()
    print("[PYNQ] BITSTREAM_LOADED", flush=True)

    lib = ensure_matcher_lib()

    tpg = overlay.v_tpg_0
    vdma = overlay.dma_block.axi_vdma_0
    dma = overlay.dma_block.axi_dma_0
    image_resolution = overlay.axi_gpios.axi_gpio_1
    threshold_gpio = overlay.axi_gpios.axi_gpio_2
    stream_switch = overlay.orb_pipeline.axis_switch_0

    threshold_gpio.channel1.write(args.fast_thresh, 0xFFFFFFFF)
    dummy_desc_buf = allocate(shape=(65536,), dtype=np.uint32)

    if not dma.recvchannel.running:
        dma.recvchannel.start()
    dma.recvchannel.transfer(dummy_desc_buf)

    current_descs = None
    current_desc_count = 0

    def process_and_rearm_dma():
        nonlocal current_descs, current_desc_count
        status = dma.read(0x34)
        if (status & 0x1) or (status & 0x70):
            dma.write(0x30, 0x4)
            t_rst = time.monotonic()
            while dma.read(0x30) & 0x4:
                if time.monotonic() - t_rst > 2.0:
                    raise TimeoutError("Timeout reset axi_dma_0")
            dma.recvchannel.start()
            dma.recvchannel.transfer(dummy_desc_buf)
            return

        if status & 0x2:
            num_pkts = dma.read(0x58) // 64
            if num_pkts > 0:
                current_descs = parse_descriptors(dummy_desc_buf, num_pkts)
                current_desc_count = len(current_descs) if current_descs is not None else 0
            else:
                current_descs = None
                current_desc_count = 0
            dma.recvchannel.transfer(dummy_desc_buf)

    def wait_vdma_frame(timeout_sec=5.0):
        t0 = time.monotonic()
        while (vdma.readchannel._mmio.read(0x34) & 0x1000) == 0:
            process_and_rearm_dma()
            if time.monotonic() - t0 > timeout_sec:
                raise TimeoutError("Timeout attente frame VDMA S2MM lors du prechargement")
            time.sleep(0.001)
        _ = vdma.readchannel.readframe()
        process_and_rearm_dma()

    def inject_ref_image(image_path):
        tpg.write(0x0000, 0x0)
        stream_switch.write(0x040, 0x80000000)
        stream_switch.write(0x000, 0x2)
        if vdma.readchannel.running:
            vdma.readchannel.stop()

        image_resolution.channel1.write(IN_HEIGHT, 0xFFFFFFFF)
        image_resolution.channel2.write(IN_WIDTH, 0xFFFFFFFF)
        tpg.write(0x0010, IN_HEIGHT)
        tpg.write(0x0018, IN_WIDTH)

        if not dma.recvchannel.running:
            dma.recvchannel.start()
        if dma.recvchannel.idle:
            dma.recvchannel.transfer(dummy_desc_buf)

        if not vdma.writechannel.running:
            vdma.writechannel.mode = VideoMode(IN_WIDTH, IN_HEIGHT, 24)
            vdma.writechannel.start()

        vdma.readchannel.mode = VideoMode(IN_WIDTH, IN_HEIGHT, 24)
        vdma.readchannel.start()
        vdma.readchannel.irqframecount = 1

        stream_switch.write(0x040, VideoSource.PS_TO_PL.value[1])
        stream_switch.write(0x000, 0x2)

        img = cv2.imread(image_path)
        if img is None:
            raise FileNotFoundError(f"Impossible de lire {image_path}")
        img = letterbox(img, IN_WIDTH, IN_HEIGHT)
        frame_rgb = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
        buf = vdma.writechannel.newframe()
        np.copyto(buf, frame_rgb)
        vdma.writechannel.writeframe(buf)

    def service_vdma_readchannel():
        """Maintient vdma.readchannel en vie et vide toutes les frames prêtes en HDMI."""
        try:
            for _ in range(4):
                r_status = vdma.readchannel._mmio.read(0x34)
                if r_status & 0x1000:
                    _ = vdma.readchannel.readframe()
                else:
                    break
            r_status = vdma.readchannel._mmio.read(0x34)
            if r_status & 0x1:
                vdma.readchannel.stop()
                vdma.readchannel.start()
                vdma.readchannel.irqframecount = 1
            if r_status & 0xC990:
                vdma.readchannel._mmio.write(0x34, 0xC990)
        except Exception:
            pass

    try:
        ref_paths = resolve_ref_paths(args.refs)
        lib.clear_references()
        ref_entries = []  # (label, num_descs)

        print(f"[PYNQ] Prechargement de {len(ref_paths)} image(s) de reference via PS_TO_PL...", flush=True)
        for idx, ref_path in enumerate(ref_paths):
            if stop_requested:
                return
            current_descs = None
            current_desc_count = 0
            inject_ref_image(ref_path)

            stable_frames = 0
            last_count = -1
            tries = 0
            while stable_frames < 5 and tries < 60:
                wait_vdma_frame(timeout_sec=5.0)
                tries += 1
                if current_desc_count > 0:
                    if current_desc_count == last_count:
                        stable_frames += 1
                    else:
                        stable_frames = 1
                    last_count = current_desc_count
                else:
                    stable_frames = 0
                    last_count = 0

            if current_descs is None or stable_frames < 5:
                raise RuntimeError(f"Echec stabilisation des descripteurs pour {ref_path}")

            ref_arr = np.ascontiguousarray(current_descs, dtype=np.uint8)
            n_ref = int(ref_arr.shape[0])
            lib.add_reference(ref_arr.ctypes.data, n_ref)
            label = os.path.splitext(os.path.basename(ref_path))[0]
            ref_entries.append((label, n_ref))
            print(f"  - [{idx}] {label} : {n_ref} descripteurs verrouilles", flush=True)

        # Bascule propre PS_TO_PL -> HDMI exactement comme setupVideoSource() de script.ipynb
        # (sans dma.recvchannel.stop() ni reset SLCR qui bloquaient la transition)
        print("[PYNQ] Basculement du pipeline vers HDMI IN...", flush=True)
        tpg.write(0x0000, 0x0)
        if vdma.writechannel.running:
            vdma.writechannel.stop()

        stream_switch.write(0x040, 0x80000000)
        stream_switch.write(0x000, 0x2)

        if vdma.readchannel.running:
            vdma.readchannel.stop()

        image_resolution.channel1.write(IN_HEIGHT, 0xFFFFFFFF)
        image_resolution.channel2.write(IN_WIDTH, 0xFFFFFFFF)
        threshold_gpio.channel1.write(args.hdmi_fast_thresh, 0xFFFFFFFF)

        vdma.readchannel.mode = VideoMode(IN_WIDTH, IN_HEIGHT, 24)
        vdma.readchannel.start()
        vdma.readchannel.irqframecount = 1

        if not dma.recvchannel.running:
            dma.recvchannel.start()

        stream_switch.write(0x040, VideoSource.HDMI.value[1])
        stream_switch.write(0x000, 0x2)

        rc = lib.start_dma_worker(
            dummy_desc_buf.ctypes.data,
            dummy_desc_buf.physical_address,
            dummy_desc_buf.nbytes,
            5,  # ignorer les 5 premieres trames HDMI apres basculement
        )
        if rc != 0:
            raise RuntimeError("Impossible de demarrer le thread DMA C++.")

        print(
            f"[PYNQ] READY_FOR_HDMI (RefFAST={args.fast_thresh}, HdmiFAST={args.hdmi_fast_thresh}, "
            f"Hamming<{args.hamming_thresh}, Seuil>={args.min_matches} matches & >={args.min_ratio * 100:.0f}%, "
            f"Marge>={args.min_margin:.2f}x)",
            flush=True,
        )

        num_refs = len(ref_entries)
        matches_arr = (ctypes.c_int * num_refs)()
        scene_count = ctypes.c_int(0)
        dma_frames = ctypes.c_uint64(0)
        last_seq = ctypes.c_uint64(0)

        min_interval = 1.0 / max(args.report_hz, 0.5)
        last_print_t = 0.0
        last_status_key = None
        last_wait_msg_t = 0.0
        last_dma_frames = 0
        last_fps_t = time.monotonic()
        fps_est = 0.0
        prev_candidate_idx = -1
        confirm_streak = 0

        while not stop_requested:
            service_vdma_readchannel()

            ret = lib.match_latest_frame(
                args.hamming_thresh,
                15,
                ctypes.byref(last_seq),
                ctypes.byref(scene_count),
                ctypes.byref(dma_frames),
                matches_arr,
                num_refs,
            )

            now = time.monotonic()
            if ret == -1:
                if now - last_wait_msg_t >= 2.0:
                    vdma_sr = vdma.readchannel._mmio.read(0x34)
                    dma_sr = dma.read(0x34)
                    print(
                        f"[PYNQ] En attente d'un flux HDMI 1280x720 actif sur HDMI IN "
                        f"(VDMA_SR=0x{vdma_sr:04x}, DMA_SR=0x{dma_sr:04x})...",
                        flush=True,
                    )
                    last_wait_msg_t = now
                    last_status_key = "__WAIT__"
                continue
            elif ret == -4:
                continue
            elif ret < 0:
                break

            dt = now - last_fps_t
            if dt >= 0.5:
                cur_dma = dma_frames.value
                fps_est = (cur_dma - last_dma_frames) / dt
                last_dma_frames = cur_dma
                last_fps_t = now

            best_idx = -1
            best_ratio = 0.0
            best_matches = 0
            details = []

            for i, (label, n_ref) in enumerate(ref_entries):
                m = matches_arr[i]
                ratio = (m / n_ref) if n_ref > 0 else 0.0
                details.append(f"{label}:{m}/{n_ref}")
                if m >= args.min_matches and ratio >= args.min_ratio:
                    if ratio > best_ratio or (ratio == best_ratio and m > best_matches):
                        best_idx = i
                        best_ratio = ratio
                        best_matches = m

            if best_idx >= 0 and num_refs > 1:
                second_best_matches = max(
                    matches_arr[j] for j in range(num_refs) if j != best_idx
                )
                if best_matches < args.min_margin * second_best_matches:
                    best_idx = -1

            if best_idx >= 0 and best_idx == prev_candidate_idx:
                confirm_streak += 1
            elif best_idx >= 0:
                confirm_streak = 1
            else:
                confirm_streak = 0
            prev_candidate_idx = best_idx

            confirmed_idx = best_idx if confirm_streak >= 2 else -1

            if confirmed_idx >= 0:
                det_label, det_total = ref_entries[confirmed_idx]
                status_key = det_label
                msg = (
                    f"[DETECTE] {det_label} ({best_matches}/{det_total} = {best_ratio * 100:.1f}%) "
                    f"| Coins scene: {scene_count.value:4d} | HDMI ~{fps_est:4.1f} FPS "
                    f"| {', '.join(details)}"
                )
            else:
                status_key = "__NONE__"
                msg = (
                    f"[AUCUN OBJET] | Coins scene: {scene_count.value:4d} | HDMI ~{fps_est:4.1f} FPS "
                    f"| {', '.join(details)}"
                )

            if status_key != last_status_key or (now - last_print_t) >= min_interval:
                print(msg, flush=True)
                last_status_key = status_key
                last_print_t = now

    finally:
        lib.stop_dma_worker()
        try:
            tpg.write(0x00, 0x0)
            if vdma.readchannel.running:
                vdma.readchannel.stop()
            if vdma.writechannel.running:
                vdma.writechannel.stop()
        except Exception:
            pass
        dummy_desc_buf.close()
        print("\n[PYNQ] Ressources DMA/VDMA liberees.", flush=True)


if __name__ == "__main__":
    main()
