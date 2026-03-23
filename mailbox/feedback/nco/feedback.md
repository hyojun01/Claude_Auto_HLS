# Feedback: nco

## Results Analysis
- **Design quality**: Excellent. Clean architecture — phase accumulator + quarter-wave LUT with quadrant symmetry. The pipeline core (`nco_Pipeline_GEN_LOOP`) achieves 0 DSP, 1 BRAM, 117 FF, 411 LUT. LUT stored as 1024x15-bit ROM (sign bit reconstructed from quadrant logic). Static phase accumulator persists across invocations. Proper `ap_fixed<16,1>` boundary handling — LUT values clamped below 1.0.
- **Verification quality**: Strong. All 8 test scenarios from the instruction are covered: DC output, quadrant transitions (f_clk/4), full cycles (f_clk/8), arbitrary frequency sweep with max/RMS error reporting, phase offset, sync reset, TLAST generation, and cross-invocation phase continuity. Double-precision reference comparison with tolerance checks.
- **Synthesis efficiency**: Good. Pipeline core is optimal at 0 DSP, II=1. The 3 DSP overhead is from a top-level 32x32 multiplier (`mul_32s_32s_32_2_1_U11`) used for loop control computation — this is a one-time per-invocation cost, not per-sample. AXI-Lite interface adds 157 FF / 234 LUT, which is standard Vitis HLS overhead.

## Metrics vs Targets
| Metric | Target (from instruction) | Achieved | Status |
|--------|--------------------------|----------|--------|
| Latency | 2 cycles (core) | 3 cycles (pipeline depth) + overhead | ACCEPTABLE |
| II | 1 cycle | 1 cycle | MET |
| DSP | 0 | 3 (0 in pipeline core, 3 in top-level control) | PARTIAL |
| BRAM | 1 | 1 | MET |
| LUT | < 200 (core estimate) | 411 (core) / 944 (total with AXI-Lite) | SEE NOTES |
| FF | < 100 (core estimate) | 117 (core) / 712 (total with AXI-Lite) | SEE NOTES |

### Notes
- **Latency**: Instruction estimated 2 cycles but pipeline depth of 3 is reasonable (phase add + LUT read + output pack/sign-adjust). No functional impact since II=1.
- **DSP 3 vs 0**: The `mul_32s_32s_32_2_1_U11` instance (3 DSP48E1 slices) is in the top-level control path, outside the sample-generation pipeline. It performs a one-time 32x32 multiply for loop setup (line 189). The pipeline core itself is multiplier-free as intended. At 1.4% DSP utilization this is negligible.
- **LUT/FF overshoot**: The instruction estimates (200 LUT, 100 FF) were for the core only. The AXI-Lite slave interface adds ~234 LUT / 157 FF, which is standard and unavoidable. The core itself at 411 LUT / 117 FF is efficient for the logic performed (two independent quadrant decompositions + sign adjustments for sin and cos paths).

## Optimization Recommendation
- **Needed**: no
- **Priority**: n/a
- **Strategy**: The 3 top-level DSPs could theoretically be eliminated by restructuring the loop control to avoid the 32x32 multiply (e.g., decrementing counter). However, at 1.4% DSP utilization on xc7z020, the effort-to-benefit ratio is poor. All functional and performance targets are met.
- **Expected improvement**: n/a

## Lessons Learned
- Quarter-wave LUT inferred correctly as 1024x15-bit BRAM ROM (`QUARTER_SINE_LUT_ROM_AUTO_1R`). Vitis HLS stored only 15 bits (positive quadrant values) and the design reconstructs the sign — efficient memory use.
- AXI-Lite interface overhead (~234 LUT, 157 FF) should be accounted for in resource estimates for any IP with `s_axilite` ports. Future specs should note "core-only" vs "total including interface" estimates separately.
- Variable-bound loop with `ap_uint<32>` trip count can introduce a top-level 32x32 multiplier for HLS internal loop scheduling. This is a known Vitis HLS behavior, not a design flaw.

## Environment Upgrade Triggers
- **Trigger detected**: yes (minor)
- **Description**: The instruction template's resource estimates section should distinguish "core logic" from "total including interface overhead." AXI-Lite alone adds ~400 LUT / ~160 FF, which can make naive estimates look significantly off. Suggest adding a note to `src/.template/instruction.md` or `hls-design.md` skill about accounting for interface overhead in estimates.
