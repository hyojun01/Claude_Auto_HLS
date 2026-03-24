# Changelog: depthwise_conv

## [1.0.0] - 2026-03-24

### Added
- Initial implementation of 3x3 depthwise convolution IP
- INT8 data path with 32-bit accumulator
- Line buffer architecture (2 x 128 x 8-bit in BRAM)
- "Same" zero-padding with virtual boundary checks
- Optional ReLU activation (configurable per invocation)
- Shift-based requantization with INT8 saturation clamping
- AXI-Stream input/output (8-bit, TLAST on final pixel)
- AXI-Lite control bundle (weights, bias, height, width, out_shift, relu_en)
- Comprehensive testbench with 7 test cases (4,436 individual checks)

### Synthesis Metrics (xc7z020-clg400-1, 100 MHz)
- Timing: 7.179 ns estimated (2.82 ns slack)
- Pixel loop II: 1 (target met)
- Pipeline depth: 10 cycles
- BRAM_18K: 2 (0.7%)
- DSP: 7 (3.2%)
- FF: 1,428 (1.3%)
- LUT: 1,836 (3.5%)
