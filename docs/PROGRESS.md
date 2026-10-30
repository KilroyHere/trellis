# Progress log

Newest entry first. Each entry records the date, the task, what changed, the evidence (commands and their key results), follow-ups and anything waiting for the owner.

## Owner decisions

- **2026-10-30**, answered in the session:
  - **Spike recommendations accepted** as Trellis's design, to be recorded as Accepted in the ADRs (T0.8):
    - S1: the operator set for deduced discovery.
    - S3: the error rendering. NFR-07 is reworded to "the first error line names the error ID; the same diagnostic names the type".
    - S4: atomic state-machine slots using `atomic::wait`, with a mutex fallback; `std::call_once` is rejected.
  - **Golden normalization approved.** S3's rules plus `/` path separators, which also fixes the Windows self-test. This is task T0.10, which may regenerate the sample golden through `tools/golden.py --update`.
  - **Copyright holder: Aryan Patel.** Task T0.11 adds `Copyright (c) 2026 Aryan Patel` above every SPDX line.
  - **Phase order: diagnostics (plan Phase 4) before binding features (plan Phase 3).** Applies when T3.0 and T4.0 are expanded: expand and implement Phase 4's tasks before Phase 3's.
- **Defaults taken from plan §16**, unchanged so far:
  - D2: C++20 baseline.
  - D3: Boost Software License 1.0.
  - D4: servers and applications as the main audience.
  - D6: lazy, thread-safe singletons by default.
  - D7: GoogleTest.
  - D9: develop in this repository.

## Waiting for owner

- **Still open:** D5, whether the runtime container ships in 1.0. Needed before T5.0.
- **CI on a private repo:** Linux runs on every push; macOS and Windows run weekly or on demand to save Actions minutes. Making the repository public would allow the full matrix on every push.
- **Stale branches on GitHub:** `wip/t0.1-scaffold`, `t0.2-ci` and `t0.10-goldens` can be deleted; their content is on `main`. The proxy here refuses branch deletion.
- **Guardrail changes.** Auto mode treats edits to `scripts/hooks/` and `.claude/` as agent self-modification, so these are the owner's to make:
  - Make `scripts/hooks/protect_files.py` protect only committed files, so a task can fix tests it created itself.
  - Add `tests/golden_manifest.txt` to the protected paths.
  - Update `.claude/skills/add-compile-fail-test/SKILL.md`. Proposed text: regexes match diagnostic lines only, not quoted source or carets; expect-id must appear as a whole token in the first error line; every test needs an expect-id or at least one expect-regex; misspelt or late directives are errors; to add a golden, list `<area>/<name> <compiler>-<major> ...` in `tests/golden_manifest.txt`, then run `tools/golden.py --preset <preset> --update --reason "<why>" <area>/<name>`.
- **Environment setup script:** add `g++-14 clang-20 libc++-18-dev libc++abi-18-dev libgtest-dev libgmock-dev libbenchmark-dev clangd-18 ccache libclang-rt-18-dev` so new containers start with the full toolchain (`libclang-rt-18-dev` is the TSan runtime). Also add `uv tool install actionlint-py shellcheck-py`.

## 2026-10-30: T0.9 CI polish and T0.11 copyright notices (passed)

- **T0.9:**
  - **Concurrency:** every commit on `main` keeps its own CI verdict, through a per-SHA concurrency group with no cancel on main. Other branches and fork PRs still cancel superseded runs.
  - **Runner image:** the tools it already provides are documented, and `ninja-build` is no longer installed.
  - **`ci-status.sh`:** prints annotations in log order and drops the generic "Process completed with exit code N." line.
  - **Host conditions:** `check-full.sh` and CONTRIBUTING.md describe them accurately.
  - The nightly benchmark step already failed when its script was missing (done in T0.3).
- **T0.11:**
  - `Copyright (c) 2026 Aryan Patel` now sits above the SPDX line in `include/trellis/config.hpp` and `include/trellis/trellis.hpp`, the only files with SPDX lines.
  - None of the protected test files under `tests/compile_fail/` or `tests/diagnostics_golden/` has an SPDX line, so none was skipped.
  - CONTRIBUTING.md asks new files for the same header.
