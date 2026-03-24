# CLAUDE.md — AMD Vitis HLS IP Design & Verification Environment

Automated FPGA IP design and verification using AMD Vitis HLS, orchestrated by Claude Code agents. Users specify IP requirements in Markdown; agents handle design, synthesis, simulation, optimization, and documentation.

## Project Layout

- `src/<ip_name>/` — IP source directories (one per IP)
- `src/.template/` — instruction.md / optimization.md templates
- `docs/` — Generated IP documentation
- `mailbox/` — Async inter-session communication (queue → approved → results → feedback)
- `scripts/` — Report parser, scaffolding, TCL/C++/mailbox templates
- `.claude/agents/` — 7 agents (main + 6 subagents)
- `.claude/commands/` — User-facing slash commands
- `.claude/skills/` — Domain knowledge (HLS design, optimization, synthesis, etc.)
- `.claude/rules/` — Project rules (auto-loaded)
- `.claude/upgrades/` — Upgrade log and backups

## Commands

| Command | Purpose |
|---------|---------|
| `/design-ip <ip_name>` | Design, implement, simulate, and synthesize an IP |
| `/optimize-ip <ip_name>` | Optimize an existing IP against performance targets |
| `/generate-spec` | Interactive spec generation with domain expert agent |
| `/review-results <ip_name>` | Analyze synthesis results and propose next steps |
| `/upgrade-env` | Propose and apply environment improvements |

## How to Run Vitis HLS

Execute TCL scripts with `vitis-run`, **not** `vitis_hls`:
```bash
vitis-run --tcl src/<ip_name>/tcl/run_csim.tcl --work_dir src/<ip_name>
vitis-run --tcl src/<ip_name>/tcl/run_csynth.tcl --work_dir src/<ip_name>
```

Parse synthesis reports:
```bash
python3 scripts/parse_hls_report.py parse src/<ip_name>/proj_<ip_name>/sol1/syn/report/csynth.xml
python3 scripts/parse_hls_report.py check src/<ip_name>/proj_<ip_name>/sol1/syn/report/csynth.xml --target-ii 1
```

## IP Directory Structure

Each IP lives under `src/<ip_name>/`:
```
src/<ip_name>/
├── instruction.md          # [Required] User-written spec
├── optimization.md         # [Optional] Optimization goals
├── src/<ip_name>.{hpp,cpp} # IP source
├── tb/tb_<ip_name>.cpp     # Testbench (returns 0 on pass)
├── tcl/run_{csim,csynth,cosim}.tcl
└── reports/                # Auto-generated synthesis reports
```

## Key Rules

See `.claude/rules/` for full details. Critical highlights not covered elsewhere:
- **Templates**: Always read `src/.template/` and `scripts/templates/` before generating new files
- **Naming consistency**: IP name = folder name = top function name = file names
