---
name: next-task
description: Run one Trellis work session. Orient, take the next ready task from docs/tasks.json, implement it test-first, verify, get an independent review, record progress and commit.
disable-model-invocation: true
---

Follow the session protocol in CLAUDE.md for exactly one task.

1. **Orient.** Run `pwd`, `git log --oneline -15` and `git status`, and read the newest entries of `docs/PROGRESS.md`. Then run `scripts/tasks.py next`, which shows the task, its acceptance criteria, its verify commands and its plan references.
2. **Smoke test.** Run `scripts/init.sh`. If something already fails, fix that first.
3. **Read** the plan and ADR sections the task cites, plus any spike report it names.
4. **Plan.** Name the files, the tests and the commands that will prove the task is done. For tasks that touch discovery, resolution, storage or diagnostics, show the plan and wait for approval if someone is watching.
5. **Test first.** Add tests for every acceptance criterion, run them, and check that they fail for the right reason.
6. **Implement** the smallest change that passes, following the CLAUDE.md conventions.
7. **Verify.** Run `scripts/tasks.py verify <id>` and quote its summary lines.
8. **Review.** Ask the `reviewer` subagent to review task `<id>`. Fix blocking issues and run verify again.
9. **Record.** Run `scripts/tasks.py pass <id>`, then add a `docs/PROGRESS.md` entry: date, task, what changed, the evidence, follow-ups, benchmark numbers, and anything waiting for the owner.
10. **Commit and push.** Use an imperative subject and a body that explains what and why. No AI attribution trailers and no session links.
11. **Stop.** Don't start another task in this session.
