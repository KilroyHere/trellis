---
name: phase-gate
description: Check a phase's exit criteria from docs/plan.md §12.2 and, once the owner approves, tag the release.
disable-model-invocation: true
argument-hint: "<phase number>"
---

Phase: $ARGUMENTS

1. Read this phase's exit criteria in `docs/plan.md` §12.2, and the targets they name in §3.2.
2. Confirm that every task for the phase passes: `scripts/tasks.py list --phase <n>`.
3. Run `scripts/check-full.sh`, `scripts/bench-compile.sh` (the full sweep), `scripts/bench-runtime.sh` and `scripts/ci-status.sh`.
4. Ask the `reviewer` subagent for a phase-level review covering API consistency across the phase, documentation and test coverage.
5. Write `docs/gates/phase-<n>.md`, listing each criterion with its evidence and a pass or fail.
6. If every criterion passes and the owner approves, update `CHANGELOG.md`, tag `v0.<n>.0`, and push the commit and the tag.
