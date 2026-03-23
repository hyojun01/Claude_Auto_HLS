# NCO — IP Documentation

## 1. Overview

A Numerically Controlled Oscillator (NCO) generating continuous sin/cos (I/Q) waveforms at a software-programmable frequency. The IP uses a 32-bit phase accumulator and a 1024-entry quarter-wave sine lookup table stored in BRAM. Both cosine (I) and sine (Q) channels are output simultaneously as a packed 32-bit AXI-Stream word, producing one sample per clock cycle.

Key features:
- Multiplier-free pipeline (0 DSP in NCO core) — all computation via LUT lookup and phase arithmetic
- Quarter-wave symmetry exploits a single 1024-entry BRAM to cover the full 360-degree sine/cosine cycle
- Software-programmable frequency via 32-bit phase increment (resolution: f_clk / 2^32 ~ 0.023 Hz at 100 MHz)
- Static phase offset for phase-coherent multi-channel or modulation applications
- Synchronous phase accumulator reset for deterministic restart

## 2. Specifications

| Parameter         | Value                |
|-------------------|----------------------|
| HLS Tool Version  | Vitis HLS 2025.2     |
| Target FPGA Part  | xc7z020clg400-1      |
| Clock Period       | 10 ns (100 MHz)      |
| Top Function       | nco                  |
| Language           | C++ (C++14)          |

## 3. I/O Ports

| Port Name    | Direction | Data Type            | Bit Width | Interface Protocol       | Description |
|--------------|-----------|----------------------|-----------|--------------------------|-------------|
| out_stream   | OUT       | ap_axiu<32,0,0,0>   | 32        | AXI-Stream (axis)        | Output: cos[31:16] \| sin[15:0], TLAST on last sample |
| phase_inc    | IN        | ap_uint<32>          | 32        | AXI-Lite (s_axilite)     | Phase increment per clock (frequency word) |
| phase_offset | IN        | ap_uint<32>          | 32        | AXI-Lite (s_axilite)     | Static phase offset added to accumulator |
| num_samples  | IN        | ap_uint<32>          | 32        | AXI-Lite (s_axilite)     | Number of samples per invocation |
| sync_reset   | IN        | ap_uint<1>           | 1         | AXI-Lite (s_axilite)     | Reset phase accumulator to 0 |
| (return)     | —         | —                    | —         | AXI-Lite (s_axilite)     | Block-level control (ap_start/ap_done/ap_idle/ap_ready) |

### Output Stream Packing

| Bit Range   | Field    | Type             | Description |
|-------------|----------|------------------|-------------|
| [31:16]     | cos_out  | ap_fixed<16,1>   | Cosine (I channel), Q1.15 format |
| [15:0]      | sin_out  | ap_fixed<16,1>   | Sine (Q channel), Q1.15 format |

TLAST is asserted on the final sample of each burst (when the sample counter equals `num_samples - 1`).

## 4. Functional Description

### Algorithm

1. **Phase Accumulator**: A 32-bit unsigned static variable persists across invocations. If `sync_reset = 1`, the accumulator is cleared to 0 before sample generation begins.

2. **Phase Computation**: For each output sample, the effective phase is computed as:
   ```
   effective_phase = phase_acc + phase_offset
   ```

3. **Quarter-Wave LUT Lookup**: A 1024-entry table stores `sin(k * pi / (2 * 1024))` for k = 0..1023, quantized to `ap_fixed<16,1>` (Q1.15). Values near 1.0 are clamped to 0.999969482421875 to prevent overflow in the Q1.15 representation.

4. **Quadrant Symmetry Decomposition**: The full sine wave is reconstructed from the quarter-wave LUT using two bits of the phase:
   - `quadrant = phase[31:30]`
   - `lut_index = phase[29:20]` (10-bit address)
   - Address reflection (mirror) in quadrants 1 and 3: `addr = 1023 - lut_index`
   - Sign negation in quadrants 2 and 3: `output = -lut_val`

5. **Cosine via Phase Offset**: Cosine is computed using the same LUT with a +pi/2 phase offset (`0x40000000`):
   ```
   sin_val = phase_to_sin(eff_phase)
   cos_val = phase_to_sin(eff_phase + 0x40000000)
   ```

6. **Output Packing**: Cosine and sine are packed into a 32-bit AXI-Stream TDATA word:
   ```
   TDATA[31:16] = cos_val
   TDATA[15:0]  = sin_val
   ```

7. **Phase Advance**: After each sample, the accumulator advances:
   ```
   phase_acc += phase_inc
   ```
   The 32-bit accumulator wraps naturally at 2^32, providing continuous phase.

### Frequency Relationship

The output frequency is determined by:
```
f_out = f_clk * phase_inc / 2^32
```

For a 100 MHz clock, the frequency resolution is 100e6 / 2^32 ~ 0.023 Hz.

