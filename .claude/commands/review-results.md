# /review-results — Review HLS Build Results

## Description
Reviews synthesis and simulation results from Session B, analyzes whether targets were met, provides engineering feedback, and generates optimization proposals when further improvement is needed or possible.

## Usage
```
/review-results <ip_name>
```

## Arguments
- `<ip_name>`: The IP whose results are to be reviewed. Must have results in `mailbox/results/<ip_name>/`.

## Prerequisites
- `mailbox/results/<ip_name>/status.md` must exist
- `mailbox/results/<ip_name>/synthesis_summary.md` must exist
- `scripts/templates/mailbox/feedback.md` must exist

## Execution Flow

### Step 1: Collect Results
**Agent**: `spec-generator-agent`
- Read `mailbox/results/<ip_name>/status.md` — verify state is `complete` or `failed`
- Read `mailbox/results/<ip_name>/synthesis_summary.md` — extract all metrics
- Read `mailbox/results/<ip_name>/issues.md` (if exists) — note any problems encountered
- Read the original `src/<ip_name>/instruction.md` — recall the original targets

### Step 2: Engineering Analysis of Results
**Agent**: `spec-generator-agent`
**Skill**: `.claude/skills/algorithm-analysis.md`, `.claude/skills/fpga-system-design.md`
- Compare actual synthesis metrics against instruction targets:
  - Timing: did estimated clock meet the target period? How much slack?
  - Latency & II: achieved vs. target, identify bottleneck loops if II > target
  - Resources: DSP, BRAM, LUT, FF usage vs. estimates and device capacity
- Perform root cause analysis for any missed targets:
  - DSP overuse → check operand widths vs. DSP48 native width, unnecessary casts
  - BRAM overuse → check PIPO buffers, array storage, LUT-vs-BRAM threshold
  - II violation → check memory port conflicts, loop-carried dependencies
  - Timing violation → check critical path (wide fabric multiplies, deep logic chains)
- Assess whether the implementation matches the intended algorithm architecture:
  - Did the design agent choose the right data flow pattern?
  - Are the interface protocols correctly implemented?
  - Is the numerical precision sufficient (check testbench error reports)?

### Step 3: Write Feedback
**Agent**: `spec-generator-agent`
- Read `scripts/templates/mailbox/feedback.md` for feedback format
- Write `mailbox/feedback/<ip_name>/feedback.md` containing:
  - Target-vs-actual comparison table
  - Root cause analysis for any gaps
  - Assessment of implementation quality
  - Recommendation: finalize / optimize / redesign

### Step 4: Generate Optimization Proposal (if needed)
**Agent**: `spec-generator-agent`
**Skill**: `.claude/skills/algorithm-analysis.md`, `.claude/skills/fpga-system-design.md`
- If optimization is needed (targets not met, or significant improvement possible):
  a. Formulate an optimization strategy grounded in root cause analysis (Step 2):
     - Map each missed target to a specific optimization technique
     - Predict the expected improvement with engineering rationale
     - Identify trade-offs (e.g., "DSP ↓ but LUT ↑")
  b. Read `src/.template/optimization.md` for optimization format
  c. Read `scripts/templates/mailbox/proposal.md` for proposal format
  d. Generate `proposal.md` (optimization) with:
     - Specific metrics from synthesis_summary.md (e.g., "Current DSP: 80, Target: ≤30")
     - Concrete optimization actions (e.g., "Narrow acc_t from 45 to 25 bits")
     - Expected outcome per action with reasoning
  e. Generate draft `optimization.md` with all sections filled:
     - Current baseline populated from actual synthesis results
     - Optimization goals with concrete current → target numbers
     - Trade-offs stated explicitly
     - Constraints from the original instruction and device capacity
  f. Write both files to `mailbox/queue/<ip_name>/` for user approval
- If no optimization needed: mark the IP as finalized in feedback

### Step 5: Present to User
**Agent**: `spec-generator-agent`
- Present the feedback summary to the user
- If optimization was proposed: present the proposal and draft optimization.md
- Wait for user decision on the optimization proposal (approve / reject / modify)

### Step 6: Environment Upgrade Evaluation (Conditional)
**Agent**: `spec-generator-agent`
- Review the design/optimization session for patterns worth capturing:
  - Did Session B encounter repeated failures that indicate a skills/rules gap?
  - Did the synthesis reveal a new optimization pattern not in `hls-optimization.md`?
  - Did the instruction.md cause ambiguity that required Session B to improvise?
- If triggers detected: note them in the feedback for future `/upgrade-env` consideration

## Quality Checklist
Before presenting feedback (Step 5), verify:
- [ ] All metrics cite actual numbers from synthesis_summary.md, not estimates
- [ ] Root cause analysis is specific (not just "resources are high")
- [ ] Optimization actions map directly to identified root causes
- [ ] Trade-offs are stated explicitly for each proposed optimization
- [ ] Optimization targets are achievable (not aspirational) based on engineering analysis
- [ ] The optimization.md is self-contained: Session B should not need to ask questions
