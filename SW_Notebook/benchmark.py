# run_bench.py — Lancer en SSH avec : sudo python3 run_bench.py
from pynq import Overlay, allocate, PL
from pynq.lib.video import VideoMode
import cv2
import numpy as np
from enum import Enum
import time
import os
import ctypes

PROJECT_DIR = "/home/xilinx/jupyter_notebooks/PynqZ2-ORB-Accelerator"
IN_WIDTH, IN_HEIGHT = 1280, 720
WARMUP_FRAMES = 20
BENCH_FRAMES = 500

class VideoSource(Enum):
    HDMI = (0, 0)
    TPG_COLOR_BAR_BOX = (1, 1)
    TPG_GRID_BOX = (2, 1)
    PS_TO_PL = (3, 2)

print("Reset du PL et chargement du bitstream...")
PL.reset()
overlay = Overlay(PROJECT_DIR + "/VivadoHW.bit")
overlay.download()

# Chargement de la librairie C++
lib = ctypes.CDLL("./libfpga_bench.so")
lib.run_fpga_bench.argtypes = [
    ctypes.c_void_p,   # virt_dma_buf : Adresse virtuelle de dummy_desc_buf
    ctypes.c_uint32,   # phys_dma_addr: Adresse physique de dummy_desc_buf
    ctypes.c_uint32,   # max_dma_bytes: Taille max en octets (262144)
    ctypes.c_void_p,   # ref_descs_ptr: Pointeur vers les descripteurs de référence (uint8 contigu)
    ctypes.c_int,      # num_ref      : Nombre de descripteurs de référence
    ctypes.c_int,      # num_frames   : Nombre de frames à mesurer (500)
    ctypes.c_int,      # warmup_frames: Nombre de frames de chauffe (20)
    ctypes.c_int       # scenario_mode: 0 = FPGA seul, 1 = 1-Thread, 2 = 2-Threads OpenMP
]

tpg = overlay.v_tpg_0
vdma = overlay.dma_block.axi_vdma_0
dma = overlay.dma_block.axi_dma_0
imageResolution = overlay.axi_gpios.axi_gpio_1
threshold_gpio = overlay.axi_gpios.axi_gpio_2
stream_switch = overlay.orb_pipeline.axis_switch_0

threshold_gpio.channel1.write(60, 0xFFFFFFFF)
dummy_desc_buf = allocate(shape=(65536,), dtype=np.uint32)

# Armer le DMA dès le départ (comme dans script.ipynb) pour éviter toute
# contre-pression sur axis_broadcaster_0 avant le premier transfert VDMA.
if not dma.recvchannel.running:
    dma.recvchannel.start()
dma.recvchannel.transfer(dummy_desc_buf)

IMAGES_DIR = PROJECT_DIR + "/images"
VALID_EXT = ('.jpg', '.jpeg', '.png', '.bmp')
available_images = sorted([os.path.join(IMAGES_DIR, f) for f in os.listdir(IMAGES_DIR) if f.lower().endswith(VALID_EXT)])
ref_images = [f for f in available_images if '_ref' in os.path.basename(f).lower()]
scene_images = [f for f in available_images if '_ref' not in os.path.basename(f).lower()]

if not ref_images or not scene_images:
    raise RuntimeError(f"Images manquantes dans {IMAGES_DIR} (besoin d'une image '*_ref.*' et d'une image de scène).")

current_pts = None
current_descs = None
current_desc_count = 0

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
    """Extrait les coordonnées (X, Y) et les descripteurs 256-bit (32 octets) lors du bootstrap Python."""
    if count <= 0:
        return None, None
    raw = np.frombuffer(buf, dtype=np.uint8, count=count * 64).reshape(count, 64).copy()
    descs = np.ascontiguousarray(raw[:, :32])
    pts = raw[:, 32:36].copy().view(np.uint16).astype(np.float32)
    valid = (pts[:, 0] != 0) | (pts[:, 1] != 0)
    return pts[valid], np.ascontiguousarray(descs[valid])

