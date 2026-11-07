#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <cmath>
#include <thread>
#include <algorithm>
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

constexpr int TEST_THRESH = 50; 

typedef ap_axiu<24, 1, 1, 1> axisIn;
typedef ap_axiu<8, 1, 1, 1> axisOut;
typedef ap_axiu<8, 1, 1, 1> axiData;
typedef ap_axiu<512, 1, 1, 1> axiDescriptor;

struct HWResult {
    std::string img_name;
    std::vector<cv::KeyPoint> kp;
    cv::Mat desc;
    int matches;
};

struct SWResult {
    std::string img_name;
    std::vector<cv::KeyPoint> kp;
    cv::Mat desc;
    int ref_descs;
    int thresh;
    int matches;
    float overlap;
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
    std::vector<cv::DMatch> good_matches;
    if (!desc1.empty() && !desc2.empty()) {
        cv::BFMatcher matcher(cv::NORM_HAMMING, true); 
        std::vector<cv::DMatch> matches;
        matcher.match(desc1, desc2, matches);

        for (size_t i = 0; i < matches.size(); i++) {
            if (matches[i].distance < hamming_thresh) { 
                good_matches.push_back(matches[i]);
            }
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
    if (img.empty() || out_filename.empty()) return;
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

void find_yield_matched_opencv(cv::Ptr<cv::ORB> orb, const cv::Mat& img1, int target_yield,
                               std::vector<cv::KeyPoint>& best_kp1, cv::Mat& best_desc1, int& final_th) {
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
}

std::string get_scene_label(const std::string& path) {
    size_t last_slash_idx = path.find_last_of("\\/");
    std::string filename = (last_slash_idx != std::string::npos) ? path.substr(last_slash_idx + 1) : path;
    size_t period_idx = filename.rfind('.');
    std::string base = (period_idx != std::string::npos) ? filename.substr(0, period_idx) : filename;

    const std::string p1 = "orb_imperfect_inputs_";
    const std::string p2 = "orb_inperfect_inputs_";
    if (base.find(p1) == 0) return base.substr(p1.length());
    if (base.find(p2) == 0) return base.substr(p2.length());
    return base;
}

// Change: Split SW evaluation into a dedicated thread-safe worker
void sw_scene_worker(const cv::Mat& scene_img, const cv::Mat& ref_img, const HWResult& hw_res, SWResult* sw_res) {
    sw_res->img_name = hw_res.img_name;
    sw_res->thresh = DEFAULT_FALLBACK_THRESH;
    sw_res->ref_descs = 0;
    sw_res->matches = 0;
    sw_res->overlap = 0.0f;

    int target_yield = hw_res.kp.size();
    cv::Ptr<cv::ORB> orb_sw_dyn = cv::ORB::create(MAX_FEATURES_OPENCV); 
    orb_sw_dyn->setNLevels(ORB_LEVELS); 

    if (target_yield > 0) {
        find_yield_matched_opencv(orb_sw_dyn, scene_img, target_yield, sw_res->kp, sw_res->desc, sw_res->thresh);
    } else {
        sw_res->thresh = TEST_THRESH;
        orb_sw_dyn->setFastThreshold(sw_res->thresh);
        orb_sw_dyn->detectAndCompute(scene_img, cv::noArray(), sw_res->kp, sw_res->desc);
    }

    orb_sw_dyn->setFastThreshold(sw_res->thresh);
    std::vector<cv::KeyPoint> ref_kp_sw; cv::Mat ref_desc_sw;
    orb_sw_dyn->detectAndCompute(ref_img, cv::noArray(), ref_kp_sw, ref_desc_sw);
    sw_res->ref_descs = ref_kp_sw.size();

    evaluate_matches(scene_img, sw_res->kp, sw_res->desc, ref_img, ref_kp_sw, ref_desc_sw, "imperfect_" + sw_res->img_name + "_sw_match.jpg", DEFAULT_HAMMING_THRESH, sw_res->matches);
    draw_all_keypoints(scene_img, sw_res->kp, "imperfect_" + sw_res->img_name + "_sw_kpts.jpg");

    sw_res->overlap = calculate_spatial_overlap(hw_res.kp, sw_res->kp, SPATIAL_TOLERANCE_PX, false);
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: <executable> <image1> <image2> ... (must contain one _ref image)" << std::endl;
        return -1;
    }

    std::string ref_img_path = "";
    std::vector<std::string> scene_img_paths;

    for (int i = 1; i < argc; i++) {
        std::string path = argv[i];
        if (path.find("_ref") != std::string::npos) {
            if (!ref_img_path.empty()) {
                std::cerr << "Error: Multiple reference images found containing '_ref'." << std::endl;
                return -1;
            }
            ref_img_path = path;
        } else {
            scene_img_paths.push_back(path);
        }
    }

    if (ref_img_path.empty() || scene_img_paths.empty()) {
        std::cerr << "Error: Missing reference image or scene image(s)." << std::endl;
        return -1;
    }

    // Added: Sort scene paths for deterministic table order
    std::sort(scene_img_paths.begin(), scene_img_paths.end());

    cv::Mat ref_img = cv::imread(ref_img_path, cv::IMREAD_COLOR);
    if (ref_img.empty()) return -1;
    cv::resize(ref_img, ref_img, cv::Size(IMG_WIDTH, IMG_HEIGHT));

    std::cout << "\nProcessing Reference Image (HW)..." << std::endl;
    std::vector<cv::KeyPoint> ref_kp_hw; cv::Mat ref_desc_hw;
    run_hardware_pipeline(ref_img, TEST_THRESH, ref_kp_hw, ref_desc_hw);
    draw_all_keypoints(ref_img, ref_kp_hw, "imperfect_ref_hw_kpts.jpg");

    size_t num_scenes = scene_img_paths.size();
    std::vector<cv::Mat> scene_imgs(num_scenes);
    std::vector<HWResult> hw_results(num_scenes);
    std::vector<SWResult> sw_results(num_scenes);

    // Change: Run HLS hardware pipeline sequentially across all scenes
    std::cout << "Running hardware pipeline sequentially across " << num_scenes << " imperfect scenes..." << std::endl;
    for (size_t i = 0; i < num_scenes; ++i) {
        scene_imgs[i] = cv::imread(scene_img_paths[i], cv::IMREAD_COLOR);
        if (scene_imgs[i].empty()) return -1;
        cv::resize(scene_imgs[i], scene_imgs[i], cv::Size(IMG_WIDTH, IMG_HEIGHT));

        std::string label = get_scene_label(scene_img_paths[i]);
        hw_results[i].img_name = label;
        run_hardware_pipeline(scene_imgs[i], TEST_THRESH, hw_results[i].kp, hw_results[i].desc);
        evaluate_matches(scene_imgs[i], hw_results[i].kp, hw_results[i].desc, ref_img, ref_kp_hw, ref_desc_hw, "imperfect_" + label + "_hw_match.jpg", DEFAULT_HAMMING_THRESH, hw_results[i].matches);
        draw_all_keypoints(scene_imgs[i], hw_results[i].kp, "imperfect_" + label + "_hw_kpts.jpg");
    }

    // Change: Dispatch OpenCV yield-matching and evaluation to threads
    std::cout << "Dispatching OpenCV yield-matched evaluation to threads..." << std::endl;
    std::vector<std::thread> threads;
    for (size_t i = 0; i < num_scenes; ++i) {
        threads.emplace_back(sw_scene_worker, std::cref(scene_imgs[i]), std::cref(ref_img), std::cref(hw_results[i]), &sw_results[i]);
    }
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    std::cout << "\n=========================================================================================================================================" << std::endl;
    std::cout << " IMPERFECT INPUTS COMPARISON (HW Thresh = " << TEST_THRESH << ", Ref Descs HW = " << ref_kp_hw.size() << ")" << std::endl;
    std::cout << "=========================================================================================================================================" << std::endl;
    std::cout << std::left << std::setw(30) << "Scene" 
              << std::setw(9)  << "| Impl" 
              << std::setw(10) << "| Thresh" 
              << std::setw(14) << "| Scene Descs" 
              << std::setw(12) << "| Ref Descs" 
              << std::setw(10) << "| Matches" 
              << std::setw(19) << "| Match/Scene (%)" 
              << std::setw(17) << "| Match/Ref (%)" 
              << "| Scene Overlap" << std::endl;
    std::cout << std::string(137, '-') << std::endl;

    for (size_t i = 0; i < num_scenes; i++) {
        float m_hw_sc = hw_results[i].kp.size() > 0 ? (float)hw_results[i].matches / hw_results[i].kp.size() * 100.0f : 0.0f;
        float m_hw_rf = ref_kp_hw.size() > 0 ? (float)hw_results[i].matches / ref_kp_hw.size() * 100.0f : 0.0f;
        float m_sw_sc = sw_results[i].kp.size() > 0 ? (float)sw_results[i].matches / sw_results[i].kp.size() * 100.0f : 0.0f;
        float m_sw_rf = sw_results[i].ref_descs > 0 ? (float)sw_results[i].matches / sw_results[i].ref_descs * 100.0f : 0.0f;

        std::cout << std::left << std::setw(30) << hw_results[i].img_name
                  << std::setw(9)  << "| OpenCV" 
                  << "| " << std::setw(7)  << sw_results[i].thresh 
                  << "| " << std::setw(11) << sw_results[i].kp.size() 
                  << "| " << std::setw(9)  << sw_results[i].ref_descs
                  << "| " << std::setw(7)  << sw_results[i].matches 
                  << "| " << std::fixed << std::setprecision(1) << std::setw(17) << m_sw_sc 
                  << "| " << std::fixed << std::setprecision(1) << std::setw(15) << m_sw_rf
                  << "| " << std::fixed << std::setprecision(1) << sw_results[i].overlap << "%" << std::endl;

        std::cout << std::left << std::setw(30) << "" 
                  << std::setw(9)  << "| HLS" 
                  << "| " << std::setw(7)  << TEST_THRESH 
                  << "| " << std::setw(11) << hw_results[i].kp.size() 
                  << "| " << std::setw(9)  << ref_kp_hw.size()
                  << "| " << std::setw(7)  << hw_results[i].matches 
                  << "| " << std::fixed << std::setprecision(1) << std::setw(17) << m_hw_sc 
                  << "| " << std::fixed << std::setprecision(1) << std::setw(15) << m_hw_rf
                  << "| -" << std::endl;
        
        std::cout << std::string(137, '-') << std::endl;
    }

    return 0;
}