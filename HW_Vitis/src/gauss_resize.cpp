#include "ap_axi_sdata.h"
#include "hls_stream.h"
#include "common/xf_common.hpp"
#include "common/xf_utility.hpp"
#include "common/xf_infra.hpp"
#include "imgproc/xf_gaussian_filter.hpp"
#include "imgproc/xf_resize.hpp"

#define IN_ROWS  720
#define IN_COLS  1280
#define OUT_ROWS 360
#define OUT_COLS 640

typedef ap_axiu<8, 1, 1, 1> axiData;

void gauss_resize(hls::stream<axiData>& streamIn, hls::stream<axiData>& streamOut, int in_rows, int in_cols)
{
#pragma HLS INTERFACE mode=axis register_mode=both port=streamIn register
#pragma HLS INTERFACE mode=axis register_mode=both port=streamOut register
#pragma HLS INTERFACE mode=ap_none port=in_cols
#pragma HLS INTERFACE mode=ap_none port=in_rows
#pragma HLS INTERFACE mode=ap_ctrl_none port=return
#pragma HLS DATAFLOW

        xf::cv::Mat<XF_8UC1, IN_ROWS, IN_COLS, XF_NPPC1, 2>   matIn(in_rows, in_cols);
        xf::cv::Mat<XF_8UC1, IN_ROWS, IN_COLS, XF_NPPC1, 2>   matBlurred(in_rows, in_cols);
        xf::cv::Mat<XF_8UC1, OUT_ROWS, OUT_COLS, XF_NPPC1, 2>  matOut(in_rows / 2, in_cols / 2);

        xf::cv::AXIvideo2xfMat(streamIn, matIn);

        xf::cv::GaussianBlur<5, XF_BORDER_REPLICATE, XF_8UC1, IN_ROWS, IN_COLS, XF_NPPC1>(matIn, matBlurred, 1.0f);

        xf::cv::resize<XF_INTERPOLATION_BILINEAR, XF_8UC1, IN_ROWS, IN_COLS, OUT_ROWS, OUT_COLS, XF_NPPC1, 2, 2, 2>(matBlurred, matOut);

        xf::cv::xfMat2AXIvideo(matOut, streamOut);
}