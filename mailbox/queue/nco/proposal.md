# IP Proposal: nco

## Domain
DSP

## Rationale
The NCO (Numerically Controlled Oscillator) is a fundamental DSP building block that generates digitally-controlled sinusoidal waveforms. It produces I/Q (cos/sin) output at a programmable frequency, making it essential for:
- **Digital up/down conversion** (DDC/DUC) — mixing baseband signals to/from IF
- **Carrier generation** for communications modulators
- **Reference signal generation** for radar pulse synthesis
- **Local oscillator replacement** in software-defined radio

The existing CORDIC IP provides an alternative phase-to-amplitude path, but a dedicated LUT-based NCO is preferred for continuous tone generation because it achieves lower latency (2 cycles vs 8), zero LUT-fabric cost for the conversion itself (uses BRAM instead), and simpler runtime control via an AXI-Lite register interface.

**Dependency note**: This NCO is a standalone IP. Downstream IPs that will benefit: Digital Downconverter (DDC), QAM Modulator, OFDM Modulator.

## Specification Summary
- **Algorithm**: Phase accumulator (32-bit) + quarter-wave symmetric LUT (1024 entries in BRAM) producing 16-bit sin/cos
- **I/O**: AXI-Stream output (32-bit TDATA: cos[16] | sin[16]), AXI-Lite control (phase_inc, phase_offset, sync_reset)
- **Data types**: Phase accumulator `ap_uint<32>`, output `ap_fixed<16,1>` per component
- **Target**: xc7z020clg400-1 @ 10 ns (100 MHz)
- **Complexity estimate**: Low

## Optimization Strategy (if applicable)
- **Phase 1**: Baseline functional design with quarter-wave LUT in BRAM, II=1 streaming output
- **Phase 2**: If needed, optimize SFDR via Taylor-series interpolation or dithering (not expected to be necessary for 16-bit output with 10-bit LUT address)

## Dependencies
- **Requires**: None
- **Required by**: Digital Downconverter (DDC), QAM Modulator, OFDM Modulator (future IPs)

## User Decision
<!-- User fills this section -->
- [ ] Approve as-is -> proceed to implementation
- [ ] Approve with modifications -> (describe modifications below)
- [ ] Reject -> (reason)
- [ ] Defer -> (revisit later)

### Modifications (if applicable)
<!-- User writes requested changes here -->
