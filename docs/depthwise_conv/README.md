# depthwise_conv -- 3x3 Depthwise Convolution IP

## Overview

This IP performs a 3x3 depthwise convolution on a single input feature map channel with stride=1 and "same" zero-padding (output dimensions equal input dimensions). It implements the spatial filtering stage of a depthwise separable convolution layer as used in MobileNet and similar efficient deep learning architectures.

The IP processes one channel per invocation. The host iterates over all channels by writing new 3x3 weights and bias via AXI-Lite before each invocation. Input and output activations are streamed as 8-bit signed integers (INT8) over AXI-Stream interfaces. The data path uses a 32-bit accumulator with shift-based requantization and optional fused ReLU activation.

## Specifications

| Parameter | Value |
|-----------|-------|
| Tool Version | Vitis HLS 2025.2 |
| FPGA Part | xc7z020-clg400-1 (Zynq-7000) |
| Clock Period | 10 ns (100 MHz) |
| Top Function | `depthwise_conv` |
| Flow Target | Vivado IP Flow |
| C++ Standard | C++14 |

## I/O Ports

| Port Name | Direction | Data Type | Bit Width | Interface Protocol | Description |
|-----------|-----------|-----------|-----------|-------------------|-------------|
| data_in | IN | hls::stream\<ap_axiu\<8,0,0,0\>\> | 8 | AXI-Stream (axis) | Input activation pixels, signed INT8 in TDATA[7:0], row-major order. TLAST=1 on last pixel. |
| data_out | OUT | hls::stream\<ap_axiu\<8,0,0,0\>\> | 8 | AXI-Stream (axis) | Output activation pixels, signed INT8 in TDATA[7:0], row-major order. TLAST=1 on last pixel. |
| weights | IN | ap_int\<8\> [9] | 72 (9x8) | AXI-Lite (s_axilite, bundle=ctrl) | 3x3 kernel weights in row-major order. Signed INT8. |
| bias | IN | ap_int\<32\> | 32 | AXI-Lite (s_axilite, bundle=ctrl) | Pre-scaled bias added after MAC. INT32. |
| height | IN | ap_uint\<8\> | 8 | AXI-Lite (s_axilite, bundle=ctrl) | Input feature map height (3-128). |
| width | IN | ap_uint\<8\> | 8 | AXI-Lite (s_axilite, bundle=ctrl) | Input feature map width (3-128). |
| out_shift | IN | ap_uint\<5\> | 5 | AXI-Lite (s_axilite, bundle=ctrl) | Right-shift for requantization (0-31). |
| relu_en | IN | ap_uint\<1\> | 1 | AXI-Lite (s_axilite, bundle=ctrl) | ReLU enable flag (1=on, 0=off). |
| return | -- | -- | -- | AXI-Lite (s_axilite, bundle=ctrl) | Block-level control: ap_ctrl_hs. |

## Functional Description

### Data Flow

```
data_in (axis) --> [Read Pixel] --> [Line Buffer + Window Shift] --> [Zero-Pad Mask]
    --> [3x3 MAC + Bias] --> [ReLU] --> [Shift + Clamp] --> [Write Pixel] --> data_out (axis)
```

### Algorithm

1. **Weight Loading**: 9 kernel weights are copied from AXI-Lite registers to local registers at the start of each invocation.

2. **Line Buffer**: A 2-row line buffer (2 x 128 x 8-bit, stored in 2 BRAM_18K) maintains completed rows for the 3x3 sliding window. The buffer is partitioned across the row dimension for parallel read access.

3. **Sliding Window**: A 3x3 register array tracks the column-delayed pixel values for three window rows. The window center at position [1][1] corresponds to image pixel (row-1, col-1), providing the one-row, one-column delay needed for centered convolution.

4. **Zero-Padding**: Virtual "same" padding is applied by checking pixel coordinates against image bounds. Window elements that fall outside [0, H) x [0, W) are replaced with zero before the MAC operation.

5. **MAC**: All 9 multiply-accumulate operations are fully unrolled, computing in parallel on DSP slices and LUT-based multipliers.

6. **ReLU**: Optional clamp of negative accumulator values to zero (single multiplexer).

7. **Requantization**: Arithmetic right-shift followed by saturation clamping to the INT8 range [-128, 127].

### Weight Layout

```
weights[0]  weights[1]  weights[2]     top-left     top-center    top-right
weights[3]  weights[4]  weights[5]  =  mid-left     center        mid-right
weights[6]  weights[7]  weights[8]     bottom-left  bottom-center bottom-right
```

Row-major order: index = ky * 3 + kx.

## Architecture

```
                    +------------------+
    data_in ------->| Read + LB Update |
   (axis, 8-bit)   |                  |
                    | Line Buffer [2]  |      +------------------+
                    | [BRAM 128x8 x2] |----->| Window 3x3 Shift |
                    +------------------+      | Registers        |
                                              +--------+---------+
                                                       |
                                              +--------v---------+
               weights[9] ----+               | Zero-Pad Mask    |
               (AXI-Lite)     |               +--------+---------+
                              |                        |
                    +---------v--------+      +--------v---------+
                    | Local Weight Reg |----->| 3x3 MAC + Bias   |
                    | [9 x 8-bit]     |      | (7 DSP, LUT)     |
                    +------------------+      +--------+---------+
                                                       |
                              bias ---------> (added to acc)
                                                       |
                                              +--------v---------+
                    relu_en ----------------->| ReLU (optional)  |
                                              +--------+---------+
                                                       |
                                              +--------v---------+
                    out_shift --------------->| >> shift + clamp |
                                              +--------+---------+
                                                       |
                                              +--------v---------+
    data_out <--------------------------------| Write + TLAST    |
   (axis, 8-bit)                              +------------------+
```

