---
name: generate-spec
description: IP specification generation pipeline — systematic algorithm analysis, FPGA architecture planning, and engineering trade-off evaluation to produce HLS-ready instruction.md
argument-hint: <domain> [ip_name]
disable-model-invocation: true
---

# /generate-spec — IP Specification Generation Pipeline

## Description
Generates a complete, HLS-ready IP specification (instruction.md) through systematic algorithm analysis, FPGA architecture planning, and engineering trade-off evaluation. The spec must be self-contained so that Session B (/design-ip) can implement it without ambiguity.

## Usage
```
/generate-spec <domain> [ip_name]
```

## Arguments
- `<domain>`: dsp | comm | radar | image | dl
- `[ip_name]`: (optional) specific IP from the domain catalog

## Prerequisites
- `.claude/skills/domain-catalog/SKILL.md` must exist
- `src/.template/instruction.md` must exist
- `scripts/templates/mailbox/proposal.md` must exist

## Execution Flow

### Step 1: Domain & IP Selection
**Agent**: `spec-generator-agent`
- If `ip_name` is not provided:
  - Read `.claude/skills/domain-catalog/SKILL.md`
  - Present the selected domain's IP catalog to the user
  - Ask the user to select an IP
- If `ip_name` is provided: confirm it exists in the domain catalog

### Step 2: Algorithm Analysis
**Agent**: `spec-generator-agent`
**Skill**: `.claude/skills/algorithm-analysis/SKILL.md`
- Analyze the selected algorithm's computational structure:
  - Identify core operations (multiply-accumulate, butterfly, rotation, comparison, etc.)
  - Map the computational graph: data dependencies, parallelism potential, pipeline stages
  - Determine memory access patterns (streaming, random, reuse distance)
  - Evaluate algorithm variants and select the most FPGA-suitable one with rationale
- Perform numerical precision analysis:
  - Determine minimum bit widths from SNR/accuracy requirements
  - Size accumulators to prevent overflow across the full computation
  - Identify fixed-point boundary value risks (e.g., ap_fixed overflow at ±1.0)
- Estimate computational complexity → expected resource profile (DSP count, BRAM for buffers/LUTs, etc.)

### Step 3: FPGA System Architecture Planning
**Agent**: `spec-generator-agent`
**Skill**: `.claude/skills/fpga-system-design/SKILL.md`
- Select interface protocols for each port with engineering rationale:
  - Apply the interface selection decision tree (streaming vs. memory-mapped vs. register)
  - Determine data packing format for each interface
- Design the internal data flow architecture:
  - Choose pattern: pure streaming, batch (load-compute-store), or DATAFLOW pipeline
  - Identify pipeline stages and inter-stage buffering requirements
  - Determine if the IP requires persistent state (static variables) across invocations
- Perform resource budget estimation:
  - Count DSP operations per sample/cycle
  - Size BRAM for LUTs, buffers, FIFOs
  - Estimate LUT/FF for control logic and data path
  - Add interface overhead (AXI-Lite ~230 LUT, AXI-Stream ~10 LUT, AXI-MM ~600 LUT)
- Set realistic performance targets:
  - Target II based on algorithm structure and resource constraints
  - Target latency based on pipeline depth
  - Target throughput = f_clk / II

### Step 4: Generate Proposal & Draft Instruction
**Agent**: `spec-generator-agent`
- Read templates:
  - `scripts/templates/mailbox/proposal.md` for proposal format
  - `src/.template/instruction.md` for instruction format
  - `src/.template/optimization.md` for optimization format (if applicable)
- Generate `proposal.md` incorporating the analysis from Steps 2–3:
  - Domain, rationale, specification summary, optimization strategy, dependencies
- Generate draft `instruction.md` with all sections filled:
  - Functional description grounded in algorithm analysis (Step 2)
  - I/O ports with protocols justified by system architecture (Step 3)
  - Data types with bit widths derived from precision analysis (Step 2)
  - Algorithm description with FPGA-specific implementation notes
  - Resource estimates broken down into core logic + interface overhead (Step 3)
  - Test scenarios covering normal operation, boundary values, and edge cases
- Write both files to `mailbox/queue/<ip_name>/`

### Step 5: User Review
**Agent**: `spec-generator-agent`
- Present the proposal to the user for review
- Highlight key engineering decisions and trade-offs made in Steps 2–3
- Wait for user decision: approve / reject / modify

### Step 6: Finalize
**Agent**: `spec-generator-agent`
- On approval: user copies approved files from `queue/` to `mailbox/approved/<ip_name>/`
- On modification: revise the instruction per user feedback and return to Step 5
- On rejection: log the reason and return to Step 1

## Quality Checklist
Before presenting the proposal (Step 5), verify:
- [ ] All data types use concrete Vitis HLS types (ap_fixed<W,I>, ap_uint<N>), not vague descriptions
- [ ] Accumulator widths are sized to prevent overflow (data + coeff + ceil(log2(N)) bits)
- [ ] Fixed-point boundary values are identified and clamping rules specified
- [ ] Interface protocols are justified (not just defaulted to AXI-Stream)
- [ ] Resource estimates are broken into core logic + interface overhead
- [ ] At least 3 test scenarios: normal, boundary/edge, and stress/sweep
- [ ] Performance targets are realistic for the target FPGA and clock
- [ ] The spec is self-contained: Session B should not need to ask questions
