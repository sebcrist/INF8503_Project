#include <cstdint>
#include <cstring>
#include <vector>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <omp.h>

// Adresse physique de base de axi_dma_0
#define AXI_DMA_BASE_ADDR 0x40400000
#define AXI_DMA_MAP_SIZE  0x10000     // 64 Ko

// Offsets des registres S2MM
#define S2MM_DMACR        (0x30 >> 2)
#define S2MM_DMASR        (0x34 >> 2)
#define S2MM_DA           (0x48 >> 2)
#define S2MM_LENGTH       (0x58 >> 2)

// Masques de statut DMA
#define DMA_STATUS_HALTED 0x00000001
#define DMA_STATUS_IDLE   0x00000002
#define DMA_STATUS_IOC    0x00001000
#define DMA_STATUS_ERR    0x00000770

struct __attribute__((packed, aligned(8))) RawOrbPacket {
    uint64_t desc[4];      // 32 octets : Descripteur rBRIEF (256 bits)
    uint16_t x;            // 2 octets  : Coordonnée X
    uint16_t y;            // 2 octets  : Coordonnée Y
    uint8_t  padding[28];  // 28 octets : Padding pour atteindre 64 octets
};

struct CachedDescriptor {
    uint64_t desc[4];
};

inline void rearm_dma(volatile uint32_t* dma_regs, uint32_t phys_addr, uint32_t max_bytes) {
    dma_regs[S2MM_DMASR] = DMA_STATUS_IOC | DMA_STATUS_ERR;
    dma_regs[S2MM_DMACR] |= 0x1;
    dma_regs[S2MM_DA] = phys_addr;
    dma_regs[S2MM_LENGTH] = max_bytes;
    (void)dma_regs[S2MM_DMACR];
}

inline void reset_and_rearm_dma(volatile uint32_t* dma_regs, uint32_t phys_addr, uint32_t max_bytes) {
    dma_regs[S2MM_DMACR] = 0x4;
    while (dma_regs[S2MM_DMACR] & 0x4);
    rearm_dma(dma_regs, phys_addr, max_bytes);
}

