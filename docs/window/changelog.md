# Changelog: 256-Point Hamming Window

## [1.0.0] — 2026-03-22

### Added
- Initial implementation of 256-point Hamming window IP
- AXI-Stream input/output with packed complex samples (32-bit: real[31:16] + imag[15:0])
- DATAFLOW architecture with 3 pipelined stages (read, apply, write)
- Precomputed Hamming coefficients stored in ROM (ap_ufixed<16,0>)
- Free-running mode (ap_ctrl_none) for real-time streaming
- Testbench with 5 test cases: sine wave, zeros, DC, symmetry, back-to-back packets

### Synthesis Results (xc7z020clg400-1 @ 100 MHz)
- Timing: 5.58 ns estimated (4.42 ns slack)
- Latency: 264 cycles (2.640 μs)
- Throughput: 1 sample/cycle (256-cycle initiation interval)
- BRAM: 5 / 280 (1.8%)
- DSP: 2 / 220 (0.9%)
- FF: 522 / 106,400 (0.5%)
- LUT: 509 / 53,200 (1.0%)
