# Feedback: mac_array

## Results Analysis
- **Design quality**: Excellent. The implementation maps directly from the specification's algorithm pseudocode with no improvisation required. All 16 MACs are correctly unrolled inside a pipelined K-loop achieving II=1. The DSP48 handles both multiply and accumulate natively (8x8 operands well within 25x18 signed limit), which explains the extremely low LUT footprint. Interface pragmas, data packing, TLAST protocol, and drain phase are all correctly implemented. Code follows project conventions: labeled loops, partitioned arrays, commented pragmas.
- **Verification quality**: Strong. 6 test scenarios with 8 sub-tests produce 144 individual value checks plus TLAST position and stream-empty assertions per test. The golden reference model (`golden_matmul`) provides independent C++ verification with sanity cross-checks. Coverage spans normal operation, identity, INT8 boundary extremes, accumulator stress at K=4096, zero inputs, and single-step outer product. Minor gaps: no back-to-back invocation test (verifying accumulator reset between calls) and no randomized/mixed-sign test with varying K.
- **Synthesis efficiency**: Outstanding. 16 DSP is the theoretical minimum for 16 parallel INT8 MACs. 850 LUT and 757 FF are well below estimates, because the DSP48 absorbs both multiply and accumulate, leaving only unpack wiring + control in fabric. Timing margin of 3.7 ns (37% of budget) indicates the design could run at ~150 MHz. Total device utilization under 8% across all resource types.

## Metrics vs Targets
| Metric | Target (from instruction) | Achieved | Status |
|--------|--------------------------|----------|--------|
| II (compute loop) | 1 cycle | 1 cycle | MET |
| Latency per tile | K + 16 + overhead | K + 29 cycles (13 cycles overhead) | MET |
| Throughput | 16 MACs/cycle (3.2 GOPS) | 16 MACs/cycle | MET |
| DSP | 16 | 16 | MET (exact) |
| BRAM | 0 | 0 | MET (exact) |
| LUT | 2000–2800 (estimated) | 850 (1.6%) | EXCEEDED — 66% below lower estimate |
| FF | 1500–2000 (estimated) | 757 (0.7%) | EXCEEDED — 50% below lower estimate |
| Timing | 10 ns (100 MHz) | 6.283 ns (159 MHz) | MET — 3.717 ns slack |

## Optimization Recommendation
- **Needed**: No — all targets met or exceeded
- **Priority**: N/A
- **Strategy**: The baseline is already optimal for its 4x4 configuration. Potential future directions (not optimizations of this design):
  - **Scale-up**: Increase to 8x4 (32 DSP) or 8x8 (64 DSP) for 2-4x throughput — requires a new spec, not /optimize-ip
  - **Free-running mode**: Switch to `ap_ctrl_none` for use in a streaming pipeline without per-tile host overhead
  - **DATAFLOW overlap**: Overlap drain of tile N with compute of tile N+1 for back-to-back streaming
- **Expected improvement**: N/A (baseline is sufficient)

## Lessons Learned
1. **Resource estimation for DSP-dominant INT8 designs**: The spec's 2.0-2.5x HLS infrastructure overhead multiplier was too pessimistic. When all multiply-accumulate operations map to DSP48 natively (operands ≤18 bits), the fabric datapath is nearly zero — just wiring and control. Actual LUT was 850 vs. estimated 2000-2800 (3.0x over-estimate). For pure-DSP MAC designs, a 1.0-1.2x multiplier on LUT/FF is more accurate.
2. **Spec clarity**: The data packing byte layout, TLAST protocol, and two-phase (compute/drain) structure translated directly to implementation without ambiguity. This validates the spec format for streaming MAC designs.

## Environment Upgrade Triggers
- **Trigger detected**: yes (minor)
- **Description**: The resource estimation methodology in `algorithm-analysis/SKILL.md` Section 5 applies a blanket 2.0-2.5x overhead multiplier for small designs (core LUT < 2000). For designs where the datapath is entirely absorbed by DSP48 (INT8/INT16 MACs with operands within DSP native width), this multiplier is 2-3x too high. A note could be added distinguishing "DSP-absorbed datapath" designs (multiplier ~1.0-1.2x) from "fabric datapath" designs (existing 2.0-2.5x). Low priority — the over-estimate is conservative and did not cause any incorrect decisions.
