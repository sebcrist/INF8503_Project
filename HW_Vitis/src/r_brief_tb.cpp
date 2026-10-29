#include <iostream>
#include <vector>
#include "opencv2/opencv.hpp"
#include "hls_stream.h"
#include "ap_axi_sdata.h"
#include "common/xf_axi.hpp"

typedef ap_axiu<8, 1, 1, 1> axiData;
typedef ap_axiu<256, 1, 1, 1> axiDescriptor;

void r_brief(hls::stream<axiData>& streamIn, hls::stream<axiData>& cornerMask, hls::stream<axiDescriptor>& descriptors, int rows, int cols);

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input_image_path> <corner_mask_path>" << std::endl;
        return -1;
    }

    cv::Mat img_in = cv::imread(argv[1], cv::IMREAD_GRAYSCALE);
    cv::Mat mask_in = cv::imread(argv[2], cv::IMREAD_GRAYSCALE);

    if (img_in.empty()) {
        std::cerr << "Error: Cannot open image " << argv[1] << std::endl;
        return -1;
    }
    if (mask_in.empty()) {
        std::cerr << "Error: Cannot open mask image " << argv[2] << std::endl;
        return -1;
    }
    if (img_in.rows > 720 || img_in.cols > 1280) {
        std::cerr << "Error: Image size is too big (max = 720p) (" << argv[1] << ")" << std::endl;
        return -1;
    }
    if (img_in.rows != mask_in.rows || img_in.cols != mask_in.cols) {
        std::cerr << "Error: Image and mask dimensions do not match." << std::endl;
        return -1;
    }

    int rows = img_in.rows;
    int cols = img_in.cols;
    int radius = 15;

    int expected_descriptors = 0;

    // Scan the provided mask image to count corners that are safely inside the 15-pixel border
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            if (mask_in.at<unsigned char>(r, c) > 0) {
                if (c >= radius && c < cols - radius && r >= radius && r < rows - radius) {
                    expected_descriptors++;
                }
            }
        }
    }

    hls::stream<axiData> stream_in("src_stream");
    hls::stream<axiData> stream_mask("mask_stream");
    hls::stream<axiDescriptor> stream_out("dst_stream");

    xf::cv::cvMat2AXIvideoxf<1, 8>(img_in, stream_in);
    xf::cv::cvMat2AXIvideoxf<1, 8>(mask_in, stream_mask);

    r_brief(stream_in, stream_mask, stream_out, rows, cols);

    int hls_descriptors = 0;
    while (!stream_out.empty()) {
        axiDescriptor desc;
        stream_out.read(desc);
        hls_descriptors++;
    }

    std::cout << "Expected Descriptors (from valid border): " << expected_descriptors << std::endl;
    std::cout << "HLS Descriptors Generated: " << hls_descriptors << std::endl;

    if (hls_descriptors != expected_descriptors) {
        std::cerr << "Test FAILED! Descriptor count mismatch." << std::endl;
        return -1;
    }

    std::cout << "Test PASSED!" << std::endl;
    return 0;
}