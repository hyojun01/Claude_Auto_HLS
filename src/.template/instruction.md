# IP Instruction Template: <IP Name>

## 1. Functional Description
<!-- Describe what this IP does in plain language -->

## 2. I/O Ports

| Port Name | Direction | Data Type | Bit Width | Interface Protocol | Description |
|-----------|-----------|-----------|-----------|-------------------|-------------|
| example_in | IN | ap_uint<32> | 32 | AXI-Stream (axis) | Input data stream |
| example_out | OUT | ap_uint<32> | 32 | AXI-Stream (axis) | Output data stream |
| ctrl_reg | IN | int | 32 | AXI-Lite (s_axilite) | Control parameter |

## 3. Data Types
<!-- Define custom types, fixed-point formats, struct definitions -->

## 4. Algorithm / Processing
<!-- Describe the algorithm, math, or processing steps -->

## 5. Target Configuration

| Parameter | Value |
|-----------|-------|
| FPGA Part | xc7z020clg400-1 |
| Clock Period | 10 ns (100 MHz) |
| Target Latency | (optional) |
| Target II | (optional) |
| Target Throughput | (optional) |

## 6. Test Scenarios
<!-- Describe test cases: typical inputs, edge cases, expected outputs -->

## 7. Additional Notes
<!-- Any other constraints, requirements, or preferences -->
