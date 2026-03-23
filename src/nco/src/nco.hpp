#ifndef NCO_HPP
#define NCO_HPP

#include <ap_int.h>
#include <ap_fixed.h>
#include <ap_axi_sdata.h>
#include <hls_stream.h>

// ============================================================
// Type Definitions
// ============================================================

// Phase accumulator: 32-bit unsigned, wraps naturally at 2^32
typedef ap_uint<32> phase_t;

// LUT output / final output: Q1.15 fixed-point [-1, +1)
typedef ap_fixed<16, 1> sample_t;

// LUT address: 10-bit index into quarter-wave table
typedef ap_uint<10> lut_addr_t;

// AXI-Stream packet: 32-bit TDATA (cos[31:16] | sin[15:0])
typedef ap_axiu<32, 0, 0, 0> axis_pkt;

// ============================================================
// Constants
// ============================================================
const int LUT_DEPTH = 1024;

// ============================================================
// Top Function Prototype
// ============================================================
void nco(
    hls::stream<axis_pkt>& out_stream,
    ap_uint<32> phase_inc,
    ap_uint<32> phase_offset,
    ap_uint<32> num_samples,
    ap_uint<1> sync_reset
);

#endif // NCO_HPP
