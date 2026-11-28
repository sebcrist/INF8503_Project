#pragma once

#include "ap_int.h"
#include "ap_axi_sdata.h"
#include "hls_stream.h"
#include "common/xf_common.hpp"
#include "common/xf_structs.hpp"
#include "common/xf_video_mem.hpp"
#include "orb_pattern.h"
const int WIN_SZ = 31;
const int RADIUS = WIN_SZ / 2;

typedef ap_axiu<8, 1, 1, 1> axiData;
typedef ap_axiu<512, 1, 1, 1> axiDescriptor;
typedef ap_uint<8> pix_t;
typedef ap_uint<1> flag_t;

static const ap_uint<14> TAN_LUT[8] = { 101, 311, 547, 840, 1248, 1916, 3376, 10397 };

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
    ap_uint<5> k = 8;
    for (int i = 7; i >= 0; i--) {
#pragma HLS UNROLL
        if (ap_uint<34>(abs_x) * TAN_LUT[i] > y_scaled) k = i;
    }
    ap_uint<5> sector = 0;
    if (x >= 0 && y >= 0) sector = k;
    else if (x < 0 && y >= 0) sector = 16 - k;
    else if (x < 0 && y < 0) sector = 16 + k;
    else sector = (k == 0) ? ap_uint<5>(0) : ap_uint<5>(32 - k);
    return sector;
}

static pix_t select_row(const ap_uint<8 * WIN_SZ>& w, ap_uint<5> row) {
#pragma HLS INLINE
    pix_t out = 0;
    for (int k = 0; k < WIN_SZ; k++) {
#pragma HLS UNROLL
        if (row == k) out = w.range(8 * k + 7, 8 * k);
    }
    return out;
}

template <int ROWS, int COLS, int SCALE_LEVEL>
void detect_and_queue(hls::stream<axiData>& streamIn,
                      hls::stream<axiData>& cornerMask,
                      hls::stream<ColEvent>& col_stream)
{
    typedef ap_uint<8 * (WIN_SZ - 1)> lbword_t;
    lbword_t line_mem[COLS];
#pragma HLS BIND_STORAGE variable=line_mem type=ram_s2p impl=bram
#pragma HLS DEPENDENCE variable=line_mem inter false

    ap_uint<1> mask_ring[16 * COLS];
#pragma HLS BIND_STORAGE variable=mask_ring type=ram_s2p impl=bram
#pragma HLS DEPENDENCE variable=mask_ring inter false

    ap_uint<13> hist_S[WIN_SZ];
    ap_int<16> hist_m01[WIN_SZ];
#pragma HLS ARRAY_PARTITION variable=hist_S complete
#pragma HLS ARRAY_PARTITION variable=hist_m01 complete

    int m00_acc = 0;
    int m01_acc = 0;
    int m10_acc = 0;
    int ptr = 0;
    int delay_offset = (COLS << 4) - COLS + RADIUS;

    Row_Loop:
    for (int r = 0; r < ROWS; r++) {
        Col_Loop:
        for (int c = 0; c < COLS; c++) {
#pragma HLS PIPELINE II=1

            axiData pix_pkt = streamIn.read();
            axiData mask_pkt = cornerMask.read();
            pix_t in_pix = pix_pkt.data(7, 0);
            flag_t in_mask = (mask_pkt.data != 0) ? 1 : 0;

            ColEvent ev;
            ev.is_corner = 0;
            ev.sector = 0;

            lbword_t old_w = line_mem[c];
            lbword_t new_w = (old_w >> 8) | (lbword_t(in_pix) << (8 * (WIN_SZ - 2)));
            line_mem[c] = new_w;
            for (int i = 0; i < WIN_SZ - 1; i++) {
#pragma HLS UNROLL
                ev.col[i] = old_w.range(8 * i + 7, 8 * i);
            }
            ev.col[WIN_SZ - 1] = in_pix;

            mask_ring[ptr] = in_mask;
            int rd_idx = ptr - delay_offset;
            if (rd_idx < 0) {
                rd_idx += (16 * COLS);
            }
            ap_uint<1> center_mask = mask_ring[rd_idx];

            ptr++;
            if (ptr == (16 * COLS)) {
                ptr = 0;
            }

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
            int step_m10 = RADIUS * int(S_in) + (RADIUS + 1) * int(S_out);
            int next_m10 = m10_acc - m00_acc + step_m10;

            m00_acc = (c == 0) ? int(S_in) : next_m00;
            m01_acc = (c == 0) ? int(m01_in) : next_m01;
            m10_acc = (c == 0) ? (RADIUS * int(S_in)) : next_m10;

            if (r >= 2 * RADIUS && c >= 2 * RADIUS && center_mask) {
                ev.is_corner = 1;
                ev.sector = compute_angle_sector(m10_acc, m01_acc);
            }
            col_stream.write(ev);
        }
    }
}

