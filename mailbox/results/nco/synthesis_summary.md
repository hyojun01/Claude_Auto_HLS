# Synthesis Summary: nco

## Target Configuration
- **FPGA Part**: xc7z020clg400-1
- **Clock Period**: 10 ns
- **HLS Tool Version**: Vitis HLS 2025.2

## Timing
| Metric | Value |
|--------|-------|
| Target clock | 10.00 ns |
| Estimated clock | 7.293 ns |
| Uncertainty | 2.70 ns |
| Timing margin | 2.707 ns |

## Performance
| Metric | Value |
|--------|-------|
| Latency (cycles) | 7 – 4102 |
| Latency (absolute) | 70 ns – 41.020 us |
| Initiation Interval | 1 cycle (achieved) |
| Pipeline type | loop pipeline (GEN_LOOP) |

## Resource Utilization
| Resource | Used | Available | Utilization (%) |
|----------|------|-----------|--------------------|
| BRAM_18K | 1 | 280 | 0.4 |
| DSP | 3 | 220 | 1.4 |
| FF | 712 | 106400 | 0.7 |
| LUT | 944 | 53200 | 1.8 |
| URAM | 0 | 0 | 0.0 |

## Assessment
- **Timing**: PASS — 2.707 ns margin, Fmax 137.11 MHz
- **C-Simulation**: PASS — all 8 tests passed
- **Resource fit**: PASS — well within xc7z020 capacity
- **Meets instruction targets**: PARTIAL — II=1 achieved, 1 BRAM as expected; DSP is 3 (vs 0 target) due to top-level control path multiplier, pipeline core uses 0 DSP; LUT/FF higher than instruction estimates due to AXI-Lite interface overhead
