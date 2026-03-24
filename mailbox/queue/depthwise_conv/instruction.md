# IP Instruction Template: Depthwise Convolution (depthwise_conv)

## 1. Functional Description

This IP performs a 3x3 depthwise convolution on a single input feature map
channel, producing one output channel. It implements the spatial filtering stage
of a depthwise separable convolution layer as used in MobileNet and similar
efficient DL architectures.

The IP processes one channel per invocation. The host iterates over all channels
by writing new 3x3 weights and bias via AXI-Lite before each invocation. Input
and output activations are streamed as 8-bit signed integers (INT8) over
AXI-Stream interfaces.

Key features:
- 3x3 kernel with stride=1
- "Same" zero-padding (output dimensions equal input dimensions)
- INT8 quantized data path (activations and weights)
- 32-bit accumulator with pre-scaled INT32 bias
- Shift-based requantization to INT8 output
- Optional fused ReLU activation (configurable per invocation)
- Streaming architecture with line buffer for single-pass processing

## 2. I/O Ports

| Port Name | Direction | Data Type | Bit Width | Interface Protocol | Description |
|-----------|-----------|-----------|-----------|-------------------|-------------|
| data_in | IN | hls::stream\<ap_axiu\<8,0,0,0\>\> | 8 | AXI-Stream (axis) | Input activation pixels, signed INT8 in TDATA[7:0], row-major order, H*W elements per invocation. TLAST=1 on the final pixel. |
| data_out | OUT | hls::stream\<ap_axiu\<8,0,0,0\>\> | 8 | AXI-Stream (axis) | Output activation pixels, signed INT8 in TDATA[7:0], row-major order, H*W elements per invocation. TLAST=1 on the final pixel. |
| weights | IN | ap_int\<8\> [9] | 72 (9x8) | AXI-Lite (s_axilite, bundle=ctrl) | 3x3 kernel weights in row-major order: weights[0]=top-left, weights[1]=top-center, ..., weights[8]=bottom-right. Signed INT8. |
| bias | IN | ap_int\<32\> | 32 | AXI-Lite (s_axilite, bundle=ctrl) | Pre-scaled bias value added to the accumulator after the 9 MACs. INT32, already scaled to the accumulator domain. |
| height | IN | ap_uint\<8\> | 8 | AXI-Lite (s_axilite, bundle=ctrl) | Input feature map height in pixels. Valid range: 3 to 128. |
| width | IN | ap_uint\<8\> | 8 | AXI-Lite (s_axilite, bundle=ctrl) | Input feature map width in pixels. Valid range: 3 to 128. |
| out_shift | IN | ap_uint\<5\> | 5 | AXI-Lite (s_axilite, bundle=ctrl) | Right-shift amount for requantization (0 to 31). Output = clamp(acc >> out_shift, -128, 127). |
| relu_en | IN | ap_uint\<1\> | 1 | AXI-Lite (s_axilite, bundle=ctrl) | ReLU enable flag. When 1, negative accumulator values are clamped to 0 before requantization. When 0, no clamping. |
| return | -- | -- | -- | AXI-Lite (s_axilite, bundle=ctrl) | Block-level control: ap_ctrl_hs. Software writes AP_START, reads AP_DONE/AP_IDLE. |

All scalar parameters (weights, bias, height, width, out_shift, relu_en) and the
block control (return) share a single AXI-Lite bundle named "ctrl" to minimize
interface overhead.

## 3. Data Types

```cpp
#include <ap_int.h>
#include <ap_fixed.h>
#include <hls_stream.h>
#include <ap_axi_sdata.h>

// Compile-time constants
const int MAX_WIDTH = 128;    // Maximum supported feature map width (line buffer sizing)
const int KERNEL_SIZE = 3;    // Fixed 3x3 kernel

// Stream element type (top-level AXI-Stream ports only)
typedef ap_axiu<8, 0, 0, 0> axis8_t;

// Core data types
typedef ap_int<8>   act_t;      // Activations: signed INT8, range [-128, 127]
typedef ap_int<8>   weight_t;   // Weights: signed INT8, range [-128, 127]
typedef ap_int<32>  bias_t;     // Bias: INT32, pre-scaled to accumulator domain
typedef ap_int<32>  acc_t;      // Accumulator: 32-bit signed
                                //   Max |MAC| = 9 * 127 * 127 = 145,161
                                //   Max |MAC+bias| fits easily in 32 bits
typedef ap_uint<5>  shift_t;    // Requantization right-shift amount (0-31)
typedef ap_uint<8>  dim_t;      // Feature map dimensions (3-128)
```

