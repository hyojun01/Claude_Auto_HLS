#ifndef WINDOW_HPP
#define WINDOW_HPP

#include <ap_int.h>
#include <ap_fixed.h>
#include <hls_stream.h>
#include <ap_axi_sdata.h>

// ============================================================
// Constants
// ============================================================
const int WIN_SIZE = 256;

// ============================================================
// Type Definitions
// ============================================================
// Input/output sample: Q1.15 signed fixed-point (range [-1, ~1))
typedef ap_fixed<16, 1> data_t;

// Window coefficient: unsigned 0.16 fixed-point (range [0, ~1))
// Hamming coefficients are in [0.08, ~0.9999], all non-negative and < 1.0
typedef ap_ufixed<16, 0> coeff_t;

// Full-precision product: data_t(16,1) * coeff_t(16,0) = (32,1)
// Prevents overflow during multiplication
typedef ap_fixed<32, 1> acc_t;

// AXI-Stream word: 32-bit data (upper 16 = real, lower 16 = imag) with TLAST
typedef ap_axiu<32, 0, 0, 0> axis_t;

// Internal data word for DATAFLOW inter-stage communication
// (ap_axiu cannot be used for non-port streams in Vitis HLS)
typedef ap_uint<32> internal_t;

// ============================================================
// Top Function Prototype
// ============================================================
void window(hls::stream<axis_t>& in, hls::stream<axis_t>& out);

#endif // WINDOW_HPP
