# Integration Guide: Depthwise Convolution

## Prerequisites
- Vivado Design Suite 2023.x or later
- Vitis HLS-generated IP exported as Vivado IP

## Export from Vitis HLS

Using TCL:
```tcl
open_project proj_depthwise_conv
open_solution sol1
export_design -format ip_catalog -output depthwise_conv_ip.zip
```

Or from the Vitis HLS GUI: Solution → Export RTL → IP Catalog.

## Add to Vivado Project

1. In Vivado, open **Settings → IP → Repository**
2. Add the directory containing the exported IP
3. In the Block Design, click **Add IP** and search for `depthwise_conv`
4. Place the IP and connect interfaces

## Connect Interfaces

### Clock and Reset
- Connect `ap_clk` to the system clock (100 MHz recommended)
- Connect `ap_rst_n` to the system active-low reset

### AXI-Lite Control (ctrl)
- Connect to an AXI Interconnect driven by the processing system (PS)
- Assign an address range (minimum 4 KB, 7-bit address space = 128 bytes used)
- Register map:

| Offset | Register | Width | Description |
|--------|----------|-------|-------------|
| 0x00 | Control | 32 | Bit 0: ap_start, Bit 1: ap_done, Bit 2: ap_idle |
| 0x10–0x30 | weights[0..8] | 8 (in 32-bit reg) | 3x3 kernel weights, signed INT8, row-major |
| 0x38 | bias | 32 | Pre-scaled INT32 bias |
| 0x40 | height | 8 (in 32-bit reg) | Feature map height (3–128) |
| 0x48 | width | 8 (in 32-bit reg) | Feature map width (3–128) |
| 0x50 | out_shift | 5 (in 32-bit reg) | Right-shift for requantization (0–31) |
| 0x58 | relu_en | 1 (in 32-bit reg) | ReLU enable (1=on, 0=off) |

> **Note**: Register offsets above are illustrative. Consult the auto-generated driver header (`xdepthwise_conv_hw.h`) for exact offsets, which may vary with HLS version.

### AXI-Stream Data
- **data_in**: Connect to upstream data source (e.g., DMA S2MM channel)
  - 8-bit TDATA, signed INT8 activations in row-major order
  - H × W pixels per invocation
  - TLAST=1 on the final pixel
- **data_out**: Connect to downstream consumer (e.g., DMA MM2S channel or next processing stage)
  - 8-bit TDATA, signed INT8 output activations in row-major order
  - H × W pixels per invocation
  - TLAST=1 on the final pixel

### Typical Block Design

```
                                                    +--------------------+
PS (AXI-Lite) ──→ AXI Interconnect ──→ [s_axi_ctrl] |                    | [data_in] ←── AXI DMA (MM2S)
                                                    | depthwise_conv     |
                                                    |                    | [data_out] ──→ AXI DMA (S2MM)
                                                    +--------------------+
                                                         ap_clk  ap_rst_n
                                                           ↑        ↑
                                                      FCLK_CLK0  proc_sys_reset
```

## Software Driver

### Basic Operation Sequence

```c
#include "xdepthwise_conv.h"  // Auto-generated driver

XDepthwise_conv dconv;
XDepthwise_conv_Config *cfg = XDepthwise_conv_LookupConfig(XPAR_DEPTHWISE_CONV_0_DEVICE_ID);
XDepthwise_conv_CfgInitialize(&dconv, cfg);

// Configure kernel weights (3x3, row-major, signed INT8)
int8_t weights[9] = {1, 2, 1, 0, 0, 0, -1, -2, -1};  // Sobel vertical
for (int i = 0; i < 9; i++) {
    XDepthwise_conv_Set_weights(/* per-element setter, see driver header */);
}

// Configure parameters
XDepthwise_conv_Set_bias(&dconv, 0);
XDepthwise_conv_Set_height(&dconv, 64);
XDepthwise_conv_Set_width(&dconv, 64);
XDepthwise_conv_Set_out_shift(&dconv, 2);
XDepthwise_conv_Set_relu_en(&dconv, 0);

// Configure DMA transfers for data_in (64x64 = 4096 bytes) and data_out
// ... (set up AXI DMA descriptors) ...

// Start IP
XDepthwise_conv_Start(&dconv);

// Poll for completion
while (!XDepthwise_conv_IsDone(&dconv));
```

### Multi-Channel Depthwise Convolution

For a depthwise separable layer with C channels, iterate:

```c
for (int ch = 0; ch < num_channels; ch++) {
    // 1. Load channel-specific weights
    for (int i = 0; i < 9; i++) {
        XDepthwise_conv_Set_weights_i(&dconv, i, kernel_weights[ch][i]);
    }
    XDepthwise_conv_Set_bias(&dconv, bias_values[ch]);

    // 2. Configure DMA for this channel's input/output buffers
    setup_dma_transfer(input_buf[ch], output_buf[ch], H * W);

    // 3. Start and wait
    XDepthwise_conv_Start(&dconv);
    while (!XDepthwise_conv_IsDone(&dconv));
}
```

### Quantization Parameter Setup

The bias must be pre-scaled to the accumulator domain:

```c
// Given floating-point model parameters:
//   input_scale, weight_scale, bias_float
// Compute quantized bias:
int32_t bias_quant = (int32_t)round(bias_float / (input_scale * weight_scale));

// Compute output shift from output scale:
//   out_shift = log2(input_scale * weight_scale / output_scale)
uint8_t out_shift = (uint8_t)round(log2(input_scale * weight_scale / output_scale));
```

## Latency and Throughput

| Frame Size | Latency (cycles) | Latency (time @ 100 MHz) | Pixel Throughput |
|------------|-------------------|--------------------------|------------------|
| 4×4 | ~153 | ~1.53 us | 100 Mpixels/s |
| 8×8 | ~209 | ~2.09 us | 100 Mpixels/s |
| 64×64 | ~4,353 | ~43.5 us | 100 Mpixels/s |
| 128×128 | ~16,769 | ~167.7 us | 100 Mpixels/s |

Latency includes 128-cycle line buffer initialization + (H+1)×(W+1) pipeline iterations.

## Resource Utilization (xc7z020clg400-1)

| Resource | Used | Available | Utilization |
|----------|------|-----------|-------------|
| BRAM_18K | 2 | 280 | 0.7% |
| DSP | 7 | 220 | 3.2% |
| FF | 1,428 | 106,400 | 1.3% |
| LUT | 1,836 | 53,200 | 3.5% |

Timing: 7.179 ns estimated (10 ns target), 28% margin.

## Design Considerations

- **Line buffer initialization**: Each invocation spends 128 cycles zeroing the line buffer. For small frames this is a meaningful overhead; for 64×64+ it is negligible.
- **Back-pressure**: The IP stalls (TREADY de-asserted on data_in) if the output DMA is not ready to accept data. Ensure DMA buffers are configured before starting the IP.
- **TLAST handling**: The IP asserts TLAST on the final output pixel. Configure the DMA to use TLAST as the end-of-transfer indicator.
- **Frame dimensions**: Height and width must be in [3, 128]. Values outside this range produce undefined behavior.
