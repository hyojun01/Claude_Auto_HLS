---
name: hls-design
description: HLS C++ design patterns and code templates for AMD Vitis HLS IP implementation — streaming, memory-mapped, dataflow, FIR, FSM patterns and interface protocol reference
user-invocable: false
---

# Skill: HLS C++ Design Patterns

## AMD Vitis HLS Synthesizable C++ Patterns

### Streaming IP Pattern (AXI-Stream)
```cpp
#include <ap_int.h>
#include <ap_axi_sdata.h>
#include <hls_stream.h>

typedef ap_axiu<32, 0, 0, 0> axis_pkt;

void stream_ip(
    hls::stream<axis_pkt>& in_stream,
    hls::stream<axis_pkt>& out_stream,
    int param
) {
    #pragma HLS INTERFACE axis port=in_stream
    #pragma HLS INTERFACE axis port=out_stream
    #pragma HLS INTERFACE s_axilite port=param
    #pragma HLS INTERFACE s_axilite port=return

    axis_pkt pkt;
    do {
        pkt = in_stream.read();
        // Process pkt.data
        pkt.data = pkt.data + param;
        out_stream.write(pkt);
    } while (!pkt.last);
}
```

### Memory-Mapped IP Pattern (AXI Master)
```cpp
void mem_ip(
    int* mem_in,
    int* mem_out,
    int size
) {
    #pragma HLS INTERFACE m_axi port=mem_in  offset=slave bundle=gmem0
    #pragma HLS INTERFACE m_axi port=mem_out offset=slave bundle=gmem1
    #pragma HLS INTERFACE s_axilite port=mem_in
    #pragma HLS INTERFACE s_axilite port=mem_out
    #pragma HLS INTERFACE s_axilite port=size
    #pragma HLS INTERFACE s_axilite port=return

    int local_buf[MAX_SIZE];

    // Burst read
    READ_LOOP:
    for (int i = 0; i < size; i++) {
        #pragma HLS PIPELINE II=1
        local_buf[i] = mem_in[i];
    }

    // Process
    PROC_LOOP:
    for (int i = 0; i < size; i++) {
        #pragma HLS PIPELINE II=1
        local_buf[i] = local_buf[i] * 2;
    }

    // Burst write
    WRITE_LOOP:
    for (int i = 0; i < size; i++) {
        #pragma HLS PIPELINE II=1
        mem_out[i] = local_buf[i];
    }
}
```

### Register-Based IP Pattern (AXI-Lite only)
```cpp
void reg_ip(
    int a,
    int b,
    int* result
) {
    #pragma HLS INTERFACE s_axilite port=a
    #pragma HLS INTERFACE s_axilite port=b
    #pragma HLS INTERFACE s_axilite port=result
    #pragma HLS INTERFACE s_axilite port=return

    *result = a + b;
}
```

### Dataflow Pattern
```cpp
void dataflow_ip(hls::stream<data_t>& in, hls::stream<data_t>& out) {
    #pragma HLS DATAFLOW

    hls::stream<data_t> pipe1;
    hls::stream<data_t> pipe2;
    #pragma HLS STREAM variable=pipe1 depth=16
    #pragma HLS STREAM variable=pipe2 depth=16

    stage_read(in, pipe1);
    stage_process(pipe1, pipe2);
    stage_write(pipe2, out);
}
```

### FIR Filter Pattern
```cpp
void fir(hls::stream<data_t>& in, hls::stream<data_t>& out) {
    static data_t shift_reg[NUM_TAPS];
    #pragma HLS ARRAY_PARTITION variable=shift_reg type=complete

    data_t sample = in.read();
    acc_t acc = 0;

    SHIFT_LOOP:
    for (int i = NUM_TAPS - 1; i > 0; i--) {
        #pragma HLS UNROLL
        shift_reg[i] = shift_reg[i - 1];
    }
    shift_reg[0] = sample;

    MAC_LOOP:
    for (int i = 0; i < NUM_TAPS; i++) {
        #pragma HLS UNROLL
        acc += shift_reg[i] * coeffs[i];
    }

    out.write((data_t)acc);
}
```

### Finite State Machine Pattern
```cpp
enum state_t { IDLE, LOAD, COMPUTE, STORE };

void fsm_ip(hls::stream<data_t>& in, hls::stream<data_t>& out, int cmd) {
    static state_t state = IDLE;

    switch (state) {
        case IDLE:
            if (cmd == 1) state = LOAD;
            break;
        case LOAD:
            // Read data
            state = COMPUTE;
            break;
        case COMPUTE:
            // Process
            state = STORE;
            break;
        case STORE:
            // Write results
            state = IDLE;
            break;
    }
}
```

## Common Data Type Recipes

| Use Case | Type | Example |
|----------|------|---------|
| Pixel (8-bit unsigned) | `ap_uint<8>` | `ap_uint<8> pixel;` |
| Audio sample (16-bit signed) | `ap_int<16>` | `ap_int<16> sample;` |
| Fixed-point 1.15 | `ap_fixed<16,1>` | `ap_fixed<16,1> coeff;` |
| Wide bus (512-bit) | `ap_uint<512>` | `ap_uint<512> bus_data;` |
| AXI-Stream packet | `ap_axiu<W,0,0,0>` | `ap_axiu<32,0,0,0> pkt;` |

## Interface Protocol Quick Reference

| Protocol | Pragma | Use Case |
|----------|--------|----------|
| AXI4-Lite | `#pragma HLS INTERFACE s_axilite` | Control registers, small data |
| AXI4-Stream | `#pragma HLS INTERFACE axis` | Streaming data |
| AXI4 Master | `#pragma HLS INTERFACE m_axi` | DDR/HBM access |
| No protocol | `#pragma HLS INTERFACE ap_none` | Pure wires |
| Valid/Ack | `#pragma HLS INTERFACE ap_vld` | Simple handshake |
| FIFO | `#pragma HLS INTERFACE ap_fifo` | FIFO-based data |