Example phase_inc values:
| Desired f_out | phase_inc (hex) | phase_inc (decimal) |
|---------------|-----------------|---------------------|
| 1 MHz         | 0x028F5C29      | 42,949,673          |
| 10 MHz        | 0x19999999      | 429,496,730         |
| 25 MHz        | 0x40000000      | 1,073,741,824       |

## 5. Architecture

### 5.1 Block Diagram (Textual)

```
AXI-Lite Registers
    |
    +-- phase_inc ---+
    +-- phase_offset |
    +-- num_samples  |
    +-- sync_reset   |
                     v
             +----------------+
             | Phase Accum    | (32-bit, static, persists across calls)
             |  += phase_inc  |
             +-------+--------+
                     | effective_phase = phase_acc + phase_offset
                     +---------------+-----------------+
                     v               v                 |
              +-----------+   +-----------+            |
              | Sin Path  |   | Cos Path  |            |
              | quadrant  |   | (+pi/2    |            |
              | decompose |   |  offset)  |            |
              +-----+-----+   +-----+-----+            |
                    |               |                   |
                    v               v                   |
              +---------------------------+             |
              | Quarter-Wave Sine LUT     |             |
              | (1024 x 16-bit, BRAM)     |             |
              +-------+--------+----------+             |
                      |        |                        |
              +-------v--+ +---v--------+               |
              | Sin Sign | | Cos Sign   |               |
              | Adjust   | | Adjust     |               |
              +-------+--+ +---+--------+               |
                      |        |                        |
                      v        v                        |
              +-------------------+                     |
              | Pack & TLAST      |<--------------------+
              | cos[31:16]|sin    |   (sample counter)
              +---------+---------+
                        |
                        v
                AXI-Stream Output
```

### 5.2 Data Flow

1. Read control registers via AXI-Lite (phase_inc, phase_offset, num_samples, sync_reset)
2. Optionally clear phase accumulator (if sync_reset = 1)
3. For each of `num_samples` iterations (pipelined at II=1):
   a. Compute effective phase from accumulator + offset
   b. Perform two parallel quarter-wave LUT lookups (sin path and cos path)
   c. Apply quadrant sign correction
   d. Pack cos[31:16] | sin[15:0] into 32-bit TDATA
   e. Assert TLAST on final sample
   f. Write AXI-Stream packet
   g. Advance phase accumulator by phase_inc

### 5.3 Internal State

The phase accumulator (`phase_acc`) is declared `static` inside the top function. It persists across invocations, enabling phase-continuous waveform generation across successive bursts. It can be reset to 0 by asserting `sync_reset`.

## 6. Performance

### 6.1 Timing

| Metric          | Value     |
|-----------------|-----------|
| Target Clock    | 10.00 ns  |
| Estimated Clock | 7.293 ns  |
| Uncertainty     | 2.70 ns   |
| Timing Margin   | 2.707 ns  |
| Estimated Fmax  | 137.11 MHz|

### 6.2 Latency & Throughput

| Metric              | Value                                 |
|---------------------|---------------------------------------|
| Latency (min)       | 7 cycles (70 ns)                      |
| Latency (max)       | 4102 cycles (41.020 us)               |
| Initiation Interval | 1 cycle (GEN_LOOP pipeline, achieved) |
| Throughput          | 100 MSamples/s at 100 MHz             |
| Frequency Resolution| f_clk / 2^32 ~ 0.023 Hz              |

Pipeline loop detail: GEN_LOOP achieves II=1, iteration latency=3, trip count 1-4096.

### 6.3 Resource Utilization

| Resource | Used | Available | Utilization (%) |
|----------|------|-----------|-----------------|
| BRAM_18K | 1    | 280       | 0.4             |
| DSP      | 3    | 220       | 1.4             |
| FF       | 712  | 106400    | 0.7             |
| LUT      | 944  | 53200     | 1.8             |
| URAM     | 0    | 0         | —               |

**Resource breakdown by module:**

| Module                | BRAM | DSP | FF  | LUT |
|-----------------------|------|-----|-----|-----|
| nco_Pipeline_GEN_LOOP | 1    | 0   | 117 | 411 |
| control_s_axi         | 0    | 0   | 157 | 234 |
| mul_32s_32s_32 (ctrl) | 0    | 3   | 165 | 50  |
| Top-level logic       | 0    | 0   | 273 | 249 |

The NCO pipeline core uses **0 DSP**. The 3 DSPs come from a 32-bit multiplier in the top-level control path. The LUT ROM is stored as 1024 x 15-bit (tool optimized sign bit away) and auto-replicated to 2 copies for dual-port sin/cos access.

### 6.4 Output Accuracy

| Metric                | Value                |
|-----------------------|----------------------|
| Max absolute error    | 0.00156 (sin and cos)|
| RMS error             | 0.00077              |
| Error bound           | pi/(2*1024) ~ 0.00153|

Error is dominated by phase quantization (10-bit LUT index without interpolation). Amplitude quantization adds up to 2^-15 ~ 0.000031.

## 7. Interface Details

