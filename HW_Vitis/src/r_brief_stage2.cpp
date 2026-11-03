#include "r_brief.hpp"


void r_brief_stage2(hls::stream<axiData>& streamIn, 
                    hls::stream<axiData>& cornerMask, 
                    hls::stream<axiDescriptor>& descriptors) 
{
#pragma HLS INTERFACE axis port=streamIn
#pragma HLS INTERFACE axis port=cornerMask
#pragma HLS INTERFACE axis port=descriptors
#pragma HLS INTERFACE ap_ctrl_none port=return

    r_brief<180, 320, 2>(streamIn, cornerMask, descriptors);
}