def process_and_rearm_dma():
    global current_desc_count, current_pts, current_descs
    status = dma.read(0x34)

    if (status & 0x1) or (status & 0x70):
        print(f"[PY] Reset DMA (DMASR=0x{status:08x})", flush=True)
        dma.write(0x30, 0x4)
        t_rst = time.monotonic()
        while dma.read(0x30) & 0x4:
            if time.monotonic() - t_rst > 2.0:
                raise TimeoutError("Timeout lors du reset logiciel de axi_dma_0.")
        dma.recvchannel.start()
        dma.recvchannel.transfer(dummy_desc_buf)
        return

    if (status & 0x2):
        nbrOfDescriptors = dma.read(0x58) // 64
        if nbrOfDescriptors > 0:
            actual_corners = max(0, nbrOfDescriptors - 1)
            current_pts, current_descs = parse_descriptors(dummy_desc_buf, actual_corners)
            current_desc_count = len(current_descs) if current_descs is not None else 0
        else:
            current_desc_count = 0
            current_pts, current_descs = None, None

        dma.recvchannel.transfer(dummy_desc_buf)

def send_image_to_vdma(image_path, enable_readchannel=True):
    """Charge une image 720p dans le canal MM2S de la VDMA."""
    print(f"[PY] Configuration VDMA/Switch pour : {os.path.basename(image_path)}", flush=True)
    tpg.write(0x0000, 0x0)
    stream_switch.write(0x040, 0x80000000)
    stream_switch.write(0x000, 0x2)

    if vdma.readchannel.running:
        vdma.readchannel.stop()

    imageResolution.channel1.write(IN_HEIGHT, 0xFFFFFFFF)
    imageResolution.channel2.write(IN_WIDTH, 0xFFFFFFFF)
    tpg.write(0x0010, IN_HEIGHT)
    tpg.write(0x0018, IN_WIDTH)

    if not dma.recvchannel.running:
        dma.recvchannel.start()
    if dma.recvchannel.idle:
        dma.recvchannel.transfer(dummy_desc_buf)

    if not vdma.writechannel.running:
        vdma.writechannel.mode = VideoMode(IN_WIDTH, IN_HEIGHT, 24)
        vdma.writechannel.start()

    if enable_readchannel:
        vdma.readchannel.mode = VideoMode(IN_WIDTH, IN_HEIGHT, 24)
        vdma.readchannel.start()
        vdma.readchannel.irqframecount = 1

    stream_switch.write(0x040, VideoSource.PS_TO_PL.value[1])
    stream_switch.write(0x000, 0x2)

    img = cv2.imread(image_path)
    if img is None:
        raise FileNotFoundError(f"Impossible de charger l'image : {image_path}")
    img = letterbox(img, IN_WIDTH, IN_HEIGHT)
    frame_rgb = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)

    buf = vdma.writechannel.newframe()
    np.copyto(buf, frame_rgb)
    vdma.writechannel.writeframe(buf)
    print("[PY] Frame injectée dans vdma.writechannel.", flush=True)

def wait_vdma_frame(timeout_sec=5.0):
    """Attend la prochaine frame S2MM de la VDMA tout en réarmant le DMA des descripteurs."""
    t0 = time.monotonic()
    while (vdma.readchannel._mmio.read(0x34) & 0x1000) == 0:
        process_and_rearm_dma()
        if time.monotonic() - t0 > timeout_sec:
            vdma_s2mm_cr = vdma.readchannel._mmio.read(0x30)
            vdma_s2mm_sr = vdma.readchannel._mmio.read(0x34)
            vdma_mm2s_cr = vdma.writechannel._mmio.read(0x00)
            vdma_mm2s_sr = vdma.writechannel._mmio.read(0x04)
            dma_cr = dma.read(0x30)
            dma_sr = dma.read(0x34)
            raise TimeoutError(
                f"Timeout ({timeout_sec:.1f}s) en attente d'une frame VDMA S2MM | "
                f"VDMA S2MM CR=0x{vdma_s2mm_cr:08x} SR=0x{vdma_s2mm_sr:08x} | "
                f"VDMA MM2S CR=0x{vdma_mm2s_cr:08x} SR=0x{vdma_mm2s_sr:08x} | "
                f"DMA S2MM CR=0x{dma_cr:08x} SR=0x{dma_sr:08x}"
            )
        time.sleep(0.001)

    _ = vdma.readchannel.readframe()
    process_and_rearm_dma()

