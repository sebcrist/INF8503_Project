#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <cmath>
#include <thread>
#include "ap_int.h"
#include "hls_stream.h"
#include "ap_axi_sdata.h"
#include "common/xf_common.hpp"
#include "common/xf_utility.hpp"
#include "imgproc/xf_custom_convolution.hpp"
#include "imgproc/xf_resize.hpp"
#include "features/xf_fast.hpp"
#include <opencv2/opencv.hpp>
#include <opencv2/features2d.hpp>

constexpr int IMG_WIDTH = 1280;
constexpr int IMG_HEIGHT = 720;
constexpr int DUMMY_DESC_COUNT = 5000;
constexpr int DEFAULT_HAMMING_THRESH = 60;
constexpr int MAX_FEATURES_OPENCV = 100000;
constexpr int ORB_LEVELS = 2;
constexpr float SPATIAL_TOLERANCE_PX = 2.0f; 

constexpr int BS_MAX_ITER = 10;
constexpr int BS_LOW_THRESH = 1;
constexpr int BS_HIGH_THRESH = 255;
constexpr int DEFAULT_FALLBACK_THRESH = 50;

constexpr int SWEEP_THRESHOLDS[] = {30, 50, 70};
constexpr int SWEEP_HAMMING[] = {10, 15, 20, 25, 30, 35, 40};

typedef ap_axiu<24, 1, 1, 1> axisIn;
typedef ap_axiu<8, 1, 1, 1> axisOut;
typedef ap_axiu<8, 1, 1, 1> axiData;
typedef ap_axiu<512, 1, 1, 1> axiDescriptor;

struct ResultRowA {
    int thresh;
    std::string impl;
    int scene_descriptors;
    int ref_descriptors;
    int matches;
    float match_scene_pct;
    float match_ref_pct;
};

struct ResultRowB {
    int scene_descriptors;
    std::string impl;
    int thresh;
    int ref_descriptors;
    int matches;
    float match_scene_pct;
    float match_ref_pct;
    float overlap_scene;
    float overlap_ref;
};

struct HWResult {
    int thresh;
    std::vector<cv::KeyPoint> kp1;
    cv::Mat desc1;
    std::vector<cv::KeyPoint> kp2;
    cv::Mat desc2;
    int matches;
};

struct SWWorkerOutput {
    ResultRowA rowA_sw;
    ResultRowB rowB_sw;
    std::vector<cv::KeyPoint> kp1_sw_c;
    std::vector<cv::KeyPoint> kp2_sw_c;
    cv::Mat desc1_sw_c;
    cv::Mat desc2_sw_c;
};

void rgb_to_gray(hls::stream<axisIn>& streamIn, hls::stream<axisOut>& streamOut, int rows, int cols);
void gauss_resize(hls::stream<axiData>& streamIn, hls::stream<axiData>& streamOut, hls::stream<axiData>& streamOutScaledDown, int rows, int cols, int* rowsScaledDown, int* colsScaledDown);
void fast_corner(hls::stream<axiData>& streamIn, hls::stream<axiData>& streamOut, hls::stream<axiData>& cornerMask, int rows, int cols, int fastThreshold);
void r_brief_stage0(hls::stream<axiData>& streamIn, hls::stream<axiData>& cornerMask, hls::stream<axiDescriptor>& descriptors);
void r_brief_stage1(hls::stream<axiData>& streamIn, hls::stream<axiData>& cornerMask, hls::stream<axiDescriptor>& descriptors);
void r_brief_stage2(hls::stream<axiData>& streamIn, hls::stream<axiData>& cornerMask, hls::stream<axiDescriptor>& descriptors);
void descriptor_merger(hls::stream<axiDescriptor>& stream_stage0, hls::stream<axiDescriptor>& stream_stage1, hls::stream<axiDescriptor>& stream_stage2, hls::stream<axiDescriptor>& stream_out);

