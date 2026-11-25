#include <opencv2/opencv.hpp>
#include <opencv2/features2d.hpp>
#include <vector>
#include <cstdint>
#include <chrono>
#include <iostream>
#include <omp.h>

struct __attribute__((packed, aligned(8))) RawOrbPacket {
    uint64_t desc[4];
    uint16_t x;
    uint16_t y;
    uint8_t  padding[28];
};

struct CachedDescriptor {
    uint64_t desc[4];
};

static std::vector<CachedDescriptor> g_ref_descs;
static std::vector<cv::Point2f> g_ref_pts;

extern "C" {

struct PipelineResult {
    int good_matches;
    int total_corners;
    double fps;
    int jpeg_size;
};

int init_ref_descriptors(const void* virt_dma_buf, uint32_t max_dma_bytes) {
    if (!virt_dma_buf || max_dma_bytes == 0) return 0;

    const RawOrbPacket* dma_packets = reinterpret_cast<const RawOrbPacket*>(virt_dma_buf);
    const size_t max_packets = max_dma_bytes / sizeof(RawOrbPacket);

    g_ref_descs.clear();
    g_ref_pts.clear();
    g_ref_descs.reserve(max_packets);
    g_ref_pts.reserve(max_packets);

    for (size_t i = 0; i < max_packets; ++i) {
        if (dma_packets[i].x != 0 || dma_packets[i].y != 0) {
            CachedDescriptor cd;
            cd.desc[0] = dma_packets[i].desc[0];
            cd.desc[1] = dma_packets[i].desc[1];
            cd.desc[2] = dma_packets[i].desc[2];
            cd.desc[3] = dma_packets[i].desc[3];
            g_ref_descs.push_back(cd);
            g_ref_pts.push_back(cv::Point2f(static_cast<float>(dma_packets[i].x), static_cast<float>(dma_packets[i].y)));
        }
    }

    return static_cast<int>(g_ref_descs.size());
}

int process_and_draw_frame(
    const void* virt_dma_buf,
    uint32_t max_dma_bytes,
    uint8_t* frame_data,
    int width,
    int height,
    int hamming_thresh,
    double ransac_thresh,
    uint8_t* out_jpeg_buf,
    int max_jpeg_buf_size,
    PipelineResult* out_result
) {
    if (!frame_data || width <= 0 || height <= 0) return -1;

    cv::Mat frame(height, width, CV_8UC3, frame_data);

    const RawOrbPacket* dma_packets = reinterpret_cast<const RawOrbPacket*>(virt_dma_buf);
    const size_t max_packets = max_dma_bytes / sizeof(RawOrbPacket);

    std::vector<CachedDescriptor> scene_descs;
    std::vector<cv::Point2f> scene_pts;
    scene_descs.reserve(max_packets);
    scene_pts.reserve(max_packets);

    for (size_t i = 0; i < max_packets; ++i) {
        if (dma_packets[i].x != 0 || dma_packets[i].y != 0) {
            CachedDescriptor cd;
            cd.desc[0] = dma_packets[i].desc[0];
            cd.desc[1] = dma_packets[i].desc[1];
            cd.desc[2] = dma_packets[i].desc[2];
            cd.desc[3] = dma_packets[i].desc[3];
            scene_descs.push_back(cd);
            scene_pts.push_back(cv::Point2f(static_cast<float>(dma_packets[i].x), static_cast<float>(dma_packets[i].y)));
        }
    }

    int total_corners = static_cast<int>(scene_descs.size());
    int num_ref = static_cast<int>(g_ref_descs.size());
    int good_matches = 0;

    std::vector<cv::Point2f> matched_ref_pts;
    std::vector<cv::Point2f> matched_scene_pts;

    if (num_ref > 0 && total_corners > 0) {
        std::vector<int> match_idx(num_ref, -1);

        #pragma omp parallel for schedule(dynamic) reduction(+:good_matches)
        for (int r = 0; r < num_ref; ++r) {
            uint64_t r0 = g_ref_descs[r].desc[0];
            uint64_t r1 = g_ref_descs[r].desc[1];
            uint64_t r2 = g_ref_descs[r].desc[2];
            uint64_t r3 = g_ref_descs[r].desc[3];

            int best_dist = 256;
            int best_s = -1;

            for (int s = 0; s < total_corners; ++s) {
                int dist = __builtin_popcountll(r0 ^ scene_descs[s].desc[0]) +
                           __builtin_popcountll(r1 ^ scene_descs[s].desc[1]) +
                           __builtin_popcountll(r2 ^ scene_descs[s].desc[2]) +
                           __builtin_popcountll(r3 ^ scene_descs[s].desc[3]);
                if (dist < best_dist) {
                    best_dist = dist;
                    best_s = s;
                }
            }

            if (best_s >= 0 && best_dist < hamming_thresh) {
                good_matches++;
                match_idx[r] = best_s;
            }
        }

        matched_ref_pts.reserve(good_matches);
        matched_scene_pts.reserve(good_matches);
        for (int r = 0; r < num_ref; ++r) {
            if (match_idx[r] != -1) {
                matched_ref_pts.push_back(g_ref_pts[r]);
                matched_scene_pts.push_back(scene_pts[match_idx[r]]);
            }
        }
    }

    std::vector<cv::Point2f> filtered_pts;
    if (matched_scene_pts.size() >= 8) {
        std::vector<uchar> inliers;
        cv::findFundamentalMat(matched_ref_pts, matched_scene_pts, cv::FM_RANSAC, ransac_thresh, 0.80, inliers);

        for (size_t i = 0; i < matched_scene_pts.size(); ++i) {
            if (!inliers.empty() && inliers[i]) {
                filtered_pts.push_back(matched_scene_pts[i]);
            }
        }
    } else {
        filtered_pts = matched_scene_pts;
    }

    if (filtered_pts.size() >= 4) {
        cv::RotatedRect box = cv::minAreaRect(filtered_pts);
        cv::Point2f vertices[4];
        box.points(vertices);
        for (int i = 0; i < 4; i++) {
            cv::line(frame, vertices[i], vertices[(i+1)%4], cv::Scalar(0, 255, 0), 3);
        }
    }

    std::string stat_str = "Matches: " + std::to_string(good_matches) +
                           " | Inliers: " + std::to_string(filtered_pts.size()) +
                           " | Corners: " + std::to_string(total_corners);
    cv::putText(frame, stat_str, cv::Point(15, 35), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 0), 2);

    static auto last_time = std::chrono::steady_clock::now();
    static bool first_frame = true;
    auto now = std::chrono::steady_clock::now();
    double current_fps = 0.0;
    if (!first_frame) {
        double elapsed_ms = std::chrono::duration<double, std::milli>(now - last_time).count();
        current_fps = (elapsed_ms > 0.0) ? (1000.0 / elapsed_ms) : 0.0;
    }
    first_frame = false;
    last_time = now;

    std::string fps_str = "FPS: " + std::to_string(static_cast<int>(current_fps));
    cv::putText(frame, fps_str, cv::Point(15, 70), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 255), 2);

    std::vector<uchar> compressed_img;
    std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 70};
    cv::imencode(".jpg", frame, compressed_img, params);

    int jpeg_size = static_cast<int>(compressed_img.size());
    if (jpeg_size <= max_jpeg_buf_size) {
        std::memcpy(out_jpeg_buf, compressed_img.data(), jpeg_size);
    } else {
        jpeg_size = 0;
    }

    out_result->good_matches = good_matches;
    out_result->total_corners = total_corners;
    out_result->fps = current_fps;
    out_result->jpeg_size = jpeg_size;

    return 0;
}

}