# Feedback: depthwise_conv

## Results Analysis
- **Design quality**: Excellent. Clean, faithful implementation of the instruction spec. Line buffer architecture with `ARRAY_PARTITION dim=1`, local weight copies, fully unrolled 3x3 MAC, virtual zero-padding via coordinate masking, optional ReLU, and shift-based requantization with INT8 clamping. The (H+1)x(W+1) loop strategy with offset output coordinates is an elegant approach to border handling that avoids special-casing the "not enough rows buffered" condition. `DEPENDENCE inter false` pragma on line_buf correctly resolves the read-before-write dependency for II=1.
- **Verification quality**: Comprehensive. 7 test cases covering identity kernel, uniform kernel (border behavior), checkerboard with ReLU, positive and negative saturation, 64x64 stress test with Sobel-like kernel, 4x4 exact border verification, and bias+shift combination. 4,436 individual pixel checks with zero errors. TLAST assertion and extra-data checks on every test case.
- **Synthesis efficiency**: Good. All resources under 5% utilization on xc7z020. DSP usage (7) is 2 below the estimated 9 — HLS mapped some 8-bit multiplies to LUT fabric, which is appropriate for narrow operands. BRAM usage (2) is 1 above estimate due to per-row BRAM allocation from `dim=1` partitioning. LUT and FF actuals are 28–36% below estimates, indicating the 2.0x HLS infrastructure overhead factor was conservative for this design.

## Metrics vs Targets
| Metric | Target (from instruction) | Achieved | Status |
|--------|--------------------------|----------|--------|
| II | 1 cycle | 1 cycle | MET |
| Timing | 10 ns (100 MHz) | 7.179 ns (139 MHz) | MET (2.82 ns slack) |
| Throughput | 100 Mpixels/s | 100 Mpixels/s (1 pixel/cycle) | MET |
| DSP | ~9 | 7 | MET (under estimate) |
| BRAM_18K | ~1 | 2 | MET (trivial, <1% device) |
| LUT | ~2,540 | 1,836 (3.5%) | MET (under estimate) |
| FF | ~2,220 | 1,428 (1.3%) | MET (under estimate) |
| Functional | All 6 instruction tests | 7 tests, 4,436 checks, 0 errors | MET |

## Optimization Recommendation
- **Needed**: No
- **Priority**: N/A
- **Strategy**: All instruction targets met with comfortable margins. The design is already efficient for a single-channel 3x3 depthwise convolution on a Zynq-7020. No optimization round is warranted.
- **Expected improvement**: N/A

## Lessons Learned
- The 2.0x HLS infrastructure overhead factor for LUT/FF was conservative for this INT8 streaming design. Actual overhead was closer to 1.45x for LUT and 1.3x for FF. This may reflect the simplicity of the data path (no ap_fixed, no complex FSMs). For future INT8 streaming IPs on Zynq-7000, a 1.5x overhead factor may be more accurate.
- HLS mapped 2 of the expected 9 DSPs to LUT fabric. For 8x8 signed multiplies, the tool prefers LUT implementation when timing allows, since an 8x8 multiply fits comfortably in fabric at 100 MHz. This is a useful calibration point: INT8 designs should estimate DSP usage assuming some multiplies will be absorbed by LUTs.
- The BRAM estimate of 1 was based on total bits (2 x 128 x 8 = 2048 bits), but with `ARRAY_PARTITION dim=1` each row maps to a separate BRAM_18K. Future estimates for partitioned arrays should count BRAM per partition, not total bits.

## Environment Upgrade Triggers
- **Trigger detected**: No
- **Description**: The design session was clean — no unexpected failures, no improvisation needed, no skill gaps exposed. The minor discrepancies in resource estimation (BRAM count, DSP-to-LUT mapping, overhead factor) are inherent to the estimation process and do not indicate actionable environment gaps.
