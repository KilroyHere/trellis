# Building Trellis with Claude Code: prompts and setup

This repository is set up for the multi-session pattern from Anthropic's "Effective harnesses for long-running agents": a one-time setup session, then one task per session, with state on disk. It also follows the verification and review practices from the Claude Code best-practices guide. The research behind each choice is in `research/research_notes/Claude Code implementation practices/`.

## What the kit contains

| File | Role |
|---|---|
| `CLAUDE.md` | Short standing instructions: session protocol, commands, rules, conventions, environment limits |
| `docs/tasks.json` and `scripts/tasks.py` | The task list. `verify` runs a task's checks and records the evidence; `pass` refuses unless the last verify passed on the current tree. Tasks are never removed or reworded. |
| `docs/PROGRESS.md` | The log carried between sessions: what changed, the evidence, what's next, what waits for the owner |
| `.claude/settings.json` and `scripts/hooks/` | Guardrails: format on edit; block edits to tests, goldens, budgets, the task list and the hooks themselves; remind the agent of the protocol at session start and after compaction; optionally verify on stop |
| `.claude/agents/reviewer.md` | An independent reviewer, since agents grade their own work too kindly |
| `.claude/skills/` | `/next-task`, `/spike`, `/add-compile-fail-test`, `/update-golden`, `/phase-gate` |

## One-time setup on your machine

1. Clone the repo and install the toolchain: GCC 13/14, Clang 18/20, libc++, CMake ≥ 3.28, Ninja, ccache, clang-format-18 and clang-tidy-18, GoogleTest/gMock, Google Benchmark. Then run `scripts/init.sh` (available once T0.1 is done).
2. **clangd plugin.** It gives Claude compiler diagnostics after every edit, plus go-to-definition and find-references. It is the only option here that understands templates.
   ```bash
   sudo apt install clangd-18      # or: brew install llvm
   ln -sf build/clang-18-debug/compile_commands.json compile_commands.json   # gitignored
   ```
   Then, inside Claude Code, run `/plugin install clangd-lsp@claude-plugins-official`. The plugin runs only in terminal sessions; cloud sessions don't start plugin language servers.
3. **Verify-on-stop.** This makes Claude run `scripts/check-fast.sh` before it may end a turn with code changes. It's off by default, so that orchestrating sessions aren't blocked mid-task. Turn it on locally by creating `.claude/settings.local.json`, which is gitignored:
   ```json
   { "env": { "TRELLIS_VERIFY_ON_STOP": "1" } }
   ```
4. **Effort.** Run `/effort high` for template-metaprogramming tasks; the default is medium. Phrases like "think hard" are ordinary text now and do nothing special. `ultrathink` still deepens a single turn.

## Prompt A: every work session

Start a fresh session, then type `/next-task`, or paste this:

> You're continuing work on Trellis, a header-only C++20 dependency injection library. The project runs across many sessions, and the task list, the progress log and the git history are the only memory between them, so keep them accurate. Follow the session protocol in CLAUDE.md for exactly one task.
>
> First, orient. Run `pwd`, `git log --oneline -15` and `git status`, read the newest entries in docs/PROGRESS.md, then run `scripts/tasks.py next`. Run `scripts/init.sh` and make sure everything already passes before you change anything; if something is broken, fixing it comes first.
>
> Read only the sections of docs/plan.md, docs/adr/ and docs/spikes/ that the task cites. Then tell me your plan: the files you'll touch, the tests you'll add, and the commands that will prove the task is done.
>
> Work test-first. Add tests that encode every acceptance criterion and show they fail for the right reason, then implement the smallest change that makes them pass. Implement for all valid inputs, not just the ones the tests use. If an existing test, golden file or budget looks wrong, stop and tell me instead of changing it.
>
> When you're done, run `scripts/tasks.py verify <id>` and show me its summary lines. Then ask the reviewer subagent to review the change, fix any blocking issues, and verify again. Finally run `scripts/tasks.py pass <id>`, add a docs/PROGRESS.md entry (what changed, the evidence, follow-ups, numbers), commit with a clear message (no AI attribution or session links) and push. Then stop.

Why it's written this way:
- The first sentence gives the motivation, which the prompting guide says improves results.
- One task per session, a JSON task list and a progress log come from Anthropic's long-running-agent harness.
- Test-first, the ban on hard-coding to the tests, and "tell me instead of changing the test" are taken almost word for word from the prompting guide.
- An independent reviewer and "evidence, not claims" come from the best-practices page and the harness-design post.

## Prompt B: a spike, run in parallel in its own worktree

```bash
claude -w s1-discovery
```

> Run task T0.4 (spike S1) with the /spike skill. Answer only the spike's question, keep compiler output in log files, run compiles one at a time under the memory cap, and end with a recommendation in docs/spikes/. Don't change include/. Commit only spikes/ and docs/spikes/, then stop.

Run independent spikes in parallel (S1, S3 and S4 have no dependencies on each other). Keep the core library work sequential, in one session at a time. Several agents editing one tightly coupled codebase overwrite each other's fixes.

## Prompt C: independent review, in a fresh session

> Act as an independent reviewer for task <id>. Use the reviewer subagent's checklist in .claude/agents/reviewer.md. Review the diff, not the author's description. Rerun `scripts/tasks.py verify <id>` and `scripts/check-full.sh`. Report VERDICT: PASS or FAIL with file:line evidence. Don't modify files.

## Prompt D: phase gate

