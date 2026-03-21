# /upgrade-env — Environment Upgrade Pipeline

## Description
Evaluates the current environment configuration and proposes upgrades based on recent design/optimization sessions or user feedback. All upgrades require explicit user confirmation before being applied.

## Usage
```
/upgrade-env [<ip_name>]
/upgrade-env --feedback "<description>"
```

## Arguments
- `<ip_name>` (optional) — Evaluate upgrades based on a specific IP's design/optimization session. If omitted, evaluates across all recent sessions.
- `--feedback "<description>"` (optional) — User-provided description of an issue or improvement idea.

## Execution Flow

### Step 1: Gather Context
**Agent**: `main-agent`
- If `<ip_name>` provided: read all artifacts from `src/<ip_name>/` (source, reports, testbench results, error logs)
- If `--feedback` provided: parse the user's description
- If neither: scan recent sessions for patterns (check `reports/` directories, `.claude/upgrades/upgrade-log.md`)
- Load current environment state: read all `.claude/` files

### Step 2: Analyze & Identify Gaps
**Agent**: `upgrade-agent`
- Compare session outcomes against environment expectations
- Identify patterns: recurring failures, missing knowledge, new techniques applied ad hoc
- Cross-reference with upgrade log to avoid proposing duplicate or reverted changes
- Prioritize findings by impact (HIGH / MEDIUM / LOW)

### Step 3: Generate Upgrade Proposals
**Agent**: `upgrade-agent`
- Create a structured upgrade batch (see `upgrade-agent.md` for format)
- Each proposal includes: target file, exact diff, rationale, impact, rollback plan
- Order proposals by priority

### Step 4: Main Agent Review
**Agent**: `main-agent`
- Review each proposal for:
  - Correctness: does the proposed change actually address the identified gap?
  - Safety: could this break existing designs or conflict with other rules?
  - Scope: is the change appropriately scoped (not too broad, not too narrow)?
  - Consistency: does it align with the overall project conventions?
- Reject or refine proposals that don't pass review
- Add implementation notes if needed

### Step 5: Present to User for Approval
**Format**: Present each proposal to the user clearly:

```
╔══════════════════════════════════════════════╗
║  ENVIRONMENT UPGRADE PROPOSAL               ║
╠══════════════════════════════════════════════╣
║  Batch: <date> | Proposals: N               ║
║  Risk: Low/Medium/High                       ║
╚══════════════════════════════════════════════╝

Proposal 1/N: <title> [PRIORITY]
  File:   <path>
  Change: <brief description>
  Reason: <one-line rationale>
  Risk:   <one-line risk assessment>

  [Show full diff? Y/N]

───────────────────────────────────────────────
Options:
  (A) Approve all proposals
  (a) Approve this proposal
  (s) Skip this proposal
  (m) Modify this proposal
  (R) Reject all and abort
───────────────────────────────────────────────
```

**Critical rule**: NEVER apply any upgrade without explicit user approval. The user must confirm each proposal individually or approve the entire batch.

### Step 6: Apply Approved Upgrades
**Agent**: `upgrade-agent` (supervised by `main-agent`)
- For each approved proposal:
  1. Create a backup of the target file: `.claude/upgrades/backups/<filename>_<timestamp>.bak`
  2. Apply the change to the target file
  3. Verify the file is valid after modification
  4. Log the upgrade in `.claude/upgrades/upgrade-log.md`
- For skipped/rejected proposals: log them as "deferred" or "rejected" with reason

### Step 7: Post-Upgrade Verification
**Agent**: `main-agent`
- Verify all modified files are internally consistent
- Check cross-references between files (e.g., if a new skill is added, verify agents reference it)
- Confirm the environment is in a valid state
- Report the final summary to the user

## Integration with Design/Optimization Pipelines

This command can be triggered automatically at the end of `/design-ip` and `/optimize-ip` pipelines:
- After design/optimization completes, the main agent evaluates whether an upgrade evaluation is warranted
- If triggers are detected (see `upgrade-agent.md`), the main agent informs the user and offers to run `/upgrade-env`
- The user can accept or decline the upgrade evaluation

## Rollback

To revert a specific upgrade:
```
/upgrade-env --rollback <upgrade_id>
```
This restores the backed-up file and logs the rollback.

## Output

```
.claude/upgrades/
├── upgrade-log.md                    # Complete upgrade history
└── backups/
    └── <filename>_<timestamp>.bak    # Backup of each modified file
```
