# Progress log

Newest entry first. Each entry records the date, the task, what changed, the evidence (commands and their key results), follow-ups and anything waiting for the owner.

## Waiting for owner

- **Defaults taken from plan §16.** Each is easy to change:
  - D2: C++20 baseline.
  - D3: Boost Software License 1.0.
  - D4: servers and applications as the main audience.
  - D6: lazy, thread-safe singletons by default.
  - D7: GoogleTest.
  - D9: develop in this repository.
- **Still open:** D5, whether the runtime container ships in 1.0.
- **Phase order:** the plan suggests moving Phase 4 (diagnostics) ahead of Phase 3. Decide before T3.0.
- **First commit cleanup:** the first commit (`d105b55`) still has a Claude co-author trailer and a session link. Removing them needs a force-push to `main`.
- **CI on a private repo:** Linux runs on every push; macOS and Windows run weekly or on demand to save Actions minutes. Making the repository public would allow the full matrix on every push.

## 2026-10-30: setup session

- **Repository.** It holds the plan (`docs/plan.md`) and the background research (`research/`): the survey of C++ DI frameworks, hands-on measurements, the benchmark harness, and notes on Claude Code practices.
- **Claude Code kit:**
  - `CLAUDE.md`.
  - `docs/tasks.json` with `scripts/tasks.py`: Phase 0 and Phase 1 broken into tasks, later phases as one expansion task each.
  - This log.
  - Hooks in `.claude/settings.json` and `scripts/hooks/`: format on edit; protect tests, goldens, budgets, the task list and the hooks; remind the agent at session start; verify on stop, opt-in.
  - The `reviewer` subagent.
  - The skills `/next-task`, `/spike`, `/add-compile-fail-test`, `/update-golden` and `/phase-gate`.
  - Prompts and setup notes in `docs/agent/PROMPTS.md`.
- **Tool choices** (from the tooling research): clangd with the `clangd-lsp` plugin, CMake presets, Ninja and ccache, a pinned clang-format, clang-tidy on changed lines, sanitizer presets, and `-ftime-trace` with ClangBuildAnalyzer. graphify is not adopted: it parses C++ with tree-sitter only, can't see templates, concepts or modules, and adds hooks on every file read.
- **Toolchain in the cloud container:** GCC 13.3 and 14.2, Clang 18.1 and 20.1, libc++ 18, GoogleTest/gMock 1.14, Google Benchmark 1.8.3, CMake 3.28, Ninja, ccache, and clang-format, clang-tidy and clangd 18. MSVC isn't available, and the network blocks godbolt.org, so MSVC is covered only by CI.
- **Next:** T0.1, the scaffold. The independent spikes S1, S3 and S4 can run alongside it.
