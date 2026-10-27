#include "ap_int.h"
#include "ap_fixed.h"
#include "ap_axi_sdata.h"
#include "hls_stream.h"
#include "common/xf_common.hpp"
#include "common/xf_structs.hpp"
#include "common/xf_video_mem.hpp"

const int MAX_ROWS = 720;
const int MAX_COLS = 1280;
const int WIN_SZ = 31;
const int RADIUS = WIN_SZ / 2;
const int TRIP_ROWS = MAX_ROWS + RADIUS;
const int TRIP_COLS = MAX_COLS + RADIUS;

typedef ap_axiu<8, 1, 1, 1> axiData;
typedef ap_uint<8> pix_t;
typedef ap_uint<1> flag_t;
typedef ap_int<24> mom_t;
typedef ap_fixed<24, 12> coord_t;
typedef ap_fixed<18, 6> tan_t;

static const tan_t TAN_LUT[7] = {
    0.105104,
    0.324920,
    0.577350,
    0.900404,
    1.376382,
    2.246037,
    4.704630
};

static int compute_angle_sector(coord_t x, coord_t y) {
#pragma HLS INLINE
#pragma HLS ARRAY_PARTITION variable=TAN_LUT complete dim=1

    coord_t abs_x = (x >= 0) ? x : (coord_t)(-x);
    coord_t abs_y = (y >= 0) ? y : (coord_t)(-y);

    int k = 7;
    for (int i = 6; i >= 0; i--) {
#pragma HLS UNROLL
        if (abs_x * TAN_LUT[i] > abs_y) {
            k = i;
        }
    }

    int sector = 0;
    if (x >= 0 && y >= 0) {
        sector = k;
    } else if (x < 0 && y >= 0) {
        sector = 15 - k;
    } else if (x < 0 && y < 0) {
        sector = 15 + k;
    } else {
        sector = (k == 0) ? 0 : (30 - k);
    }

    return sector;
}

void r_brief(hls::stream<axiData>& streamIn, hls::stream<axiData>& cornerMask, hls::stream<axiData>& streamOut, int rows, int cols) {
#pragma HLS INTERFACE axis port=streamIn
#pragma HLS INTERFACE axis port=cornerMask
#pragma HLS INTERFACE axis port=streamOut
#pragma HLS INTERFACE s_axilite port=rows
#pragma HLS INTERFACE s_axilite port=cols
#pragma HLS INTERFACE s_axilite port=return

    xf::cv::LineBuffer<WIN_SZ, MAX_COLS, pix_t> line_buf;
    xf::cv::Window<WIN_SZ, WIN_SZ, pix_t> win;

    xf::cv::LineBuffer<WIN_SZ, MAX_COLS, flag_t> mask_line_buf;
    xf::cv::Window<WIN_SZ, WIN_SZ, flag_t> mask_win;

    Row_Loop:
    for (int r = 0; r < rows + RADIUS; r++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=TRIP_ROWS
        Col_Loop:
        for (int c = 0; c < cols + RADIUS; c++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=TRIP_COLS
#pragma HLS PIPELINE II=1

            pix_t in_pix = 0;
            flag_t in_mask = 0;

            if (r < rows && c < cols) {
                axiData pix_pkt = streamIn.read();
                axiData mask_pkt = cornerMask.read();
                in_pix = pix_pkt.data(7, 0);
                in_mask = (mask_pkt.data != 0) ? 1 : 0;
            }

            win.shift_pixels_left();
            mask_win.shift_pixels_left();

            if (c < cols) {
                line_buf.shift_pixels_up(c);
                line_buf.insert_bottom_row(in_pix, c);

                mask_line_buf.shift_pixels_up(c);
                mask_line_buf.insert_bottom_row(in_mask, c);

                for (int wr = 0; wr < WIN_SZ; wr++) {
#pragma HLS UNROLL
                    win.insert_pixel(line_buf.getval(wr, c), wr, WIN_SZ - 1);
                    mask_win.insert_pixel(mask_line_buf.getval(wr, c), wr, WIN_SZ - 1);
                }
            } else {
                for (int wr = 0; wr < WIN_SZ; wr++) {
#pragma HLS UNROLL
                    win.insert_pixel(0, wr, WIN_SZ - 1);
                    mask_win.insert_pixel(0, wr, WIN_SZ - 1);
                }
            }

            if (r >= RADIUS && c >= RADIUS) {
                int center_r = r - RADIUS;
                int center_c = c - RADIUS;

                bool border_valid = (center_r >= RADIUS) && (center_r < rows - RADIUS) &&
                                    (center_c >= RADIUS) && (center_c < cols - RADIUS);

                flag_t is_corner = mask_win.getval(RADIUS, RADIUS);

                if (border_valid && is_corner) {
                    // RESOURCE-SAVING PROFILE ACCUMULATOR
                    ap_uint<13> col_sum[WIN_SZ];
                    ap_uint<13> row_sum[WIN_SZ];
#pragma HLS ARRAY_PARTITION variable=col_sum complete dim=1
#pragma HLS ARRAY_PARTITION variable=row_sum complete dim=1

                    for (int i = 0; i < WIN_SZ; i++) {
#pragma HLS UNROLL
                        col_sum[i] = 0;
                        row_sum[i] = 0;
                    }

                    // Partially unrolled loop: unrolls columns only to restrict logic explosion
                    for (int wr = 0; wr < WIN_SZ; wr++) {
                        for (int wc = 0; wc < WIN_SZ; wc++) {
#pragma HLS UNROLL
                            int dx = wc - RADIUS;
                            int dy = wr - RADIUS;
                            if (dx * dx + dy * dy <= RADIUS * RADIUS) {
                                pix_t p = win.getval(wr, wc);
                                col_sum[wc] += p;
                                row_sum[wr] += p;
                            }
                        }
                    }

                    mom_t m10 = 0;
                    mom_t m01 = 0;

                    for (int d = 1; d <= RADIUS; d++) {
#pragma HLS UNROLL
                        ap_int<14> diff_x = (ap_int<14>)col_sum[RADIUS + d] - (ap_int<14>)col_sum[RADIUS - d];
                        ap_int<14> diff_y = (ap_int<14>)row_sum[RADIUS + d] - (ap_int<14>)row_sum[RADIUS - d];
                        m10 += d * diff_x;
                        m01 += d * diff_y;
                    }

                    int sector = compute_angle_sector((coord_t)m10, (coord_t)m01);

                    axiData out_pkt;
                    out_pkt.data = (ap_uint<64>)sector;
                    out_pkt.keep = 0xFF;
                    out_pkt.strb = 0xFF;
                    out_pkt.last = 0;
                    
                    streamOut.write(out_pkt);
                }
            }
        }
    }
}