void run_hardware_pipeline(const cv::Mat& input_img, int fastThreshold, std::vector<cv::KeyPoint>& hw_keypoints, cv::Mat& hw_descriptors) {
    int rows = input_img.rows;
    int cols = input_img.cols;
    
    hls::stream<axisIn> s_axis_video;
    hls::stream<axisOut> s_gray_out;
    hls::stream<axiData> s_gray_to_gauss;
    hls::stream<axiData> s_stage0_in, s_stage1_in;
    int rowsScaledDown, colsScaledDown;
    
    hls::stream<axiData> s_stage0_pixels, s_stage0_mask;
    hls::stream<axiData> s_stage1_pixels, s_stage1_mask;
    hls::stream<axiData> s_stage2_pixels, s_stage2_mask;
    hls::stream<axiDescriptor> s_desc0, s_desc1, s_desc2, s_final_descriptors;

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            cv::Vec3b pix = input_img.at<cv::Vec3b>(r, c);
            axisIn val;
            val.data(7, 0) = pix[0];
            val.data(15, 8) = pix[1];
            val.data(23, 16) = pix[2];
            val.keep = -1;
            val.last = (c == cols - 1) ? 1 : 0;
            val.user = (r == 0 && c == 0) ? 1 : 0;
            s_axis_video.write(val);
        }
    }

    rgb_to_gray(s_axis_video, s_gray_out, rows, cols);
    while (!s_gray_out.empty()) {
        axisOut tmp = s_gray_out.read();
        axiData nxt; 
        nxt.data = tmp.data; 
        nxt.user = tmp.user; 
        nxt.last = tmp.last; 
        nxt.keep = tmp.keep;
        s_gray_to_gauss.write(nxt);
    }

    gauss_resize(s_gray_to_gauss, s_stage0_in, s_stage1_in, rows, cols, &rowsScaledDown, &colsScaledDown);
    
    axiData dummy_pixel; 
    dummy_pixel.data = 0; 
    dummy_pixel.user = 0; 
    dummy_pixel.last = 0; 
    dummy_pixel.keep = -1;
    for (int i = 0; i < DUMMY_DESC_COUNT; i++) { 
        s_stage0_in.write(dummy_pixel); 
        s_stage1_in.write(dummy_pixel); 
    }

    fast_corner(s_stage0_in, s_stage0_pixels, s_stage0_mask, rows, cols, fastThreshold);
    fast_corner(s_stage1_in, s_stage1_pixels, s_stage1_mask, rowsScaledDown, colsScaledDown, fastThreshold);

    s_stage2_pixels.write(axiData()); 
    s_stage2_mask.write(axiData());

    r_brief_stage0(s_stage0_pixels, s_stage0_mask, s_desc0);
    r_brief_stage1(s_stage1_pixels, s_stage1_mask, s_desc1);
    
    for (int i = 0; i < DUMMY_DESC_COUNT; i++) {
        axiDescriptor dummy_desc; 
        dummy_desc.data = 0; 
        dummy_desc.user = 0; 
        dummy_desc.last = (i == (DUMMY_DESC_COUNT - 1)) ? 1 : 0;
        s_desc2.write(dummy_desc);
    }
    
    while (!s_desc0.empty() || !s_desc1.empty() || !s_desc2.empty()) {
        descriptor_merger(s_desc0, s_desc1, s_desc2, s_final_descriptors);
    }

    hw_descriptors = cv::Mat(0, 32, CV_8UC1);
    while (!s_final_descriptors.empty()) {
        axiDescriptor d = s_final_descriptors.read();
        if (d.data == 0) continue; 
        
        int final_x = d.data.range(271, 256).to_int();
        int final_y = d.data.range(287, 272).to_int();
        hw_keypoints.push_back(cv::KeyPoint(final_x, final_y, 1.0f));
        
        cv::Mat single_desc(1, 32, CV_8UC1);
        for (int i = 0; i < 32; i++) {
            single_desc.at<uchar>(0, i) = static_cast<uchar>(d.data.range(i * 8 + 7, i * 8).to_int());
        }
        hw_descriptors.push_back(single_desc);
    }

    while (!s_axis_video.empty())       s_axis_video.read();
    while (!s_gray_out.empty())         s_gray_out.read();
    while (!s_gray_to_gauss.empty())    s_gray_to_gauss.read();
    while (!s_stage0_in.empty())        s_stage0_in.read();
    while (!s_stage1_in.empty())        s_stage1_in.read();
    while (!s_stage0_pixels.empty())    s_stage0_pixels.read();
    while (!s_stage0_mask.empty())      s_stage0_mask.read();
    while (!s_stage1_pixels.empty())    s_stage1_pixels.read();
    while (!s_stage1_mask.empty())      s_stage1_mask.read();
    while (!s_stage2_pixels.empty())    s_stage2_pixels.read();
    while (!s_stage2_mask.empty())      s_stage2_mask.read();
    while (!s_desc0.empty())            s_desc0.read();
    while (!s_desc1.empty())            s_desc1.read();
    while (!s_desc2.empty())            s_desc2.read();
    while (!s_final_descriptors.empty()) s_final_descriptors.read();
}

