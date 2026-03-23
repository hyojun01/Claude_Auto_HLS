# Integration Guide: NCO

## Prerequisites

- Vivado Design Suite 2025.2 or later
- Vitis HLS 2025.2 or later (for IP export)
- Vitis HLS-generated IP exported as Vivado IP Catalog format

## Export from Vitis HLS

Run the synthesis and export from the project directory:

```tcl
open_project proj_nco
open_solution sol1
export_design -format ip_catalog -description "Numerically Controlled Oscillator" -vendor "user" -display_name "NCO"
```

This creates a packaged IP in `proj_nco/sol1/impl/ip/`. The exported IP contains the AXI-Lite slave interface for control/parameters and the AXI-Stream master interface for output.

## Add to Vivado Project

1. Open your Vivado project
2. **Settings -> IP -> Repository**: Click **+** and add the `proj_nco/sol1/impl/ip/` directory
3. Click **OK** to refresh the IP catalog
4. In the Block Design editor, right-click the canvas -> **Add IP** -> search for "nco"
5. The IP block appears with:
   - `s_axi_control` (AXI-Lite slave) for registers and block-level control
   - `out_stream` (AXI-Stream master) for I/Q output data
   - `ap_clk` and `ap_rst_n` clock/reset ports

## Connect Interfaces

### Clock and Reset

- Connect `ap_clk` to your system clock (must match the synthesis target: 100 MHz / 10 ns period)
- Connect `ap_rst_n` to an active-low synchronous reset (e.g., from the Processor System Reset IP)

### AXI-Lite Control (`s_axi_control`)

Connect to the PS AXI master or an AXI Interconnect for software register access:

1. Use **Run Connection Automation** or manually connect to an AXI Interconnect tied to the PS GP port
2. Assign an address range in the Address Editor (minimum 4 KB: 0x1000)
3. The base address + offsets below give access to all NCO parameters

### AXI-Lite Register Map

| Offset | Name         | Width  | R/W | Description |
|--------|--------------|--------|-----|-------------|
| 0x00   | CTRL         | 32-bit | R/W | Block-level control: ap_start (bit 0), ap_done (bit 1), ap_idle (bit 2), ap_ready (bit 3) |
| 0x10   | phase_inc    | 32-bit | W   | Phase increment — sets output frequency: f_out = f_clk * phase_inc / 2^32 |
| 0x18   | phase_offset | 32-bit | W   | Static phase offset added to accumulator each sample |
| 0x20   | num_samples  | 32-bit | W   | Number of samples to generate per burst |
| 0x28   | sync_reset   | 1-bit  | W   | Write 1 to clear the phase accumulator to 0 |

> Confirm actual register offsets from the generated driver header (`xnco_hw.h`) after IP export.

### AXI-Stream Output (`out_stream`)

Connect to a downstream AXI-Stream slave. Common topologies:

**Option A: DMA to DDR**
- Connect `out_stream` -> AXI DMA (S2MM channel) -> DDR via HP port
- Configure DMA for 32-bit stream width
- Use TLAST to mark burst boundaries (DMA uses TLAST for transfer completion)

**Option B: Direct to downstream IP**
- Connect `out_stream` directly to another AXI-Stream slave (e.g., mixer, filter, DAC interface)
- TDATA width: 32 bits
- TLAST: asserted on the final sample of each burst

**Option C: AXI-Stream FIFO**
- Insert an AXI-Stream Data FIFO between the NCO and consumer for rate decoupling
- Set FIFO depth >= num_samples if the consumer may stall

### Output Data Format

The 32-bit TDATA carries two Q1.15 fixed-point samples:

| Bits     | Field   | Type           | Range                      |
|----------|---------|----------------|----------------------------|
| [31:16]  | cos_out | ap_fixed<16,1> | [-0.999969, +0.999969]     |
| [15:0]   | sin_out | ap_fixed<16,1> | [-0.999969, +0.999969]     |

To extract in software (C/C++ on Zynq PS):

```c
int32_t tdata = /* read from DMA buffer */;

// Extract as signed 16-bit integers (Q1.15 fixed-point)
int16_t sin_raw = (int16_t)(tdata & 0xFFFF);
int16_t cos_raw = (int16_t)((tdata >> 16) & 0xFFFF);

// Convert to floating-point
double sin_val = sin_raw / 32768.0;
double cos_val = cos_raw / 32768.0;
```

### Typical System Topology

