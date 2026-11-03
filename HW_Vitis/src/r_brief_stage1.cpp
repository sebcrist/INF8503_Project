#include "r_brief.hpp"


void r_brief_stage1(hls::stream<axiData>& streamIn, 
                    hls::stream<axiData>& cornerMask, 
                    hls::stream<axiDescriptor>& descriptors) 
{
#pragma HLS INTERFACE axis port=streamIn
#pragma HLS INTERFACE axis port=cornerMask
#pragma HLS INTERFACE axis port=descriptors
#pragma HLS INTERFACE ap_ctrl_none port=return

    r_brief<360, 640, 1>(streamIn, cornerMask, descriptors);
}