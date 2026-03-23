# Changelog: NCO

## [v1.0] - 2026-03-23

### Added
- Initial NCO IP design with 32-bit phase accumulator and quarter-wave sine LUT
- AXI-Stream output interface (32-bit TDATA: cos[31:16] | sin[15:0])
- AXI-Lite control interface for phase_inc, phase_offset, num_samples, sync_reset
- 1024-entry quarter-wave sine LUT with Q1.15 output, boundary values clamped to 0.999969482421875
- Quadrant symmetry decomposition (address reflection + sign negation)
- Cosine output via +pi/2 phase offset sharing the same LUT
- TLAST assertion on final sample of each burst
- Static phase accumulator with sync_reset for phase-coherent operation
- Comprehensive testbench with 8 test scenarios (`src/nco/tb/tb_nco.cpp`)
- TCL scripts for C-simulation, C-synthesis, and co-simulation

### Performance (Vitis HLS 2025.2, xc7z020clg400-1, 100 MHz)
- Timing: 7.293 ns estimated (target 10 ns), Fmax 137.11 MHz
- Latency: 7 – 4102 cycles (min/max burst)
- Initiation Interval: 1 cycle (achieved)
- Throughput: 100 MSamples/s at 100 MHz
- BRAM: 1 (1024 x 15-bit quarter-wave LUT, auto-replicated for dual-port access)
- DSP: 3 total (0 in pipeline core, 3 in top-level control path)
- LUT: 944
- FF: 712

### Verification
- C-Simulation: **PASS** (all 8 tests passed)
- C-Synthesis: **PASS** (timing met, II=1 achieved, no violations)
- Output accuracy: max error 0.00156, RMS error 0.00077

### Known Limitations
- Output amplitude range [-0.999969, +0.999969] due to Q1.15 format (1.0 not representable)
- 10-bit phase quantization to LUT (no interpolation), max error ~ pi/(2*1024) ~ 0.00153
- Phase accumulator not readable via AXI-Lite
- Burst-mode operation only (no free-running mode)
- num_samples must be >= 1
