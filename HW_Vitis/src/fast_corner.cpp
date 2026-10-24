#include "ap_axi_sdata.h"
#include "hls_stream.h"
#include "common/xf_common.hpp"
#include "common/xf_utility.hpp"
#include "common/xf_infra.hpp"
#include "features/xf_fast.hpp"

#define MAX_HEIGHT 720
#define MAX_WIDTH  1280

#define USE_NMS 1
#define FAST_THRESHOLD 20

typedef ap_axiu<8, 1, 1, 1> axiData;

void fast_corner(hls::stream<axiData>& streamIn, hls::stream<axiData>& streamOut, int rows, int cols)
{
#pragma HLS INTERFACE mode=axis register_mode=both port=streamIn register
#pragma HLS INTERFACE mode=axis register_mode=both port=streamOut register
#pragma HLS INTERFACE mode=ap_none port=rows
#pragma HLS INTERFACE mode=ap_none port=cols
#pragma HLS INTERFACE mode=ap_ctrl_none port=return
#pragma HLS DATAFLOW

    xf::cv::Mat<XF_8UC1, MAX_HEIGHT, MAX_WIDTH, XF_NPPC1, 2> matIn(rows, cols);
    xf::cv::Mat<XF_8UC1, MAX_HEIGHT, MAX_WIDTH, XF_NPPC1, 2> matOut(rows, cols);

    xf::cv::AXIvideo2xfMat(streamIn, matIn);

    xf::cv::fast<USE_NMS, XF_8UC1, MAX_HEIGHT, MAX_WIDTH, XF_NPPC1, 2, 2>( matIn, matOut, FAST_THRESHOLD);

    xf::cv::xfMat2AXIvideo(matOut, streamOut);
}