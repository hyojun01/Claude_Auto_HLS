---
name: algorithm-analysis
description: Algorithm analysis methodology for HLS implementation — computational structure, numerical precision, memory access patterns, variant selection, and resource estimation
user-invocable: false
---

# Skill: Algorithm Analysis for HLS Implementation

This skill provides the engineering methodology for analyzing algorithms before writing an IP specification. The goal is to make informed decisions about implementation architecture, data types, and performance targets — not to guess.

## 1. Computational Structure Analysis

### Step 1: Identify Core Operations

Decompose the algorithm into its fundamental operations and classify each:

| Operation Class | Examples | FPGA Resource | Key Constraint |
|----------------|---------|---------------|----------------|
| Multiply-accumulate (MAC) | FIR, convolution, matrix multiply | DSP48 | Operand width ≤ DSP native (25×18 on 7-series) |
| Butterfly (add/sub pair) | FFT, DCT | LUT/FF | Parallelism = N/2 per stage |
| Coordinate rotation | CORDIC, QR decomposition | LUT/FF + shift | Iterations = bit precision |
| Table lookup | NCO sine, log/exp, activation | BRAM or LUTRAM | Table size = 2^(address bits) × data width |
| Comparison/selection | Sorting, median, CFAR, max pooling | LUT | Comparator tree depth = log2(N) |
| Accumulation/reduction | Histogram, sum, norm | LUT/FF | Loop-carried dependency on accumulator |
| Bit manipulation | CRC, scrambler, barrel shift | LUT | Purely combinational, near-zero cost |

### Step 2: Map the Computational Graph

For each algorithm, draw the data dependency structure:

```
Questions to answer:
1. What is the critical path? (longest chain of dependent operations)
2. Which operations are independent and can execute in parallel?
3. Are there loop-carried dependencies? (iteration N depends on N-1)
4. What is the reuse pattern? (same data read multiple times)
```

**Classification by graph structure:**

- **Feed-forward pipeline**: No feedback, each sample is independent (FIR, image filter)
  → Natural fit for PIPELINE II=1, high throughput
- **Iterative convergence**: Output feeds back as input for N iterations (CORDIC, Newton-Raphson)
  → Latency = N iterations × 1 cycle, throughput limited by iteration count
- **Block-based transform**: Collect N samples, transform as a group (FFT, DCT)
  → Requires N-sample buffer, latency ≥ N cycles, DATAFLOW between stages
- **Recursive/stateful**: Current output depends on previous outputs (IIR, CIC integrator)
  → Loop-carried dependency limits II; may need code restructuring to break dependency

### Step 3: Parallelism Analysis

| Parallelism Type | Description | HLS Mechanism | Resource Cost |
|-----------------|-------------|---------------|---------------|
| Data-level | Same operation on independent data elements | UNROLL | Linear in unroll factor |
| Pipeline | Overlap successive iterations | PIPELINE | Registers for each stage |
| Task-level | Independent functions overlap | DATAFLOW | FIFOs/PIPOs between stages |
| Bit-level | Narrow custom widths reduce logic | ap_uint/ap_fixed sizing | Sublinear savings |

**Decision rule for parallelism strategy:**
```
If throughput target = 1 sample/cycle:
  → PIPELINE II=1 on the innermost sample loop
  → UNROLL inner computation loops (MAC, butterfly) to fit within 1 cycle

If throughput target = N samples/cycle:
  → UNROLL the sample loop by factor N
  → Each parallel path needs its own memory port → ARRAY_PARTITION

If latency is the primary goal:
  → DATAFLOW between stages to overlap
  → UNROLL inner loops to minimize per-stage latency
```

## 2. Numerical Precision Analysis

### Fixed-Point Bit Width Determination

