# Integration Guide: 256-Point Hamming Window

## Vivado Integration

### Exporting the IP

From the `src/window/` directory, run synthesis and export:

```tcl
open_project proj_window
set_top window
add_files src/window.cpp
add_files src/window.hpp
open_solution "sol1" -flow_target vivado
set_part {xc7z020clg400-1}
create_clock -period 10 -name default
csynth_design
export_design -format ip_catalog
close_project
```

The exported IP will be in `proj_window/sol1/impl/ip/`.

### Adding to Vivado Block Design

1. Open your Vivado project and create or open a block design.
2. **Add IP Repository**: Settings → IP → Repository → Add path to `proj_window/sol1/impl/ip/`.
3. **Add IP**: Right-click canvas → Add IP → search for "window".
4. **Connect AXI-Stream ports**:
   - `in_r` (slave) → connect to upstream data source (e.g., ADC DMA, another processing IP)
   - `out_r` (master) → connect to downstream consumer (e.g., FFT IP, DMA)
5. **Clock and Reset**:
   - `ap_clk` → connect to your system clock (100 MHz)
   - `ap_rst_n` → connect to active-low synchronous reset

### Control Interface

This IP uses `ap_ctrl_none` — there is no start/done/idle handshake. The IP begins processing as soon as:
- Reset is deasserted (`ap_rst_n` = 1)
- Valid data is available on the input AXI-Stream

No software driver or register configuration is needed.

### Data Format

The 32-bit AXI-Stream TDATA carries packed complex samples:

```
Bit [31:16] — Real component (ap_fixed<16, 1>, Q1.15)
Bit [15:0]  — Imaginary component (ap_fixed<16, 1>, Q1.15)
```

The Q1.15 format provides:
- Range: [−1.0, +0.999969482...]
- Resolution: 2^(−15) ≈ 3.05 × 10^(−5)

### Packet Framing

- The IP reads exactly **256 consecutive samples** per packet.
- TLAST is asserted on the 256th output sample.
- The upstream source should provide 256 samples continuously without gaps for optimal throughput.
- If the upstream pauses (TVALID deasserted), the IP will stall gracefully and resume when data is available.

## Typical System Topologies

### ADC → Window → FFT

```
┌─────┐    AXI-S    ┌────────┐    AXI-S    ┌─────┐
│ ADC │───────────▶ │ Window │───────────▶ │ FFT │
│ DMA │             │  IP    │             │ IP  │
└─────┘             └────────┘             └─────┘
```

### Multi-channel with AXI-Stream Switch

```
┌──────┐    ┌───────────┐    ┌────────┐    ┌─────┐
│ Ch 0 │──▶│           │    │        │    │     │
│ Ch 1 │──▶│  AXI-S    │──▶│ Window │──▶│ FFT │
│ Ch 2 │──▶│  Switch   │    │  IP    │    │ IP  │
└──────┘    └───────────┘    └────────┘    └─────┘
```

## Verification

### C-Simulation
```bash
cd src/window/
vitis-run --tcl tcl/run_csim.tcl
```

### C-Synthesis
```bash
cd src/window/
vitis-run --tcl tcl/run_csynth.tcl
```

### Co-Simulation
```bash
cd src/window/
vitis-run --tcl tcl/run_cosim.tcl
```
