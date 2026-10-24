#include <iostream>
#include "opencv2/opencv.hpp"
#include "hls_stream.h"
#include "ap_axi_sdata.h"
#include "common/xf_axi.hpp"

typedef ap_axiu<8, 1, 1, 1> axiData;

void gauss_resize(hls::stream<axiData>& streamIn, hls::stream<axiData>& streamOut, int in_rows, int in_cols);

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <input_image_path>" << std::endl;
        return -1;
    }
    cv::Mat img_in = cv::imread(argv[1], cv::IMREAD_COLOR);
    if (img_in.empty() ) {
        std::cerr << "Error: Cannot open image " << argv[1] << std::endl;
        return -1;
    }else if(img_in.rows > 720|| img_in.cols > 1280)
    {
        std::cerr << "Error: Image size is too big (max = 720p) (" << argv[1]<<")" << std::endl;
        return -1;
    }

    int in_rows = img_in.rows;
    int in_cols = img_in.cols;
    int out_rows = in_rows / 2;
    int out_cols = in_cols / 2;

    hls::stream<axiData> stream_in("src");
    hls::stream<axiData> stream_out("dst");
    xf::cv::cvMat2AXIvideoxf<1, 8>(img_in, stream_in);

    // Run function
    std::cout << "Running gauss_resize..." << std::endl;
    gauss_resize(stream_in, stream_out, in_rows, in_cols);

    cv::Mat img_out(out_rows, out_cols, CV_8UC1);
    xf::cv::AXIvideo2cvMatxf<1, 8>(stream_out, img_out);


    // Run opencv
    cv::Mat ref_blurred, ref_out;
    cv::GaussianBlur(img_in, ref_blurred, cv::Size(5, 5), 1.0);
    cv::resize(ref_blurred, ref_out, cv::Size(out_cols, out_rows), 0, 0, cv::INTER_LINEAR);


    // Compare
    cv::Mat diff;
    cv::absdiff(ref_out, img_out, diff);

    int err_cnt = 0;
    for (int i = 0; i < diff.rows; i++)
        for (int j = 0; j < diff.cols; j++)
            if (diff.at<unsigned char>(i, j) > 2)
                err_cnt++;

    std::cout << "Nbr of errors: " << err_cnt << std::endl;
    cv::imwrite("./out_gauss_resize_hls.png", img_out);
    cv::imwrite("./out_gauss_resize_ref.png", ref_out);

    double total_pixels = static_cast<double>(out_rows * out_cols);
    double success_percentage = ((total_pixels - err_cnt) / total_pixels) * 100.0;

    if (success_percentage  < 95.0)
        std::cerr << "Test FAILED! " << success_percentage << "%" << std::endl;
    else
        std::cout << "Test PASSED! " << success_percentage << "%" << std::endl;

    return (success_percentage >= 90.0) ? 0 : -1;
}