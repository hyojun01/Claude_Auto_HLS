#include <iostream>
#include <cmath>
#include <cstdlib>
#include "../src/window.hpp"

// ============================================================
// Test Utilities
// ============================================================
int error_count = 0;

void check_sample(int idx, double actual, double expected, const char* component, double tol) {
    double diff = std::fabs(actual - expected);
    if (diff > tol) {
        std::cerr << "[FAIL] Sample " << idx << " " << component
                  << " — expected: " << expected
                  << ", got: " << actual
                  << ", diff: " << diff << std::endl;
        error_count++;
    }
}

// ============================================================
// Test 1: Single-tone sine wave (10 MHz at 100 MHz sampling)
// ============================================================
void test_sine_wave() {
    std::cout << "--- Test: Single-tone sine wave (10 MHz) ---" << std::endl;

    hls::stream<axis_t> in_stream("in");
    hls::stream<axis_t> out_stream("out");

    const double PI = acos(-1.0);
    const double FREQ = 10.0e6;
    const double FS = 100.0e6;

    // Store quantized input for golden reference comparison
    data_t real_in[WIN_SIZE], imag_in[WIN_SIZE];

    // Generate input: complex sinusoid
    for (int i = 0; i < WIN_SIZE; i++) {
        double phase = 2.0 * PI * FREQ * (double)i / FS;
        double real_val = cos(phase);
        double imag_val = sin(phase);

        // Quantize to ap_fixed<16,1>
        real_in[i] = (data_t)real_val;
        imag_in[i] = (data_t)imag_val;

        // Pack into 32-bit AXI-Stream word
        axis_t word;
        word.data = 0;
        word.data.range(31, 16) = real_in[i].range();
        word.data.range(15, 0) = imag_in[i].range();
        word.last = (i == WIN_SIZE - 1) ? 1 : 0;
        word.keep = -1;
        word.strb = -1;
        in_stream.write(word);
    }

    // Run DUT
    window(in_stream, out_stream);

    // Verify output against golden reference
    // Tolerance: 1e-3 accounts for coefficient quantization + product truncation
    const double TOL = 1e-3;

    for (int i = 0; i < WIN_SIZE; i++) {
        axis_t out_word = out_stream.read();

        // Extract output
        data_t real_out, imag_out;
        real_out.range() = out_word.data.range(31, 16);
        imag_out.range() = out_word.data.range(15, 0);

        // Golden reference: input * Hamming coefficient (double precision)
        double w = 0.54 - 0.46 * cos(2.0 * PI * (double)i / (WIN_SIZE - 1));
        double exp_real = (double)real_in[i] * w;
        double exp_imag = (double)imag_in[i] * w;

        check_sample(i, (double)real_out, exp_real, "real", TOL);
        check_sample(i, (double)imag_out, exp_imag, "imag", TOL);

        // Check TLAST
        if (i == WIN_SIZE - 1 && out_word.last != 1) {
            std::cerr << "[FAIL] TLAST not asserted on last sample" << std::endl;
            error_count++;
        }
        if (i < WIN_SIZE - 1 && out_word.last != 0) {
            std::cerr << "[FAIL] TLAST asserted on sample " << i
                      << " (expected only on last)" << std::endl;
            error_count++;
        }
    }

    if (error_count == 0)
        std::cout << "[PASS] Single-tone sine wave test" << std::endl;
}

// ============================================================
// Test 2: All-zeros input
// ============================================================
void test_zeros() {
    std::cout << "--- Test: All-zeros input ---" << std::endl;

    hls::stream<axis_t> in_stream("in");
    hls::stream<axis_t> out_stream("out");

    for (int i = 0; i < WIN_SIZE; i++) {
        axis_t word;
        word.data = 0;
        word.last = (i == WIN_SIZE - 1) ? 1 : 0;
        word.keep = -1;
        word.strb = -1;
        in_stream.write(word);
    }

    window(in_stream, out_stream);

    for (int i = 0; i < WIN_SIZE; i++) {
        axis_t out_word = out_stream.read();
        if (out_word.data != 0) {
            std::cerr << "[FAIL] Sample " << i
                      << " — expected 0, got " << out_word.data << std::endl;
            error_count++;
        }
    }

    if (error_count == 0)
        std::cout << "[PASS] All-zeros test" << std::endl;
}

// ============================================================
// Test 3: DC input (constant value)
// ============================================================
void test_dc_input() {
    std::cout << "--- Test: DC input (real=0.5, imag=-0.25) ---" << std::endl;

    hls::stream<axis_t> in_stream("in");
    hls::stream<axis_t> out_stream("out");

    const double PI = acos(-1.0);
    data_t dc_real = (data_t)0.5;
    data_t dc_imag = (data_t)(-0.25);

    for (int i = 0; i < WIN_SIZE; i++) {
        axis_t word;
        word.data = 0;
        word.data.range(31, 16) = dc_real.range();
        word.data.range(15, 0) = dc_imag.range();
        word.last = (i == WIN_SIZE - 1) ? 1 : 0;
        word.keep = -1;
        word.strb = -1;
        in_stream.write(word);
    }

    window(in_stream, out_stream);

    const double TOL = 1e-3;
    int local_errors = 0;

    for (int i = 0; i < WIN_SIZE; i++) {
        axis_t out_word = out_stream.read();

        data_t real_out, imag_out;
        real_out.range() = out_word.data.range(31, 16);
        imag_out.range() = out_word.data.range(15, 0);

        double w = 0.54 - 0.46 * cos(2.0 * PI * (double)i / (WIN_SIZE - 1));
        double exp_real = (double)dc_real * w;
        double exp_imag = (double)dc_imag * w;

        check_sample(i, (double)real_out, exp_real, "real", TOL);
        check_sample(i, (double)imag_out, exp_imag, "imag", TOL);
    }

    if (local_errors == 0)
        std::cout << "[PASS] DC input test" << std::endl;
}