void evaluate_matches(const cv::Mat& img1, const std::vector<cv::KeyPoint>& kpts1, const cv::Mat& desc1,
                      const cv::Mat& img2, const std::vector<cv::KeyPoint>& kpts2, const cv::Mat& desc2,
                      const std::string& out_filename, int hamming_thresh, int& out_hamming_matches) 
{
    if (desc1.empty() || desc2.empty()) { out_hamming_matches = 0; return; }

    cv::BFMatcher matcher(cv::NORM_HAMMING, true); 
    std::vector<cv::DMatch> matches;
    matcher.match(desc1, desc2, matches);

    std::vector<cv::DMatch> good_matches;
    for (size_t i = 0; i < matches.size(); i++) {
        if (matches[i].distance < hamming_thresh) { 
            good_matches.push_back(matches[i]);
        }
    }
    out_hamming_matches = good_matches.size();

    if (!out_filename.empty()) {
        cv::Mat img_matches;
        cv::drawMatches(img1, kpts1, img2, kpts2, good_matches, img_matches, 
                        cv::Scalar::all(-1), cv::Scalar::all(-1), std::vector<char>(), cv::DrawMatchesFlags::NOT_DRAW_SINGLE_POINTS);
        cv::imwrite(out_filename, img_matches);
    }
}

void draw_all_keypoints(const cv::Mat& img, const std::vector<cv::KeyPoint>& kpts, const std::string& out_filename) {
    if (img.empty() || kpts.empty() || out_filename.empty()) return;
    cv::Mat img_with_kpts;
    cv::drawKeypoints(img, kpts, img_with_kpts, cv::Scalar(0, 0, 255), cv::DrawMatchesFlags::DEFAULT);
    cv::imwrite(out_filename, img_with_kpts);
}

float calculate_spatial_overlap(const std::vector<cv::KeyPoint>& kp_hw, const std::vector<cv::KeyPoint>& kp_sw, float tolerance, bool denom_sw = false) {
    if (kp_hw.empty() || kp_sw.empty()) return 0.0f;
    int overlap_count = 0;
    for (const auto& ksw : kp_sw) {
        for (const auto& khw : kp_hw) {
            float dx = khw.pt.x - ksw.pt.x;
            float dy = khw.pt.y - ksw.pt.y;
            if (std::sqrt(dx*dx + dy*dy) <= tolerance) {
                overlap_count++;
                break; 
            }
        }
    }
    float base = denom_sw ? static_cast<float>(kp_sw.size()) : static_cast<float>(kp_hw.size());
    return (base > 0.0f) ? (static_cast<float>(overlap_count) / base * 100.0f) : 0.0f;
}

void find_yield_matched_opencv(cv::Ptr<cv::ORB> orb, const cv::Mat& img1, const cv::Mat& img2, int target_yield,
                               std::vector<cv::KeyPoint>& best_kp1, cv::Mat& best_desc1,
                               std::vector<cv::KeyPoint>& best_kp2, cv::Mat& best_desc2, int& final_th) {
    int low = BS_LOW_THRESH, high = BS_HIGH_THRESH;
    int best_diff = 999999;
    final_th = DEFAULT_FALLBACK_THRESH; 

    for (int iter = 0; iter < BS_MAX_ITER; iter++) {
        if (low > high) break;
        int mid = (low + high) / 2;
        orb->setFastThreshold(mid);
        
        std::vector<cv::KeyPoint> kp1; cv::Mat desc1;
        orb->detectAndCompute(img1, cv::noArray(), kp1, desc1);
        
        int count = kp1.size();
        int diff = std::abs(count - target_yield);
        
        if (diff < best_diff) {
            best_diff = diff;
            final_th = mid;
            best_kp1 = kp1; best_desc1 = desc1;
        }
        
        if (count == target_yield) break;
        if (count > target_yield) low = mid + 1; 
        else high = mid - 1;                     
    }
    orb->setFastThreshold(final_th);
    orb->detectAndCompute(img2, cv::noArray(), best_kp2, best_desc2);
}

