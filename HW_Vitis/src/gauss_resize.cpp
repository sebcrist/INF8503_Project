#include "ap_axi_sdata.h"
#include "hls_stream.h"
#include "common/xf_common.hpp"
#include "common/xf_utility.hpp"
#include "common/xf_infra.hpp"
#include "imgproc/xf_gaussian_filter.hpp"
#include "imgproc/xf_resize.hpp"
#include "imgproc/xf_duplicateimage.hpp"

#define IN_ROWS  720
#define IN_COLS  1280
#define OUT_ROWS 360
#define OUT_COLS 640
#define MAT_FIFO_DEPTH 2

typedef ap_axiu<8, 1, 1, 1> axiData;

void gauss_resize(hls::stream<axiData>& streamIn, hls::stream<axiData>& streamOut, hls::stream<axiData>& streamOutScaledDown, int rows, int cols, int* rowsScaledDown, int* colsScaledDown)
{
#pragma HLS INTERFACE mode=axis register_mode=both port=streamIn register
#pragma HLS INTERFACE mode=axis register_mode=both port=streamOut register
#pragma HLS INTERFACE mode=axis register_mode=both port=streamOutScaledDown register
#pragma HLS INTERFACE mode=ap_none port=cols
#pragma HLS INTERFACE mode=ap_none port=rows
#pragma HLS INTERFACE mode=ap_none port=rowsScaledDown
#pragma HLS INTERFACE mode=ap_none port=colsScaledDown
#pragma HLS INTERFACE mode=ap_ctrl_none port=return
#pragma HLS DATAFLOW

    int rowsScaled = rows / 2;
    int colsScaled = cols / 2;

    *rowsScaledDown = rowsScaled;
    *colsScaledDown = colsScaled;

    xf::cv::Mat<XF_8UC1, IN_ROWS, IN_COLS, XF_NPPC1, MAT_FIFO_DEPTH>   matIn(rows, cols);
    xf::cv::Mat<XF_8UC1, IN_ROWS, IN_COLS, XF_NPPC1, MAT_FIFO_DEPTH>   matBlurred(rows, cols);
    xf::cv::Mat<XF_8UC1, IN_ROWS, IN_COLS, XF_NPPC1, MAT_FIFO_DEPTH>   matBlurredOutput(rows, cols);
    xf::cv::Mat<XF_8UC1, IN_ROWS, IN_COLS, XF_NPPC1, MAT_FIFO_DEPTH>   matBlurredForScaleDown(rows, cols);
    xf::cv::Mat<XF_8UC1, OUT_ROWS, OUT_COLS, XF_NPPC1, MAT_FIFO_DEPTH>  matBlurredScaleDownOutput(rowsScaled, colsScaled);

    xf::cv::AXIvideo2xfMat(streamIn, matIn);
    xf::cv::GaussianBlur<5, XF_BORDER_REPLICATE, XF_8UC1, IN_ROWS, IN_COLS, XF_NPPC1>(matIn, matBlurred, 1.0f);
    xf::cv::duplicateMat(matBlurred, matBlurredOutput, matBlurredForScaleDown);
    xf::cv::xfMat2AXIvideo(matBlurredOutput, streamOut);
    xf::cv::resize<XF_INTERPOLATION_BILINEAR, XF_8UC1, IN_ROWS, IN_COLS, OUT_ROWS, OUT_COLS, XF_NPPC1, MAT_FIFO_DEPTH, MAT_FIFO_DEPTH, 2>(matBlurredForScaleDown, matBlurredScaleDownOutput);
    xf::cv::xfMat2AXIvideo(matBlurredScaleDownOutput, streamOutScaledDown);
}
