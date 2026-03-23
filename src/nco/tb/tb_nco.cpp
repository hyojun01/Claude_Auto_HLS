#include <iostream>
#include <cstdlib>
#include <cmath>
#include <cstdio>
#include "../src/nco.hpp"

// ============================================================
// Test Utilities
// ============================================================
int error_count = 0;
int test_errors = 0;

// Unpack I/Q from AXI-Stream packet
void unpack_iq(const axis_pkt& pkt, double& cos_out, double& sin_out) {
    sample_t s, c;
    s.range(15, 0) = pkt.data.range(15, 0);
    c.range(15, 0) = pkt.data.range(31, 16);
    sin_out = (double)s;
    cos_out = (double)c;
}

// Reference: double-precision sin/cos from 32-bit phase
void ref_sincos(uint32_t phase, double& ref_sin, double& ref_cos) {
    double angle = (double)phase / 4294967296.0 * 2.0 * M_PI;
    ref_sin = sin(angle);
    ref_cos = cos(angle);
}

// ============================================================
// Test 1: DC Output (f_out = 0 Hz)
// ============================================================
void test_dc_output() {
    printf("\n--- Test 1: DC output (phase_inc=0) ---\n");
    test_errors = 0;
    hls::stream<axis_pkt> out;

    nco(out, 0, 0, 16, 1);

    for (int i = 0; i < 16; i++) {
        axis_pkt pkt = out.read();
        double cv, sv;
        unpack_iq(pkt, cv, sv);

        // cos(0) ≈ 0.999969 (max positive), sin(0) = 0.0
        if (fabs(sv) > 0.0001) {
            printf("  [FAIL] sample %d: sin=%.6f (expected 0.0)\n", i, sv);
            test_errors++;
        }
        if (fabs(cv - 0.999969482421875) > 0.0001) {
            printf("  [FAIL] sample %d: cos=%.6f (expected ~1.0)\n", i, cv);
            test_errors++;
        }
    }
    if (test_errors == 0) printf("[PASS] Test 1: DC output\n");
    else printf("[FAIL] Test 1: %d errors\n", test_errors);
    error_count += test_errors;
}

// ============================================================
// Test 2: f_out = f_clk/4 (quadrant transitions)
// ============================================================
void test_fclk_div4() {
    printf("\n--- Test 2: f_clk/4 (phase_inc=0x40000000) ---\n");
    test_errors = 0;
    hls::stream<axis_pkt> out;

    nco(out, 0x40000000, 0, 8, 1);

    // Expected approximate sequences:
    // cos: {~1, 0, ~-1, 0, ~1, 0, ~-1, 0}
    // sin: {0, ~1, 0, ~-1, 0, ~1, 0, ~-1}
    const double exp_cos[] = { 0.999969, 0.0, -0.999969, 0.0, 0.999969, 0.0, -0.999969, 0.0};
    const double exp_sin[] = { 0.0, 0.999969, 0.0, -0.999969, 0.0, 0.999969, 0.0, -0.999969};

    for (int i = 0; i < 8; i++) {
        axis_pkt pkt = out.read();
        double cv, sv;
        unpack_iq(pkt, cv, sv);

        double tol = 0.001;
        if (fabs(sv - exp_sin[i]) > tol) {
            printf("  [FAIL] sample %d: sin=%.6f (expected %.6f)\n", i, sv, exp_sin[i]);
            test_errors++;
        }
        if (fabs(cv - exp_cos[i]) > tol) {
            printf("  [FAIL] sample %d: cos=%.6f (expected %.6f)\n", i, cv, exp_cos[i]);
            test_errors++;
        }
    }
    if (test_errors == 0) printf("[PASS] Test 2: f_clk/4 quadrant transitions\n");
    else printf("[FAIL] Test 2: %d errors\n", test_errors);
    error_count += test_errors;
}

// ============================================================
// Test 3: f_out = f_clk/8 (compare vs math.h)
// ============================================================
void test_fclk_div8() {
    printf("\n--- Test 3: f_clk/8 (phase_inc=0x20000000, 16 samples) ---\n");
    test_errors = 0;
    hls::stream<axis_pkt> out;

    nco(out, 0x20000000, 0, 16, 1);

    double max_sin_err = 0, max_cos_err = 0;
    uint32_t phase = 0;
    // 1024-entry quarter-wave LUT without interpolation: worst-case error
    // bounded by sin(pi/(2*LUT_DEPTH)) ~ pi/(2*1024) ~ 0.00153
    double tol = 0.002;  // conservative bound for 1024-entry LUT

    for (int i = 0; i < 16; i++) {
        axis_pkt pkt = out.read();
        double cv, sv;
        unpack_iq(pkt, cv, sv);

        double ref_s, ref_c;
        ref_sincos(phase, ref_s, ref_c);

        double serr = fabs(sv - ref_s);
        double cerr = fabs(cv - ref_c);
        if (serr > max_sin_err) max_sin_err = serr;
        if (cerr > max_cos_err) max_cos_err = cerr;

        if (serr > tol || cerr > tol) {
            printf("  [FAIL] sample %d: sin=%.6f (ref=%.6f, err=%.6f), cos=%.6f (ref=%.6f, err=%.6f)\n",
                   i, sv, ref_s, serr, cv, ref_c, cerr);
            test_errors++;
        }
        phase += 0x20000000;
    }
    printf("  Max sin error: %.8f, max cos error: %.8f (tol: %.8f)\n",
           max_sin_err, max_cos_err, tol);
    if (test_errors == 0) printf("[PASS] Test 3: f_clk/8\n");
    else printf("[FAIL] Test 3: %d errors\n", test_errors);
    error_count += test_errors;
}

