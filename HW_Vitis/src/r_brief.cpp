#include "ap_int.h"
#include "ap_axi_sdata.h"
#include "hls_stream.h"
#include "common/xf_common.hpp"
#include "common/xf_structs.hpp"
#include "common/xf_video_mem.hpp"
#include "orb_pattern.h"

const int MAX_ROWS = 720;
const int MAX_COLS = 1280;
const int WIN_SZ = 31;
const int RADIUS = WIN_SZ / 2;

typedef ap_axiu<8, 1, 1, 1> axiData;
typedef ap_axiu<512, 1, 1, 1> axiDescriptor;
typedef ap_uint<8> pix_t;
typedef ap_uint<1> flag_t;

static const ap_uint<13> TAN_LUT[7] = { 108, 333, 591, 922, 1409, 2300, 4818 }; 

struct ColEvent {
    pix_t col[WIN_SZ]; 
    ap_uint<5> sector; 
    ap_uint<1> is_corner; 
};

static ap_uint<5> compute_angle_sector(int x, int y) { 
#pragma HLS INLINE
#pragma HLS ARRAY_PARTITION variable=TAN_LUT complete dim=1
    ap_uint<20> abs_x = (x >= 0) ? x : -x;
    ap_uint<20> abs_y = (y >= 0) ? y : -y;
    ap_uint<30> y_scaled = ap_uint<30>(abs_y) << 10;
    ap_uint<5> k = 7; 
    for (int i = 6; i >= 0; i--) {
#pragma HLS UNROLL
        if (ap_uint<33>(abs_x) * TAN_LUT[i] > y_scaled) k = i; 
    }
    ap_uint<5> sector = 0; 
    if (x >= 0 && y >= 0) sector = k;
    else if (x < 0 && y >= 0) sector = 15 - k;
    else if (x < 0 && y < 0) sector = 15 + k;
    else sector = (k == 0) ? ap_uint<5>(0) : ap_uint<5>(30 - k); 
    return sector;
}

void detect_and_queue(hls::stream<axiData>& streamIn, 
                      hls::stream<axiData>& cornerMask, 
                      hls::stream<ColEvent>& col_stream, 
                      int rows, int cols) 
{
    xf::cv::LineBuffer<WIN_SZ, MAX_COLS, pix_t> line_buf;
    ap_uint<1> mask_ring[32768];
#pragma HLS BIND_STORAGE variable=mask_ring type=ram_s2p impl=bram
#pragma HLS DEPENDENCE variable=mask_ring inter false

    ap_uint<13> hist_S[WIN_SZ]; 
    ap_int<16> hist_m01[WIN_SZ]; 
#pragma HLS ARRAY_PARTITION variable=hist_S complete
#pragma HLS ARRAY_PARTITION variable=hist_m01 complete

    int m00_acc = 0;
    int m01_acc = 0;
    int m10_acc = 0;
    ap_uint<15> ptr = 0; 
    ap_uint<15> delay_offset = (ap_uint<15>(cols) << 4) - ap_uint<15>(cols) + RADIUS; 

    Row_Loop:
    for (int r = 0; r < rows; r++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=MAX_ROWS
        Col_Loop:
        for (int c = 0; c < cols; c++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=MAX_COLS
#pragma HLS PIPELINE II=1

            axiData pix_pkt = streamIn.read();
            axiData mask_pkt = cornerMask.read();
            pix_t in_pix = pix_pkt.data(7, 0);
            flag_t in_mask = (mask_pkt.data != 0) ? 1 : 0;

            ColEvent ev;
            ev.is_corner = 0; 
            ev.sector = 0;

            line_buf.shift_pixels_up(c);
            line_buf.insert_bottom_row(in_pix, c);
            for (int i = 0; i < WIN_SZ; i++) {
#pragma HLS UNROLL
                ev.col[i] = line_buf.getval(i, c);
            }

            mask_ring[ptr] = in_mask; 
            ap_uint<15> rd_idx = ptr - delay_offset; 
            ap_uint<1> center_mask = mask_ring[rd_idx];
            ptr++;

            ap_uint<13> S_in = ev.col[RADIUS]; 
            ap_int<16> m01_in = 0; 
            for (int k = 1; k <= RADIUS; k++) { 
#pragma HLS UNROLL
                ap_uint<8> p_pos = ev.col[RADIUS + k]; 
                ap_uint<8> p_neg = ev.col[RADIUS - k]; 
                S_in += p_pos + p_neg; 
                m01_in += ap_int<16>(k) * (ap_int<9>(p_pos) - ap_int<9>(p_neg)); 
            }

            ap_uint<13> S_out = (c < WIN_SZ) ? ap_uint<13>(0) : hist_S[0]; 
            ap_int<16> m01_out = (c < WIN_SZ) ? ap_int<16>(0) : hist_m01[0]; 

            for (int k = 0; k < WIN_SZ - 1; k++) {
#pragma HLS UNROLL
                hist_S[k] = hist_S[k + 1];
                hist_m01[k] = hist_m01[k + 1];
            }
            hist_S[WIN_SZ - 1] = S_in;
            hist_m01[WIN_SZ - 1] = m01_in;

            int next_m00 = m00_acc + int(S_in) - int(S_out); 
            int next_m01 = m01_acc + int(m01_in) - int(m01_out); 
            int step_m10 = (RADIUS - 1) * int(S_in) + (RADIUS + 2) * int(S_out); 
            int next_m10 = m10_acc - m00_acc + step_m10;

            m00_acc = (c == 0) ? int(S_in) : next_m00; 
            m01_acc = (c == 0) ? int(m01_in) : next_m01; 
            m10_acc = (c == 0) ? ((RADIUS - 1) * int(S_in)) : next_m10; 

            if (r >= 2 * RADIUS && c >= 2 * RADIUS && center_mask) { 
                ev.is_corner = 1; 
                ev.sector = compute_angle_sector(m10_acc, m01_acc);
            }
            col_stream.write(ev);
        }
    }
}

