#include <iostream>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include "../src/cordic.hpp"

// ============================================================
// Test Configuration
// ============================================================
int error_count = 0;

const double TOLERANCE = 1.0 / (1 << 12); // 2^(-12) ~ 0.000244
const double CORDIC_K = 1.64676025812107;
const double CORDIC_INV_K = 0.60725293500888;

// ============================================================
// Helper Functions
// ============================================================

// Pack fields into an AXI-Stream input packet
axis_in_t make_input(double x, double y, double phase, int mode, int last) {
    axis_in_t pkt;
    data_t x_d = x;
    data_t y_d = y;
    data_t phase_d = phase;

    ap_uint<48> data;
    data.range(15, 0) = x_d.range(15, 0);
    data.range(31, 16) = y_d.range(15, 0);
    data.range(47, 32) = phase_d.range(15, 0);

    pkt.data = data;
    pkt.user = mode;
    pkt.last = last;
    pkt.keep = -1;
    pkt.strb = -1;
    return pkt;
}

// Unpack fields from an AXI-Stream output packet
void unpack_output(const axis_out_t& pkt,
                   double& x, double& y, double& phase, int& last) {
    ap_uint<48> data = pkt.data;
    data_t x_d, y_d, phase_d;
    x_d.range(15, 0) = data.range(15, 0);
    y_d.range(15, 0) = data.range(31, 16);
    phase_d.range(15, 0) = data.range(47, 32);

    x = x_d.to_double();
    y = y_d.to_double();
    phase = phase_d.to_double();
    last = pkt.last;
}

// Check a value against expected with tolerance
void check_val(const char* label, double actual, double expected, double tol) {
    double err = fabs(actual - expected);
    if (err > tol) {
        printf("  [FAIL] %s: expected %.8f, got %.8f, err %.8f > tol %.6f\n",
               label, expected, actual, err, tol);
        error_count++;
    } else {
        printf("  [PASS] %s: expected %.8f, got %.8f, err %.8f\n",
               label, expected, actual, err);
    }
}

// ============================================================
// Test 1: Rotation Mode — Known Angles
// ============================================================
void test_rotation_known_angles() {
    printf("\n=== Test 1: Rotation Mode — Known Angles ===\n");

    // Angles within CORDIC convergence range (~[-1.74, 1.74] rad)
    const double angles[] = {
        0.0,
        M_PI / 6.0,   // 30 deg
        M_PI / 4.0,   // 45 deg
        M_PI / 3.0,   // 60 deg
        M_PI / 2.0,   // 90 deg (near convergence boundary)
        -M_PI / 4.0,  // -45 deg
        -M_PI / 2.0   // -90 deg
    };
    const int N = sizeof(angles) / sizeof(angles[0]);

    hls::stream<axis_in_t> in_stream("test1_in");
    hls::stream<axis_out_t> out_stream("test1_out");

    // Push all inputs: x = 1/K (gain pre-compensation), y = 0
    for (int i = 0; i < N; i++) {
        in_stream.write(make_input(CORDIC_INV_K, 0.0, angles[i], 0, (i == N - 1)));
    }

    // Process all samples
    for (int i = 0; i < N; i++) {
        cordic(in_stream, out_stream);
    }

    // Verify outputs
    for (int i = 0; i < N; i++) {
        double x_out, y_out, phase_out;
        int last;
        unpack_output(out_stream.read(), x_out, y_out, phase_out, last);

        double exp_cos = cos(angles[i]);
        double exp_sin = sin(angles[i]);

        char label_cos[64], label_sin[64];
        snprintf(label_cos, sizeof(label_cos), "cos(%.4f)", angles[i]);
        snprintf(label_sin, sizeof(label_sin), "sin(%.4f)", angles[i]);

        check_val(label_cos, x_out, exp_cos, TOLERANCE);
        check_val(label_sin, y_out, exp_sin, TOLERANCE);
    }
}

// ============================================================
// Test 2: Vectoring Mode — Known Vectors
// ============================================================
void test_vectoring_known_vectors() {
    printf("\n=== Test 2: Vectoring Mode — Known Vectors ===\n");

    struct vec_test {
        double x, y;
        double exp_mag;   // K * sqrt(x^2 + y^2) — raw CORDIC output
        double exp_phase;
        bool in_range;    // whether result is within convergence domain
    };

    // Convergence constraints:
    //   1. x > 0 (or x = 0 with y != 0) for vectoring mode convergence
    //   2. K * sqrt(x^2 + y^2) must fit in acc_t range [-2, 2)
    //      i.e., sqrt(x^2 + y^2) < 2/K ~ 1.214
    vec_test tests[] = {
        { 1.0,    0.0,    CORDIC_K * 1.0,                    atan2(0.0, 1.0),      true  },
        { 0.0,    1.0,    CORDIC_K * 1.0,                    atan2(1.0, 0.0),      true  },
        { 1.0,    1.0,    CORDIC_K * sqrt(2.0),              atan2(1.0, 1.0),      false }, // K*sqrt(2) > 2.0, overflows acc_t
        { 0.5,    0.866,  CORDIC_K * sqrt(0.25 + 0.749956),  atan2(0.866, 0.5),    true  },
        {-1.0,    1.0,    CORDIC_K * sqrt(2.0),              atan2(1.0, -1.0),     false }, // x < 0
    };
    const int N = sizeof(tests) / sizeof(tests[0]);

    hls::stream<axis_in_t> in_stream("test2_in");
    hls::stream<axis_out_t> out_stream("test2_out");

    for (int i = 0; i < N; i++) {
        in_stream.write(make_input(tests[i].x, tests[i].y, 0.0, 1, (i == N - 1)));
    }

    for (int i = 0; i < N; i++) {
        cordic(in_stream, out_stream);
    }

    for (int i = 0; i < N; i++) {
        double x_out, y_out, phase_out;
        int last;
        unpack_output(out_stream.read(), x_out, y_out, phase_out, last);

        char label_mag[64], label_phase[64];
        snprintf(label_mag, sizeof(label_mag), "mag(%.2f,%.2f)", tests[i].x, tests[i].y);
        snprintf(label_phase, sizeof(label_phase), "phase(%.2f,%.2f)", tests[i].x, tests[i].y);

        if (tests[i].in_range) {
            check_val(label_mag, x_out, tests[i].exp_mag, TOLERANCE * CORDIC_K);
            check_val(label_phase, phase_out, tests[i].exp_phase, TOLERANCE);
        } else {
            printf("  [INFO] %s: outside representable/convergence domain "
                   "(got mag=%.6f, phase=%.6f)\n",
                   label_mag, x_out, phase_out);
        }
    }
}