Numerical properties:
- No ap_fixed types are used; the entire data path is pure integer arithmetic.
- The 32-bit accumulator has 23 bits of headroom beyond the maximum MAC magnitude
  (145,161 < 2^18), so bias values up to +/-2^31 are safe.
- Requantization uses arithmetic right-shift (sign-preserving) followed by
  saturation clamping to [-128, 127].

## 4. Algorithm / Processing

### 4.1 High-Level Data Flow

```
data_in --> [Read + Line Buffer] --> [Window Extract] --> [3x3 MAC + Bias] --> [ReLU] --> [Requantize + Clamp] --> data_out
```

All stages are fused into a single pipelined loop that processes one pixel per
clock cycle (II=1). There is no DATAFLOW decomposition; the entire computation
fits within one pipeline stage per pixel.

### 4.2 Line Buffer Design

The line buffer holds the (KERNEL_SIZE - 1) = 2 most recent rows to provide the
3x3 window for convolution. It is implemented as a 2D shift register array:

```cpp
static act_t line_buf[KERNEL_SIZE - 1][MAX_WIDTH];
// line_buf[0][*] = row (current_row - 2) — oldest row in the window
// line_buf[1][*] = row (current_row - 1) — middle row in the window
// Current pixel arrives from data_in — newest row in the window
```

On each pixel arrival at column `col`:
1. Read the two stored values at `line_buf[0][col]` and `line_buf[1][col]`
2. Shift: `line_buf[0][col] = line_buf[1][col]` (old middle becomes old top)
3. Shift: `line_buf[1][col] = current_pixel` (current becomes new middle)
4. The 3x3 window is formed from `line_buf[0][col-1..col+1]`,
   `line_buf[1][col-1..col+1]`, and the last 3 pixels of the current row

A small 3-element shift register tracks the last 3 pixels of the current row
for the bottom row of the window.

The line buffer should be partitioned to allow parallel access:
```cpp
#pragma HLS ARRAY_PARTITION variable=line_buf complete dim=1
```

### 4.3 Zero-Padding Logic ("Same" Padding)

For a 3x3 kernel with stride=1, "same" padding adds 1 pixel of zeros on each
border. Rather than physically padding the input stream, the IP applies virtual
zero-padding by checking pixel coordinates:

```
For output pixel at (row, col), the 3x3 window centered at (row, col):
  win[ky][kx] is valid if:
    (row + ky - 1) >= 0  AND  (row + ky - 1) < height  AND
    (col + kx - 1) >= 0  AND  (col + kx - 1) < width
  Otherwise: win[ky][kx] = 0
```

Where ky, kx are in {0, 1, 2} (kernel row/column indices), and the kernel is
centered at (ky=1, kx=1).

Boundary conditions:
- Top row (row=0): the top row of the kernel window is zero-padded
- Bottom row (row=height-1): the bottom row of the kernel window is zero-padded
- Left column (col=0): the left column of the kernel window is zero-padded
- Right column (col=width-1): the right column of the kernel window is zero-padded
- Corners: two sides are zero-padded

### 4.4 MAC Computation

For each output pixel, compute the 3x3 multiply-accumulate:

```cpp
acc_t acc = (acc_t)bias;
KERNEL_ROW:
for (int ky = 0; ky < KERNEL_SIZE; ky++) {
    KERNEL_COL:
    for (int kx = 0; kx < KERNEL_SIZE; kx++) {
        #pragma HLS UNROLL
        acc += (acc_t)window[ky][kx] * (acc_t)weights[ky * KERNEL_SIZE + kx];
    }
}
```

Both inner loops are fully unrolled so all 9 MACs execute in one pipeline stage.
The weights array should be copied to local registers at the start of the
function to ensure constant access:

```cpp
weight_t w_local[9];
#pragma HLS ARRAY_PARTITION variable=w_local complete
LOAD_WEIGHTS:
for (int i = 0; i < 9; i++) {
    #pragma HLS UNROLL
    w_local[i] = weights[i];
}
```

### 4.5 ReLU Activation

After MAC + bias:

```cpp
if (relu_en && acc < 0) {
    acc = 0;
}
```

This adds a single multiplexer to the data path (negligible cost).

### 4.6 Requantization and Output Clamping

Convert the 32-bit accumulator back to INT8:

```cpp
acc_t shifted = acc >> out_shift;   // Arithmetic right-shift (sign-preserving)

// Saturate to INT8 range
act_t out_val;
if (shifted > 127) {
    out_val = 127;
} else if (shifted < -128) {
    out_val = -128;
} else {
    out_val = (act_t)shifted;
}
```

### 4.7 Main Processing Loop Structure