// ============================================================
// Test 4: Arbitrary frequency — full sweep verification
// ============================================================
void test_arbitrary_freq() {
    printf("\n--- Test 4: Arbitrary frequency (phase_inc=0x01000000, 256 samples) ---\n");
    test_errors = 0;
    hls::stream<axis_pkt> out;

    nco(out, 0x01000000, 0, 256, 1);

    double max_sin_err = 0, max_cos_err = 0;
    double sum_sin_err2 = 0, sum_cos_err2 = 0;
    uint32_t phase = 0;
    double tol = 0.002;  // 1024-entry quarter-wave LUT bound

    for (int i = 0; i < 256; i++) {
        axis_pkt pkt = out.read();
        double cv, sv;
        unpack_iq(pkt, cv, sv);

        double ref_s, ref_c;
        ref_sincos(phase, ref_s, ref_c);

        double serr = fabs(sv - ref_s);
        double cerr = fabs(cv - ref_c);
        sum_sin_err2 += serr * serr;
        sum_cos_err2 += cerr * cerr;
        if (serr > max_sin_err) max_sin_err = serr;
        if (cerr > max_cos_err) max_cos_err = cerr;

        if (serr > tol || cerr > tol) {
            if (test_errors < 5) {
                printf("  [FAIL] sample %d: sin err=%.8f, cos err=%.8f\n", i, serr, cerr);
            }
            test_errors++;
        }
        phase += 0x01000000;
    }
    double rms_sin = sqrt(sum_sin_err2 / 256.0);
    double rms_cos = sqrt(sum_cos_err2 / 256.0);
    printf("  Max error — sin: %.8f, cos: %.8f\n", max_sin_err, max_cos_err);
    printf("  RMS error — sin: %.8f, cos: %.8f\n", rms_sin, rms_cos);
    if (test_errors == 0) printf("[PASS] Test 4: Arbitrary frequency\n");
    else printf("[FAIL] Test 4: %d errors\n", test_errors);
    error_count += test_errors;
}

// ============================================================
// Test 5: Phase offset (sin with pi/2 offset should match cos)
// ============================================================
void test_phase_offset() {
    printf("\n--- Test 5: Phase offset ---\n");
    test_errors = 0;

    // Run 1: with pi/2 offset
    hls::stream<axis_pkt> out1;
    nco(out1, 0x10000000, 0x40000000, 16, 1);

    // Run 2: without offset (need to reset phase accumulator)
    hls::stream<axis_pkt> out2;
    nco(out2, 0x10000000, 0, 16, 1);

    for (int i = 0; i < 16; i++) {
        axis_pkt p1 = out1.read();
        axis_pkt p2 = out2.read();
        double c1, s1, c2, s2;
        unpack_iq(p1, c1, s1);
        unpack_iq(p2, c2, s2);

        // sin(phase + pi/2) = cos(phase), cos(phase + pi/2) = -sin(phase)
        double err1 = fabs(s1 - c2);  // sin with offset should equal cos without
        double err2 = fabs(c1 - (-s2)); // cos with offset should equal -sin without

        if (err1 > 0.0001 || err2 > 0.0001) {
            printf("  [FAIL] sample %d: sin_offset=%.6f vs cos_ref=%.6f (err=%.6f), "
                   "cos_offset=%.6f vs -sin_ref=%.6f (err=%.6f)\n",
                   i, s1, c2, err1, c1, -s2, err2);
            test_errors++;
        }
    }
    if (test_errors == 0) printf("[PASS] Test 5: Phase offset\n");
    else printf("[FAIL] Test 5: %d errors\n", test_errors);
    error_count += test_errors;
}

