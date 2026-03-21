# Project Settings

## Default Configuration
These defaults are used when the user's `instruction.md` does not specify a value.

| Setting | Default Value | Description |
|---------|---------------|-------------|
| FPGA Part | `xc7z020clg400-1` | Zynq-7000 (PYNQ-Z2) |
| Clock Period | `10` ns (100 MHz) | Default clock constraint |
| Flow Target | `vivado` | Vitis HLS flow target |
| C++ Standard | C++14 | HLS-compatible C++ standard |
| Solution Name | `sol1` | Baseline solution name |
| Opt Solution | `sol_opt` | Optimized solution name |

## Supported FPGA Families

Agents should accept any valid Xilinx/AMD part number. Common choices:

- **Zynq-7000**: `xc7z010clg400-1`, `xc7z020clg400-1`, `xc7z045ffg900-2`
- **Zynq UltraScale+**: `xczu3eg-sbva484-1-i`, `xczu9eg-ffvb1156-2-e`
- **Alveo**: `xcu250-figd2104-2L-e`, `xcu280-fsvh2892-2L-e`
- **Versal**: `xcvc1902-vsva2197-2MP-e-S`

## Vitis HLS Version Compatibility
- Minimum: Vitis HLS 2022.2
- Recommended: Vitis HLS 2023.1+
- TCL command syntax follows the unified Vitis HLS format (not legacy Vivado HLS)