```cpp
void depthwise_conv(
    hls::stream<axis8_t>& data_in,
    hls::stream<axis8_t>& data_out,
    weight_t weights[9],
    bias_t bias,
    dim_t height,
    dim_t width,
    shift_t out_shift,
    ap_uint<1> relu_en
) {
    #pragma HLS INTERFACE axis port=data_in
    #pragma HLS INTERFACE axis port=data_out
    #pragma HLS INTERFACE s_axilite port=weights bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=bias bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=height bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=width bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=out_shift bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=relu_en bundle=ctrl
    #pragma HLS INTERFACE s_axilite port=return bundle=ctrl

    // Copy weights to local registers
    // Declare line buffer and window (static for persistence is not needed
    // since each invocation processes one complete channel)
    // Pixel loop: H * W iterations, PIPELINE II=1

    PIXEL_LOOP:
    for (dim_t row = 0; row < height; row++) {
        for (dim_t col = 0; col < width; col++) {
            #pragma HLS PIPELINE II=1
            // 1. Read input pixel from stream
            // 2. Update line buffer and extract 3x3 window
            // 3. Apply zero-padding mask
            // 4. Compute MAC
            // 5. Apply ReLU
            // 6. Requantize and clamp
            // 7. Write output pixel to stream
            // Set TLAST on the final pixel (row==height-1 && col==width-1)
        }
    }
}
```

### 4.8 Weight Layout Reference

The 3x3 kernel weights map to spatial positions as follows:

```
weights[0]  weights[1]  weights[2]     top-left     top-center    top-right
weights[3]  weights[4]  weights[5]  =  mid-left     center        mid-right
weights[6]  weights[7]  weights[8]     bottom-left  bottom-center bottom-right
```

Row-major order: index = ky * 3 + kx.

## 5. Target Configuration

| Parameter | Value |
|-----------|-------|
| FPGA Part | xc7z020clg400-1 |
| Clock Period | 10 ns (100 MHz) |
| Target II | 1 (one pixel per clock cycle, on the inner pixel loop) |
| Target Latency | H * W + ~10 cycles (pipeline fill + drain) |
| Target Throughput | 100 Mpixels/sec at 100 MHz (1 pixel/cycle) |

### Expected Resource Usage

| Resource | Core Estimate | Interface Overhead | Total Estimate |
|----------|---------------|--------------------|----------------|
| BRAM_18K | 1 (line buffer: 2 x 128 x 8 = 2048 bits) | 0 | 1 |
| DSP | 9 (one 8x8 multiply per kernel tap; HLS may use LUTs for 8-bit) | 0 | 9 |
| FF | ~500 (window 72, weights 72, pipeline ~200, counters ~100, misc ~56) | ~610 (AXI-Lite ~550 for 13 registers, 2x AXI-Stream ~60) | ~2220 (with 2.0x HLS infrastructure overhead) |
| LUT | ~500 (adder tree ~130, ReLU ~30, shift+clamp ~50, control ~290) | ~770 (AXI-Lite ~750 for 13 registers, 2x AXI-Stream ~20) | ~2540 (with 2.0x HLS infrastructure overhead) |

HLS infrastructure overhead factor: 2.0x applied to (core + interface) LUT and
FF totals. This accounts for pipeline control, valid/ready propagation,
multi-cycle scheduling FSMs, and other synthesis artifacts. DSP and BRAM
estimates are not multiplied.

Device utilization on xc7z020: LUT ~5%, FF ~2%, BRAM <1%, DSP ~4%.

## 6. Test Scenarios

### Test 1: Identity Kernel (Basic Correctness)
- **Input**: 8x8 feature map with known pixel values (e.g., row*8+col, signed range)
- **Weights**: [0, 0, 0, 0, 1, 0, 0, 0, 0] (center-only, identity filter)
- **Bias**: 0
- **out_shift**: 0
- **relu_en**: 0
- **Expected**: Output equals input exactly for all pixels (zero-padding has no
  effect since only the center weight is nonzero and the center pixel is always
  valid). Verifies basic MAC and streaming correctness.

### Test 2: Uniform Kernel (Accumulation + Border Behavior)
- **Input**: 6x6 feature map, all pixels = 1
- **Weights**: [1, 1, 1, 1, 1, 1, 1, 1, 1] (all-ones kernel)
- **Bias**: 0
- **out_shift**: 0
- **relu_en**: 0
- **Expected output values**:
  - Corner pixels (e.g., [0,0]): sum of 4 valid neighbors = 4
  - Edge pixels (e.g., [0,1]): sum of 6 valid neighbors = 6
  - Interior pixels (e.g., [1,1]): sum of 9 valid neighbors = 9
- Verifies zero-padding logic and accumulation correctness.

