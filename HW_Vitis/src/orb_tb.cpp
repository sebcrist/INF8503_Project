#include <iostream>
#include <vector>
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

// --- AXI Stream Definitions ---
typedef ap_axiu<24, 1, 1, 1> axisIn;
typedef ap_axiu<8, 1, 1, 1> axisOut;
typedef ap_axiu<8, 1, 1, 1> axiData;
typedef ap_axiu<512, 1, 1, 1> axiDescriptor;

// --- Function Declarations ---
void rgb_to_gray(hls::stream<axisIn>& streamIn, hls::stream<axisOut>& streamOut, int rows, int cols);
void gauss_resize(hls::stream<axiData>& streamIn, hls::stream<axiData>& streamOut, hls::stream<axiData>& streamOutScaledDown, int rows, int cols, int* rowsScaledDown, int* colsScaledDown);
void fast_corner(hls::stream<axiData>& streamIn, hls::stream<axiData>& streamOut, hls::stream<axiData>& cornerMask, int rows, int cols, int fastThreshold);
void r_brief_stage0(hls::stream<axiData>& streamIn, hls::stream<axiData>& cornerMask, hls::stream<axiDescriptor>& descriptors);
void r_brief_stage1(hls::stream<axiData>& streamIn, hls::stream<axiData>& cornerMask, hls::stream<axiDescriptor>& descriptors);
void r_brief_stage2(hls::stream<axiData>& streamIn, hls::stream<axiData>& cornerMask, hls::stream<axiDescriptor>& descriptors);
void descriptor_merger(hls::stream<axiDescriptor>& stream_stage0, hls::stream<axiDescriptor>& stream_stage1, hls::stream<axiDescriptor>& stream_stage2, hls::stream<axiDescriptor>& stream_out);

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <image_path>" << std::endl;
        return -1;
    }

    cv::Mat input_img = cv::imread(argv[1], cv::IMREAD_COLOR);
    if (input_img.empty()) {
        std::cerr << "Error: Could not load image: " << argv[1] << std::endl;
        return -1;
    }
    
    cv::resize(input_img, input_img, cv::Size(1280, 720));
    int rows = input_img.rows;
    int cols = input_img.cols;
    int fastThreshold = 75; 

    std::cout << "Running Integration Test on Image: " << argv[1] << " (" << cols << "x" << rows << ")" << std::endl;

    // =============================================================
    // PART A: FULL OPENCV SOFTWARE BASELINE (Standard defaults)
    // =============================================================
    std::cout << "\n[OPENCV-FULL] Running standard OpenCV ORB Baseline..." << std::endl;
    cv::Ptr<cv::ORB> cv_orb_full = cv::ORB::create(5000); 
    cv_orb_full->setFastThreshold(fastThreshold);
    cv_orb_full->setNLevels(2); 
    // Defaults: ScaleFactor = 1.2, ScoreType = HARRIS_SCORE
    
    std::vector<cv::KeyPoint> cv_keypoints_full;
    cv::Mat cv_descriptors_full;
    cv_orb_full->detectAndCompute(input_img, cv::noArray(), cv_keypoints_full, cv_descriptors_full);
    
    cv::Mat img_cv_full_out;
    cv::drawKeypoints(input_img, cv_keypoints_full, img_cv_full_out, cv::Scalar(0, 255, 0), cv::DrawMatchesFlags::DEFAULT);
    cv::imwrite("opencv_full_baseline.jpg", img_cv_full_out);


    // =============================================================
    // PART B: TUNED OPENCV BASELINE (Matched to Hardware)
    // =============================================================
    std::cout << "\n[OPENCV-TUNED] Running tuned OpenCV ORB to mirror hardware..." << std::endl;
    cv::Ptr<cv::ORB> cv_orb_tuned = cv::ORB::create(5000); 
    cv_orb_tuned->setFastThreshold(fastThreshold);
    cv_orb_tuned->setNLevels(2); 
    cv_orb_tuned->setScaleFactor(2.0f); // Match hardware gauss_resize downsample ratio
    cv_orb_tuned->setScoreType(cv::ORB::FAST_SCORE); // Disable Harris sorting
    
    std::vector<cv::KeyPoint> cv_keypoints_tuned;
    cv::Mat cv_descriptors_tuned;
    cv_orb_tuned->detectAndCompute(input_img, cv::noArray(), cv_keypoints_tuned, cv_descriptors_tuned);
    
    cv::Mat img_cv_tuned_out;
    cv::drawKeypoints(input_img, cv_keypoints_tuned, img_cv_tuned_out, cv::Scalar(255, 255, 0), cv::DrawMatchesFlags::DEFAULT);
    cv::imwrite("opencv_tuned_baseline.jpg", img_cv_tuned_out);


    // =============================================================
    // PART C: HARDWARE PIPELINE EXECUTION
    // =============================================================
    std::cout << "\n[PIPELINE] Populating AXI Streams..." << std::endl;
    hls::stream<axisIn> s_axis_video;
    hls::stream<axisOut> s_gray_out;
    hls::stream<axiData> s_gray_to_gauss;
    hls::stream<axiData> s_stage0_in, s_stage1_in;
    int rowsScaledDown, colsScaledDown;
    
    hls::stream<axiData> s_stage0_pixels, s_stage0_mask;
    hls::stream<axiData> s_stage1_pixels, s_stage1_mask;
    hls::stream<axiData> s_stage2_pixels, s_stage2_mask;
    hls::stream<axiDescriptor> s_desc0, s_desc1, s_desc2, s_final_descriptors;

    // Stream image in
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

    std::cout << "[PIPELINE] Executing hardware modules..." << std::endl;
    
    rgb_to_gray(s_axis_video, s_gray_out, rows, cols);
    
    while(!s_gray_out.empty()){
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
    dummy_pixel.data = 0; dummy_pixel.user = 0; dummy_pixel.last = 0; dummy_pixel.keep = -1;
    for (int i = 0; i < 5000; i++) { 
        s_stage0_in.write(dummy_pixel);
        s_stage1_in.write(dummy_pixel);
    }

    fast_corner(s_stage0_in, s_stage0_pixels, s_stage0_mask, rows, cols, fastThreshold);
    fast_corner(s_stage1_in, s_stage1_pixels, s_stage1_mask, rowsScaledDown, colsScaledDown, fastThreshold);

    s_stage2_pixels.write(axiData());
    s_stage2_mask.write(axiData());

    r_brief_stage0(s_stage0_pixels, s_stage0_mask, s_desc0);
    r_brief_stage1(s_stage1_pixels, s_stage1_mask, s_desc1);
    
    // Provide dummy descriptors to stage 2 to prevent merger starvation
    for(int i = 0; i < 5000; i++) {
        axiDescriptor dummy_desc;
        dummy_desc.data = 0; dummy_desc.user = 0; 
        dummy_desc.last = (i == 4999) ? 1 : 0;
        s_desc2.write(dummy_desc);
    }
    
    // Simulate continuous hardware execution until all input FIFOs are fully drained
    while (!s_desc0.empty() || !s_desc1.empty() || !s_desc2.empty()) {
        descriptor_merger(s_desc0, s_desc1, s_desc2, s_final_descriptors);
    }

    std::cout << "[PIPELINE] Execution finished. Parsing output..." << std::endl;

    // =============================================================
    // PART D: EXTRACT AND VISUALIZE HARDWARE OUTPUT
    // =============================================================
    std::vector<cv::KeyPoint> hw_keypoints;
    int descriptor_count = 0;
    
    // Drain the final stream entirely
    while (!s_final_descriptors.empty()) {
        axiDescriptor d = s_final_descriptors.read();
        
        if (d.data == 0) continue; // Skip dummy data
        
        descriptor_count++;
        
        int final_x = d.data.range(271, 256).to_int();
        int final_y = d.data.range(287, 272).to_int();
        
        hw_keypoints.push_back(cv::KeyPoint(final_x, final_y, 1.0f));
    }
    
    cv::Mat img_hw_out;
    cv::drawKeypoints(input_img, hw_keypoints, img_hw_out, cv::Scalar(0, 0, 255), cv::DrawMatchesFlags::DEFAULT);
    cv::imwrite("hls_pipeline_output.jpg", img_hw_out);


    // =============================================================
    // PART E: SUMMARY COMPARISON
    // =============================================================
    std::cout << "\n=============================================" << std::endl;
    std::cout << " SUMMARY COMPARISON (FAST Threshold: " << fastThreshold << ")" << std::endl;
    std::cout << "=============================================" << std::endl;
    std::cout << " 1. Full OpenCV ORB (Harris, 1.2 Scale):  " << cv_keypoints_full.size() << std::endl;
    std::cout << " 2. Tuned OpenCV ORB (FAST, 2.0 Scale):   " << cv_keypoints_tuned.size() << std::endl;
    std::cout << " 3. Hardware HLS Pipeline:                " << descriptor_count << std::endl;
    std::cout << "=============================================" << std::endl;
    std::cout << " Visualizations saved to:" << std::endl;
    std::cout << "  - opencv_full_baseline.jpg (Green)" << std::endl;
    std::cout << "  - opencv_tuned_baseline.jpg (Cyan)" << std::endl;
    std::cout << "  - hls_pipeline_output.jpg (Red)" << std::endl;
    std::cout << "=============================================" << std::endl;

    return 0;
}