**Step 1: Start from the application's SNR requirement.**
```
Required SNR (dB) → minimum fractional bits:
  fractional_bits ≥ (SNR_dB - 1.76) / 6.02

Examples:
  60 dB SNR → ≥ 10 fractional bits (typ. 12 for margin)
  80 dB SNR → ≥ 13 fractional bits (typ. 16 for margin)
  96 dB SNR → ≥ 16 fractional bits (typ. 18-20 for margin)
  Audio CD quality (96 dB) → ap_fixed<16,1> or ap_fixed<24,1>
  Radar (60 dB) → ap_fixed<12,1> or ap_fixed<14,1>
```

**Step 2: Size the integer bits to prevent overflow.**
```
For accumulation of N terms:
  integer_bits ≥ input_integer_bits + ceil(log2(N))

For multiplication of two values:
  result_integer_bits = A_integer_bits + B_integer_bits
  result_total_bits = A_total_bits + B_total_bits

Examples:
  16-tap FIR, input Q1.15, coeff Q1.15:
    MAC result: 1+1 + ceil(log2(16)) = 6 integer bits
    Accumulator: ap_fixed<32, 6> minimum (16+16 total, 6 integer)

  1024-point FFT, input Q1.15:
    Growth per stage: 1 bit (butterfly add)
    10 stages: 10 additional integer bits
    Accumulator: ap_fixed<26, 11> minimum
```

**Step 3: Verify against DSP48 native width.**
```
Target: keep multiply operands within DSP48 limits:
  7-series:    A ≤ 25 bits, B ≤ 18 bits (signed)
  UltraScale:  A ≤ 27 bits, B ≤ 18 bits (signed)
  Versal:      A ≤ 27 bits, B ≤ 24 bits (signed)

If operand exceeds limit:
  Option 1: Narrow the type (lose precision at LSBs)
  Option 2: Accept multi-DSP chaining (2-4× DSP cost per multiply)
  Option 3: Use fabric multiply (only for narrow operands ≤ 20 bits)
```

### Boundary Value Hazards

**Always check these conditions for ap_fixed types:**

| Condition | Risk | Mitigation |
|-----------|------|------------|
| Value = +1.0 in Q1.N format | Wraps to -1.0 (AP_WRAP) | Clamp to (1.0 - 2^-N) or use AP_SAT |
| Value = -1.0 in Q1.N format | Representable, but verify | No issue for standard Q1.N |
| sin(π/2), cos(0) = 1.0 | LUT initialization overflow | Clamp LUT entries at max representable |
| Accumulator at full scale | Overflow after N additions | Size integer bits per Step 2 |
| Multiplication of two max-magnitude values | Product overflow | Verify result type width ≥ A+B bits |

## 3. Memory Access Pattern Analysis

### Classification

| Pattern | Description | FPGA Mapping | Bandwidth |
|---------|-------------|-------------|-----------|
| Sequential streaming | Read/write each element once, in order | AXI-Stream or pipelined loop | 1 element/cycle at II=1 |
| Sliding window | Reuse N consecutive elements per output | Shift register (ARRAY_PARTITION complete) | N reads → 1 shift + 1 new read |
| Random access | Index computed at runtime | BRAM (dual-port) | 2 reads/cycle max per BRAM |
| Block transpose | Read row-major, write column-major | Ping-pong buffer in BRAM | Double-buffer to overlap |
| Lookup table | Address from data, read-only | BRAM (≥1024 entries) or LUTRAM (<1024) | 1-2 reads/cycle per port |
| Scatter/gather | Indices from external list | AXI-MM with index buffer | Limited by DDR bandwidth |

### Buffer Sizing Rules

```
Sliding window (FIR, image convolution):
  Buffer size = window_width (1D) or window_height × image_width (2D line buffer)
  Storage: shift register (complete partition) if ≤ 64 elements; BRAM otherwise

Block transform (FFT, DCT):
  Buffer size = transform_length × data_width
  Storage: BRAM if > 512 bits total; LUTRAM if ≤ 512 bits
  Double-buffer (ping-pong) if streaming continuous blocks

Lookup table:
  Entries × data_width < 18 Kbits → 1 BRAM_18K
  Entries × data_width < 36 Kbits → 1 BRAM_36K
  Quarter-wave optimization: reduce by 4× for symmetric functions (sin/cos)
```

