# Execution

Use this file to keep project execution moving after intake/approval.

## Default Execution Mode

After intake and plan approval, continue autonomously through all safe unblocked work until the project is complete or genuinely blocked.

Phase boundaries are progress checkpoints, not permission gates.

## Background / Delegation Threshold

Move work out of active chat into background execution, delegation, or scheduling when it:

- is expected to take more than a few minutes,
- has multiple independent subtasks,
- waits on long-running commands, tests, research, or external processes, or
- can proceed while Bob answers another question.

Short interactive work may stay in chat.

## Questions During Execution

If Bob's answer is needed but other useful work remains unblocked:

1. Ask the question in chat.
2. State exactly what action the question blocks.
3. State what work is continuing.
4. Continue the unblocked work in the background where practical.

## Real Approval Gates

Ask before actions that are:

- destructive,
- costly,
- public or external,
- privacy-exposing,
- credential/security-sensitive,
- live-system-changing, or
- changing the goal, major architecture, or definition of done.

Everything else is normal execution progress, not a permission gate.

## Worker / Background Completion Contract

Before a worker, subagent, or background process reports completion, it must update or return enough information for the orchestrator to update project state:

- work done,
- files changed,
- commands/tests run,
- verification evidence,
- blockers,
- open questions,
- recommended next action.

The orchestrator verifies worker claims before telling Bob the work is complete.

## State Update Checklist

After meaningful work, update as applicable:

- `STATUS.md` — current state, next action, blockers, waiting-on-Bob
- `TASKS.md` — completed/in-progress/pending/blocked tasks
- `ARTIFACTS.md` — new reports, outputs, scripts, decisions, reviews
- `DECISION_GATES.md` — consequential open questions and scoped blockers
- `HANDOFF.md` — current status and next actions for resume