extern "C" {

void run_fpga_bench(
    const void* virt_dma_buf,   // Pointeur virtuel de dummy_desc_buf
    uint32_t    phys_dma_addr,  // Adresse physique de dummy_desc_buf
    uint32_t    max_dma_bytes,  // Taille max du buffer DMA
    const void* ref_descs_ptr,  // Pointeur vers les descripteurs de référence (N_ref * 32 octets)
    int         num_ref,        // Nombre de descripteurs de référence
    int         num_frames,     // Nombre de frames à mesurer
    int         warmup_frames,
    int         scenario_mode
) {
    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        std::cerr << "[Erreur C++] Impossible d'ouvrir /dev/mem. Lancez avec sudo." << std::endl;
        return;
    }

    void* map_base = mmap(NULL, AXI_DMA_MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, AXI_DMA_BASE_ADDR);
    if (map_base == MAP_FAILED) {
        std::cerr << "[Erreur C++] Échec du mmap sur axi_dma_0 (0x40400000)." << std::endl;
        close(fd);
        return;
    }

    volatile uint32_t* dma_regs = reinterpret_cast<volatile uint32_t*>(map_base);
    const RawOrbPacket* dma_packets = reinterpret_cast<const RawOrbPacket*>(virt_dma_buf);

    std::vector<CachedDescriptor> ref_descs(num_ref);
    if (ref_descs_ptr != nullptr && num_ref > 0) {
        std::memcpy(ref_descs.data(), ref_descs_ptr, num_ref * sizeof(CachedDescriptor));
    }

    const size_t max_packets = max_dma_bytes / sizeof(RawOrbPacket);
    std::vector<RawOrbPacket> cached_packets(max_packets);
    std::vector<CachedDescriptor> scene_descs;
    scene_descs.reserve(max_packets);

    std::vector<double> t_wait(num_frames, 0.0);
    std::vector<double> t_copy(num_frames, 0.0);
    std::vector<double> t_match(num_frames, 0.0);
    std::vector<double> t_total(num_frames, 0.0);

    size_t total_valid_descs = 0;
    size_t total_good_matches = 0;

    uint32_t init_sr = dma_regs[S2MM_DMASR];
    if ((init_sr & DMA_STATUS_ERR) || (init_sr & DMA_STATUS_HALTED)) {
        reset_and_rearm_dma(dma_regs, phys_dma_addr, max_dma_bytes);
    } else if ((init_sr & DMA_STATUS_IDLE) || (init_sr & DMA_STATUS_IOC)) {
        rearm_dma(dma_regs, phys_dma_addr, max_dma_bytes);
    }

    int total_iterations = warmup_frames + num_frames;
    using clock = std::chrono::steady_clock;
    clock::time_point bench_start;

    for (int iter = 0; iter < total_iterations; ++iter) {
        if (iter == warmup_frames) {
            bench_start = clock::now();
        }
        auto f_start = clock::now();

        bool saw_busy = false;
        uint32_t status = dma_regs[S2MM_DMASR];
        int timeout_guard = 0;
        while (true) {
            if ((status & DMA_STATUS_ERR) || (status & DMA_STATUS_HALTED)) {
                reset_and_rearm_dma(dma_regs, phys_dma_addr, max_dma_bytes);
                saw_busy = false;
                timeout_guard = 0;
            } else {
                if ((status & DMA_STATUS_IDLE) == 0) {
                    saw_busy = true;
                }
                if ((status & DMA_STATUS_IOC) || (saw_busy && (status & DMA_STATUS_IDLE))) {
                    break;
                }
            }
            if (++timeout_guard > 20000000) {
                reset_and_rearm_dma(dma_regs, phys_dma_addr, max_dma_bytes);
                break;
            }
            status = dma_regs[S2MM_DMASR];
        }
        auto after_wait = clock::now();

        uint32_t bytes_transferred = dma_regs[S2MM_LENGTH];
        size_t num_packets = bytes_transferred / sizeof(RawOrbPacket);
        if (num_packets > max_packets) num_packets = max_packets;

        size_t actual_corners = (num_packets > 0) ? (num_packets - 3) : 0;

        scene_descs.clear();
        if (scenario_mode > 0 && actual_corners > 0) {
            std::memcpy(cached_packets.data(), dma_packets, actual_corners * sizeof(RawOrbPacket));
            for (size_t i = 0; i < actual_corners; ++i) {
                if (cached_packets[i].x != 0 || cached_packets[i].y != 0) {
                    CachedDescriptor cd;
                    cd.desc[0] = cached_packets[i].desc[0];
                    cd.desc[1] = cached_packets[i].desc[1];
                    cd.desc[2] = cached_packets[i].desc[2];
                    cd.desc[3] = cached_packets[i].desc[3];
                    scene_descs.push_back(cd);
                }
            }
        }

        rearm_dma(dma_regs, phys_dma_addr, max_dma_bytes);
        auto after_copy = clock::now();

        int frame_good_matches = 0;
        const size_t n_scene = (scenario_mode > 0) ? scene_descs.size() : actual_corners;

        if (scenario_mode > 0 && num_ref > 0 && n_scene > 0) {
            int threads = (scenario_mode == 2) ? 2 : 1;

            #pragma omp parallel for num_threads(threads) reduction(+:frame_good_matches) schedule(static)
            for (int r = 0; r < num_ref; ++r) {
                const uint64_t r0 = ref_descs[r].desc[0];
                const uint64_t r1 = ref_descs[r].desc[1];
                const uint64_t r2 = ref_descs[r].desc[2];
                const uint64_t r3 = ref_descs[r].desc[3];

                int best_dist = 256;
                int best_s = -1;

                for (size_t s = 0; s < n_scene; ++s) {
                    int dist = __builtin_popcountll(r0 ^ scene_descs[s].desc[0]) +
                               __builtin_popcountll(r1 ^ scene_descs[s].desc[1]) +
                               __builtin_popcountll(r2 ^ scene_descs[s].desc[2]) +
                               __builtin_popcountll(r3 ^ scene_descs[s].desc[3]);
                    if (dist < best_dist) {
                        best_dist = dist;
                        best_s = static_cast<int>(s);
                    }
                }

                if (best_s >= 0 && best_dist < 60) {
                    const uint64_t s0 = scene_descs[best_s].desc[0];
                    const uint64_t s1 = scene_descs[best_s].desc[1];
                    const uint64_t s2 = scene_descs[best_s].desc[2];
                    const uint64_t s3 = scene_descs[best_s].desc[3];

                    bool is_mutual_best = true;
                    for (int r2_idx = 0; r2_idx < num_ref; ++r2_idx) {
                        if (r2_idx == r) continue;
                        int dist_back = __builtin_popcountll(s0 ^ ref_descs[r2_idx].desc[0]) +
                                        __builtin_popcountll(s1 ^ ref_descs[r2_idx].desc[1]) +
                                        __builtin_popcountll(s2 ^ ref_descs[r2_idx].desc[2]) +
                                        __builtin_popcountll(s3 ^ ref_descs[r2_idx].desc[3]);
                        if (dist_back < best_dist) {
                            is_mutual_best = false;
                            break;
                        }
                    }

                    if (is_mutual_best) {
                        frame_good_matches++;
                    }
                }
            }
        }
        auto after_match = clock::now();

        if (iter >= warmup_frames) {
            int f = iter - warmup_frames;
            t_wait[f]  = std::chrono::duration<double, std::micro>(after_wait - f_start).count();
            t_copy[f]  = std::chrono::duration<double, std::micro>(after_copy - after_wait).count();
            t_match[f] = std::chrono::duration<double, std::micro>(after_match - after_copy).count();
            t_total[f] = std::chrono::duration<double, std::micro>(after_match - f_start).count();

            total_valid_descs += n_scene;
            total_good_matches += frame_good_matches;
        }
    }

    auto bench_end = clock::now();
    double total_sec = std::chrono::duration<double>(bench_end - bench_start).count();

    munmap(map_base, AXI_DMA_MAP_SIZE);
    close(fd);

    auto print_stats = [&](const char* name, const std::vector<double>& v) {
        double min_v = *std::min_element(v.begin(), v.end());
        double max_v = *std::max_element(v.begin(), v.end());
        double avg_v = std::accumulate(v.begin(), v.end(), 0.0) / v.size();
        std::cout << std::left << std::setw(18) << name
                  << " | Min: " << std::setw(8) << std::fixed << std::setprecision(1) << min_v << " us"
                  << " | Moy: " << std::setw(8) << avg_v << " us"
                  << " | Max: " << std::setw(8) << max_v << " us\n";
    };

    std::cout << "RESULTATS DU BENCHMARK C++ / FPGA\n";
    std::cout << "Frames testees          : " << num_frames << "\n";
    std::cout << "Descripteurs Ref        : " << num_ref << "\n";
    std::cout << "Descripteurs Scene (moy): " << (total_valid_descs / num_frames) << " / frame\n";
    std::cout << "Matches < 60 (moy)      : " << (total_good_matches / num_frames) << " / frame\n";
    std::cout << "--------------------------------------------------------------------\n";
    print_stats("1. Attente DMA", t_wait);
    print_stats("2. Copie + Rearm", t_copy);
    print_stats("3. Matching ARM", t_match);
    std::cout << "--------------------------------------------------------------------\n";
    print_stats("Total / Frame", t_total);
    std::cout << "Debit global            : " << std::fixed << std::setprecision(2)
              << (num_frames / total_sec) << " FPS\n";
    std::cout << "====================================================================\n";
}

} // extern "C"
