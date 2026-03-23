# IP Instruction: NCO (Numerically Controlled Oscillator)

## 1. Functional Description
A Numerically Controlled Oscillator that generates continuous sin/cos (I/Q) waveforms at a software-programmable frequency. The core architecture is:

1. **Phase accumulator**: A 32-bit unsigned accumulator that increments by a programmable phase step each clock cycle. The output frequency is: `f_out = f_clk * phase_inc / 2^32`
2. **Quarter-wave LUT**: A 1024-entry lookup table storing one quarter-wave of sine in BRAM. The full sin/cos waveform is reconstructed using quadrant symmetry, reducing memory by 4x.
3. **I/Q output**: Both cosine (I) and sine (Q) are output simultaneously as a packed AXI-Stream word, one sample per clock.

The NCO supports:
- **Runtime frequency control** via AXI-Lite register (phase increment)
- **Phase offset** for multi-channel coherent operation or beam steering
- **Synchronous reset** to restart the phase accumulator to zero (phase-coherent restart)

## 2. I/O Ports

| Port Name | Direction | Data Type | Bit Width | Interface Protocol | Description |
|-----------|-----------|-----------|-----------|-------------------|-------------|
| out_stream | OUT | ap_axiu<32,0,0,0> | 32 | AXI-Stream (axis) | Output: cos[31:16] \| sin[15:0], TLAST every N samples |
| phase_inc | IN | ap_uint<32> | 32 | AXI-Lite (s_axilite) | Phase increment per clock (controls output frequency) |
| phase_offset | IN | ap_uint<32> | 32 | AXI-Lite (s_axilite) | Static phase offset added to accumulator output |
| num_samples | IN | ap_uint<32> | 32 | AXI-Lite (s_axilite) | Number of samples to generate per invocation (TLAST on last) |
| sync_reset | IN | ap_uint<1> | 1 | AXI-Lite (s_axilite) | When 1, reset phase accumulator to 0 on next call |

### AXI-Lite Register Map
| Offset | Name | R/W | Description |
|--------|------|-----|-------------|
| 0x10 | phase_inc | W | Phase increment (frequency word) |
| 0x18 | phase_offset | W | Phase offset |
| 0x20 | num_samples | W | Samples per burst |
| 0x28 | sync_reset | W | Reset phase accumulator |

### Output Stream Packing
```
TDATA[31:16] = cos_out (ap_fixed<16,1>, Q1.15)
TDATA[15:0]  = sin_out (ap_fixed<16,1>, Q1.15)
TLAST = 1 on the final sample of each burst (sample index == num_samples - 1)
```

## 3. Data Types
```cpp
// Phase accumulator: 32-bit unsigned, wraps naturally at 2^32
typedef ap_uint<32> phase_t;

// LUT output / final output: Q1.15 fixed-point [-1, +1)
typedef ap_fixed<16, 1> sample_t;

// LUT address: top 10 bits of phase (after quadrant extraction)
// Quadrant bits: phase[31:30]
// LUT index:     phase[29:20]
typedef ap_uint<10> lut_addr_t;

// Quarter-wave LUT depth
const int LUT_DEPTH = 1024;
```

## 4. Algorithm / Processing

### Phase Accumulation
```
// Persistent across invocations (static)
phase_acc += phase_inc;   // 32-bit unsigned wraparound = full 2*pi cycle
effective_phase = phase_acc + phase_offset;
```

### Quarter-Wave Symmetry Decomposition
```
quadrant = effective_phase[31:30];      // 2 bits: 0,1,2,3
lut_index = effective_phase[29:20];     // 10-bit LUT address

// Address reflection for quadrants 1 and 3
if (quadrant[0] == 1):
    lut_addr = 1023 - lut_index    // mirror
else:
    lut_addr = lut_index

sin_lut_val = quarter_sine_lut[lut_addr];  // always positive from LUT

// Sign adjustment based on quadrant
//   Q0 (0 to pi/2):     sin = +lut, cos = +lut(complementary)
//   Q1 (pi/2 to pi):    sin = +lut, cos = -lut(complementary)
//   Q2 (pi to 3pi/2):   sin = -lut, cos = -lut(complementary)
//   Q3 (3pi/2 to 2pi):  sin = -lut, cos = +lut(complementary)
```

### Cosine via Phase Shift
Cosine is computed by reading the same LUT with a +pi/2 phase offset:
```
cos_phase = effective_phase + 0x40000000;  // add pi/2 (quarter cycle)
// Then apply same quadrant decomposition to cos_phase
```