Type `/phase-gate 1`. The skill checks the exit criteria in plan §12.2, runs the full checks and benchmarks, writes `docs/gates/phase-1.md`, and tags the release only after you approve.

## Prompt E: expanding the next phase into tasks

> Do task T2.0. Break Phase 2 of docs/plan.md §12.2 into tasks shaped like the Phase 1 tasks in docs/tasks.json. Each task needs an id, plan_refs, interfaces with exact signatures where they're known, acceptance criteria that would fail without the work, and verify commands. Keep each task small enough to finish, review and commit in one session; split a week of plan into 3–8 tasks. Show me the list before you add it with `scripts/tasks.py add`.

## Prompt F: longer unattended runs (optional)

Unattended runs work only where the checker is close to perfect. Here that means the unit, compile-fail, golden and property tests plus the budgets. Use them for well-specified tasks such as documentation snippets, error-catalog pages, packaging or fixing red CI. Don't use them for the spikes or the core engine, where close supervision paid off in every report found in the research.

- **Inside a session:** `/goal T1.7 passes scripts/tasks.py verify and the reviewer reports PASS`. A separate model checks the condition after every turn.
- **Headless, one task per run:**
  ```bash
  claude -p "$(sed -n '/^> You.re continuing/,/Then stop.$/p' docs/agent/PROMPTS.md | sed 's/^> //')" \
    --permission-mode acceptEdits --max-turns 80 --max-budget-usd 15 --output-format json
  ```
  Check that the run loads CLAUDE.md and hooks. Don't pass `--bare`, which skips them; the docs say it will become the default for `-p`. Review every commit afterwards.

## Prompt G: the setup session (already done; kept for reference)

This is how the repository was set up, and how you'd set up a similar project from a plan:

> Read docs/plan.md. Set this repository up for a long, multi-session implementation by Claude Code:
> 1. A CLAUDE.md under 200 lines, covering the session protocol, exact commands, the rules agents get wrong, conventions and environment limits. No architecture overview; Claude can read the repo for that.
> 2. docs/tasks.json, with Phase 0 and Phase 1 broken into tasks that each have acceptance criteria, verify commands and dependencies, all initially failing. Later phases get one "expand this phase" task each.
> 3. scripts/tasks.py, so that only `passes` can change and only after a verify that passed on the current tree.
> 4. docs/PROGRESS.md.
> 5. Hooks that format on edit, protect tests, goldens, budgets and the task list, and re-inject the protocol after compaction.
> 6. A read-only reviewer subagent, and skills for the recurring procedures.
>
> Don't write library code. Commit the setup, then stop.

## Tools: what to use and what to skip

Recommended, all wired into tasks T0.1 and T0.2:
1. **clangd with the `clangd-lsp` plugin.** Real compiler diagnostics after each edit, and symbol navigation that understands templates.
2. **CMake presets, Ninja and ccache, exporting `compile_commands.json`.** One command per configuration and fast rebuilds. Never use ccache for timing builds.
3. **clang-format-18 as a format-on-edit hook,** with the same version in CI.
4. **clang-tidy on changed lines only** (`clang-tidy-diff`), plus a full run nightly.
5. **Sanitizer presets** (ASan + UBSan, TSan).
6. **`-ftime-trace` with ClangBuildAnalyzer,** because compile time is this project's main performance risk.
7. **CTest compile-fail and golden tests,** plus header self-containment checks.
8. **The `gh` CLI** for CI logs. That's the only place MSVC problems show up.

Not recommended for this project:
- **graphify** (Graphify-Labs/graphify). It's popular and actively maintained.
  - It parses C++ with tree-sitter only. It has no notion of concepts, `requires` clauses, specializations or instantiations, it ignores `.cppm` files, and its node IDs drop namespaces.
  - `graphify claude install` adds hooks that run before every Read, Grep, Glob and Bash call.
  - Its token-savings figures come from the author and weren't measured on C++.
  - Trellis is small enough for Claude to read directly, and clangd covers navigation.
  - If you want to try it anyway, pin it, keep it offline and code-only, and skip the hooks:
    ```bash
    uv tool install graphifyy==0.9.97
    GRAPHIFY_BACKEND=none GRAPHIFY_QUERY_LOG_DISABLE=1 graphify extract . --code-only
    ```
- **Other code-graph tools** (GitNexus, code-graph-rag) share the tree-sitter blind spot. GitNexus is under a noncommercial licence.

Optional:
- **Serena.** GPL-3.0 since v2. It mostly duplicates clangd-lsp, and the marketplace entry installs an unpinned version.
- **Context7.** It has no cppreference source.
- **Compiler Explorer's API, for quick MSVC checks.** It needs godbolt.org allowed in the environment's network settings, and the code you send becomes public.

## Habits that matter most

- **Watch the first minute** of a task. Press Esc the moment the agent drifts. After two failed corrections, run `/clear` and write a better prompt.
- **Ask for evidence:** the command and its output, not "done". `scripts/tasks.py verify` makes that the default.
- **Fix rules with hooks, not emphasis.** If you keep adding emphasis to a CLAUDE.md line and it's still missed, turn the line into a hook.
- **Grow CLAUDE.md one line per repeated mistake.** Keep it under 200 lines, and keep progress notes in the log, not in CLAUDE.md.
- **Keep one builder at a time.** Use subagents for reading logs and reviewing, and parallel sessions only for independent work.
- **Run a cleanup task after each phase,** and track the 6,000-line core budget (NFR-12).
