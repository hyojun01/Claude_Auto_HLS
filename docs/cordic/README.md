# CORDIC — IP Documentation

## 1. Overview

A CORDIC (COordinate Rotation DIgital Computer) engine that computes trigonometric and vector operations using iterative shift-and-add arithmetic, requiring zero multipliers (0 DSP). The IP supports two modes selectable per-sample via a sideband signal:

- **Rotation mode**: Rotates an input vector by a given angle, producing sin/cos when initialized with gain-compensated unit vector.
- **Vectoring mode**: Computes the magnitude and phase angle of an input vector.

## 2. Specifications

| Parameter         | Value                |
|-------------------|----------------------|
| HLS Tool Version  | Vitis HLS 2025.1     |
| Target FPGA Part  | xc7z020clg400-1      |
| Clock Period       | 10 ns (100 MHz)      |
| Top Function       | cordic               |
| Language           | C++ (C++14)          |
| CORDIC Iterations  | 14                   |

## 3. I/O Ports

| Port Name   | Direction | Data Type             | Interface Protocol | Description |
|-------------|-----------|----------------------|-------------------|-------------|
| in_stream   | IN        | ap_axiu<48,1,0,0>    | AXI-Stream (axis) | Input: x[15:0], y[31:16], phase[47:32] in TDATA; mode in TUSER; TLAST |
| out_stream  | OUT       | ap_axiu<48,0,0,0>    | AXI-Stream (axis) | Output: x[15:0], y[31:16], phase[47:32] in TDATA; TLAST |
| (control)   | —         | —                     | ap_ctrl_none      | Free-running, no control handshake |

### TDATA Field Packing

| Bit Range | Field     | Type              | Description |
|-----------|-----------|-------------------|-------------|
| [15:0]    | x         | ap_fixed<16,2>    | X component |
| [31:16]   | y         | ap_fixed<16,2>    | Y component |
| [47:32]   | phase     | ap_fixed<16,2>    | Phase angle |

### Input Interpretation by Mode

| Mode (TUSER) | x_in          | y_in      | phase_in       |
|--------------|---------------|-----------|----------------|
| 0 (Rotation) | Initial X (set to 1/K for sin/cos) | Initial Y (set to 0) | Target angle |
| 1 (Vectoring)| I component   | Q component | Initial angle (typically 0) |

### Output Interpretation by Mode

| Mode | x_out                  | y_out              | phase_out         |
|------|------------------------|--------------------|-------------------|
| 0    | cos(theta) (with 1/K pre-comp) | sin(theta)    | Residual (~0)     |
| 1    | K * magnitude          | Residual (~0)      | atan2(y, x)       |

## 4. Functional Description

### CORDIC Algorithm

The CORDIC algorithm iteratively rotates a 2D vector using only additions and bit-shifts. For each of 14 iterations:

```
if rotation_mode:
    sigma_i = sign(z_i)           // drive z toward 0
else:
    sigma_i = -sign(y_i)          // drive y toward 0

x_{i+1} = x_i - sigma_i * y_i * 2^(-i)
y_{i+1} = y_i + sigma_i * x_i * 2^(-i)
z_{i+1} = z_i - sigma_i * atan(2^(-i))
```

### CORDIC Gain

The iterative rotation introduces a gain factor K = product(sqrt(1 + 2^(-2i))) for i=0..13 ≈ 1.64676. The hardware does **not** compensate for this gain (to remain multiplier-free):

- **Rotation mode**: Caller pre-loads `x_in = 1/K ≈ 0.60725`, `y_in = 0` to get direct sin/cos output.
- **Vectoring mode**: Output `x_out = K * sqrt(x^2 + y^2)`. Divide by K externally if true magnitude is needed.

### Convergence Domain

- **Rotation mode**: Input angles must be within approximately [-1.74, 1.74] radians (the sum of all atan(2^(-i)) values).
- **Vectoring mode**: `x_in` should be non-negative. Vectors with `K * magnitude >= 2.0` will overflow the internal accumulator (ap_fixed<32,2> range [-2, 2)).

## 5. Architecture

### 5.1 Block Diagram (Textual)

```
AXI-Stream IN ──► Unpack ──► CORDIC Pipeline (14 unrolled iterations) ──► Pack ──► AXI-Stream OUT
                  (TDATA→    (shift-and-add, fully combinational          (data_t→
                   x,y,z,     stages registered by HLS pipeline)           TDATA,
                   mode,last)                                              TLAST)
```

### 5.2 Data Flow