void compute_descriptors(hls::stream<ColEvent>& col_stream, 
                         hls::stream<axiDescriptor>& streamOut,
                         int rows, int cols) 
{
    pix_t win_buf[WIN_SZ][32]; 

    Row_Loop:
    for (int r = 0; r < rows; r++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=MAX_ROWS
        Col_Loop:
        for (int c = 0; c < cols; c++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=MAX_COLS
            
            ColEvent ev = col_stream.read();
            ap_uint<5> wr_col = c; 
            
            for (int i = 0; i < WIN_SZ; i++) {
#pragma HLS UNROLL
                win_buf[i][wr_col] = ev.col[i]; 
            }

            if (ev.is_corner || (r == rows - 1 && c == cols - 1)) {
                ap_uint<256> descriptor = 0;

                if (ev.is_corner) {
                    ap_uint<5> center_col = wr_col - RADIUS; 
                    Desc_Loop:
                    for (int i = 0; i < 256; i++) {
#pragma HLS PIPELINE II=1
                        int8_t x1 = ORB_PATTERN[ev.sector][i][0];
                        int8_t y1 = ORB_PATTERN[ev.sector][i][1];
                        int8_t x2 = ORB_PATTERN[ev.sector][i][2];
                        int8_t y2 = ORB_PATTERN[ev.sector][i][3];
                        
                        ap_uint<5> r1 = RADIUS + y1; 
                        ap_uint<5> c1 = center_col + x1; 
                        ap_uint<5> r2 = RADIUS + y2; 
                        ap_uint<5> c2 = center_col + x2; 

                        pix_t p1 = win_buf[r1][c1]; 
                        pix_t p2 = win_buf[r2][c2]; 
                        
                        descriptor = (descriptor >> 1) | (ap_uint<256>(p1 < p2) << 255); 
                    }
                }
                ap_uint<512> combined_data = descriptor; 

                if (ev.is_corner) {
                    combined_data(271, 256) = wr_col; 
                    combined_data(287, 272) = r;      
                    combined_data(511, 288) = 0; // Remplit le reste avec des zéros
                } else {
                    combined_data(511, 256) = 0; // Remplit le reste avec des zéros     
                }

                axiDescriptor out_pkt;
                out_pkt.data = combined_data;
                out_pkt.keep = -1; 
                out_pkt.strb = -1;
                out_pkt.last = (r == rows - 1 && c == cols - 1) ? 1 : 0; 
                streamOut.write(out_pkt);
            }
        }
    }
}

void r_brief(hls::stream<axiData>& streamIn, 
             hls::stream<axiData>& cornerMask, 
             hls::stream<axiDescriptor>& descriptors, 
             int rows, int cols) 
{
#pragma HLS INTERFACE axis port=streamIn
#pragma HLS INTERFACE axis port=cornerMask
#pragma HLS INTERFACE axis port=descriptors
#pragma HLS INTERFACE ap_none port=rows
#pragma HLS INTERFACE ap_none port=cols
#pragma HLS INTERFACE ap_ctrl_none port=return
#pragma HLS DATAFLOW

    hls::stream<ColEvent> col_stream;
#pragma HLS STREAM variable=col_stream depth=512

    detect_and_queue(streamIn, cornerMask, col_stream, rows, cols);
    compute_descriptors(col_stream, descriptors, rows, cols);
}