All stages are fused into a single pipelined loop (II=1, depth=10).

## Performance

### Latency

| Frame Size | Latency (cycles) | Latency (time) | Throughput |
|------------|------------------|-----------------|------------|
| 4x4 | ~25 + 128 init | ~1.53 us | 100 Mpixels/s |
| 8x8 | ~81 + 128 init | ~2.09 us | 100 Mpixels/s |
| 64x64 | ~4225 + 128 init | ~43.5 us | 100 Mpixels/s |
| 128x128 (max) | ~16641 + 128 init | ~167.7 us | 100 Mpixels/s |

Note: The loop runs (H+1)x(W+1) iterations to drain the pipeline for border pixels. The 128-cycle initialization loop zeros the line buffer.

### Pipeline Performance

| Parameter | Value |
|-----------|-------|
| Initiation Interval (II) | 1 cycle |
| Pipeline Depth | 10 cycles |
| Pixel Throughput | 1 pixel/cycle (100 Mpixels/s at 100 MHz) |
| Estimated Fmax | 139.29 MHz |

### Worst-Case Latency (128x128)

| Metric | Value |
|--------|-------|
| Min Latency | 152 cycles (1.52 us) |
| Max Latency | 65,687 cycles (0.657 ms) |
| Min Interval | 153 cycles |
| Max Interval | 65,688 cycles |

## Resource Utilization

| Resource | Used | Available | Utilization |
|----------|------|-----------|-------------|
| BRAM_18K | 2 | 280 | 0.7% |
| DSP | 7 | 220 | 3.2% |
| FF | 1,428 | 106,400 | 1.3% |
| LUT | 1,836 | 53,200 | 3.5% |
| URAM | 0 | 0 | -- |

### Resource Breakdown

| Component | BRAM | DSP | FF | LUT |
|-----------|------|-----|-----|-----|
| AXI-Lite (ctrl) | 0 | 0 | 216 | 220 |
| Line Buffer Init | 0 | 0 | 10 | 56 |
| Pixel Pipeline | 0 | 6 | 1,025 | 1,260 |
| Line Buffer (BRAM) | 2 | 0 | 0 | 0 |
| Top-level + Mux | 0 | 1 | 177 | 300 |
| **Total** | **2** | **7** | **1,428** | **1,836** |

## Interface Details

### AXI-Lite Register Map (ctrl bundle)

The AXI-Lite interface uses a 7-bit address space (128 bytes). All registers are 32-bit. The register map is auto-generated by Vitis HLS. Key registers include:

| Register | Description |
|----------|-------------|
| AP_CTRL (0x00) | Control register (bit 0: AP_START, bit 1: AP_DONE, bit 2: AP_IDLE) |
| weights[0..8] | 9 weight registers (32-bit each, only low 8 bits significant) |
| bias | 32-bit bias register |
| height | Feature map height (low 8 bits) |
| width | Feature map width (low 8 bits) |
| out_shift | Shift amount (low 5 bits) |
| relu_en | ReLU enable (bit 0) |

### AXI-Stream Protocol

- **data_in**: 8-bit TDATA, TKEEP, TSTRB, TLAST. TVALID/TREADY handshake.
- **data_out**: Same signals. TLAST asserted on the final output pixel.

### Block-Level Control

Uses `ap_ctrl_hs` protocol:
1. Write parameters (weights, bias, height, width, out_shift, relu_en) via AXI-Lite
2. Assert AP_START (write 1 to AP_CTRL[0])
3. Stream H x W input pixels into data_in
4. Read H x W output pixels from data_out
5. Poll AP_DONE (AP_CTRL[1]) or wait for interrupt

## Usage Example

```cpp
// Minimal testbench snippet
#include "depthwise_conv.hpp"

int main() {
    hls::stream<axis8_t> data_in, data_out;
    weight_t weights[9] = {0, 0, 0, 0, 1, 0, 0, 0, 0}; // identity
    bias_t bias = 0;
    dim_t height = 8, width = 8;
    shift_t out_shift = 0;

    // Fill input stream (8x8, values 0..63)
    for (int i = 0; i < 64; i++) {
        axis8_t pkt;
        pkt.data = (act_t)i;
        pkt.keep = 1; pkt.strb = 1;
        pkt.last = (i == 63) ? 1 : 0;
        data_in.write(pkt);
    }

    // Run
    depthwise_conv(data_in, data_out, weights, bias,
                   height, width, out_shift, (ap_uint<1>)0);

    // Read output
    for (int i = 0; i < 64; i++) {
        axis8_t pkt = data_out.read();
        printf("pixel[%d] = %d\n", i, (int)(ap_int<8>)pkt.data);
    }
    return 0;
}
```

## Constraints and Limitations

- **Frame size**: Height and width must be in [3, 128]. Values outside this range produce undefined behavior.
- **Stride**: Fixed stride=1 only. Stride=2 support deferred to optimization.
- **Kernel size**: Fixed 3x3 only.
- **Quantization**: Symmetric quantization with zero_point=0. Asymmetric quantization requires host-side zero-point offset.
- **Line buffer initialization**: 128 cycles are spent initializing the line buffer to zero at the start of each invocation.
- **Single-channel**: Processes one channel per invocation. Multi-channel requires host iteration.
