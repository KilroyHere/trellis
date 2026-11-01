# Trellis

Trellis is a header-only C++20 dependency injection library (pre-alpha). The design is in `docs/plan.md`, which is long: read the sections a task cites, not the whole file. Decisions are recorded in `docs/adr/`, and the background research lives in `research/`.

## Session protocol

The project runs across many sessions. The task list, the progress log and the git history are the only memory between them, so keep them accurate.

1. **Orient.** Run `pwd`, `git log --oneline -15` and `git status`, and read the newest entries of `docs/PROGRESS.md`. Then run `scripts/tasks.py next`, which names the task to work on. A builder works on that one task only; an orchestrator running a team (see Parallel team) takes every task `scripts/tasks.py next --all` lists. If the log and git history say something is done, trust them; don't redo it.
2. **Smoke test.** Run `scripts/init.sh` before changing anything (it arrives with T0.1). If something is already broken, fixing it comes first.
3. **Plan.** Read only the plan and ADR sections the task cites. For anything beyond a small change, write a short plan first: files to touch, tests to add, and the commands that will prove the task is done. Where the plan is ambiguous, pick the simplest reading, mark it `[NEEDS CLARIFICATION: ...]` in the progress entry, and carry on.
4. **Test first.** Add tests that encode the acceptance criteria. These can be unit tests, compile-fail tests or golden tests. Run them and confirm they fail for the right reason before you implement.
5. **Implement** the smallest change that makes them pass.
6. **Verify.** `scripts/tasks.py verify <id>` runs the task's verify commands and saves the evidence. Quote its summary lines.
7. **Review.** Ask the `reviewer` subagent to review the change. Fix blocking issues, then verify again.
8. **Record.** `scripts/tasks.py pass <id>` (it refuses unless the last verify passed on the current tree). Then add a `docs/PROGRESS.md` entry: what changed, the evidence, follow-ups, and benchmark numbers if any. Then commit and push.
9. **Stop.** The next task gets a fresh builder: a new session, or a new worktree subagent when an orchestrator runs the team.

## Parallel team

Parallel work is the default. One orchestrator session runs several builders at once and keeps the shared memory consistent.

