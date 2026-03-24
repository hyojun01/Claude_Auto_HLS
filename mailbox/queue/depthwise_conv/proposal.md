# IP Proposal: depthwise_conv

## Domain
Deep Learning

## Rationale
Depthwise convolution is the first half of the depthwise separable convolution
layer used in MobileNet, EfficientNet, and other efficient DL architectures. It
applies a separate spatial filter to each input channel independently, reducing
the compute cost from O(C_in * C_out * K^2) to O(C * K^2) compared to standard
convolution. On FPGA, this maps naturally to a streaming line-buffer architecture
with one 3x3 MAC unit processing one channel at a time, reused across all
channels by the host.

This IP provides the channel-independent spatial filtering stage. It pairs with
a 1x1 pointwise convolution IP to form a complete depthwise separable layer. The
INT8 quantized data path is compatible with TFLite and ONNX Runtime quantization
schemes, making it suitable for deploying pre-trained models on edge FPGAs.

The single-channel-per-invocation design keeps the IP small and modular: the host
iterates over channels, writing new weights via AXI-Lite before each invocation.
This trades host-side orchestration for minimal FPGA resource usage, which is
appropriate for the xc7z020 target.

## Specification Summary
- **Algorithm**: 3x3 depthwise convolution, single channel per invocation, INT8
  activations and weights, 32-bit bias and accumulator, "same" zero-padding,
  fused ReLU activation, shift-based requantization to INT8 output
- **I/O**: data_in (AXI-Stream 8-bit), data_out (AXI-Stream 8-bit), weights/bias/
  height/width/out_shift/relu_en (AXI-Lite bundle=ctrl), ap_ctrl_hs return
- **Data types**: ap_int<8> activations/weights, ap_int<32> bias/accumulator,
  ap_uint<8> dimensions, ap_uint<5> shift, ap_uint<1> relu enable
- **Target**: xc7z020clg400-1 @ 10 ns (100 MHz)
- **Complexity estimate**: Medium

## Optimization Strategy (if applicable)
- **Phase 1**: Baseline functional design with PIPELINE II=1 on the pixel loop,
  line buffer in shift registers, all 9 multiplies in one pipeline stage. Target:
  correct functionality, II=1, timing met at 100 MHz.
- **Phase 2**: If DSP usage exceeds 9 or timing is tight, consider narrowing
  multiply paths or using LUT-based 8-bit multiplies. If line buffer exceeds
  BRAM expectations, adjust MAX_WIDTH or partitioning.

## Dependencies
- **Requires**: None
- **Required by**: Depthwise separable convolution layer (future: pointwise_conv
  IP provides the channel-mixing second stage)

## User Decision
<!-- User fills this section -->
- [ ] Approve as-is -> proceed to implementation
- [ ] Approve with modifications -> (describe modifications below)
- [ ] Reject -> (reason)
- [ ] Defer -> (revisit later)

### Modifications (if applicable)
<!-- User writes requested changes here -->
