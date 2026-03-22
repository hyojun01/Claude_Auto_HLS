#ifndef CORDIC_HPP
#define CORDIC_HPP

#include <ap_int.h>
#include <ap_fixed.h>
#include <ap_axi_sdata.h>
#include <hls_stream.h>

// ============================================================
// Constants
// ============================================================
const int NUM_ITERATIONS = 14;

// CORDIC gain: K = product(sqrt(1 + 2^(-2i))) for i=0..13 ~ 1.64676025812107
// Inverse gain: 1/K ~ 0.60725293500888
// No gain compensation in hardware — caller must account for K:
//   Rotation mode: set x_in = 1/K, y_in = 0 for direct sin/cos output
//   Vectoring mode: output x_out = K * magnitude (divide by K externally)

// ============================================================
// Type Definitions
// ============================================================

// Data path fixed-point: range [-2, 2), 14 fractional bits
typedef ap_fixed<16, 2> data_t;

// Internal accumulator: range [-2, 2), 30 fractional bits
typedef ap_fixed<32, 2> acc_t;

// Phase angle type
typedef ap_fixed<16, 2> phase_t;

// AXI-Stream types
// Input: 48-bit TDATA (x[16] | y[16] | phase[16]), 1-bit TUSER (mode), TLAST
typedef ap_axiu<48, 1, 0, 0> axis_in_t;

// Output: 48-bit TDATA (x[16] | y[16] | phase[16]), TLAST
typedef ap_axiu<48, 0, 0, 0> axis_out_t;

// ============================================================
// Top Function Prototype
// ============================================================
void cordic(
    hls::stream<axis_in_t>& in_stream,
    hls::stream<axis_out_t>& out_stream
);

#endif // CORDIC_HPP