void sw_worker(int th, const cv::Mat& img1, const cv::Mat& img2, const HWResult& hw, bool is_expC_baseline, SWWorkerOutput* out) {
    cv::Ptr<cv::ORB> orb_sw = cv::ORB::create(MAX_FEATURES_OPENCV); 
    orb_sw->setFastThreshold(th);
    orb_sw->setNLevels(ORB_LEVELS); 
    std::vector<cv::KeyPoint> kp1_sw, kp2_sw; 
    cv::Mat desc1_sw, desc2_sw; 
    int match_sw;
    orb_sw->detectAndCompute(img1, cv::noArray(), kp1_sw, desc1_sw);
    orb_sw->detectAndCompute(img2, cv::noArray(), kp2_sw, desc2_sw);
    evaluate_matches(img1, kp1_sw, desc1_sw, img2, kp2_sw, desc2_sw, "expA_th" + std::to_string(th) + "_sw.jpg", DEFAULT_HAMMING_THRESH, match_sw);
    
    float m_scene_a = kp1_sw.size() > 0 ? (float)match_sw / kp1_sw.size() * 100.0f : 0.0f;
    float m_ref_a = kp2_sw.size() > 0 ? (float)match_sw / kp2_sw.size() * 100.0f : 0.0f;
    out->rowA_sw = {th, "SW", (int)kp1_sw.size(), (int)kp2_sw.size(), match_sw, m_scene_a, m_ref_a};

    int target_yield = hw.kp1.size();
    if (target_yield > 0) {
        cv::Ptr<cv::ORB> orb_sw_dyn = cv::ORB::create(MAX_FEATURES_OPENCV); 
        orb_sw_dyn->setNLevels(ORB_LEVELS); 
        std::vector<cv::KeyPoint> kp1_swx, kp2_swx; 
        cv::Mat desc1_swx, desc2_swx; 
        int match_swx, th_swx;
        find_yield_matched_opencv(orb_sw_dyn, img1, img2, target_yield, kp1_swx, desc1_swx, kp2_swx, desc2_swx, th_swx);
        
        evaluate_matches(img1, kp1_swx, desc1_swx, img2, kp2_swx, desc2_swx, "expB_tgt" + std::to_string(target_yield) + "_sw.jpg", DEFAULT_HAMMING_THRESH, match_swx);
        draw_all_keypoints(img1, kp1_swx, "expB_tgt" + std::to_string(target_yield) + "_kpts_sw.jpg");

        float overlap_sc = calculate_spatial_overlap(hw.kp1, kp1_swx, SPATIAL_TOLERANCE_PX, false);
        float overlap_rf = calculate_spatial_overlap(hw.kp2, kp2_swx, SPATIAL_TOLERANCE_PX, true);
        
        float m_scene_b = kp1_swx.size() > 0 ? (float)match_swx / kp1_swx.size() * 100.0f : 0.0f;
        float m_ref_b = kp2_swx.size() > 0 ? (float)match_swx / kp2_swx.size() * 100.0f : 0.0f;

        out->rowB_sw = {(int)kp1_swx.size(), "SW", th_swx, (int)kp2_swx.size(), match_swx, m_scene_b, m_ref_b, overlap_sc, overlap_rf};

        if (is_expC_baseline) {
            out->kp1_sw_c = kp1_swx; 
            out->kp2_sw_c = kp2_swx; 
            out->desc1_sw_c = desc1_swx; 
            out->desc2_sw_c = desc2_swx;
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 3) return -1;

    // Change: Parse dynamic arguments to find exactly one reference and one scene image
    std::string scene_path = "";
    std::string ref_path = "";

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg.find("_ref") != std::string::npos) {
            ref_path = arg;
        } else if (scene_path.empty()) {
            scene_path = arg;
        }
    }

    if (scene_path.empty() || ref_path.empty()) return -1;

    cv::Mat img1 = cv::imread(scene_path, cv::IMREAD_COLOR);
    cv::Mat img2 = cv::imread(ref_path, cv::IMREAD_COLOR);
    
    if (img1.empty() || img2.empty()) return -1;
    
    cv::resize(img1, img1, cv::Size(IMG_WIDTH, IMG_HEIGHT));
    cv::resize(img2, img2, cv::Size(IMG_WIDTH, IMG_HEIGHT));
    
    constexpr size_t NUM_TESTS = sizeof(SWEEP_THRESHOLDS) / sizeof(SWEEP_THRESHOLDS[0]);
    int base_th_expC = SWEEP_THRESHOLDS[1]; 

    std::cout << "\nRunning hardware pipelines sequentially..." << std::endl;
    std::vector<HWResult> hw_results(NUM_TESTS);
    for (size_t i = 0; i < NUM_TESTS; ++i) {
        int th = SWEEP_THRESHOLDS[i];
        hw_results[i].thresh = th;
        run_hardware_pipeline(img1, th, hw_results[i].kp1, hw_results[i].desc1);
        run_hardware_pipeline(img2, th, hw_results[i].kp2, hw_results[i].desc2);
        evaluate_matches(img1, hw_results[i].kp1, hw_results[i].desc1, img2, hw_results[i].kp2, hw_results[i].desc2, "expA_th" + std::to_string(th) + "_hw.jpg", DEFAULT_HAMMING_THRESH, hw_results[i].matches);
    }

    std::cout << "Dispatching software yield matching and OpenCV to threads..." << std::endl;
    std::vector<SWWorkerOutput> sw_outputs(NUM_TESTS);
    std::vector<std::thread> sw_threads;

    for (size_t i = 0; i < NUM_TESTS; ++i) {
        int th = SWEEP_THRESHOLDS[i];
        bool is_baseline = (th == base_th_expC);
        sw_threads.emplace_back(sw_worker, th, std::cref(img1), std::cref(img2), std::cref(hw_results[i]), is_baseline, &sw_outputs[i]);
    }

    for (auto& t : sw_threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    std::vector<ResultRowA> resultsA;
    std::vector<ResultRowB> resultsB;
    SWWorkerOutput expC_sw;
    HWResult expC_hw;

    for (size_t i = 0; i < NUM_TESTS; ++i) {
        resultsA.push_back(sw_outputs[i].rowA_sw);
        
        float hw_m_scene_a = hw_results[i].kp1.size() > 0 ? (float)hw_results[i].matches / hw_results[i].kp1.size() * 100.0f : 0.0f;
        float hw_m_ref_a = hw_results[i].kp2.size() > 0 ? (float)hw_results[i].matches / hw_results[i].kp2.size() * 100.0f : 0.0f;
        resultsA.push_back({hw_results[i].thresh, "HW", (int)hw_results[i].kp1.size(), (int)hw_results[i].kp2.size(), hw_results[i].matches, hw_m_scene_a, hw_m_ref_a});
        
        int target_yield = hw_results[i].kp1.size();
        if (target_yield > 0) {
            resultsB.push_back(sw_outputs[i].rowB_sw);
            
            int temp_hw_matches = 0;
            evaluate_matches(img1, hw_results[i].kp1, hw_results[i].desc1, 
                             img2, hw_results[i].kp2, hw_results[i].desc2, 
                             "expB_tgt" + std::to_string(target_yield) + "_hw.jpg", 
                             DEFAULT_HAMMING_THRESH, temp_hw_matches);
            
            draw_all_keypoints(img1, hw_results[i].kp1, "expB_tgt" + std::to_string(target_yield) + "_kpts_hw.jpg");
            
            float hw_m_scene_b = hw_results[i].kp1.size() > 0 ? (float)hw_results[i].matches / hw_results[i].kp1.size() * 100.0f : 0.0f;
            float hw_m_ref_b = hw_results[i].kp2.size() > 0 ? (float)hw_results[i].matches / hw_results[i].kp2.size() * 100.0f : 0.0f;
            resultsB.push_back({target_yield, "HW", hw_results[i].thresh, (int)hw_results[i].kp2.size(), hw_results[i].matches, hw_m_scene_b, hw_m_ref_b, -1.0f, -1.0f});
        }

        if (SWEEP_THRESHOLDS[i] == base_th_expC) {
            expC_sw = sw_outputs[i];
            expC_hw = hw_results[i];
        }
    }

    std::cout << "\n=====================================================================================================================" << std::endl;
    std::cout << " EXPERIMENT A: FAST THRESHOLD SWEEP" << std::endl;
    std::cout << "=====================================================================================================================" << std::endl;
    std::cout << std::left << std::setw(8) << "Thresh" << std::setw(8) << "| Impl" << std::setw(15) << "| Scene Descs" << std::setw(13) << "| Ref Descs" << std::setw(11) << "| Matches" << std::setw(21) << "| Match / Scene (%)" << "| Match / Ref (%)" << std::endl;
    std::cout << std::string(93, '-') << std::endl;
    
    for (size_t i = 0; i < resultsA.size(); i++) {
        std::string th_str = (i % 2 == 0) ? std::to_string(resultsA[i].thresh) : "";
        std::cout << std::left << std::setw(8) << th_str 
                  << std::setw(8) << ("| " + resultsA[i].impl) 
                  << "| " << std::setw(13) << resultsA[i].scene_descriptors 
                  << "| " << std::setw(11) << resultsA[i].ref_descriptors 
                  << "| " << std::setw(9) << resultsA[i].matches 
                  << "| " << std::fixed << std::setprecision(1) << std::setw(19) << resultsA[i].match_scene_pct 
                  << "| " << std::fixed << std::setprecision(1) << resultsA[i].match_ref_pct << "%" << std::endl;
        if (i % 2 == 1) std::cout << std::string(93, '-') << std::endl;
    }

    std::cout << "\n=================================================================================================================================================" << std::endl;
    std::cout << " EXPERIMENT B: YIELD-MATCHED SPATIAL COMPARISON" << std::endl;
    std::cout << "=================================================================================================================================================" << std::endl;
    std::cout << std::left << std::setw(13) << "Scene Descs" 
              << std::setw(8)  << "| Impl" 
              << std::setw(12) << "| Thresh" 
              << std::setw(13) << "| Ref Descs" 
              << std::setw(11) << "| Matches" 
              << std::setw(21) << "| Match / Scene (%)" 
              << std::setw(19) << "| Match / Ref (%)" 
              << std::setw(17) << "| Scene Overlap" 
              << "| Ref Overlap (SW Recall)" << std::endl;
    std::cout << std::string(145, '-') << std::endl;

    for (size_t i = 0; i < resultsB.size(); i++) {
        std::string sc_str = (resultsB[i].overlap_scene < 0.0f) ? "-" : (std::to_string((int)(resultsB[i].overlap_scene * 10 + 0.5f) / 10) + "." + std::to_string((int)(resultsB[i].overlap_scene * 10 + 0.5f) % 10) + "%");
        std::string rf_str = (resultsB[i].overlap_ref < 0.0f) ? "-" : (std::to_string((int)(resultsB[i].overlap_ref * 10 + 0.5f) / 10) + "." + std::to_string((int)(resultsB[i].overlap_ref * 10 + 0.5f) % 10) + "%");

        std::cout << std::left << std::setw(13) << resultsB[i].scene_descriptors
                  << std::setw(8)  << ("| " + resultsB[i].impl) 
                  << "| Th = " << std::setw(5) << resultsB[i].thresh 
                  << "| " << std::setw(11) << resultsB[i].ref_descriptors 
                  << "| " << std::setw(9)  << resultsB[i].matches 
                  << "| " << std::fixed << std::setprecision(1) << std::setw(19) << resultsB[i].match_scene_pct 
                  << "| " << std::fixed << std::setprecision(1) << std::setw(17) << resultsB[i].match_ref_pct
                  << "| " << std::setw(15) << sc_str
                  << "| " << rf_str << std::endl;
        if (i % 2 == 1) std::cout << std::string(145, '-') << std::endl;
    }
    
    int tgt_yield_c = expC_hw.kp1.size();
    std::cout << "\n===========================================================================" << std::endl;
    std::cout << " EXPERIMENT C: HAMMING DISTANCE SWEEP (Target Yield: " << tgt_yield_c << " features)" << std::endl;
    std::cout << "===========================================================================" << std::endl;
    std::cout << std::left << std::setw(18) << "Hamming Limit" << std::setw(23) << "| HW Match/Scene (%)" << "| SW Match/Scene (%)" << std::endl;
    std::cout << std::string(75, '-') << std::endl;

    for (int h_thresh : SWEEP_HAMMING) {
        int m_hw = 0, m_sw = 0;
        evaluate_matches(img1, expC_hw.kp1, expC_hw.desc1, img2, expC_hw.kp2, expC_hw.desc2, "", h_thresh, m_hw);
        evaluate_matches(img1, expC_sw.kp1_sw_c, expC_sw.desc1_sw_c, img2, expC_sw.kp2_sw_c, expC_sw.desc2_sw_c, "", h_thresh, m_sw);
        
        float r_hw = tgt_yield_c > 0 ? (float)m_hw / tgt_yield_c * 100.0f : 0;
        float r_sw = expC_sw.kp1_sw_c.size() > 0 ? (float)m_sw / expC_sw.kp1_sw_c.size() * 100.0f : 0;

        std::cout << std::left << std::setw(18) << ("< " + std::to_string(h_thresh))
                  << "| " << std::fixed << std::setprecision(1) << std::setw(21) << r_hw 
                  << "| " << std::fixed << std::setprecision(1) << r_sw << std::endl;
    }
    std::cout << std::string(75, '-') << std::endl;

    return 0;
}