### 7.1 Control Interface

The block-level control uses `ap_ctrl_hs` via AXI-Lite (`#pragma HLS INTERFACE s_axilite port=return`). The standard Vitis HLS control signals are mapped to AXI-Lite register offset 0x00:

| Bit | Signal   | Description |
|-----|----------|-------------|
| 0   | ap_start | Write 1 to start one burst of num_samples |
| 1   | ap_done  | Asserted when burst completes |
| 2   | ap_idle  | Asserted when IP is idle |
| 3   | ap_ready | Asserted when IP can accept new parameters |

The IP must be started (`ap_start = 1`) for each burst. It does not free-run.

### 7.2 AXI-Lite Register Map

All scalar parameters and control are accessed through a single AXI-Lite slave interface:

| Offset | Name         | Width | R/W | Description |
|--------|--------------|-------|-----|-------------|
| 0x00   | CTRL         | 32    | R/W | ap_start / ap_done / ap_idle / ap_ready |
| 0x10   | phase_inc    | 32    | W   | Phase increment (frequency word) |
| 0x18   | phase_offset | 32    | W   | Phase offset |
| 0x20   | num_samples  | 32    | W   | Number of samples per burst |
| 0x28   | sync_reset   | 1     | W   | Reset phase accumulator to 0 |

> Register offsets are based on default Vitis HLS AXI-Lite packing. Confirm actual offsets from the generated driver header (`xnco_hw.h`) after IP export.

### 7.3 Data Interface

**out_stream (AXI-Stream Master)**:
- TDATA: 32-bit, packed as cos[31:16] | sin[15:0] (two Q1.15 ap_fixed<16,1> values)
- TKEEP: 4-bit (32/8 bytes), always all-ones
- TSTRB: 4-bit, always all-ones
- TLAST: 1-bit, asserted on the final sample of each burst
- Handshake: TVALID/TREADY

## 8. Usage Example

```cpp
#include "nco.hpp"

hls::stream<axis_pkt> out_stream;

// Generate 256 samples at approximately 1.49 MHz (100 MHz clock)
// phase_inc = round(1.49e6 / 100e6 * 2^32) = 0x01000000 = 16777216
nco(out_stream, 0x01000000, 0, 256, 1);  // sync_reset=1 for first call

// Read output samples
for (int i = 0; i < 256; i++) {
    axis_pkt pkt = out_stream.read();

    sample_t sin_val, cos_val;
    sin_val.range(15, 0) = pkt.data.range(15, 0);
    cos_val.range(15, 0) = pkt.data.range(31, 16);

    // sin_val and cos_val are now Q1.15 fixed-point values
    // Convert to double if needed: double s = (double)sin_val;
}

// Continue with phase-coherent second burst (sync_reset=0)
nco(out_stream, 0x01000000, 0, 256, 0);
```

## 9. Design Constraints & Limitations

1. **Amplitude range**: Output values span [-0.999969, +0.999969], not [-1.0, +1.0], due to Q1.15 format. The value 1.0 is not representable and the LUT clamps boundary values to 0.999969482421875.
2. **Phase quantization**: Only the upper 10 bits of the phase (bits [29:20]) index the LUT. The lower 20 bits of phase resolution are truncated (no interpolation), introducing phase quantization noise.
3. **Phase accumulator not readable**: The current accumulator value cannot be read back over AXI-Lite. Only write access is provided for the control parameters.
4. **Burst-mode operation**: The IP must be invoked (ap_start) for each burst of `num_samples`. It does not free-run continuously.
5. **num_samples >= 1**: The IP requires at least one sample per invocation. Behavior with `num_samples = 0` is undefined.
6. **Dual LUT access**: Both sin and cos paths read the same BRAM LUT each cycle. Vitis HLS auto-replicates the ROM to 2 copies, fitting both in a single BRAM_18K (1024 x 15-bit per copy).

## 10. Optimization History

| Version | Date       | Optimization Applied | Latency (cycles) | II | LUT | DSP | Notes |
|---------|------------|---------------------|------------------|----|-----|-----|-------|
| v1.0    | 2026-03-23 | Baseline (PIPELINE II=1) | 7 – 4102 | 1 | 944 | 3 | Initial design, quarter-wave LUT, 0 DSP in pipeline |

## 11. Source Files

| File | Path | Description |
|------|------|-------------|
| Header    | `src/nco/src/nco.hpp`       | Types, constants, top function prototype |
| Source    | `src/nco/src/nco.cpp`       | Top function, quarter-wave sine LUT, phase_to_sin helper |
| Testbench | `src/nco/tb/tb_nco.cpp`    | Comprehensive testbench with 8 test scenarios |
| C-Sim TCL | `src/nco/tcl/run_csim.tcl` | C-simulation script |
| Synth TCL | `src/nco/tcl/run_csynth.tcl`| C-synthesis script |
| Co-Sim TCL| `src/nco/tcl/run_cosim.tcl` | Co-simulation script |