- **Roles.** A builder is a subagent in its own git worktree (`isolation: worktree`) with one task and a written brief. It follows steps 2–7 and commits on its branch. The orchestrator writes the briefs, runs the reviewer on each finished branch (subagents can't start subagents), squash-lands it on main, then verifies, passes, records and pushes. Only the orchestrator changes main, `docs/tasks.json` and `docs/PROGRESS.md`.
- **What runs at once.** Every task that `scripts/tasks.py next --all` lists, plus any task whose dependency is written but not yet landed. Branch it from the dependency's head, and once that lands, `git rebase --onto main <old-head>`. Prefer tasks that touch different files; when two must share one, land the smaller first.
- **Machine.** At most three builders, each with `-j2` and its own worktree's build directories. The benchmarks time wall clock, so run `bench-compile.sh`, `bench-runtime.sh` and the verifies that include them only on a quiet machine (1-minute load under 1.5): before the builders start, or while they wait on CI.
- **One verify per tree.** Land a batch, then verify each of its tasks on that one tree. `tasks.py verify` reuses a command that already passed on the same tree, so one `check-full.sh` serves them all. Pass them in dependency order, then commit the bookkeeping once: `docs/tasks.json` and `docs/PROGRESS.md` are outside the fingerprint, but a commit changes HEAD, which is inside it.
- **CI.** Platform runs are slow and billed by the minute. Batch the fixes, dispatch only the platform a change affects (`gh workflow run platforms.yml --ref <branch> -f os=windows`), and give each platform to one builder at a time.
- **Restarts.** A container restart kills every builder. Builders commit as they go; the orchestrator pushes finished work at once and schedules a check-in (`send_later`) before a long wait.

## Commands

`init.sh`, `check-fast.sh`, `check-full.sh` and `tidy.sh` arrive with T0.1, `ci-status.sh` with T0.2, and `bench-compile.sh` and `bench-runtime.sh` with T0.3. If a command here disagrees with a script, the script is right; fix this file.

- Smoke test: `scripts/init.sh`
- Fast check (format, clang-18 debug build, tests): `scripts/check-fast.sh`
- Full local matrix, every preset one at a time (gcc-13, gcc-14, clang-18, clang-20 and clang-18 + libc++ debug; gcc-13 and clang-18 release; ASan/UBSan; TSan; timetrace): `scripts/check-full.sh`, or `scripts/check-full.sh <preset>...` for some
- One preset: `cmake --preset clang-18-debug && cmake --build --preset clang-18-debug && ctest --preset clang-18-debug`
- One test: `ctest --preset clang-18-debug -R <regex>`
- By label: `ctest --preset clang-18-debug -L compile_fail` (or `-L unit`, `-L golden`, `-L tools`, `-L examples`, `-L property`, `-L single_header`, `-L chain_depth`). `-L module` selects tests only in the Clang presets, which build the C++20 module; elsewhere it fails, because a label that selects no tests is an error.
- Single header: `python3 -I tools/amalgamate.py` writes `single_include/trellis.hpp`; `--check` checks it. A new public entity also needs a line in the module that includes its header (`modules/trellis.cppm`, or `trellis.testing.cppm` for `<trellis/testing/...>`); the test `module.exports` names the line and the file.
- Compile-time budgets: `scripts/bench-compile.sh --quick`. Runtime: `scripts/bench-runtime.sh`
- CI status and the first errors of failed jobs: `scripts/ci-status.sh`. MSVC and macOS run only in CI.
- Lint changed lines: `scripts/tidy.sh`. Formatting is automatic: a hook runs clang-format-18 on edited files.
- Tasks: `scripts/tasks.py next [--all] | show <id> | list [--phase N] | verify <id> [--fresh] | pass <id> | add <file.json>`
- `tasks.py verify` runs its commands against the build directories as they are, so configure and build the presets they name first (after landing or pulling, `scripts/init.sh` covers clang-18-debug); a stale directory makes ctest find no tests. A command that already passed on the same tree, for any task, passes again without running; `--fresh` runs it.

Build output is long. Scripts write full logs to `build/logs/` and print a summary. Read the log only for the first error, not all of it.

## Rules

- Never delete, weaken or rewrite existing tests, expected-output directives, golden files or `benchmarks/compile/budgets.json` to make something pass. If one looks wrong, stop and say why in `docs/PROGRESS.md`. A hook blocks direct edits to these files.
- Implement for all valid inputs. Never special-case or hard-code what a test checks.
- Edit `docs/tasks.json` only through `scripts/tasks.py`.
- Stay inside the task. Leave P2 items and extras for their own task. Add no abstraction layers that the plan doesn't call for.
- Tasks marked `"review": "owner"` (spikes, ADRs, phase gates, task expansion) end with a decision the owner confirms. List them under "Waiting for owner" in the progress log.
- Commits: imperative subject of at most 72 characters, and a body that says what changed and why. No Co-Authored-By or other AI attribution trailers, and no links to Claude sessions.
- `include/` is header-only and uses only the standard library. Macros live only in `config.hpp`, with a `TRELLIS_` prefix.

## Documentation (part of done)

The skeletons for these arrive with T1.12.

- Every public entity in `include/` gets a doc comment in the documented convention: purpose, constraints, the error IDs it can raise, and a short example.
- A new error ID gets a page in `docs/errors/`. A check fails when an ID used in `include/` has no page.
- A user-visible change gets a line under "Unreleased" in `CHANGELOG.md`.
- A new metaprogramming technique gets a note in `docs/internals/`: what it does, why, and which tests cover it.
- A design change updates or adds an ADR in `docs/adr/`. Never let the code drift silently from `docs/plan.md`.
- At each phase gate, update the status line and anything else outdated in `README.md`.

## C++ conventions

- C++20 is the baseline. Use C++23/26 features only behind the `TRELLIS_HAS_*` macros from `config.hpp`.
- Namespace `trellis` with `inline namespace v1`; internals go in `trellis::detail`. The one exception, decided by the owner: the compile-error vocabulary lives in `trellis::error`, outside `v1` (ADR-0006).
- snake_case for types, functions and variables; CamelCase for template parameters.
- Every public header compiles on its own. Code is warning-free under the project warning set with `-Werror`.
- Every metaprogramming trick gets a comment saying what it does and why, plus a test.
- Errors use the stable IDs in plan §7.8 (`TRL-E0xx`, `TRL-R0xx`). Each root cause produces exactly one `static_assert`. Never report misuse through warnings, deprecation or link errors.
- Raise an error from a function read through `decltype`, or from a constructor, not from a class template's body: Clang marks a class template whose `static_assert` failed as invalid and reports every later use again. Give each error a compile-fail test that writes the misuse twice, which catches this.

## Compile-time performance (plan §8.1)

- Memoize per (key, request form, consumer lifetime). Never put a path or a depth into a type on the normal resolution pass.
- Use flat pack expansions and inheritance-based maps and sets, not recursive variadic templates or linear searches. Use fold expressions only over packs that can't exceed 256 operands: Clang rejects longer ones (plan §8.1 C3, ADR-0010).
- No fluent APIs that accumulate the binding set in a type.
- After changing anything under `include/trellis/static/` or `include/trellis/discovery/`, run `scripts/bench-compile.sh --quick` and compare with the last numbers in `docs/PROGRESS.md`, and re-measure the deepest chains with `python3 -I tests/chain_depth/check_chain_depth.py --deepest --record` (NFR-09 needs 200 hops).

## Environment

- MSVC isn't installed here, and the network blocks godbolt.org, so MSVC problems surface only in CI. Keep compiler-specific workarounds in `config.hpp`, each with a comment and a test.
- The cloud container has 4 vCPUs and about 15 GB of memory, and a container restart kills running processes. Commit and push finished work promptly.
  - Use at most `-j2` while other agents might be building.
  - Never build one preset directory from two processes at once.
  - Run experimental or generated compiles under `timeout` and a memory cap (`ulimit -v 6000000` in a subshell; not for sanitizer builds).
