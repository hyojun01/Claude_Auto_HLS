# Synthesis Summary: depthwise_conv

## Target Configuration
- **FPGA Part**: xc7z020-clg400-1
- **Clock Period**: 10 ns
- **HLS Tool Version**: Vitis HLS 2025.2

## Timing
| Metric | Value |
|--------|-------|
| Target clock | 10.00 ns |
| Estimated clock | 7.179 ns |
| Uncertainty | 2.70 ns |
| Timing margin | 2.821 ns |

## Performance
| Metric | Value |
|--------|-------|
| Latency (min cycles) | 152 |
| Latency (max cycles) | 65,687 |
| Latency (max absolute) | 0.657 ms |
| Initiation Interval | 1 cycle (on pixel processing loop) |
| Pipeline type | loop pipeline (depth=10) |
| Pixel throughput | 1 pixel/cycle (100 Mpixels/s at 100 MHz) |
| Estimated Fmax | 139.29 MHz |

## Resource Utilization
| Resource | Used | Available | Utilization (%) |
|----------|------|-----------|-----------------|
| BRAM_18K | 2 | 280 | 0.7 |
| DSP | 7 | 220 | 3.2 |
| FF | 1,428 | 106,400 | 1.3 |
| LUT | 1,836 | 53,200 | 3.5 |
| URAM | 0 | 0 | 0.0 |

## Assessment
- **Timing**: PASS -- 2.82 ns slack, estimated Fmax 139.29 MHz exceeds 100 MHz target
- **C-Simulation**: PASS -- 7 test cases, 4,436 checks, zero errors
- **Resource fit**: PASS -- all resources under 5% utilization on xc7z020
- **Meets instruction targets**: YES -- II=1 achieved, timing met, all functional tests pass, resource usage well within estimates
