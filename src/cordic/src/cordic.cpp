#include "cordic.hpp"

// ============================================================
// Arctangent Lookup Table
// ============================================================
// atan(2^(-i)) for i = 0..NUM_ITERATIONS-1
// Stored as acc_t for full internal precision (30 fractional bits)
const acc_t ATAN_TABLE[NUM_ITERATIONS] = {
    0.7853981633974483,   // atan(2^0)  = pi/4
    0.4636476090008061,   // atan(2^-1)
    0.24497866312686414,  // atan(2^-2)
    0.12435499454676144,  // atan(2^-3)
    0.06241880999595735,  // atan(2^-4)
    0.031239833430268277, // atan(2^-5)
    0.015623728620476831, // atan(2^-6)
    0.007812341060101111, // atan(2^-7)
    0.0039062301319669718,// atan(2^-8)
    0.0019531225164788188,// atan(2^-9)
    0.0009765621895593195,// atan(2^-10)
    0.0004882812111948983,// atan(2^-11)
    0.00024414062014936177,// atan(2^-12)
    0.00012207031189367021 // atan(2^-13)
};

// ============================================================
// Top-Level Function
// ============================================================
void cordic(
    hls::stream<axis_in_t>& in_stream,
    hls::stream<axis_out_t>& out_stream
) {
    // Interface pragmas
    #pragma HLS INTERFACE axis port=in_stream
    #pragma HLS INTERFACE axis port=out_stream
    #pragma HLS INTERFACE ap_ctrl_none port=return

    // Pipeline the entire function for II=1 throughput
    #pragma HLS PIPELINE II=1

    // --------------------------------------------------------
    // Read input packet
    // --------------------------------------------------------
    axis_in_t in_pkt = in_stream.read();

    // Unpack TDATA: x[15:0] | y[31:16] | phase[47:32]
    ap_uint<48> in_data = in_pkt.data;
    data_t x_in, y_in, phase_in;
    x_in.range(15, 0) = in_data.range(15, 0);
    y_in.range(15, 0) = in_data.range(31, 16);
    phase_in.range(15, 0) = in_data.range(47, 32);

    // Mode from TUSER: 0 = rotation, 1 = vectoring
    ap_uint<1> mode = in_pkt.user;
    ap_uint<1> last = in_pkt.last;

    // --------------------------------------------------------
    // Initialize accumulators (widen to internal precision)
    // --------------------------------------------------------
    acc_t x = x_in;
    acc_t y = y_in;
    acc_t z = phase_in;

    // --------------------------------------------------------
    // CORDIC iterations (fully unrolled for pipeline)
    // --------------------------------------------------------
    CORDIC_ITER:
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        #pragma HLS UNROLL

        acc_t x_shift = x >> i;
        acc_t y_shift = y >> i;

        // Determine rotation direction:
        //   Rotation mode (mode=0): drive z toward 0 → sigma = sign(z)
        //   Vectoring mode (mode=1): drive y toward 0 → sigma = -sign(y)
        bool rotate_pos = (mode == 0) ? (z >= 0) : (y < 0);

        acc_t x_new, y_new, z_new;
        if (rotate_pos) {
            // Positive rotation (sigma = +1)
            x_new = x - y_shift;
            y_new = y + x_shift;
            z_new = z - ATAN_TABLE[i];
        } else {
            // Negative rotation (sigma = -1)
            x_new = x + y_shift;
            y_new = y - x_shift;
            z_new = z + ATAN_TABLE[i];
        }

        x = x_new;
        y = y_new;
        z = z_new;
    }

    // --------------------------------------------------------
    // Truncate to output width and pack
    // --------------------------------------------------------
    data_t x_out = data_t(x);
    data_t y_out = data_t(y);
    data_t z_out = data_t(z);

    ap_uint<48> out_data;
    out_data.range(15, 0) = x_out.range(15, 0);
    out_data.range(31, 16) = y_out.range(15, 0);
    out_data.range(47, 32) = z_out.range(15, 0);

    // Write output packet
    axis_out_t out_pkt;
    out_pkt.data = out_data;
    out_pkt.last = last;
    out_pkt.keep = -1; // all 6 bytes valid
    out_pkt.strb = -1;
    out_stream.write(out_pkt);
}