### Quarter-Wave Sine LUT Contents
The LUT stores `sin(k * pi / (2 * 1024))` for k = 0..1023, quantized to `ap_fixed<16,1>`:
- `lut[0] = sin(0) = 0.0`
- `lut[512] = sin(pi/4) ~ 0.7071`
- `lut[1023] = sin(pi/2 - epsilon) ~ 0.9999695` (clamped below 1.0 to avoid ap_fixed overflow)

**Critical**: `sin(pi/2) = 1.0` is NOT representable in `ap_fixed<16,1>`. The maximum value `lut[1023]` must be clamped to `(1.0 - 2^-15) = 0.999969482421875`.

## 5. Target Configuration

| Parameter | Value |
|-----------|-------|
| FPGA Part | xc7z020clg400-1 |
| Clock Period | 10 ns (100 MHz) |
| Target Latency | 2 cycles (phase accumulate + LUT read) |
| Target II | 1 (one I/Q sample per clock) |
| Target Throughput | 100 MSamples/s (one I/Q pair per clock at 100 MHz) |
| Frequency Resolution | f_clk / 2^32 = ~0.023 Hz |
| Output SFDR | > 90 dB (expected with 10-bit LUT, 16-bit output) |

### Expected Resource Usage
| Resource | Estimate | Notes |
|----------|----------|-------|
| BRAM_18K | 1 | 1024 x 16-bit quarter-wave LUT fits in one 18K BRAM |
| DSP | 0 | No multipliers needed |
| LUT | < 200 | Quadrant logic, phase addition, muxing |
| FF | < 100 | Phase accumulator, pipeline registers |

## 6. Test Scenarios

### Test 1: DC output (f_out = 0 Hz)
- Set phase_inc = 0, phase_offset = 0, num_samples = 16
- Expected: cos = constant ~1.0 (max positive), sin = constant 0.0 for all samples
- Verifies LUT value at phase = 0

### Test 2: f_out = f_clk/4 (25 MHz at 100 MHz clock)
- Set phase_inc = 0x40000000 (2^30), num_samples = 8
- Expected: cos sequence = {1, 0, -1, 0, 1, 0, -1, 0} (approximately)
- Expected: sin sequence = {0, 1, 0, -1, 0, 1, 0, -1} (approximately)
- Verifies quadrant transitions at all 4 boundaries

### Test 3: f_out = f_clk/8 (12.5 MHz)
- Set phase_inc = 0x20000000 (2^29), num_samples = 16
- Expected: 2 full sine cycles in 16 samples
- Compare against math.h sin/cos, tolerance < 2^-13 (~0.000122)

### Test 4: Arbitrary frequency — full sweep verification
- Set phase_inc = 0x01000000 (~1.49 MHz), num_samples = 256
- Compare every sample against double-precision sin/cos reference
- Report max absolute error and RMS error
- Expected: max error < 2^-13, SFDR check via output spectrum

### Test 5: Phase offset
- Set phase_inc = 0x10000000, phase_offset = 0x40000000 (pi/2 offset)
- Verify that sin output matches cos output without offset (and vice versa)
- Tolerance: exact match (same LUT path)

### Test 6: Synchronous reset
- Generate 8 samples, then call again with sync_reset = 1
- Verify that the second burst starts from phase 0 (same as initial output)
- Verifies phase accumulator reset behavior

### Test 7: TLAST generation
- Set num_samples = 10
- Verify TLAST = 0 for samples 0..8, TLAST = 1 for sample 9
- Verify no extra samples after TLAST

### Test 8: Phase continuity across invocations
- Call with phase_inc = 0x10000000, num_samples = 4, sync_reset = 0
- Call again with same phase_inc, num_samples = 4, sync_reset = 0
- Verify second burst continues from where first burst ended (no phase discontinuity)

## 7. Additional Notes
- The quarter-wave sine LUT should be declared as `const sample_t` array — Vitis HLS will infer BRAM or distribute into LUTs based on size
- The phase accumulator must be `static` inside the top function to persist across invocations
- Phase increment takes effect immediately (no interpolation needed for continuous-phase FSK or chirp if phase_inc is updated between bursts)
- For cosine, reuse the same LUT by adding a pi/2 phase offset — do NOT store a separate cosine table
- The ap_fixed<16,1> output clamp at sin(pi/2) is critical — see hls-coding.md pitfall on ap_fixed boundary overflow
