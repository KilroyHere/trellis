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
- **CI on a private repo:** Linux runs on every push; macOS and Windows run weekly or on demand to save Actions minutes. Making the repository public would allow the full matrix on every push.
- **Spike decisions.** Each recommendation is the provisional default until confirmed:
  - S1 (T0.4): the operator set for deduced constructor discovery.
  - S3 (T0.5): the error-rendering design, and rewording NFR-07 to "the first line names the error ID; the same diagnostic names the type". gcc-13 in C++20 mode puts the type on the next line.
  - S4 (T0.6): the singleton slot design (atomic state machine with `atomic::wait`, mutex fallback; `std::call_once` rejected).
- **Copyright holder** for the SPDX headers. They currently carry only `SPDX-License-Identifier: BSL-1.0`.
- **Guardrail changes.** Auto mode treats edits to `scripts/hooks/` and `.claude/` as agent self-modification, so these are the owner's to make:
  - Make `scripts/hooks/protect_files.py` protect only committed files, so a task can fix tests it created itself.
  - Add `tests/golden_manifest.txt` to the protected paths.
  - Update `.claude/skills/add-compile-fail-test/SKILL.md`. Proposed text: regexes match diagnostic lines only, not quoted source or carets; expect-id must appear as a whole token in the first error line; every test needs an expect-id or at least one expect-regex; misspelt or late directives are errors; to add a golden, list `<area>/<name> <compiler>-<major> ...` in `tests/golden_manifest.txt`, then run `tools/golden.py --preset <preset> --update --reason "<why>" <area>/<name>`.
- **Golden normalization:** aligning `tools/golden.py` with S3's rules means regenerating the sample golden, which needs approval.
- **Environment setup script:** add `g++-14 clang-20 libc++-18-dev libc++abi-18-dev libgtest-dev libgmock-dev libbenchmark-dev clangd-18 ccache libclang-rt-18-dev` so new containers start with the full toolchain. `libclang-rt-18-dev` is the TSan runtime.

## 2026-10-30: T0.1 build, test and check scaffold (passed)

- **What changed:**
  - CMake INTERFACE target `trellis::trellis` with install rules and a package config.
  - Ten presets: gcc-13, gcc-14, clang-18, clang-20 and clang-18 + libc++ debug; gcc-13 and clang-18 release; asan-ubsan; tsan; timetrace.
  - The project warning set as errors.
  - `config.hpp` with version and `TRELLIS_HAS_*` macros.
  - A header self-containment check with `-Wundef`.
  - GoogleTest: the system 1.14, with a git fallback pinned to the v1.18.0 commit.
  - `tools/compile_fail.py`, and `tools/golden.py` driven by `tests/golden_manifest.txt`, each with self-tests.
  - `scripts/{init,check-fast,check-full,tidy}.sh`, which print summaries and write full logs to `build/logs/`.
  - LICENSE (Boost Software License 1.0, identical to Debian's and GitHub's copies), CONTRIBUTING.md, and the CLAUDE.md command list corrected.
- **Review, three independent rounds:**
  - Round 1 failed on three runner defects: a crash after the expected error counted as a pass, regexes matched quoted source, and a timeout left cc1plus running.
  - Round 2 failed on a regression: the process-group fix stopped Ctrl-C and outer timeouts from reaching the compiler.
  - Round 3 passed, with every interruption reproduced on real GCC.
- **Evidence:** `scripts/tasks.py verify T0.1` passed:
  - init: 26 tests, 15 passed, 11 skipped;
  - check-full: 10 of 10 presets, 45 s;
  - the package consumer build.

  The compile-fail self-test passes all 26 checks.
- **Decisions:**
  - C++26 feature macros also require C++26 mode, because clang-20 defines some of them in C++20 mode and then warns.
  - The libc++ preset skips golden tests, because libc++ spells std names differently.
  - The version is 0.1.0, as in development.
  - Fixtures use TRL-E000 as a placeholder ID.
- **[NEEDS CLARIFICATION]:**
  - Golden normalization versus S3's rules (above).
  - User-generated `static_assert` messages before C++26, which T1.6 decides under ADR-06.
- **Known gaps:**
  - MSVC, clang-cl and macOS paths are untested until CI (T0.2).
  - Only CMake 3.28 is tested, against a declared minimum of 3.21.
  - Deleting a manifest line together with its golden drops that test silently; the reviewer finds this acceptable for now.
  - The research tooling note gave a GoogleTest `URL_HASH` that was the hash of the proxy's 403 page. It is corrected in the note.

## 2026-10-30: spikes S1, S3 and S4 (results on main; marked passing after re-verification on main)

- S1 (deduced discovery), S3 (diagnostics rendering) and S4 (storage and threading) ran in parallel git worktrees. Their outputs were copied to `main` file by file, after checking each copy matched. The write-ups are in `docs/spikes/`, and the recommendations are listed above.
- **Evidence:** each spike's `run.sh` reproduced its results on `main` through `scripts/tasks.py verify`:
  - T0.6 (S4): 229 s;
  - T0.4 (S1): 598 s;
  - T0.5 (S3): 1,121 s, slowed by agents building in parallel.

  All three are marked passing. The design decisions they recommend still wait for the owner.
- **Environment:** S4 installed `libclang-rt-18-dev` (the clang-18 TSan runtime). It is listed for the setup script above.

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
