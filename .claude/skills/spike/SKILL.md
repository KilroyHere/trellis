---
name: spike
description: Run a Phase 0 spike from plan §12.1, a time-boxed experiment that answers one design question with measurements and ends in a written recommendation for the owner to confirm.
---

A spike answers one question. Its code is throwaway, but its results must be reproducible and its write-up must lead to a decision.

**Layout.** Code goes in `spikes/<id>-<slug>/`, with a `run.sh` that rebuilds every number in the report from scratch. The write-up goes in `docs/spikes/<id>-<slug>.md`.

**Write-up sections.**

1. **Question:** taken from plan §12.1.
2. **Method:** compilers with their exact versions, flags, standard library, inputs, and how each number was measured.
3. **Results:** tables first, then short notes. Mark anything you could not test, and say why.
4. **Recommendation:** what to decide, which ADR or plan section it confirms or changes, and the cost of the alternatives.
5. **Limits and open questions.**

**Rules.**

- Test every claim on gcc-13, gcc-14, clang-18 and clang-20 (and clang-18 with libc++ where the standard library matters). MSVC isn't available here, so give an explicit MSVC risk assessment.
- Run compiles one at a time unless they are tiny. Wrap them in `timeout` with a memory cap (`ulimit -v 6000000`); report a compile that hits a cap as a result, not a failure to hide.
- Keep compiler output in log files under the spike directory, and quote only the lines that matter.
- Don't touch `include/`. A spike informs the library; it doesn't change it.
- Finish with `scripts/tasks.py verify <id>`. The recommendation waits for the owner's sign-off, so list it under "Waiting for owner" in `docs/PROGRESS.md`.
