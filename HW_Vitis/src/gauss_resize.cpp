#include "ap_axi_sdata.h"
#include "hls_stream.h"
#include "common/xf_common.hpp"
#include "common/xf_utility.hpp"
#include "common/xf_infra.hpp"
#include "imgproc/xf_custom_convolution.hpp"   // filter2D (replaces GaussianBlur: no float exp/div hardware)
#include "imgproc/xf_resize.hpp"
#include "imgproc/xf_duplicateimage.hpp"

#define IN_ROWS  720
#define IN_COLS  1280
#define OUT_ROWS 360
#define OUT_COLS 640
#define MAT_FIFO_DEPTH 2
#define GAUSS_SHIFT 16

typedef ap_axiu<8, 1, 1, 1> axiData;

static const short int GAUSS_K[25] = {
     196,  868,  1456,  868,  196,
     868, 3844,  6448, 3844,  868,
    1456, 6448, 10816, 6448, 1456,
     868, 3844,  6448, 3844,  868,
     196,  868,  1456,  868,  196
};

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
    short int kernel[25];
    for (int i = 0; i < 25; i++) kernel[i] = GAUSS_K[i];
    xf::cv::filter2D<XF_BORDER_CONSTANT, 5, 5, XF_8UC1, XF_8UC1, IN_ROWS, IN_COLS, XF_NPPC1>(matIn, matBlurred, kernel, GAUSS_SHIFT);
    xf::cv::duplicateMat(matBlurred, matBlurredOutput, matBlurredForScaleDown);
    xf::cv::xfMat2AXIvideo(matBlurredOutput, streamOut);
    xf::cv::resize<XF_INTERPOLATION_BILINEAR, XF_8UC1, IN_ROWS, IN_COLS, OUT_ROWS, OUT_COLS, XF_NPPC1, MAT_FIFO_DEPTH, MAT_FIFO_DEPTH, 2>(matBlurredForScaleDown, matBlurredScaleDownOutput);
    xf::cv::xfMat2AXIvideo(matBlurredScaleDownOutput, streamOutScaledDown);
}