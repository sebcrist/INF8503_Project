#include "ap_axi_sdata.h"
#include "hls_stream.h"
#include "common/xf_infra.hpp"
#include "imgproc/xf_cvt_color.hpp"
#include "common/xf_common.hpp"

typedef ap_axiu<24, 1, 1, 1> axisIn;
typedef ap_axiu<8,  1, 1, 1> axisOut;
void gaussian(hls::stream<axisIn> streamIn, hls::stream<axisIn> streamOut)
{
#pragma HLS INTERFACE mode=axis register_mode=both port=outStream register
#pragma HLS INTERFACE mode=axis register_mode=both port=inStream register
#pragma HLS INTERFACE mode=ap_ctrl_none port=return
#pragma HLS DATAFLOW //This is used to chain the operations in this function in order to have a continuous flow of data going through the whole function

	const int rows = 720;//TODO change this to be dynamically set if we want to have more flexibility in what ref image or video stream we feed to the ORB algorithm
	const int cols = 1280;


	xf::cv::Mat<XF_8UC3, 480, 640, XF_NPPC1> imageIn(rows, cols); //RGB 8 bit format IN
	xf::cv::Mat<XF_8UC1, 480, 640, XF_NPPC1> imageOut(rows, cols); //1 channel 8 bit format OUT


	xf::cv::AXIvideo2xfMat(streamIn,imageIn);
	xf::cv::cvtColor<XF_RGB2GRAY, HEIGHT, WIDTH, XF_NPPC1>(imageIn, imageOut);
	xf::cv::xfMat2AXIVideo(imageOut, streamOut);
}