## 4. Algorithm Variant Selection

When multiple implementations exist, choose based on FPGA suitability:

### DSP-Domain Examples

| Algorithm | Variant A | Variant B | FPGA Preference |
|-----------|-----------|-----------|-----------------|
| FIR filter | Direct form | Transposed form | **Transposed** if pipelining is critical (shorter critical path per MAC) |
| FIR filter (symmetric) | Full taps | Exploit symmetry (pre-add) | **Symmetric** — halves DSP count |
| FFT | Radix-2 DIT | Radix-4 DIT | **Radix-2** simpler; Radix-4 fewer stages but wider butterflies |
| FFT | In-place (single buffer) | Pipelined (stage buffers) | **Pipelined** for streaming throughput; **In-place** for minimum memory |
| CORDIC | Iterative (1 PE, N cycles) | Unrolled (N PEs, 1 cycle) | **Iterative** for area; **Unrolled** for throughput |
| IIR filter | Direct Form I | Direct Form II / Transposed | **Direct Form II Transposed** — better numerical stability, shorter critical path |
| Convolution | Spatial domain | FFT-based (overlap-save) | **Spatial** for small kernels (≤7×7); **FFT-based** for large kernels |

### Communications Examples

| Algorithm | Variant A | Variant B | FPGA Preference |
|-----------|-----------|-----------|-----------------|
| Viterbi decoder | Full traceback | Register exchange | **Register exchange** — no BRAM, parallel update |
| CRC | Bit-serial (1 bit/cycle) | Table-based (8/32 bits/cycle) | **Table-based** for throughput; **Bit-serial** for area |
| Scrambler | LFSR (serial) | Matrix multiply (parallel) | **LFSR** for 1 bit/cycle; **Matrix** for N bits/cycle |

### Selection Criteria Checklist
```
For each algorithm variant, evaluate:
1. Operations per sample → DSP / LUT cost
2. Memory per sample → BRAM cost
3. Achievable II → throughput
4. Pipeline depth → latency
5. Control complexity → implementation risk
6. Numerical stability → fixed-point suitability
→ Choose the variant that best fits the target's resource profile and performance goals
```

## 5. Complexity-to-Resource Estimation

### Quick Estimation Rules

```
DSP count estimation:
  = (multiplies_per_output × outputs_per_cycle) / DSP_sharing_factor
  DSP_sharing_factor = 1 at II=1; = II at II>1 (time-multiplexing)

BRAM_18K estimation:
  = ceil(total_storage_bits / 18432)
  Each BRAM_18K: 1024×18 or 2048×9 or 4096×4 (configurable)
  Dual-port: 2 independent read/write ports

LUT estimation (rough):
  Control logic: 100-500 LUT per FSM/controller
  Data path mux per bit: ~1 LUT per 4:1 mux
  Adder: ~1 LUT per bit
  Comparator: ~0.5 LUT per bit
  Fabric multiply: ~3 LUT per bit² (avoid for wide operands)

FF estimation:
  Pipeline register: 1 FF per bit per stage
  Shift register: 1 FF per bit per tap (if partitioned to registers)
```

### Device Capacity Reference (for budget checking)

| Device | LUT | FF | BRAM_18K | DSP |
|--------|-----|------|----------|-----|
| xc7z010 | 17,600 | 35,200 | 120 | 80 |
| xc7z020 | 53,200 | 106,400 | 280 | 220 |
| xc7z045 | 218,600 | 437,200 | 1,090 | 900 |
| xczu3eg | 70,560 | 141,120 | 432 | 360 |
| xczu9eg | 274,080 | 548,160 | 1,824 | 2,520 |

**Guideline**: Target ≤ 60% utilization for each resource to leave room for routing and integration overhead. Flag if any resource estimate exceeds 50% of the target device.
