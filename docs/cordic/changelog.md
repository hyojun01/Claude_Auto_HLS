# Changelog: CORDIC

## [v1.0] - 2026-03-23

### Added
- Initial CORDIC IP design with rotation and vectoring modes
- AXI-Stream interface (48-bit TDATA, 1-bit TUSER for mode select)
- 14-iteration unrolled CORDIC pipeline
- TLAST propagation with correct latency alignment
- Comprehensive testbench with 4 test scenarios

### Performance
- Latency: 8 cycles (80 ns at 100 MHz)
- Initiation Interval: 1 cycle
- Throughput: 100 MSamples/s
- DSP: 0 (multiplier-free)
- LUT: 4042 (7.6%)
- FF: 1090 (1.0%)
- Timing: 7.09 ns estimated (2.91 ns slack)

### Verification
- C-Simulation: PASS (all 4 test scenarios)
- Rotation mode: max error 0.00021 rad (tolerance 0.000244)
- Vectoring mode: magnitude and phase within tolerance
- TLAST: correctly propagated across 10-sample burst

### Known Limitations
- Rotation mode convergence: [-1.74, 1.74] radians (no quadrant pre-rotation)
- Vectoring mode: x_in must be non-negative; K*magnitude must be < 2.0
- No hardware gain compensation (caller must handle K factor)