template <int ROWS, int COLS, int SCALE_LEVEL>
void compute_descriptors(hls::stream<ColEvent>& col_stream,
                         hls::stream<axiDescriptor>& streamOut)
{
    typedef ap_uint<8 * WIN_SZ> colword_t;
    colword_t ring[32];
    colword_t winA[32];
    colword_t winB[32];
#pragma HLS BIND_STORAGE variable=ring type=ram_s2p impl=lutram
#pragma HLS BIND_STORAGE variable=winA type=ram_2p impl=lutram
#pragma HLS BIND_STORAGE variable=winB type=ram_2p impl=lutram
#pragma HLS DEPENDENCE variable=ring inter false
#pragma HLS DEPENDENCE variable=winA inter false
#pragma HLS DEPENDENCE variable=winB inter false

    const int SETTLE = 4;

    int r = 0, c = 0;
    bool all_read = false;

    bool want_copy = false;
    ap_int<8> copy_k = -SETTLE;
    ap_uint<5> p_sector = 0, p_center = 0;
    ap_uint<16> p_x = 0, p_y = 0;

    bool eng_busy = false;
    ap_int<9> t = -SETTLE;
    ap_uint<5> e_sector = 0, e_center = 0;
    ap_uint<16> e_x = 0, e_y = 0;
    ap_uint<256> desc = 0;

    Main_Loop:
    while (!(all_read && !want_copy && !eng_busy)) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=1 max=921600

        if (!want_copy && !all_read) {
            ColEvent ev = col_stream.read();
            ap_uint<5> wr_col = c;

            colword_t w = 0;
            for (int i = 0; i < WIN_SZ; i++) {
#pragma HLS UNROLL
                w.range(8 * i + 7, 8 * i) = ev.col[i];
            }
            ring[wr_col] = w;

            if (ev.is_corner) {
                want_copy = true;
                copy_k = -SETTLE;
                p_sector = ev.sector;
                p_center = wr_col - RADIUS;
                p_x = (c - RADIUS) << SCALE_LEVEL;
                p_y = (r - RADIUS) << SCALE_LEVEL;
            }

            if (r == ROWS - 1 && c == COLS - 1) all_read = true;
            if (c == COLS - 1) { c = 0; r++; } else { c++; }
        }

        if (eng_busy) {
            if (t >= 0) {
                if (t < 128) {
                    ap_uint<7> tt = t;
                    ap_uint<2> bits = 0;
                    for (int u = 0; u < 2; u++) {
#pragma HLS UNROLL
                        ap_uint<8> idx = (ap_uint<8>(tt) << 1) | u;
                        int8_t x1 = ORB_PATTERN[e_sector][idx][0];
                        int8_t y1 = ORB_PATTERN[e_sector][idx][1];
                        int8_t x2 = ORB_PATTERN[e_sector][idx][2];
                        int8_t y2 = ORB_PATTERN[e_sector][idx][3];

                        ap_uint<5> r1 = RADIUS + y1;
                        ap_uint<5> c1 = e_center + x1;
                        ap_uint<5> r2 = RADIUS + y2;
                        ap_uint<5> c2 = e_center + x2;

                        colword_t wa = winA[c1];
                        colword_t wb = winB[c2];
                        pix_t p1 = select_row(wa, r1);
                        pix_t p2 = select_row(wb, r2);

                        bits[u] = (p1 < p2);
                    }
                    ap_uint<256> d = desc >> 2;
                    d.range(255, 254) = bits;
                    desc = d;
                    t++;
                } else {
                    ap_uint<512> combined_data = (ap_uint<224>(0), e_y, e_x, desc);
                    axiDescriptor out_pkt;
                    out_pkt.data = combined_data;
                    out_pkt.keep = -1;
                    out_pkt.strb = -1;
                    out_pkt.last = 0;
                    streamOut.write(out_pkt);
                    eng_busy = false;
                }
            } else {
                t++;   // settle cycles after the snapshot
            }
        } else if (want_copy) {
            if (copy_k >= 0) {
                ap_uint<5> kk = copy_k;
                colword_t v = ring[kk];
                winA[kk] = v;
                winB[kk] = v;
            }
            if (copy_k == 31) {
                e_sector = p_sector;
                e_center = p_center;
                e_x = p_x;
                e_y = p_y;
                desc = 0;
                t = -SETTLE;
                eng_busy = true;
                want_copy = false;
                copy_k = -SETTLE;
            } else {
                copy_k++;
            }
        }
    }

    axiDescriptor end_pkt;
    end_pkt.data = 0;
    end_pkt.keep = -1;
    end_pkt.strb = -1;
    end_pkt.last = 1;
    streamOut.write(end_pkt);
}

template <int ROWS, int COLS, int SCALE_LEVEL>
void r_brief(hls::stream<axiData>& streamIn,
             hls::stream<axiData>& cornerMask,
             hls::stream<axiDescriptor>& descriptors)
{
#pragma HLS INTERFACE axis port=streamIn
#pragma HLS INTERFACE axis port=cornerMask
#pragma HLS INTERFACE axis port=descriptors
#pragma HLS INTERFACE ap_ctrl_none port=return
#pragma HLS DATAFLOW

    hls::stream<ColEvent> col_stream;
#pragma HLS STREAM variable=col_stream depth=512

    detect_and_queue<ROWS, COLS, SCALE_LEVEL>(streamIn, cornerMask, col_stream);
    compute_descriptors<ROWS, COLS, SCALE_LEVEL>(col_stream, descriptors);
}