// ============================================================
// Test 3: Full Rotation Sweep
// ============================================================
void test_rotation_sweep() {
    printf("\n=== Test 3: Rotation Sweep (-1.5 to 1.5 rad, 256 steps) ===\n");

    const int N = 256;
    const double ANGLE_MIN = -1.5;
    const double ANGLE_MAX = 1.5;
    const double STEP = (ANGLE_MAX - ANGLE_MIN) / (N - 1);

    hls::stream<axis_in_t> in_stream("test3_in");
    hls::stream<axis_out_t> out_stream("test3_out");

    double angles[N];
    for (int i = 0; i < N; i++) {
        angles[i] = ANGLE_MIN + i * STEP;
        in_stream.write(make_input(CORDIC_INV_K, 0.0, angles[i], 0, (i == N - 1)));
    }

    for (int i = 0; i < N; i++) {
        cordic(in_stream, out_stream);
    }

    double max_cos_err = 0, max_sin_err = 0;
    double sum_cos_sq = 0, sum_sin_sq = 0;

    for (int i = 0; i < N; i++) {
        double x_out, y_out, phase_out;
        int last;
        unpack_output(out_stream.read(), x_out, y_out, phase_out, last);

        double cos_err = fabs(x_out - cos(angles[i]));
        double sin_err = fabs(y_out - sin(angles[i]));

        if (cos_err > max_cos_err) max_cos_err = cos_err;
        if (sin_err > max_sin_err) max_sin_err = sin_err;
        sum_cos_sq += cos_err * cos_err;
        sum_sin_sq += sin_err * sin_err;
    }

    double rms_cos = sqrt(sum_cos_sq / N);
    double rms_sin = sqrt(sum_sin_sq / N);

    printf("  Cos — max err: %.8f, RMS err: %.8f\n", max_cos_err, rms_cos);
    printf("  Sin — max err: %.8f, RMS err: %.8f\n", max_sin_err, rms_sin);

    if (max_cos_err > TOLERANCE) {
        printf("  [FAIL] Cos max error %.8f exceeds tolerance %.6f\n",
               max_cos_err, TOLERANCE);
        error_count++;
    } else {
        printf("  [PASS] Cos sweep within tolerance\n");
    }

    if (max_sin_err > TOLERANCE) {
        printf("  [FAIL] Sin max error %.8f exceeds tolerance %.6f\n",
               max_sin_err, TOLERANCE);
        error_count++;
    } else {
        printf("  [PASS] Sin sweep within tolerance\n");
    }
}

// ============================================================
// Test 4: TLAST Propagation
// ============================================================
void test_tlast_propagation() {
    printf("\n=== Test 4: TLAST Propagation ===\n");

    const int BURST_LEN = 10;

    hls::stream<axis_in_t> in_stream("test4_in");
    hls::stream<axis_out_t> out_stream("test4_out");

    // Send 10 samples in rotation mode, TLAST only on the last
    for (int i = 0; i < BURST_LEN; i++) {
        double angle = 0.1 * (i + 1);
        int is_last = (i == BURST_LEN - 1) ? 1 : 0;
        in_stream.write(make_input(CORDIC_INV_K, 0.0, angle, 0, is_last));
    }

    for (int i = 0; i < BURST_LEN; i++) {
        cordic(in_stream, out_stream);
    }

    bool tlast_ok = true;
    for (int i = 0; i < BURST_LEN; i++) {
        axis_out_t pkt = out_stream.read();
        int expected_last = (i == BURST_LEN - 1) ? 1 : 0;
        if ((int)pkt.last != expected_last) {
            printf("  [FAIL] Sample %d: TLAST expected %d, got %d\n",
                   i, expected_last, (int)pkt.last);
            error_count++;
            tlast_ok = false;
        }
    }

    if (tlast_ok) {
        printf("  [PASS] TLAST correctly propagated on all %d samples\n", BURST_LEN);
    }
}

// ============================================================
// Main
// ============================================================
int main() {
    printf("========================================\n");
    printf("  Testbench: CORDIC\n");
    printf("========================================\n");

    test_rotation_known_angles();
    test_vectoring_known_vectors();
    test_rotation_sweep();
    test_tlast_propagation();

    printf("\n========================================\n");
    if (error_count == 0) {
        printf("  ALL TESTS PASSED\n");
    } else {
        printf("  %d TEST(S) FAILED\n", error_count);
    }
    printf("========================================\n");

    return error_count;
}