// ============================================================
// Test 4: Window symmetry verification
// ============================================================
void test_symmetry() {
    std::cout << "--- Test: Window symmetry ---" << std::endl;

    hls::stream<axis_t> in_stream("in");
    hls::stream<axis_t> out_stream("out");

    // Input: constant real=0.5, imag=0 — output should show symmetric window shape
    data_t const_val = (data_t)0.5;

    for (int i = 0; i < WIN_SIZE; i++) {
        axis_t word;
        word.data = 0;
        word.data.range(31, 16) = const_val.range();
        word.last = (i == WIN_SIZE - 1) ? 1 : 0;
        word.keep = -1;
        word.strb = -1;
        in_stream.write(word);
    }

    window(in_stream, out_stream);

    // Collect outputs
    data_t out_real[WIN_SIZE];
    for (int i = 0; i < WIN_SIZE; i++) {
        axis_t out_word = out_stream.read();
        out_real[i].range() = out_word.data.range(31, 16);
    }

    // Verify symmetry: out[i] == out[N-1-i]
    int sym_errors = 0;
    for (int i = 0; i < WIN_SIZE / 2; i++) {
        if (out_real[i] != out_real[WIN_SIZE - 1 - i]) {
            std::cerr << "[FAIL] Symmetry broken at i=" << i
                      << ": out[" << i << "]=" << (double)out_real[i]
                      << ", out[" << (WIN_SIZE - 1 - i) << "]="
                      << (double)out_real[WIN_SIZE - 1 - i] << std::endl;
            sym_errors++;
            error_count++;
        }
    }

    if (sym_errors == 0)
        std::cout << "[PASS] Window symmetry test" << std::endl;
}

// ============================================================
// Test 5: Back-to-back packets (consecutive processing)
// ============================================================
void test_back_to_back() {
    std::cout << "--- Test: Back-to-back packets ---" << std::endl;

    hls::stream<axis_t> in_stream("in");
    hls::stream<axis_t> out_stream("out");

    const double PI = acos(-1.0);

    // Push two packets
    for (int pkt = 0; pkt < 2; pkt++) {
        double freq = (pkt == 0) ? 5.0e6 : 20.0e6;
        for (int i = 0; i < WIN_SIZE; i++) {
            double phase = 2.0 * PI * freq * (double)i / 100.0e6;
            data_t r = (data_t)cos(phase);
            data_t im = (data_t)sin(phase);

            axis_t word;
            word.data = 0;
            word.data.range(31, 16) = r.range();
            word.data.range(15, 0) = im.range();
            word.last = (i == WIN_SIZE - 1) ? 1 : 0;
            word.keep = -1;
            word.strb = -1;
            in_stream.write(word);
        }
    }

    // Process two packets
    for (int pkt = 0; pkt < 2; pkt++) {
        window(in_stream, out_stream);
    }

    // Verify both packets
    const double TOL = 1e-3;
    for (int pkt = 0; pkt < 2; pkt++) {
        double freq = (pkt == 0) ? 5.0e6 : 20.0e6;
        for (int i = 0; i < WIN_SIZE; i++) {
            axis_t out_word = out_stream.read();

            data_t real_out, imag_out;
            real_out.range() = out_word.data.range(31, 16);
            imag_out.range() = out_word.data.range(15, 0);

            double phase = 2.0 * PI * freq * (double)i / 100.0e6;
            data_t r_in = (data_t)cos(phase);
            data_t im_in = (data_t)sin(phase);
            double w = 0.54 - 0.46 * cos(2.0 * PI * (double)i / (WIN_SIZE - 1));

            check_sample(i, (double)real_out, (double)r_in * w, "real", TOL);
            check_sample(i, (double)imag_out, (double)im_in * w, "imag", TOL);
        }
    }

    if (error_count == 0)
        std::cout << "[PASS] Back-to-back packets test" << std::endl;
}

// ============================================================
// Main
// ============================================================
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Testbench: 256-Point Hamming Window" << std::endl;
    std::cout << "========================================" << std::endl;

    test_sine_wave();
    test_zeros();
    test_dc_input();
    test_symmetry();
    test_back_to_back();

    std::cout << "========================================" << std::endl;
    if (error_count == 0) {
        std::cout << "  ALL TESTS PASSED" << std::endl;
    } else {
        std::cout << "  " << error_count << " TEST(S) FAILED" << std::endl;
    }
    std::cout << "========================================" << std::endl;

    return error_count;
}
