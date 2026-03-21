# CLAUDE.md — AMD Vitis HLS IP Design & Verification Environment

## Project Overview

This is an automated environment for **FPGA IP design and verification** using the **AMD Vitis HLS** toolchain, orchestrated by Claude Code. The system enables users to specify IP requirements via Markdown instruction documents, after which Claude Code agents handle design, synthesis, simulation, optimization, and documentation generation.

## Directory Structure

```
.
├── CLAUDE.md                    # This file — top-level project reference
├── .claude/
│   ├── agents/                  # Agent definitions (main + 5 subagents)
│   │   ├── main-agent.md        # Senior HLS lead — orchestrates all subagents
│   │   ├── design-agent.md      # IP design subagent (C++ implementation)
│   │   ├── synth-sim-agent.md   # Synthesis & simulation subagent (TB + TCL)
│   │   ├── optimization-agent.md# Optimization subagent (pragmas & restructuring)
│   │   ├── docs-agent.md        # Documentation generation subagent
│   │   └── upgrade-agent.md     # Environment self-improvement subagent
│   ├── commands/
│   │   ├── design-ip.md         # /design-ip  — full IP design pipeline
│   │   ├── optimize-ip.md       # /optimize-ip — optimization pipeline
│   │   └── upgrade-env.md       # /upgrade-env — environment upgrade pipeline
│   ├── skills/
│   │   ├── hls-design.md        # HLS C++ design patterns & best practices
│   │   ├── hls-synth-sim.md     # Synthesis, C-sim, co-sim knowledge
│   │   ├── hls-optimization.md  # HLS optimization techniques reference
│   │   └── ip-documentation.md  # IP documentation standards
│   ├── rules/
│   │   ├── general.md           # General project rules
│   │   ├── hls-coding.md        # HLS C++ coding standards
│   │   └── upgrade-governance.md# Environment upgrade governance rules
│   └── upgrades/
│       ├── upgrade-log.md       # Complete upgrade history (append-only)
│       └── backups/             # Pre-upgrade file backups
├── src/                         # IP source repositories (one folder per IP)
│   └── .template/               # Template files for new IPs
├── docs/                        # Generated IP documentation
├── scripts/
│   └── templates/
│       ├── tcl/                  # TCL script templates for Vitis HLS
│       └── cpp/                  # C++ boilerplate templates
└── README.md
```

## Workflow

### Stage 1: IP Design (`/design-ip`)

1. User creates a folder under `src/<ip_name>/` and adds `instruction.md`
2. The **main agent** reads the instruction, creates a design plan
3. The **design agent** implements the IP in C++ (`src/<ip_name>/src/`)
4. The **synth-sim agent** writes testbenches and runs C-simulation & synthesis
5. The **main agent** validates all outputs
6. The **docs agent** generates documentation under `docs/<ip_name>/`
7. The **main agent** evaluates whether the session revealed environment improvement opportunities → if triggers detected, offers upgrade evaluation to the user

### Stage 2: IP Optimization (`/optimize-ip`)

1. User adds `optimization.md` to `src/<ip_name>/`
2. The **main agent** reads optimization goals, creates an optimization plan
3. The **optimization agent** applies pragmas, restructures code
4. The **synth-sim agent** re-runs synthesis and simulation to verify
5. The **main agent** validates optimized results against targets
6. The **docs agent** updates documentation with optimization details
7. The **main agent** evaluates whether the session revealed environment improvement opportunities → if triggers detected, offers upgrade evaluation to the user

### Stage 3: Environment Self-Improvement (`/upgrade-env`)

The environment improves itself through use. After each design or optimization session (or on demand), the system can propose upgrades to its own configuration:

1. The **main agent** detects upgrade triggers (repeated failures, missing knowledge, new patterns, template gaps)
2. The **upgrade agent** generates structured proposals with diffs, rationale, and rollback plans
3. The **main agent** reviews proposals for safety and correctness
4. **Proposals are presented to the user for explicit approval** — no changes are applied without user confirmation
5. Approved upgrades are applied with file backups; all decisions are logged in `.claude/upgrades/upgrade-log.md`
6. Rejected or deferred proposals are logged for future reference

**Key constraint**: The environment never modifies itself silently. Every change requires user approval and is reversible via rollback.

## IP Repository Convention

Each IP lives under `src/<ip_name>/` with this structure:

```
src/<ip_name>/
├── instruction.md       # [Required] User-written IP specification
├── optimization.md      # [Optional] User-written optimization goals
├── src/
│   ├── <ip_name>.hpp    # IP header (interfaces, types, constants)
│   └── <ip_name>.cpp    # IP implementation
├── tb/
│   └── tb_<ip_name>.cpp # Testbench
├── tcl/
│   ├── run_csim.tcl     # C-simulation script
│   ├── run_csynth.tcl   # C-synthesis script
│   └── run_cosim.tcl    # Co-simulation script
├── reports/             # Synthesis & simulation reports (auto-generated)
└── directives.tcl       # HLS directives (if using TCL-based directives)
```

## Key Conventions

- **Language**: All IP source code is **C++** (C++14 compatible with Vitis HLS)
- **Headers**: Use `#include "ap_int.h"`, `"ap_fixed.h"`, `"hls_stream.h"` as needed
- **Top function**: The HLS top-level function must match `<ip_name>` in the project config
- **Testbench**: Must return `0` on pass, non-zero on failure
- **Pragmas**: Use `#pragma HLS` inline directives for optimization
- **TCL scripts**: Use Vitis HLS Tcl commands (`open_project`, `set_top`, `open_solution`, etc.)
- **Reports**: All synthesis reports are saved to `reports/` for review

## Tool Dependencies

- AMD Vitis HLS (2023.1 or later recommended)
- C++14 compatible compiler (bundled with Vitis HLS)
- TCL interpreter (bundled with Vitis HLS)
