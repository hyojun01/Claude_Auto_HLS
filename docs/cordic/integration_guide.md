# Integration Guide: CORDIC

## Prerequisites
- Vivado Design Suite 2023.x or later
- Vitis HLS-generated IP exported as Vivado IP

## Export from Vitis HLS

```tcl
open_project proj_cordic
open_solution sol1
export_design -format ip_catalog -description "CORDIC Engine" -vendor "user" -display_name "CORDIC"
```

This creates a packaged IP in `proj_cordic/sol1/impl/ip/`.

## Add to Vivado Project

1. Open your Vivado project
2. **Settings → IP → Repository**: Add the `proj_cordic/sol1/impl/ip/` directory
3. In the Block Design, right-click → **Add IP** → search for "cordic"
4. The IP block appears with `in_stream` (AXI-Stream slave) and `out_stream` (AXI-Stream master) ports

## Connect Interfaces

### Clock and Reset
- Connect `ap_clk` to your system clock (must match synthesis target: 100 MHz)
- Connect `ap_rst_n` to an active-low synchronous reset

### AXI-Stream Input (`in_stream`)
- Connect to an upstream AXI-Stream master (e.g., DMA, another HLS IP, or a custom data source)
- TDATA width: 48 bits — pack three 16-bit ap_fixed<16,2> fields:
  - Bits [15:0]: X component
  - Bits [31:16]: Y component
  - Bits [47:32]: Phase angle
- TUSER width: 1 bit — mode select (0 = rotation, 1 = vectoring)
- TLAST: Assert on the last sample of a frame/burst

### AXI-Stream Output (`out_stream`)
- Connect to a downstream AXI-Stream slave (e.g., DMA, FIFO, another processing stage)
- TDATA width: 48 bits — same packing as input
- TLAST: Propagated from input with pipeline-aligned latency (8 cycles)

### Typical System Topology

```
              ┌──────────┐      ┌────────┐      ┌──────────┐
  PS/DMA ────►│ AXI-S TX │─────►│ CORDIC │─────►│ AXI-S RX │────► PS/DMA
              └──────────┘      └────────┘      └──────────┘
                in_stream          48b axis        out_stream
                + TUSER(mode)                      + TLAST
```

## Data Preparation

### Rotation Mode (sin/cos computation)

To compute sin(theta) and cos(theta):
1. Set `x_in = 1/K = 0.60725` (in ap_fixed<16,2> representation)
2. Set `y_in = 0`
3. Set `phase_in = theta` (angle in radians, range approximately [-1.74, 1.74])
4. Set `TUSER = 0`

Output: `x_out ≈ cos(theta)`, `y_out ≈ sin(theta)`

### Vectoring Mode (magnitude/phase computation)

To compute magnitude and phase of vector (I, Q):
1. Set `x_in = I` (must be non-negative for convergence)
2. Set `y_in = Q`
3. Set `phase_in = 0`
4. Set `TUSER = 1`

Output: `x_out ≈ K * sqrt(I^2 + Q^2)`, `phase_out ≈ atan2(Q, I)`

Divide `x_out` by K = 1.64676 externally if true magnitude is needed.

## Performance Notes

- **Latency**: 8 clock cycles from input to corresponding output
- **Throughput**: 1 sample per clock (II = 1) after pipeline is filled
- **TLAST alignment**: TLAST from a given input sample appears on its corresponding output exactly 8 cycles later
- **Backpressure**: If `out_stream` is not ready (TREADY deasserted), the pipeline stalls and `in_stream` TREADY is also deasserted
