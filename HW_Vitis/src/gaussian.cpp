#include "ap_axi_sdata.h"
#include "hls_stream.h"
#include "common/xf_common.hpp"
#include "common/xf_utility.hpp"
#include "common/xf_infra.hpp"
#include "imgproc/xf_cvt_color.hpp"
#include "imgproc/xf_gaussian_filter.hpp"

typedef ap_axiu<24, 1, 1, 1> axisIn;
typedef ap_axiu<8,  1, 1, 1> axisOut;

void gaussian(hls::stream<axisIn>& streamIn, hls::stream<axisOut>& streamOut)
{
#pragma HLS INTERFACE mode=axis register_mode=both port=streamOut register
#pragma HLS INTERFACE mode=axis register_mode=both port=streamIn register
#pragma HLS INTERFACE mode=ap_ctrl_none port=return
#pragma HLS DATAFLOW

    const int rows = 720;
    const int cols = 1280;

    xf::cv::Mat<XF_8UC3, 720, 1280, XF_NPPC1> imageIn(rows, cols);
    xf::cv::Mat<XF_8UC1, 720, 1280, XF_NPPC1> imageOut(rows, cols);

    xf::cv::AXIvideo2xfMat(streamIn, imageIn);
    xf::cv::rgb2gray<XF_8UC3, XF_8UC1, 720, 1280, XF_NPPC1>(imageIn, imageOut);
    xf::cv::xfMat2AXIvideo(imageOut, streamOut);
}
