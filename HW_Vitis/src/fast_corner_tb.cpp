#include <iostream>
#include <vector>
#include "opencv2/opencv.hpp"
#include "hls_stream.h"
#include "ap_axi_sdata.h"
#include "common/xf_axi.hpp"

typedef ap_axiu<8, 1, 1, 1> axiData;

void fast_corner(hls::stream<axiData>& streamIn, hls::stream<axiData>& streamOut, int rows, int cols);

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <input_image_path>" << std::endl;
        return -1;
    }
    cv::Mat img_in = cv::imread(argv[1], cv::IMREAD_GRAYSCALE);
    if (img_in.empty() ) {
        std::cerr << "Error: Cannot open image " << argv[1] << std::endl;
        return -1;
    }else if(img_in.rows > 720|| img_in.cols > 1280)
    {
        std::cerr << "Error: Image size is too big (max = 720p) (" << argv[1]<<")" << std::endl;
        return -1;
    }

    int rows = img_in.rows;
    int cols = img_in.cols;
    int threshold = 20;

    // Run function
    cv::Mat img_hls_out(rows, cols, CV_8UC1);
    hls::stream<axiData> stream_in("src_stream");
    hls::stream<axiData> stream_out("dst_stream");

    xf::cv::cvMat2AXIvideoxf<1, 8>(img_in, stream_in);
    fast_corner(stream_in, stream_out, rows, cols);
    xf::cv::AXIvideo2cvMatxf<1, 8>(stream_out, img_hls_out);

    // Run OpenCV
    std::vector<cv::KeyPoint> cv_keypoints;
    cv::FAST(img_in, cv_keypoints, threshold, true, cv::FastFeatureDetector::TYPE_9_16);

    cv::Mat img_ref_out = cv::Mat::zeros(rows, cols, CV_8UC1);
    for (const auto& kp : cv_keypoints) {
        int x = cvRound(kp.pt.x);
        int y = cvRound(kp.pt.y);
        if (x >= 0 && x < cols && y >= 0 && y < rows) {
            img_ref_out.at<unsigned char>(y, x) = 255;
        }
    }

    // 3. Compare
    int hls_corners = 0;
    int ref_corners = 0;
    int matched_corners = 0;

    for (int r = 4; r < rows - 4; r++) {
        for (int c = 4; c < cols - 4; c++) {
            bool is_hls = img_hls_out.at<unsigned char>(r, c) > 0;
            bool is_ref = img_ref_out.at<unsigned char>(r, c) > 0;

            if (is_hls) hls_corners++;
            if (is_ref) ref_corners++;

            if (is_hls) {
                bool found_in_neighborhood = false;
                for (int dr = -1; dr <= 1; dr++) {
                    for (int dc = -1; dc <= 1; dc++) {
                        if (img_ref_out.at<unsigned char>(r + dr, c + dc) > 0) {
                            found_in_neighborhood = true;
                        }
                    }
                }
                if (found_in_neighborhood) matched_corners++;
            }
        }
    }

    float match_ratio = (hls_corners > 0) ? (100.0f * matched_corners / hls_corners) : 100.0f;
    std::cout << "OpenCV Corners: " << ref_corners << std::endl;
    std::cout << "HLS Corners:    " << hls_corners << std::endl;
    std::cout << "Matched (%):    " << match_ratio << "%" << std::endl;

    cv::Mat vis_hls, vis_ref;
    cv::cvtColor(img_in, vis_hls, cv::COLOR_GRAY2BGR);
    cv::cvtColor(img_in, vis_ref, cv::COLOR_GRAY2BGR);

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            if (img_hls_out.at<unsigned char>(r, c) > 0)
                cv::circle(vis_hls, cv::Point(c, r), 3, cv::Scalar(0, 255, 0), 1);
            if (img_ref_out.at<unsigned char>(r, c) > 0)
                cv::circle(vis_ref, cv::Point(c, r), 3, cv::Scalar(0, 0, 255), 1);
        }
    }

    cv::imwrite("./out_fast_hls_mask.png", img_hls_out);
    cv::imwrite("./out_fast_hls_vis.png", vis_hls);
    cv::imwrite("./out_fast_ref_vis.png", vis_ref);

    if (match_ratio < 90.0f) {
        std::cerr << "Test FAILED! Match ratio below 90%." << std::endl;
        return -1;
    }

    std::cout << "Test PASSED!" << std::endl;
    return 0;
}