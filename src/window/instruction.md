# IP Instruction Template: <IP window>

## 1. Functional Description
<!-- Describe what this IP does in plain language -->
256-size Hamming Window

## 2. I/O Ports

| Port Name | Direction | Data Type | Bit Width | Interface Protocol | Description |
|-----------|-----------|-----------|-----------|-------------------|-------------|
| in | IN | ap_uint<32> | 32 | AXI-Stream (axis) | Input data stream |
| out | OUT | ap_uint<32> | 32 | AXI-Stream (axis) | Output data stream |

## 3. Data Types
<!-- Define custom types, fixed-point formats, struct definitions -->
Input and Output Data is 32-bit unsigned int which is concated with 16-bit signed fixed-point real and 16-bit signed fixed-point imag.
Real and image Data Type: ap_fixed<16,1>.
Coefficient Data Type: "choose proper data type".
Accumulated Data Type: "choose proper data type", This is the data type used to prevent overflow during the computation process.

## 4. Algorithm / Processing
<!-- Describe the algorithm, math, or processing steps -->
Apply Hamming Windowing Algorithm.
IP use DATAFLOW optimization. Windowing must be performed on a packet basis with a size of 256.

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
The sampling rate is 100 MHz.
Simulate a single-tone sine wave and provide it as the input to the IP.
Verify by comparing the golden reference values with the IP output.

## 7. Additional Notes
<!-- Any other constraints, requirements, or preferences -->
Compute the window coefficients directly using a tool such as Python, and store them in internal memory.
It must be applicable to input samples arriving in real time according to the clock in the signal processing domain.
First, achieve the basic functionality of the Hamming window.