// ============================================================
// Test 6: Synchronous reset
// ============================================================
void test_sync_reset() {
    printf("\n--- Test 6: Synchronous reset ---\n");
    test_errors = 0;

    // Run 1: Initial burst from phase 0
    hls::stream<axis_pkt> out1;
    nco(out1, 0x10000000, 0, 8, 1);
    double ref_sin[8], ref_cos[8];
    for (int i = 0; i < 8; i++) {
        axis_pkt pkt = out1.read();
        unpack_iq(pkt, ref_cos[i], ref_sin[i]);
    }

    // Run 2: Advance phase (no reset) — phase now continues
    hls::stream<axis_pkt> out2;
    nco(out2, 0x10000000, 0, 4, 0);
    for (int i = 0; i < 4; i++) out2.read();

    // Run 3: Reset and regenerate — should match run 1
    hls::stream<axis_pkt> out3;
    nco(out3, 0x10000000, 0, 8, 1);
    for (int i = 0; i < 8; i++) {
        axis_pkt pkt = out3.read();
        double cv, sv;
        unpack_iq(pkt, cv, sv);

        if (fabs(sv - ref_sin[i]) > 0.0001 || fabs(cv - ref_cos[i]) > 0.0001) {
            printf("  [FAIL] sample %d after reset: sin=%.6f (expected %.6f), cos=%.6f (expected %.6f)\n",
                   i, sv, ref_sin[i], cv, ref_cos[i]);
            test_errors++;
        }
    }
    if (test_errors == 0) printf("[PASS] Test 6: Sync reset\n");
    else printf("[FAIL] Test 6: %d errors\n", test_errors);
    error_count += test_errors;
}

// ============================================================
// Test 7: TLAST generation
// ============================================================
void test_tlast() {
    printf("\n--- Test 7: TLAST generation ---\n");
    test_errors = 0;
    hls::stream<axis_pkt> out;

    nco(out, 0x10000000, 0, 10, 1);

    for (int i = 0; i < 10; i++) {
        axis_pkt pkt = out.read();
        ap_uint<1> expected_last = (i == 9) ? ap_uint<1>(1) : ap_uint<1>(0);

        if (pkt.last != expected_last) {
            printf("  [FAIL] sample %d: TLAST=%d (expected %d)\n",
                   i, (int)pkt.last, (int)expected_last);
            test_errors++;
        }
    }

    // Verify no extra samples
    if (!out.empty()) {
        printf("  [FAIL] Extra samples after TLAST\n");
        test_errors++;
    }

    if (test_errors == 0) printf("[PASS] Test 7: TLAST generation\n");
    else printf("[FAIL] Test 7: %d errors\n", test_errors);
    error_count += test_errors;
}

// ============================================================
// Test 8: Phase continuity across invocations
// ============================================================
void test_phase_continuity() {
    printf("\n--- Test 8: Phase continuity across invocations ---\n");
    test_errors = 0;

    // Generate 8 samples in a single burst (reference)
    hls::stream<axis_pkt> out_ref;
    nco(out_ref, 0x10000000, 0, 8, 1);
    double ref_sin[8], ref_cos[8];
    for (int i = 0; i < 8; i++) {
        axis_pkt pkt = out_ref.read();
        unpack_iq(pkt, ref_cos[i], ref_sin[i]);
    }

    // Generate same 8 samples as two bursts of 4 (no reset between)
    hls::stream<axis_pkt> out1, out2;
    nco(out1, 0x10000000, 0, 4, 1);
    nco(out2, 0x10000000, 0, 4, 0);  // continues from burst 1

    for (int i = 0; i < 4; i++) {
        axis_pkt pkt = out1.read();
        double cv, sv;
        unpack_iq(pkt, cv, sv);
        if (fabs(sv - ref_sin[i]) > 0.0001 || fabs(cv - ref_cos[i]) > 0.0001) {
            printf("  [FAIL] burst 1, sample %d: sin=%.6f (ref %.6f), cos=%.6f (ref %.6f)\n",
                   i, sv, ref_sin[i], cv, ref_cos[i]);
            test_errors++;
        }
    }
    for (int i = 0; i < 4; i++) {
        axis_pkt pkt = out2.read();
        double cv, sv;
        unpack_iq(pkt, cv, sv);
        if (fabs(sv - ref_sin[4 + i]) > 0.0001 || fabs(cv - ref_cos[4 + i]) > 0.0001) {
            printf("  [FAIL] burst 2, sample %d: sin=%.6f (ref %.6f), cos=%.6f (ref %.6f)\n",
                   i, sv, ref_sin[4 + i], cv, ref_cos[4 + i]);
            test_errors++;
        }
    }
    if (test_errors == 0) printf("[PASS] Test 8: Phase continuity\n");
    else printf("[FAIL] Test 8: %d errors\n", test_errors);
    error_count += test_errors;
}

// ============================================================
// Main
// ============================================================
int main() {
    printf("========================================\n");
    printf("  Testbench: NCO\n");
    printf("========================================\n");

    test_dc_output();
    test_fclk_div4();
    test_fclk_div8();
    test_arbitrary_freq();
    test_phase_offset();
    test_sync_reset();
    test_tlast();
    test_phase_continuity();

    printf("\n========================================\n");
    if (error_count == 0) {
        printf("  ALL TESTS PASSED\n");
    } else {
        printf("  %d TEST(S) FAILED\n", error_count);
    }
    printf("========================================\n");

    return error_count;
}
