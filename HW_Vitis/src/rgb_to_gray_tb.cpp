#include <iostream>
#include "opencv2/opencv.hpp"
#include "opencv2/core/core_c.h"
#include "opencv2/imgproc/imgproc_c.h"
#include "hls_stream.h"
#include "ap_axi_sdata.h"
#include "common/xf_axi.hpp"

typedef ap_axiu<24, 1, 1, 1> axisIn;
typedef ap_axiu<8,  1, 1, 1> axisOut;

void rgb_to_gray(hls::stream<axisIn>& streamIn, hls::stream<axisOut>& streamOut, int rows, int cols);

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

    //Run on our implementation
    cv::Mat img_out(img_in.rows, img_in.cols, CV_8UC1);
    hls::stream<axisIn> stream_in("src_stream");
    hls::stream<axisOut> stream_out("dst_stream");
    cv::Mat img_rgb;
    cv::cvtColor(img_in, img_rgb, cv::COLOR_BGR2RGB);
    xf::cv::cvMat2AXIvideoxf<1, 24>(img_rgb, stream_in);
    std::cout << "Starting hardware acceleration..." << std::endl;
    rgb_to_gray(stream_in, stream_out, img_in.rows, img_in.cols);
    xf::cv::AXIvideo2cvMatxf<1, 8>(stream_out, img_out);

    //Run on opencv
    cv::Mat img_ref(img_in.rows, img_in.cols, CV_8UC1);
    cv::cvtColor(img_in, img_ref, cv::COLOR_BGR2GRAY);


    //Validate
    cv::Mat diff;
    cv::absdiff(img_ref, img_out, diff);
    
    int err_cnt = 0;
    for (int i = 0; i < diff.rows; i++) {
        for (int j = 0; j < diff.cols; j++) {
            if (diff.at<unsigned char>(i, j) > 2) { 
                err_cnt++;
            }
        }
    }

    std::cout << "Nbr of errors: " << err_cnt << std::endl;
    cv::imwrite("./ImageOut/out_rgb2gray_hls.png", img_out);
    cv::imwrite("./ImageOut/out_rgb2gray_ref.png", img_ref);

    if (err_cnt)
        std::cerr << "Test FAILED!" << std::endl;
    else
        std::cout << "Test PASSED!" << std::endl;

    return err_cnt ? -1 : 0;
}