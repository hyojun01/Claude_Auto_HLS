# Environment Upgrade Log

This log tracks all environment upgrades — applied, rejected, and rolled back.
Entries are append-only. Never delete or modify existing entries.

---

<!-- Upgrade entries will be appended below this line -->
<!-- Format:
## [UPG-XXXX] <title> — <date>
- **Status**: Applied / Rejected / Rolled Back
- **Trigger**: <what prompted this>
- **Target**: <file path>
- **Category**: Additive skill / Rule refinement / Agent update / Command mod / Template update / Settings change / Structural
- **Priority**: HIGH / MEDIUM / LOW
- **Session**: <ip_name> / <stage>
- **Summary**: <one-line description>
- **Approved by**: User
- **Backup**: `.claude/upgrades/backups/<filename>_<timestamp>.bak`
- **Rollback**: <instructions or "N/A">
-->

## [UPG-0001] Add "Wide Cast in MAC Loop" bottleneck pattern — 2026-03-21
- **Status**: Applied
- **Trigger**: During `fir` optimization, explicit `(acc_t)` casts caused 76 DSPs instead of 23. Removing casts reduced DSP by 70% and improved all metrics.
- **Target**: `.claude/skills/hls-optimization.md`
- **Category**: Additive skill
- **Priority**: HIGH
- **Session**: fir / optimize-ip
- **Summary**: Added "### 6. Excessive DSP Usage from Wide Casts in MAC Loops" to Common Bottleneck Patterns with BAD/GOOD code examples and evidence from the fir IP.
- **Approved by**: User
- **Backup**: `.claude/upgrades/backups/hls-optimization.md_20260321_*.bak`
- **Rollback**: Remove section "### 6. Excessive DSP Usage from Wide Casts in MAC Loops" from `.claude/skills/hls-optimization.md`

## [UPG-0002] Add ap_fixed boundary value overflow pitfall — 2026-03-22
- **Status**: Applied
- **Trigger**: During `fft` design, `tw_real[0] = 1.0` silently wrapped to `-1.0` in `ap_fixed<16,1>` due to default `AP_WRAP` overflow mode, inverting all stage-0 butterfly outputs. Bug was only caught by C-simulation.
- **Target**: `.claude/rules/hls-coding.md`
- **Category**: Rule refinement
- **Priority**: HIGH
- **Session**: fft / design-ip
- **Summary**: Added "### Common Pitfalls" section to HLS coding standards with rules for `ap_fixed` boundary value initialization, clamping, and `AP_SAT` usage.
- **Approved by**: User
- **Backup**: `.claude/upgrades/backups/hls-coding.md_20260322_*.bak`
- **Rollback**: Remove "### Common Pitfalls" section (between "Forbidden Constructs" and "Allowed Constructs") from `.claude/rules/hls-coding.md`