- **Review:** the independent review passed both. Before merging, nightly was dispatched on the branch: TSan and the quick benchmark were green (run 41143747897).
- **Evidence:**
  - `verify T0.9`: actionlint reports 0 errors; `ci-status.sh` reports a pass for b54f4af (run 41144091511).
  - `verify T0.11`: check-full 10 of 10 presets passed, and the header check passed.
- **Non-blocking suggestions:**
  - Derive the main-branch concurrency condition from the default branch name.
  - Pass the exit-code pattern to jq through `env`.
  - CONTRIBUTING should say "after any shebang line".

## 2026-10-30: T0.10 golden normalization (passed, owner-approved)

- **What changed:**
  - **`tools/golden.py` follows S3's rules:**
    - It strips colour and OSC-8 links (both the ST and BEL terminators) and maps typographic quotes to `'`.
    - It drops quoted source, carets, fix-its, include chains and the "N errors generated." and "compilation terminated." lines.
    - It rewrites paths to `<src>/`, `<build>/`, `<std>/` or `<external>/`, always with `/`, and rewrites `:L:C` and MSVC's `(L,C)`.
  - **`<external>/` is narrower:** it applies only where a diagnostic names a file (a location at line start, `… at <path>:N` in Clang's lambda and unnamed-struct names, and missing-file messages). Path-like text inside qualifiers and messages, such as `{"/db/primary:5432"}`, survives.
  - **Beyond S3:** the `<build>/` and `<external>/` placeholders, a wider `<std>/` rule, MSVC's `(L,C)`, and the BEL terminator.
  - **Clean environment:** both tools start compilers with an allowlisted environment and `LC_ALL=C`. Planted CPATH, GCC_EXEC_PREFIX, COMPILER_PATH, DEPENDENCIES_OUTPUT, CCC_OVERRIDE_OPTIONS and others are proven not to reach the compiler.
  - **The sample golden** was regenerated through `golden.py --update`, going from 7 lines to 2, with the reason logged in `docs/golden-changes.md`.
- **Reading accepted by the coordinator, as author of the acceptance text:** golden runs use the plain-output flags. Compile-fail runs keep the default user-visible output with colours off, so NFR-07's line budget measures what users see.
- **Review:** the independent review passed three times:
  - The first pass found that `<external>/` over-normalized path-like text. That was fixed before the merge, since changing goldens later needs approval.
  - The second pass found a quoted `"/x:5432"` edge case, also fixed.
  - The coordinator spot-checked the final rules.
- **Evidence:**
  - `scripts/tasks.py verify T0.10` on main: check-full 10 of 10 presets in 368 s; golden 1 of 1.
  - **CI:** ci.yml is green on 8d7084f. Windows is green on 8d7084f, so `tools.golden_selftest` now passes on MSVC: https://github.com/KilroyHere/trellis/actions/runs/41138203055. macOS is green on fdc0a0f.
- **Known limits:**
  - Clang's `(lambda at …)` keeps its machine path when an external directory contains a space; this is documented.
  - cl prints the source file name on its own line, which stays in future MSVC goldens.
- **Branch:** `t0.10-goldens` can be deleted on GitHub; its squashed content is 92f48ba.

## 2026-10-30: T0.3 benchmark harness and equivalence oracle (passed)

- **What changed:**
  - `benchmarks/compile/`:
    - `gen_graph.py`: deterministic graphs. Fan-in 1–3 with the default seed reproduces the research graph exactly, fan-in 10 matches Fruit's setting, and the request forms are shared_ptr, reference or mixed.
    - `run.py`: median of 3, peak memory from wait4, a process-group kill on timeout, a memory cap, and no ccache.
    - `budgets.json`: NFR-03, NFR-04 and NFR-05 quoted verbatim from plan §3.2. It is now protected.
  - `benchmarks/scenario/` and `benchmarks/runtime/`: the research scenario, wired by hand, with a Google Benchmark program (1.8 or later).
  - `tests/property/`: the equivalence oracle. Each program resolves its root twice from one injector and once from a second, walks the object graph, and prints an identity checksum plus `constructed=`/`instances=`. A Python model predicts both.
  - `scripts/bench-compile.sh [--quick]` and `scripts/bench-runtime.sh`, the latter building in `build/bench/runtime/<preset>/`.
  - CI gives the property tests fresh seeds on every run: ci.yml uses `<run>000000+64`; nightly uses `<run>500000+256` under TSan.
  - T0.9's fifth criterion is already done here: nightly no longer skips the benchmark silently.
- **Review, two independent rounds:**
  - **Round 1 failed** on two points. The oracle missed a singleton built twice when the extra copy was unreachable, and `budgets.json` said "median of 3" although large compiles took one sample.
  - **Round 2 passed.** Over 82 graphs:
    - the correct wiring was never flagged;
    - every planted sharing bug was caught;
    - a legitimate move into storage wasn't flagged.
- **Evidence:** `scripts/tasks.py verify T0.3` on main:
  - bench-compile --quick: 8 programs, 0 budgets missed;
  - bench-runtime: passed;
  - property: 5 tests.

  `check-full.sh`: 10 of 10 presets pass.
- **Hand-wired baselines** (gcc-13 / clang-18; this VM is slower than in the research, but budgets are ratios):
  - Scenario: 1.50 / 1.79 s; 164 / 158 MB; 18,736 / 14,744 B stripped (identical to the research).
  - N=100: 7.4 / 8.1 s; 383 / 248 MB.
  - Runtime: graph 57 / 58 ns; resolve 27 / 30 ns.
  - Hand-wiring itself grows superlinearly on gcc-13, from 50 s at N=500 to 172 s at N=1000, so NFR-04's N=1000 ratio is measured against a superlinear baseline.
- **Readings now fixed in `budgets.json`**, recorded for the owner:
  - NFR-03 is a difference (at most +0.3 s), which depends on the machine.
  - The growth and layout targets are kept as quoted text, not automated checks.
  - Budgets apply to every measured profile, which is stricter than the research's shared_ptr fan-in 1–3 runs.
  - Ratios are compared at 2 decimals, so 1.304× passes "≤ 1.3×".
- **Note:** a verify that calls `ctest` directly needs `scripts/init.sh` first after other tasks change the CMake setup, as the session protocol's step 2 already says.

## 2026-10-30: T0.2 CI workflows (passed)

- **What changed:**
  - **ci.yml** runs on every push and on fork pull requests, across six Linux presets: gcc-13, gcc-14, clang-18, clang-20, clang-18 + libc++, and ASan+UBSan.
  - **nightly.yml** runs TSan and the quick compile-time benchmark daily.
  - **platforms.yml** builds and tests on macOS (Apple Clang) and Windows (MSVC with Ninja), weekly or on demand.
  - The scheduled workflows open or update a single issue when they fail.
  - Actions are pinned to full SHAs, and permissions are read-only except for the issue jobs.
  - New: `scripts/ci-status.sh`, problem matchers, the `appleclang-debug` and `msvc-debug` presets, and host-aware `check-full.sh`.
- **Review:** the independent review passed. Its non-blocking suggestions are now task T0.9.
- **Evidence:**
  - `scripts/tasks.py verify T0.2`: actionlint 1.7.12 reports 0 errors in 3 workflows.
  - `ci-status.sh`: "ci: passed for 4336cad" (https://github.com/KilroyHere/trellis/actions/runs/41118199428).
  - macOS passed (run 41114666449).
  - Windows MSVC builds warning-free with `/W4 /permissive- /WX`; only `tools.golden_selftest` fails (above).
- **Notes:**
  - The proxy blocks job-log downloads and GraphQL here, so `ci-status.sh` falls back to annotations, and the issue-opening jobs can't be tested from this environment.
  - actionlint and shellcheck were installed with `uv tool install actionlint-py` and `uv tool install shellcheck-py`. Add them to the environment setup script.
  - The `t0.2-ci` branch can be deleted on GitHub; its squashed content is in `4336cad`.

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