1. Read one `axis_in_t` packet from `in_stream`
2. Unpack 48-bit TDATA into three `data_t` fields (x, y, phase); extract mode from TUSER
3. Widen to `acc_t` (ap_fixed<32,2>) for internal precision
4. Execute 14 fully-unrolled CORDIC iterations (shift-and-add only)
5. Truncate results back to `data_t` (ap_fixed<16,2>)
6. Pack into 48-bit TDATA, propagate TLAST, write to `out_stream`

The function-level `#pragma HLS PIPELINE II=1` causes HLS to register intermediate results across 8 pipeline stages, achieving one new result per clock cycle after the initial 8-cycle latency.

## 6. Performance

### 6.1 Latency & Throughput

| Metric              | Value         |
|---------------------|---------------|
| Latency (cycles)     | 8             |
| Latency (time)       | 80 ns         |
| Initiation Interval  | 1 cycle       |
| Throughput            | 100 MSamples/s |
| Pipeline Type        | Function pipeline |

### 6.2 Resource Utilization

| Resource | Used  | Available | Utilization (%) |
|----------|-------|-----------|-----------------|
| BRAM     | 0     | 280       | 0.0             |
| DSP      | 0     | 220       | 0.0             |
| FF       | 1090  | 106400    | 1.0             |
| LUT      | 4042  | 53200     | 7.6             |

### 6.3 Timing

| Metric          | Value     |
|-----------------|-----------|
| Target clock    | 10.0 ns   |
| Estimated clock | 7.09 ns   |
| Slack           | 2.91 ns   |
| Fmax            | 141 MHz   |

## 7. Interface Details

### 7.1 Control Interface

`ap_ctrl_none` — the IP is free-running with no start/done/idle handshake. It continuously reads from `in_stream` when data is available and writes to `out_stream`.

### 7.2 Data Interfaces

**in_stream (AXI-Stream Slave)**:
- TDATA: 48-bit, packed as [x | y | phase] (3 x 16-bit ap_fixed<16,2>)
- TUSER: 1-bit mode select (0 = rotation, 1 = vectoring)
- TKEEP/TSTRB: 6-bit (48/8 bytes)
- TLAST: 1-bit end-of-frame marker
- Handshake: TVALID/TREADY

**out_stream (AXI-Stream Master)**:
- TDATA: 48-bit, packed as [x_out | y_out | phase_out]
- TKEEP/TSTRB: 6-bit
- TLAST: 1-bit, propagated from input with correct pipeline latency alignment
- Handshake: TVALID/TREADY

## 8. Usage Example

```cpp
#include "cordic.hpp"
#include <cmath>

// Compute sin(0.5) and cos(0.5) using rotation mode
hls::stream<axis_in_t> in_stream;
hls::stream<axis_out_t> out_stream;

const double INV_K = 0.60725293500888;

axis_in_t pkt;
data_t x_d = INV_K;  // Pre-compensate gain
data_t y_d = 0.0;
data_t phase_d = 0.5; // angle in radians

ap_uint<48> data;
data.range(15, 0) = x_d.range(15, 0);
data.range(31, 16) = y_d.range(15, 0);
data.range(47, 32) = phase_d.range(15, 0);

pkt.data = data;
pkt.user = 0;     // rotation mode
pkt.last = 1;
pkt.keep = -1;
pkt.strb = -1;
in_stream.write(pkt);

cordic(in_stream, out_stream);

axis_out_t result = out_stream.read();
// result.data[15:0]  ≈ cos(0.5) ≈ 0.8776
// result.data[31:16] ≈ sin(0.5) ≈ 0.4794
```

## 9. Design Constraints & Limitations

1. **Convergence range**: Rotation mode angles limited to approximately [-1.74, 1.74] radians. No quadrant pre-rotation is implemented.
2. **Magnitude overflow**: In vectoring mode, K * sqrt(x^2 + y^2) must be < 2.0 (acc_t range). This limits input vector magnitude to < 2.0/K ≈ 1.214.
3. **Phase type range**: ap_fixed<16,2> can represent [-2, 2) radians, which does not cover the full [-pi, pi] range. Angles outside this range wrap.
4. **Gain compensation**: Not performed in hardware. Caller must pre-scale inputs (rotation) or post-scale outputs (vectoring).
5. **Negative x in vectoring mode**: x_in < 0 may not converge. Pre-rotate the vector externally for full 4-quadrant atan2.
6. **Precision**: ~14-bit accuracy (one bit per iteration). Measured max error ≈ 0.00021 across [-1.5, 1.5] radians.

## 10. Optimization History

| Version | Date       | Optimization Applied | Latency | II | LUT  | DSP | Notes |
|---------|------------|---------------------|---------|-----|------|-----|-------|
| v1.0    | 2026-03-23 | Baseline (PIPELINE II=1, UNROLL) | 8 | 1 | 4042 | 0 | Initial design, 0 DSP |
