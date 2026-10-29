#include "ap_axi_sdata.h"
#include "hls_stream.h"
#include "common/xf_common.hpp"
#include "common/xf_utility.hpp"
#include "common/xf_infra.hpp"
#include "features/xf_fast.hpp"
#include "imgproc/xf_duplicateimage.hpp"

#define MAX_HEIGHT 720
#define MAX_WIDTH  1280
#define MAT_FIFO_DEPTH 2
#define USE_NMS 1

typedef ap_axiu<8, 1, 1, 1> axiData;

void fast_corner(hls::stream<axiData>& streamIn, hls::stream<axiData>& streamOut, hls::stream<axiData>& cornerMask, int rows, int cols, int fastThreshold)
{
#pragma HLS INTERFACE mode=axis register_mode=both port=streamIn register
#pragma HLS INTERFACE mode=axis register_mode=both port=streamOut register
#pragma HLS INTERFACE mode=axis register_mode=both port=cornerMask register
#pragma HLS INTERFACE mode=ap_none port=rows
#pragma HLS INTERFACE mode=ap_none port=cols
#pragma HLS INTERFACE mode=ap_none port=fastThreshold
#pragma HLS INTERFACE mode=ap_ctrl_none port=return
#pragma HLS DATAFLOW

    xf::cv::Mat<XF_8UC1, MAX_HEIGHT, MAX_WIDTH, XF_NPPC1, MAT_FIFO_DEPTH> matIn(rows, cols);
    xf::cv::Mat<XF_8UC1, MAX_HEIGHT, MAX_WIDTH, XF_NPPC1, MAT_FIFO_DEPTH> matIn1(rows, cols);
    xf::cv::Mat<XF_8UC1, MAX_HEIGHT, MAX_WIDTH, XF_NPPC1, MAT_FIFO_DEPTH> matIn2(rows, cols);
    xf::cv::Mat<XF_8UC1, MAX_HEIGHT, MAX_WIDTH, XF_NPPC1, MAT_FIFO_DEPTH> matMask(rows, cols);

    xf::cv::AXIvideo2xfMat(streamIn, matIn);
    xf::cv::duplicateMat(matIn, matIn1, matIn2);
    xf::cv::fast<USE_NMS, XF_8UC1, MAX_HEIGHT, MAX_WIDTH, XF_NPPC1, MAT_FIFO_DEPTH, MAT_FIFO_DEPTH>( matIn1, matMask, fastThreshold);
    xf::cv::xfMat2AXIvideo(matIn2, streamOut);
    xf::cv::xfMat2AXIvideo(matMask, cornerMask);
    //TODO possible ressource optimization is to fit 8 cornerMask packets which are 8 bits each 255 or 0, into a single 8 bit packet, this might save line buffer ressources later on 
}