ref_image_path = ref_images[0]
print(f"\nChargement de l'image de référence : {ref_image_path}", flush=True)
send_image_to_vdma(ref_image_path, enable_readchannel=True)

timeout_counter = 0
stable_frames = 0
last_corner_count = -1

while stable_frames < 5 and timeout_counter < 60:
    wait_vdma_frame(timeout_sec=5.0)
    timeout_counter += 1

    if current_desc_count > 0:
        if current_desc_count == last_corner_count:
            stable_frames += 1
        else:
            stable_frames = 1
        last_corner_count = current_desc_count
        print(f"Stabilisation Frame {timeout_counter}: {current_desc_count} descripteurs ({stable_frames}/5)")
    else:
        stable_frames = 0
        last_corner_count = 0

if current_descs is None or stable_frames < 5:
    raise RuntimeError("Échec de la stabilisation des descripteurs de référence.")

ref_descs = np.ascontiguousarray(current_descs, dtype=np.uint8)
num_ref = ref_descs.shape[0]
print(f"{num_ref} descripteurs de référence verrouillés.")

coke_scene = [f for f in scene_images if os.path.basename(f).lower() == 'park_and_coke.jpg']
scene_image_path = coke_scene[0] if coke_scene else scene_images[0]
print(f"\nInjection de l'image de test dans la VDMA (MM2S) : {scene_image_path}", flush=True)

send_image_to_vdma(scene_image_path, enable_readchannel=True)

# Laisser passer quelques frames VDMA pour purger l'ancienne image avant le chrono C++
for _ in range(3):
    wait_vdma_frame(timeout_sec=5.0)
print(f"[PY] Scène prête : {current_desc_count} descripteurs vus avant le benchmark C++.", flush=True)

# Attendre la fin éventuelle du transfert DMA en cours puis nettoyer les flags IRQ
t_dma = time.monotonic()
while (dma.read(0x34) & 0x2) == 0:
    if time.monotonic() - t_dma > 1.0:
        break
    time.sleep(0.001)
if dma.read(0x34) & 0x7000:
    dma.write(0x34, 0x7000)

virt_addr = dummy_desc_buf.ctypes.data
phys_addr = dummy_desc_buf.physical_address
max_bytes = dummy_desc_buf.nbytes
ref_ptr   = ref_descs.ctypes.data

try:
    print("\nLancement Scénario 1 : Plafond matériel pur (FPGA Extraction Only)")
    lib.run_fpga_bench(virt_addr, phys_addr, max_bytes, ref_ptr, num_ref, BENCH_FRAMES, WARMUP_FRAMES, 0)

    print("\nLancement Scénario 2a : Système complet (FPGA + Matching C++ 1-Thread)")
    lib.run_fpga_bench(virt_addr, phys_addr, max_bytes, ref_ptr, num_ref, BENCH_FRAMES, WARMUP_FRAMES, 1)

    print("\nLancement Scénario 2b : Système complet (FPGA + Matching C++ 2-Threads)")
    lib.run_fpga_bench(virt_addr, phys_addr, max_bytes, ref_ptr, num_ref, BENCH_FRAMES, WARMUP_FRAMES, 2)

finally:
    tpg.write(0x00, 0x0)
    if vdma.readchannel.running: vdma.readchannel.stop()
    if vdma.writechannel.running: vdma.writechannel.stop()
    dummy_desc_buf.close()
    print("\nRessources DMA/VDMA libérées.")
