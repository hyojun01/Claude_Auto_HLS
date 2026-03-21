# Environment Upgrade Rules

## Core Principle

The automated IP design environment is a living system that improves through use. However, all improvements must be **controlled, reversible, and user-approved**.

## Governance Rules

### 1. User Confirmation Is Mandatory
- **No environment file may be modified without explicit user approval**
- The user must see the exact proposed change (diff) before approving
- Batch approval ("approve all") is allowed but must be explicitly offered
- Silence is not consent — if the user does not respond, no upgrade is applied
- The user may approve, skip, modify, or reject any proposal

### 2. Backup Before Every Change
- Before modifying any file, create a backup at `.claude/upgrades/backups/<filename>_<timestamp>.bak`
- Backups are never automatically deleted
- The rollback command must be able to restore any backed-up version

### 3. Upgrade Scope Limits
- Each proposal targets exactly **one file**
- Each proposal addresses exactly **one concern**
- Proposals must not exceed **50 lines of additions** per change (excluding rollback instructions)
- Larger changes must be split into multiple sequential proposals
- Changes to `CLAUDE.md` require extra scrutiny as it affects the entire environment

### 4. Backward Compatibility
- Upgrades must not break existing IP designs
- New rules, patterns, or standards are **additive** unless explicitly deprecating old ones
- If a rule is modified (not just added), all existing IPs that may be affected must be listed
- Deprecations must include a migration path

### 5. Evidence-Based Only
- Every proposal must cite a specific design/optimization session as evidence
- Speculative improvements ("this might help in the future") are not allowed
- The evidence must demonstrate a clear gap, failure, or missed opportunity

### 6. No Self-Referential Upgrades
- The upgrade agent must not propose changes to its own agent definition (`.claude/agents/upgrade-agent.md`)
- Changes to upgrade-related files (upgrade rules, upgrade command, upgrade log) require **double confirmation**: the main agent must explicitly endorse the proposal before presenting it to the user

### 7. Logging Is Non-Negotiable
- Every applied upgrade is logged in `.claude/upgrades/upgrade-log.md`
- Every rejected or skipped proposal is also logged (with reason)
- Every rollback is logged
- The log is append-only — entries are never deleted or modified

### 8. Rate Limiting
- Maximum **5 upgrade proposals per session** (per design or optimization run)
- If more than 5 gaps are identified, the upgrade agent prioritizes the top 5 and notes the remainder for future evaluation
- This prevents upgrade fatigue and keeps the user's approval burden manageable

### 9. Conflict Resolution
- If a proposed upgrade conflicts with a previous upgrade, the newer one must explicitly state the conflict and propose a resolution
- If a proposed upgrade conflicts with a user customization, the user's customization takes precedence — the proposal must work around it or be withdrawn
- The main agent is the final arbiter of proposal quality before it reaches the user

### 10. Upgrade Categories and Approval Thresholds

| Category | Description | Approval Required |
|----------|-------------|-------------------|
| **Additive skill** | New pattern, technique, or reference added to a skill file | Standard (user confirms) |
| **Rule refinement** | Clarification or tightening of an existing rule | Standard (user confirms) |
| **Agent prompt update** | Change to an agent's responsibilities, templates, or checklists | Enhanced (show full before/after) |
| **Command modification** | Change to a pipeline step or flow | Enhanced (show full before/after) |
| **Template update** | Change to a TCL or C++ template | Standard (user confirms) |
| **Settings change** | Change to defaults or supported configurations | Standard (user confirms) |
| **Structural change** | New files, new directories, or file reorganization | Enhanced + main agent endorsement |

## Upgrade Evaluation Timing

### During `/design-ip`
The main agent evaluates upgrade triggers at two points:
1. **After synthesis verification** (Step 6) — if synthesis revealed unexpected issues or new patterns
2. **After final review** (Step 8) — overall session retrospective

### During `/optimize-ip`
The main agent evaluates upgrade triggers at two points:
1. **After re-verification** (Step 6) — if optimization results reveal technique gaps
2. **After final review** (Step 9) — overall session retrospective

### During `/upgrade-env`
The full evaluation runs on demand with no restrictions on trigger points.

## What Good Upgrades Look Like

### Good Example
> **Trigger**: During `fir_filter` design, the synth-sim agent had to manually construct a testbench pattern for streaming IPs with TLAST that wasn't in the templates.
>
> **Proposal**: Add a streaming testbench template to `scripts/templates/cpp/` and reference it in `synth-sim-agent.md`.
>
> **Why good**: Evidence-based, additive, scoped, directly addresses a documented gap.

### Bad Example
> **Trigger**: None specific.
>
> **Proposal**: Rewrite `hls-optimization.md` to use a different organizational structure.
>
> **Why bad**: No evidence, overly broad scope, not motivated by an actual failure.
