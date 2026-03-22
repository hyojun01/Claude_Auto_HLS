# Synthesis Summary: cordic

## Target Configuration
- **FPGA Part**: xc7z020clg400-1
- **Clock Period**: 10 ns
- **HLS Tool Version**: Vitis HLS 2025.1

## Timing
| Metric | Value |
|--------|-------|
| Target clock | 10.0 ns |
| Estimated clock | 7.09 ns |
| Uncertainty | 2.7 ns |
| Timing margin | 2.91 ns |

## Performance
| Metric | Value |
|--------|-------|
| Latency (cycles) | 8 |
| Latency (absolute) | 80 ns |
| Initiation Interval | 1 cycle |
| Pipeline type | function pipeline |

## Resource Utilization
| Resource | Used | Available | Utilization (%) |
|----------|------|-----------|--------------------|
| BRAM_18K | 0 | 280 | 0.0 |
| DSP | 0 | 220 | 0.0 |
| FF | 1090 | 106400 | 1.0 |
| LUT | 4042 | 53200 | 7.6 |
| URAM | 0 | 0 | 0.0 |

## Assessment
- **Timing**: PASS — 2.91 ns slack, estimated Fmax 141 MHz exceeds 100 MHz target
- **C-Simulation**: PASS — all 4 test scenarios pass within ±2^(-12) tolerance
- **Resource fit**: PASS — minimal resource usage (7.6% LUT, 0% DSP/BRAM)
- **Meets instruction targets**: YES — II=1 achieved, 0 DSP (multiplier-free), timing met, latency 8 cycles (better than 16-cycle target)