```
  +--------+     +-----+     +-----------+     +-----+     +--------+
  | Zynq   |<--->| AXI |<--->| NCO       |---->| DMA |---->| DDR    |
  | PS     |     | Int |     | s_axi_ctrl|     | S2MM|     | (HP)   |
  | (GP)   |     +-----+     | out_stream|     +-----+     +--------+
  +--------+                 +-----------+
                                  |
                             ap_clk  ap_rst_n
                                  |       |
                             +----+-------+----+
                             | Clk Wizard /    |
                             | Proc Sys Reset  |
                             +-----------------+
```

## Software Driver Usage

After synthesis, Vitis HLS generates a software driver in `proj_nco/sol1/impl/ip/drivers/`. The driver provides convenience functions:

```c
#include "xnco.h"

XNco nco_inst;
XNco_Config *cfg;

// Initialize
cfg = XNco_LookupConfig(XPAR_NCO_0_DEVICE_ID);
XNco_CfgInitialize(&nco_inst, cfg);

// Configure: generate 1024 samples at ~1 MHz
// phase_inc = round(1e6 / 100e6 * 2^32) = 42949673 = 0x028F5C29
XNco_Set_phase_inc(&nco_inst, 0x028F5C29);
XNco_Set_phase_offset(&nco_inst, 0x00000000);
XNco_Set_num_samples(&nco_inst, 1024);
XNco_Set_sync_reset(&nco_inst, 1);  // reset accumulator

// Start
XNco_Start(&nco_inst);

// Wait for completion
while (!XNco_IsDone(&nco_inst));

// For subsequent bursts, clear sync_reset for phase continuity
XNco_Set_sync_reset(&nco_inst, 0);
XNco_Start(&nco_inst);
```

If the generated driver is not available, use direct register writes:

```c
#include "xil_io.h"

#define NCO_BASE  0x43C00000  // example base address from Address Editor

// Write parameters
Xil_Out32(NCO_BASE + 0x10, 0x028F5C29);  // phase_inc (~1 MHz)
Xil_Out32(NCO_BASE + 0x18, 0x00000000);  // phase_offset
Xil_Out32(NCO_BASE + 0x20, 1024);        // num_samples
Xil_Out32(NCO_BASE + 0x28, 1);           // sync_reset

// Start IP
Xil_Out32(NCO_BASE + 0x00, 0x01);        // ap_start = 1

// Poll for completion
while (!(Xil_Out32(NCO_BASE + 0x00) & 0x02));  // wait for ap_done
```

## Frequency Programming Reference

The output frequency is set by the `phase_inc` register:

```
f_out = f_clk * phase_inc / 2^32
phase_inc = round(f_out / f_clk * 2^32)
```

| Desired f_out | phase_inc (hex) | phase_inc (decimal) |
|---------------|-----------------|---------------------|
| 100 Hz        | 0x00000AEC      | 2,814               |
| 1 kHz         | 0x00006D3A      | 27,962               |
| 10 kHz        | 0x00044444      | 279,620              |
| 100 kHz       | 0x002AF31D      | 2,796,203            |
| 1 MHz         | 0x028F5C29      | 42,949,673           |
| 10 MHz        | 0x19999999      | 429,496,730          |
| 25 MHz        | 0x40000000      | 1,073,741,824        |

Maximum output frequency (Nyquist): 50 MHz (phase_inc = 0x80000000). Frequencies above Nyquist produce aliased output.

## Performance Notes

- **Throughput**: 1 sample per clock cycle (II = 1, achieved) after pipeline fill
- **Burst latency**: 7 to 4102 cycles depending on num_samples (pipeline depth 3 + overhead)
- **Timing**: Estimated clock 7.293 ns, Fmax 137.11 MHz (37% margin at 100 MHz target)
- **Phase continuity**: The accumulator persists across invocations when sync_reset = 0, enabling gapless waveform generation across multiple bursts
- **Backpressure**: If the downstream AXI-Stream slave deasserts TREADY, the NCO pipeline stalls until the consumer is ready
- **TLAST**: Asserted on the final sample of each burst. DMA controllers and downstream IPs that rely on TLAST for framing will see one complete transfer per invocation

## Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| No output data | ap_start not asserted | Write 0x01 to CTRL register (offset 0x00) |
| Wrong frequency | Incorrect phase_inc | Recalculate: phase_inc = round(f_out / f_clk * 2^32) |
| Phase discontinuity between bursts | sync_reset left at 1 | Set sync_reset = 0 after the first invocation |
| DMA transfer never completes | TLAST not connected or num_samples mismatch | Verify num_samples matches expected DMA transfer length |
| Output stuck at 0 | phase_inc = 0 (DC at sin(0)=0, cos(0)=1) | Set phase_inc to a nonzero value |
