---
name: optimize-ip
description: IP optimization pipeline — reads optimization.md, applies HLS pragmas and code restructuring, re-verifies correctness, updates documentation
argument-hint: <ip_name>
disable-model-invocation: true
---

# /optimize-ip — IP Optimization Pipeline

## Description
Applies optimizations to an already-designed IP: reads the user's optimization goals, applies HLS pragmas and code restructuring, re-verifies correctness, and updates documentation.

## Usage
```
/optimize-ip <ip_name>
```

## Arguments
- `<ip_name>` — The name of the IP to optimize. Must be an IP that has already been designed via `/design-ip`.

## Prerequisites
- The IP must have been designed already (all design-stage files present)
- File `src/<ip_name>/optimization.md` must be present and contain:
  - Target latency, initiation interval, or throughput
  - Resource budget constraints (optional)
  - Acceptable trade-offs (e.g., "can use 2x more DSPs to halve latency")
  - Specific optimization requests (optional, e.g., "pipeline the main loop")
  - Priority ranking if multiple goals conflict

## Execution Flow

### Step 1: Validate Prerequisites
- Confirm `src/<ip_name>/` exists with design-stage files
- Confirm `src/<ip_name>/optimization.md` exists and is non-empty
- Confirm baseline synthesis reports exist in `src/<ip_name>/reports/`
  - If not, run C-synthesis first to establish baseline

### Step 2: Main Agent — Optimization Plan
**Agent**: `main-agent`
- Read `src/<ip_name>/optimization.md`
- Read current synthesis reports to establish baseline metrics
- Read the existing source code to understand current structure
- Produce an **Optimization Plan** (see main-agent.md for template)
- The plan covers: current baseline, optimization goals, strategy, re-verification, documentation update, acceptance criteria

### Step 3: Backup Current Design
- Copy existing source files to `src/<ip_name>/src/<ip_name>_pre_opt.hpp` and `.cpp`
- This preserves the baseline for comparison and rollback

### Step 4: Optimization Agent — Apply Optimizations
**Agent**: `optimization-agent`
- Receive the optimization strategy from the main agent's plan
- Modify `src/<ip_name>/src/<ip_name>.hpp` and `<ip_name>.cpp`
- Apply pragmas: PIPELINE, UNROLL, ARRAY_PARTITION, DATAFLOW, etc.
- Restructure code if needed (loop transformations, function splitting for DATAFLOW, etc.)
- Produce a **Change Log** documenting every modification

### Step 5: Main Agent — Optimization Review
**Agent**: `main-agent`
- Review the optimization agent's changes for:
  - Functional equivalence preservation
  - Correct pragma syntax and placement
  - Sensible optimization strategy (no conflicting pragmas)
  - Code still meets synthesizability requirements
- If issues found: provide feedback and return to Step 4 (max 3 iterations)

### Step 6: Synth-Sim Agent — Re-Verify
**Agent**: `synth-sim-agent`
- Update TCL scripts if needed (e.g., new solution name `sol_opt`)
- Re-run C-simulation to confirm functional equivalence
- Re-run C-synthesis to measure optimized metrics
- Produce a **before/after comparison** table:

```
| Metric     | Baseline | Optimized | Target   | Met? |
|------------|----------|-----------|----------|------|
| Latency    | ...      | ...       | ...      | Y/N  |
| II         | ...      | ...       | ...      | Y/N  |
| BRAM       | ...      | ...       | ...      | Y/N  |
| DSP        | ...      | ...       | ...      | Y/N  |
| FF         | ...      | ...       | ...      | Y/N  |
| LUT        | ...      | ...       | ...      | Y/N  |
```

### Step 7: Main Agent — Validate Results
**Agent**: `main-agent`
- Verify C-simulation still passes (return code = 0)
- Compare optimized metrics against targets in `optimization.md`
- Determine if ALL targets are met:
  - **All met** → proceed to documentation
  - **Partially met** → report which targets are met/unmet, ask user if acceptable or iterate
  - **None met / regression** → analyze why, provide feedback, iterate (Steps 4–6, max 3 times)

### Step 8: Docs Agent — Update Documentation
**Agent**: `docs-agent`
- Update `docs/<ip_name>/README.md`:
  - Update performance tables with optimized numbers
  - Add entry to "Optimization History" table
  - Update architecture description if structure changed
- Update `docs/<ip_name>/changelog.md`:
  - Add new version entry with all optimizations applied and before/after metrics
- Update `docs/<ip_name>/integration_guide.md` if interfaces changed

### Step 9: Main Agent — Final Review
**Agent**: `main-agent`
- Review updated documentation
- Confirm all optimized files are in place
- Produce a final summary for the user including:
  - Before/after performance comparison
  - Optimizations applied
  - Any targets not met and recommendations

### Step 10: Environment Upgrade Evaluation (Conditional)
**Agent**: `main-agent` → `upgrade-agent`
- The main agent reviews the entire optimization session for upgrade triggers:
  - Did the optimization agent apply a technique not documented in `hls-optimization.md`?
  - Did re-synthesis reveal a pattern worth capturing (e.g., a pragma combination that consistently works)?
  - Were there repeated iterations caused by a gap in the optimization strategy reference?
  - Did the before/after comparison reveal unexpected resource trade-offs worth documenting?
- If **no triggers** detected: skip this step, report optimization complete
- If **triggers detected**:
  1. Inform the user: "The optimization session revealed potential environment improvements. Would you like to evaluate upgrades?"
  2. If user accepts: delegate to the **upgrade agent** to generate proposals
  3. The main agent reviews proposals for correctness and compliance with upgrade governance rules
  4. Present reviewed proposals to the user for approval (see `/upgrade-env` for approval flow)
  5. Apply only user-approved upgrades; back up files before modification
  6. Log all decisions (applied, skipped, rejected) in `.claude/upgrades/upgrade-log.md`
  7. If user declines: log the detected triggers for future reference and proceed

## Output Summary
After successful execution:

```
src/<ip_name>/
├── instruction.md               (unchanged)
├── optimization.md              (user-provided, unchanged)
├── src/
│   ├── <ip_name>.hpp            (optimized)
│   ├── <ip_name>.cpp            (optimized)
│   ├── <ip_name>_pre_opt.hpp    (backup of baseline)
│   └── <ip_name>_pre_opt.cpp    (backup of baseline)
├── tb/
│   └── tb_<ip_name>.cpp         (may be updated)
├── tcl/
│   ├── run_csim.tcl
│   ├── run_csynth.tcl
│   └── run_cosim.tcl
└── reports/                     (updated with optimized results)

docs/<ip_name>/
├── README.md                    (updated)
├── integration_guide.md         (updated if needed)
└── changelog.md                 (new version added)
```

## Error Handling
- If `optimization.md` is missing: abort and inform the user
- If baseline design files are missing: instruct user to run `/design-ip` first
- If functional regression detected (C-sim fails): roll back to pre-optimization backup and report
- If optimization targets can't be met after 3 iterations: report the best achieved results and suggest alternative approaches to the user
- If Vitis HLS is not available: apply code changes but skip actual TCL execution; note in the output that manual execution is required
