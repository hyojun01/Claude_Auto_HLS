---
name: design-ip
description: Full IP design pipeline — reads instruction.md, implements HLS C++ IP, writes testbenches, runs C-simulation and synthesis, generates documentation
argument-hint: <ip_name>
disable-model-invocation: true
---

# /design-ip — IP Design Pipeline

## Description
Executes the full IP design pipeline: reads the user's instruction document, designs the IP, writes testbenches, runs C-simulation and synthesis, and generates documentation.

## Usage
```
/design-ip <ip_name>
```

## Arguments
- `<ip_name>` — The name of the IP to design. Must match a folder name under `src/`. The folder must contain an `instruction.md` file.

## Prerequisites
- Directory `src/<ip_name>/` must exist
- File `src/<ip_name>/instruction.md` must be present and contain:
  - Functional description of the IP
  - I/O port specifications (names, types, directions)
  - Data types (bit widths, fixed-point formats, etc.)
  - Interface protocols (AXI-Stream, AXI-Lite, AXI-MM, ap_none, etc.)
  - Target FPGA part and clock period
  - Any specific performance requirements

## Execution Flow

### Step 1: Validate Prerequisites
- Confirm `src/<ip_name>/` exists
- Confirm `src/<ip_name>/instruction.md` exists and is non-empty
- Create subdirectories if they don't exist: `src/`, `tb/`, `tcl/`, `reports/`

### Step 2: Main Agent — Plan
**Agent**: `main-agent`
- Read `src/<ip_name>/instruction.md`
- Produce a **Design Plan** (see main-agent.md for template)
- The plan covers: requirements, architecture, implementation tasks, verification tasks, documentation tasks, and acceptance criteria

### Step 3: Design Agent — Implement
**Agent**: `design-agent`
- Receive implementation tasks from the main agent's plan
- Create `src/<ip_name>/src/<ip_name>.hpp` — header with types, constants, prototype
- Create `src/<ip_name>/src/<ip_name>.cpp` — top function implementation
- Follow HLS coding standards (`.claude/rules/hls-coding.md`)
- Include only essential pragmas (interfaces + basic correctness)

### Step 4: Main Agent — Code Review
**Agent**: `main-agent`
- Review the design agent's output for:
  - Functional correctness against the instruction
  - Synthesizability (no dynamic memory, no system calls, etc.)
  - Proper interface declarations
  - Coding standard compliance
- If issues found: provide feedback and return to Step 3 (max 3 iterations)

### Step 5: Synth-Sim Agent — Testbench & Simulation
**Agent**: `synth-sim-agent`
- Write `src/<ip_name>/tb/tb_<ip_name>.cpp` — testbench with comprehensive test cases
- Generate TCL scripts:
  - `src/<ip_name>/tcl/run_csim.tcl`
  - `src/<ip_name>/tcl/run_csynth.tcl`
  - `src/<ip_name>/tcl/run_cosim.tcl`
- Execute C-simulation (if Vitis HLS is available in the environment)
- Execute C-synthesis (if Vitis HLS is available in the environment)
- Collect and format synthesis reports

### Step 6: Main Agent — Verify Results
**Agent**: `main-agent`
- Verify C-simulation passed (return code = 0)
- Review synthesis report for timing, resource usage, and latency
- Confirm results meet the acceptance criteria from the design plan
- If verification fails: identify root cause and iterate (Steps 3–5, max 3 times)

### Step 7: Docs Agent — Generate Documentation
**Agent**: `docs-agent`
- Create `docs/<ip_name>/README.md` — full IP documentation
- Create `docs/<ip_name>/integration_guide.md` — integration instructions
- Create `docs/<ip_name>/changelog.md` — initial version entry
- Include actual synthesis numbers in performance tables

### Step 8: Main Agent — Final Review
**Agent**: `main-agent`
- Review documentation for completeness and accuracy
- Confirm all files are in their correct locations
- Produce a summary for the user

### Step 9: Environment Upgrade Evaluation (Conditional)
**Agent**: `main-agent` → `upgrade-agent`
- The main agent reviews the entire design session for upgrade triggers:
  - Did any iteration fail due to a gap in skills, rules, or templates?
  - Did a subagent improvise a pattern not captured in the environment?
  - Did synthesis reveal a previously unknown constraint or best practice?
- If **no triggers** detected: skip this step, report design complete
- If **triggers detected**:
  1. Inform the user: "The design session revealed potential environment improvements. Would you like to evaluate upgrades?"
  2. If user accepts: delegate to the **upgrade agent** to generate proposals
  3. The main agent reviews proposals for correctness and compliance with upgrade governance rules
  4. Present reviewed proposals to the user for approval (see `/upgrade-env` for approval flow)
  5. Apply only user-approved upgrades; back up files before modification
  6. Log all decisions (applied, skipped, rejected) in `.claude/upgrades/upgrade-log.md`
  7. If user declines: log the detected triggers for future reference and proceed

## Output Summary
After successful execution, the following files should exist:

```
src/<ip_name>/
├── instruction.md          (user-provided, unchanged)
├── src/
│   ├── <ip_name>.hpp
│   └── <ip_name>.cpp
├── tb/
│   └── tb_<ip_name>.cpp
├── tcl/
│   ├── run_csim.tcl
│   ├── run_csynth.tcl
│   └── run_cosim.tcl
└── reports/                (populated after synthesis)

docs/<ip_name>/
├── README.md
├── integration_guide.md
└── changelog.md
```

## Error Handling
- If `instruction.md` is missing or empty: abort and inform the user
- If code review fails after 3 iterations: report the issues and ask the user for clarification
- If synthesis fails: report the error log and suggest fixes
- If Vitis HLS is not available: generate all files but skip actual execution of TCL scripts; note in the output that manual execution is required