### Test 3: Negative Values + ReLU
- **Input**: 8x8 feature map with alternating +10 and -10 values (checkerboard)
- **Weights**: [1, 1, 1, 1, 1, 1, 1, 1, 1] (all-ones kernel)
- **Bias**: 0
- **out_shift**: 0
- **relu_en**: 1
- **Expected**: Interior pixels accumulate to +10 or -10 (depending on center
  position in checkerboard). Negative sums are clamped to 0 by ReLU. Positive
  sums pass through. Verifies ReLU activation path.

### Test 4: Saturation / Clamping Test
- **Input**: 8x8 feature map, all pixels = 127 (maximum positive INT8)
- **Weights**: [127, 127, 127, 127, 127, 127, 127, 127, 127] (all max positive)
- **Bias**: 0
- **out_shift**: 7 (divide by 128)
- **relu_en**: 0
- **Expected**: Interior MAC = 9 * 127 * 127 = 145,161. After shift by 7:
  145,161 >> 7 = 1,134. Clamped to 127 (INT8 max). Verifies saturation logic.
  Also test with negative weights/inputs to verify negative clamping to -128.

### Test 5: Stress Test (64x64 Gradient Pattern)
- **Input**: 64x64 feature map with gradient pattern: pixel[r][c] = ((r+c) % 256) - 128
- **Weights**: [1, 2, 1, 0, 0, 0, -1, -2, -1] (Sobel-like vertical edge detector)
- **Bias**: 0
- **out_shift**: 2
- **relu_en**: 0
- **Expected**: Compute reference output using double-precision C++ model.
  RMS error between IP output and reference must be < 1 LSB. Verifies large
  frame processing, line buffer integrity over many rows, and fixed-point
  precision. Also verifies TLAST is asserted on exactly the final pixel (index 4095).

### Test 6: Small Frame Border/Padding Verification
- **Input**: 4x4 feature map, all pixels = 1
- **Weights**: [1, 1, 1, 1, 1, 1, 1, 1, 1] (all-ones kernel)
- **Bias**: 0
- **out_shift**: 0
- **relu_en**: 0
- **Expected output (4x4 matrix)**:
  ```
  4  6  6  4
  6  9  9  6
  6  9  9  6
  4  6  6  4
  ```
  Each value is the count of valid (non-padded) neighbors in the 3x3 window.
  This provides a bit-exact golden reference for verifying every pixel position
  class: corner (4), edge (6), and interior (9). The testbench must compare
  every pixel against this matrix.

## 7. Additional Notes

### Channel Iteration Protocol
The host processes a multi-channel depthwise convolution by iterating:
```
for each channel c in [0, num_channels):
    1. Write weights[0..8] for channel c to AXI-Lite registers
    2. Write bias for channel c
    3. Write height, width, out_shift, relu_en (may be same for all channels)
    4. Assert AP_START
    5. Stream H*W input pixels for channel c into data_in
    6. Read H*W output pixels for channel c from data_out
    7. Wait for AP_DONE
```

### Line Buffer Initialization
The line buffer does not need to persist across invocations (each invocation
processes one complete channel). It can be declared as a local array (not
static). It must be initialized to zero at the start of each invocation, or the
zero-padding logic must handle the "not enough rows buffered yet" condition for
the first (KERNEL_SIZE-1) rows.

### Stride and Dilation
This baseline IP supports stride=1 only. Stride=2 support (common in depthwise
separable layers for spatial downsampling) is deferred to an optimization round,
where it can be added as a configurable parameter with conditional output
writing.

### Pairing with Pointwise Convolution
A complete depthwise separable layer requires this IP followed by a 1x1 pointwise
convolution IP. The pointwise conv IP would read all output channels from this IP
(buffered in external memory) and compute channel-mixing via a 1x1 conv or
matrix multiply. This is a separate IP to be specified and implemented
independently.

### Quantization Compatibility
The INT8 data path with shift-based requantization is compatible with:
- TensorFlow Lite quantization (asymmetric, but this IP uses symmetric with
  zero_point=0; the host can apply zero-point offset externally)
- ONNX Runtime INT8 quantization scheme
- PyTorch quantization (per-channel weights supported via per-invocation weight loading)

The bias is expected to be pre-scaled by the host to the accumulator domain:
`bias_quantized = round(bias_float / (input_scale * weight_scale))`.

### TLAST Protocol
- The input stream must assert TLAST=1 on the last pixel of the frame (pixel
  index H*W-1). The IP reads TLAST but does not depend on it for control flow;
  it counts H*W pixels based on the height and width parameters.
- The output stream asserts TLAST=1 on the last output pixel. Downstream IPs
  or DMA engines can use this for frame boundary detection.
