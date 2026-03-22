# 256-Point Hamming Window IP

## Overview

This IP applies a 256-point Hamming window to complex (I/Q) input samples streamed via AXI-Stream. Each 32-bit input word packs a 16-bit signed fixed-point real component (upper half) and a 16-bit signed fixed-point imaginary component (lower half). The IP multiplies both components by the corresponding precomputed Hamming coefficient and outputs the windowed result on a second AXI-Stream port.

The design uses DATAFLOW task-level pipelining with three stages (read, apply, write) to achieve a sustained throughput of one sample per clock cycle.

## Specifications

| Parameter | Value |
|-----------|-------|
| Tool Version | AMD Vitis HLS 2025.1 |
| FPGA Part | xc7z020clg400-1 (Zynq-7000) |
| Clock Period | 10 ns (100 MHz) |
| Top Function | `window` |
| Window Size | 256 samples |
| Window Type | Hamming: w[n] = 0.54 − 0.46·cos(2πn/255) |

## I/O Ports

| Port | Direction | Type | Width | Protocol | Description |
|------|-----------|------|-------|----------|-------------|
| `in` | IN | `hls::stream<ap_axiu<32,0,0,0>>` | 32 | AXI-Stream | Complex input: real[31:16], imag[15:0] |
| `out` | OUT | `hls::stream<ap_axiu<32,0,0,0>>` | 32 | AXI-Stream | Windowed output: real[31:16], imag[15:0] |
| (return) | — | — | — | ap_ctrl_none | Free-running, no start/done handshake |

### AXI-Stream Signals

Both ports expose the standard AXI-Stream sideband signals: TDATA (32 bits), TKEEP (4 bits), TSTRB (4 bits), TLAST (1 bit), TVALID, TREADY.

- **TLAST**: The IP generates TLAST=1 on the 256th output sample of each packet. Input TLAST is ignored; the IP counts 256 samples internally.

## Data Types

| Type | Definition | Description |
|------|-----------|-------------|
| `data_t` | `ap_fixed<16, 1>` | Q1.15 signed fixed-point for real and imaginary components |
| `coeff_t` | `ap_ufixed<16, 0>` | Unsigned 0.16 fixed-point for Hamming coefficients (range [0, ~1)) |
| `acc_t` | `ap_fixed<32, 1>` | Full-precision product type (prevents overflow) |

## Architecture

```
           ┌──────────────┐     ┌───────────────┐     ┌──────────────┐
 AXI-S ──▶│  read_input   │──▶ │  apply_window  │──▶ │ write_output │──▶ AXI-S
  in       │  (strip AXI)  │buf1│  (× coeff ROM) │buf2│  (add AXI)   │   out
           └──────────────┘     └───────────────┘     └──────────────┘
                              DATAFLOW pipeline
```

1. **read_input**: Reads 256 `axis_t` words from the input stream, extracts 32-bit data, writes to internal FIFO `buf1`.
2. **apply_window**: Reads from `buf1`, splits into real/imag, multiplies each by `HAMMING_COEFF[i]` from ROM, truncates the 32-bit product to 16-bit output, writes packed data to `buf2`.
3. **write_output**: Reads from `buf2`, attaches AXI-Stream sideband signals (TKEEP, TSTRB, TLAST), writes to output stream.

All three stages are pipelined at II=1 and overlap via DATAFLOW.

## Performance

| Metric | Value |
|--------|-------|
| Clock Target | 10 ns (100 MHz) |
| Clock Estimated | 5.58 ns |
| Clock Slack | 4.42 ns |
| Timing Met | Yes |
| Latency | 264 cycles (2.640 μs) |
| Initiation Interval | 256 cycles |
| Throughput | 1 sample/cycle = 100 MSps at 100 MHz |
| Dataflow Throughput | 256 cycles per packet |

## Resource Utilization

| Resource | Used | Available | Utilization |
|----------|------|-----------|-------------|
| BRAM_18K | 5 | 280 | 1.8% |
| DSP48E1 | 2 | 220 | 0.9% |
| FF | 522 | 106,400 | 0.5% |
| LUT | 509 | 53,200 | 1.0% |

- **2 DSPs**: One for real multiplication, one for imaginary multiplication (16 × 16 fits within DSP48E1 native 25 × 18)
- **5 BRAMs**: 1 for coefficient ROM + 2 each for the two depth-256 internal FIFOs

## Constraints & Limitations

- Fixed window size of 256 samples (not parameterizable at runtime).
- `ap_ctrl_none` means the IP runs continuously — it must always have data available on the input or the pipeline will stall.
- Input TLAST is ignored; the IP always reads exactly 256 samples per packet.
- The coefficient table is hardcoded; changing the window function requires resynthesis.

## Usage Example

```cpp
// Minimal testbench snippet
hls::stream<axis_t> in_stream, out_stream;

// Push 256 samples
for (int i = 0; i < 256; i++) {
    axis_t word;
    word.data.range(31, 16) = real_sample[i].range();  // ap_fixed<16,1>
    word.data.range(15, 0)  = imag_sample[i].range();  // ap_fixed<16,1>
    word.last = (i == 255) ? 1 : 0;
    word.keep = -1;
    word.strb = -1;
    in_stream.write(word);
}

// Process
window(in_stream, out_stream);

// Read 256 windowed samples
for (int i = 0; i < 256; i++) {
    axis_t out_word = out_stream.read();
    // out_word.data[31:16] = windowed real
    // out_word.data[15:0]  = windowed imag
}
```
