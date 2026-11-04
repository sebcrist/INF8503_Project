#include "ap_axi_sdata.h"
#include "hls_stream.h"

typedef ap_axiu<512, 1, 1, 1> axiDescriptor;

void descriptor_merger(hls::stream<axiDescriptor>& stream_stage0,
                        hls::stream<axiDescriptor>& stream_stage1,
                        hls::stream<axiDescriptor>& stream_stage2,
                        hls::stream<axiDescriptor>& stream_out) 
{
#pragma HLS INTERFACE ap_ctrl_none port=return
#pragma HLS INTERFACE axis port=stream_stage0
#pragma HLS INTERFACE axis port=stream_stage1
#pragma HLS INTERFACE axis port=stream_stage2
#pragma HLS INTERFACE axis port=stream_out

#pragma HLS PIPELINE II=1

    axiDescriptor pkt;

    if (stream_stage2.read_nb(pkt)) 
    {
        pkt.last = 0;
        stream_out.write(pkt);
    }
    else if (stream_stage1.read_nb(pkt)) 
    {
        pkt.last = 0;
        stream_out.write(pkt);
    } 
    else if (stream_stage0.read_nb(pkt)) 
    {
        stream_out.write(pkt);
    } 
}