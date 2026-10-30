# Tooling for Claude Code on Trellis: graphify, code-intelligence tools and the C++ toolchain

> **Status:** complete, 2026-10-30. The recommendation is in [§5](#5-recommended-minimal-tool-set).
> **Scope:** external tools that help Claude Code develop Trellis, a header-only C++20 DI library with heavy template metaprogramming, compile-fail tests, golden tests of compiler diagnostics, compile-time and runtime benchmarks, and CI on GCC, Clang and MSVC (plan: `docs/plan.md` §5.2, §8, §9, §10). Anthropic's own docs and community workflows are covered in sibling notes; this file cites the Claude Code docs only for MCP, plugins, LSP and security.

## How these notes were gathered

- **Repositories were cloned as data only**, into scratch directories, and never installed, built or run. Facts were taken from their files and `git log`: graphify (`Graphify-Labs/graphify`, default branch `v8`), Serena (`oraios/serena`) and Anthropic's official plugin marketplace (`anthropics/claude-plugins-official`).
- **Pages fetched directly:** GitHub web pages, PyPI and `code.claude.com/docs`.
- **Blocked hosts:** the session's egress policy blocked graphify.com, docs.graphify.com, skills.sh, mejba.me, clang.llvm.org, cmake.org, ccache.dev, godbolt.org, cppreference.com, context7.com, docs.github.com, modelcontextprotocol.io and oraios.github.io. Where only a **search snippet** was available, it is labelled that way.
- **Star counts** come from the GitHub web page on 2026-10-30, because the REST API is blocked. Treat them as approximate.
- **Local toolchain used to check command syntax:** CMake 3.28.3, Ninja 1.11.1, ccache 4.9.1, Clang 18.1.3 and 20.1.2, clang-tidy, clang-format, `run-clang-tidy` and `clang-tidy-diff` 18.1.3, clangd 18, GCC 13.3.0, uv 0.11.32, Node 22, gh 2.89.0.

---

## 1. What "graphify" is

### 1.1 Verdict on the name

There is **one** project that matches the user's description: **Graphify-Labs/graphify**, a `/graphify` skill, CLI and MCP server that builds a knowledge graph of a codebase for AI coding assistants. Every other repository with the name is a fork, a wrapper or a setup script around it (§1.3). No unrelated "graphify" project for code knowledge graphs turned up. `pypi.org/project/graphify/` returns 404. The official package is `graphifyy`, spelled with a double y.

### 1.2 Graphify-Labs/graphify: facts

| Field | Value | Source |
|---|---|---|
| Repo | https://github.com/Graphify-Labs/graphify. `github.com/safishamsi/graphify` now serves the same repository: it was transferred. | GitHub page |
| Author | Safi Shamsi (`safishamsi`); company Graphify Labs. A README badge claims Y Combinator S26. PyPI lists one maintainer, `captainturbo`. | `git log`, NOTICE, README, PyPI |
| License | **Apache-2.0** (pyproject `license = "Apache-2.0"`). It was relicensed from MIT; `LICENSE-MIT` is kept for contributions made before the change (NOTICE). | LICENSE, NOTICE, pyproject.toml |
| First commit / release | First commit 2026-04-03 ("feat: core pipeline"). First PyPI release 0.1.1 on 2026-04-04. The first git tag, oddly, is `v1.0.0` (2026-04-05), followed by `v0.2.0` (2026-04-06). | `git log`, PyPI history |
| Latest release | **0.9.97**: PyPI 2026-10-27, tag `v0.9.97` 2026-10-27 18:15 UTC. There are 245 tags, so it releases almost daily. | PyPI, `git tag` |
| Stars | **About 139k** (139.2k), 13.3k forks and 412 watchers on 2026-10-30. Third-party figures vary widely: 68.9k (Augment, June 2026), 92.6k (graphify.com search snippet) and 134k (Enterprise DNA, 2026-10-24, search snippet). The growth is extraordinary for a six-month-old repo; treat star counts as popularity, not quality. | GitHub page; search snippets |
| Commit activity | 2,625 commits on `v8`. By month: Apr 240, May 362, Jun 317, Jul 416, Aug 303, Sep 418, Oct 569 (to Oct 27). 621 commits in the last 30 days, from 404 distinct author names. The founder accounts for about 1,234 of them (`Safi` 635 plus `safishamsi` 599). | `git log` |
| Language / runtime | Python ≥ 3.10. Dependencies: NetworkX, tree-sitter 0.25 plus about 25 grammar wheels, including `tree-sitter-cpp >=0.23,<0.25`. | pyproject.toml |

**What it does.**

- **Building the graph.** `/graphify .` (in the assistant) or `graphify extract` (headless) walks a folder and writes three files to `graphify-out/`: `graph.json`, an interactive `graph.html` and `GRAPH_REPORT.md`.
  - The report lists "god nodes", communities and suggested questions.
  - Code is parsed with tree-sitter, deterministically and locally.
  - Markdown, PDFs and images go through an LLM "semantic pass".
  - Communities are found with Leiden, or Louvain when the extra isn't installed.
- **Edge labels.** Each edge is tagged `EXTRACTED`, meaning explicit in the source, or `INFERRED`, meaning resolved by graphify.
- **Queries:**
  - `graphify query "<question>"` returns a scoped subgraph.
  - `graphify path A B` gives the shortest path between two nodes.
  - `graphify explain X` describes one node and its edges.
  - Exit codes: 0 for an answer, 3 for an ambiguous name, 4 for no match.
- **Other features:** watch mode, git hooks that rebuild the graph on commit and checkout (`graphify hook install`), a wiki export, a PR dashboard (`graphify prs`), and Neo4j and FalkorDB export.
- **Paid tier.** Graphify Cloud is a hosted product, which the README advertises heavily.

**How it hooks into Claude Code.** Checked in `graphify/install.py` and `graphify/cli.py`.

1. **`graphify install`** detects Claude Code and does two things:
   - writes the skill to `~/.claude/skills/graphify/SKILL.md`, a 793-line skill with a `references/` sidecar;
   - appends a `# graphify` registration block to `~/.claude/CLAUDE.md`.
   With `--project`, both go to `.claude/skills/graphify/SKILL.md` and `.claude/CLAUDE.md` instead.
2. **`graphify claude install`** ("always-on") also does two things:
   - writes a `## graphify` section into `./CLAUDE.md`;
   - merges two **PreToolUse hooks** into `.claude/settings.json`, keeping a backup of the old file:
     - `{"matcher": "Bash|Grep", "command": "<abs path>/graphify hook-guard search", "timeout": 10}`
     - `{"matcher": "Read|Glob", "command": "<abs path>/graphify hook-guard read", "timeout": 10}`

   The guard emits `hookSpecificOutput.additionalContext` nudges, such as "this project has a knowledge graph … `graphify query` … usually in far fewer tokens". With `--strict` (or `GRAPHIFY_HOOK_STRICT=1`) it returns `permissionDecision: "deny"` for the first raw `Read` of each session. The code comments say the guard takes about 170 ms when warm, it fails open, and it runs before **every** Bash, Grep, Read and Glob call.
3. **MCP server** (optional extra `graphifyy[mcp]`). The console script `graphify-mcp`, or `python -m graphify.serve graphify-out/graph.json`, serves the graph over stdio by default. `--transport http` binds to 127.0.0.1 and accepts `--api-key`. The tools are `query_graph`, `get_node`, `get_neighbors`, `get_community`, `god_nodes`, `graph_stats`, `shortest_path`, `list_prs`, `get_pr_impact` and `triage_prs` (from `serve.py`).
4. **Git hooks.** `graphify hook install` runs a rebuild after each commit and branch checkout.

**Install commands, copied verbatim from the README (2026-10-30, v0.9.97):**

```bash
uv tool install graphifyy      # install the CLI (or: pipx install graphifyy)
graphify install               # detects your assistant and registers the skill
# then, in the assistant:
/graphify .
# project-scoped skill instead of user profile:
graphify install --project
# "always-on" guidance + PreToolUse hooks:
graphify claude install
# optional MCP server extra:
uv tool install "graphifyy[mcp]"
# removal:
graphify uninstall            # --purge also deletes graphify-out/
```

The README says: "The PyPI package is `graphifyy` (double-y). Other `graphify*` packages on PyPI are not affiliated."

**C++ support.** Checked in `graphify/extract.py`, `graphify/extractors/engine.py`, `graphify/detect.py` and the tests.

- **Parser.** tree-sitter-cpp, with `_CPP_CONFIG` in `extract.py`. This is a purely syntactic parse: there is no compilation database and no semantic analysis.
- **Extensions.**
  - Routed to the C++ extractor: `.cpp`, `.cc`, `.cxx`, `.hpp`, `.cu`, `.cuh` and `.metal`.
  - `.h` is routed by its content to C, C++ or Objective-C.
  - `.hh`, `.hxx` and `.inl` are known as C-family header extensions.
  - **Not recognised at all:** `.cppm` and `.ixx` (module units) and `.ipp` and `.tpp`. So `modules/trellis.cppm` would be ignored.
- **Extracted:**
  - class, struct, enum and union specifiers, and function definitions;
  - `#include` edges, call expressions, and base classes, including templated bases;
  - aliases (`using X = …` and `typedef`) at namespace scope;
  - enumerators;
  - Catch2 and doctest `TEST_CASE("name")`, recovered with a regex because tree-sitter drops them (#2594).
  - Template type parameters are treated as local names, so they don't merge into one "T" god node (`tests/test_cpp_template_type_params.py`).
- **Not modelled.** No grep hits in the sources for:
  - `concept_definition`, `requires_clause` and `static_assert`;
  - explicit or partial specialisations as separate entities;
  - template instantiation;
  - C++20 module declarations.

  A comment in `engine.py` also says "C++ never populates namespace_stack", so node IDs are not namespace-qualified, and a `detail::type_list` and a `meta::type_list` can be told apart only by file.
- **Consequence for Trellis.** Most of what matters in a metaprogramming-heavy header-only library is type-level and never shows up as an AST call or include edge:
  - which specialisation of `ctor_traits<T>` is chosen;
  - which concept gates `resolvable<…>`;
  - how `binding_map` flattens;
  - where a `static_assert` fires.

  graphify would mostly map files, classes and `#include`s, which `Grep` and clangd already give.

**Token and cost claims, and who makes them:**

| Claim | Who | Basis |
|---|---|---|
| "71.5x fewer tokens per query vs reading the raw files" | graphify author, README April 2026 | One mixed corpus of 52 files: "Karpathy repos + 5 papers + 4 images" (`worked/karpathy-repos/`). It was **removed from the README on 2026-05-03** (commit `b11a8a78`, "simplify README"). The old README itself said a 6-file corpus "fits in a context window anyway". |
| "Up to 71.5x fewer tokens", "70x", "cut your AI token consumption by 70x" | Third parties: `lucasrosati/claude-code-memory-setup`, MindStudio, Medium, levelup.gitconnected and TowardsAI posts (search snippets) | They repeat the author's figure. |
| "40–70% reduction in typical workflows" | AWS Builder Center community post (search snippet) | "Practical developer reports", with no method given. |
| "5–10x in practice", plus a "71.5x caveat" | Mejba Ahmed, hands-on review (search snippet; the page itself was blocked) | Not verified. |
| "Cut agent token spend by 80%" | Graphify Labs, current README banner | About the **paid Graphify Cloud**. No method given. |
| ERPNext (about 1M LOC) cross-tool test: key-fact coverage 70.8% with a grep-and-read baseline vs 82.0% with graphify, "at about 140K tokens per query"; stuffing the whole repo into context "costs roughly 20x the tokens" | Graphify Labs, `BENCHMARKS.md` | **n = 6 questions**, judged by an LLM, on a Python repo. This is an accuracy claim, not a token-saving claim. |
| "263× fewer tokens on httpx" | `graphlore` (formerly yasinyaman/graphify-mcp), README | Self-reported; tokens estimated as chars/4; six queries on one HTTP-client library. |

None of these is an independent measurement, and none was made on C++.

**Limitations, privacy and security:**

- **What leaves the machine.**
  - Code: nothing; it is parsed locally. Video and audio: nothing; they are transcribed locally with faster-whisper.
  - **Docs, PDFs and images are sent to an LLM.** Inside Claude Code, the skill *mandates* dispatching parallel `general-purpose` subagents, which have Write and Bash access, to extract them. That spends your Claude tokens.
  - Headless `graphify extract` picks a backend from whichever API key it finds, in this order: Gemini → Kimi → Claude → OpenAI → DeepSeek → Azure → Bedrock → Ollama. **Kimi (`MOONSHOT_API_KEY`) routes to Moonshot AI servers in China** (README).
  - The fallback that labels communities can also shell out to `claude -p`, Opus by default.
  - To prevent all of this: use `--code-only`, or set `GRAPHIFY_BACKEND=none`.
- **Local logging.** Every query, path, explain and MCP `query_graph` call is appended to `~/.cache/graphify-queries.log`. Opt out with `GRAPHIFY_QUERY_LOG_DISABLE=1`. The README says there is no telemetry.
- **Code it runs on your machine:**
  - a PyPI package with about 30 native-wheel dependencies, released almost daily;
  - a hook process before every Bash, Grep, Read and Glob call;
  - optional git hooks that rebuild on every commit and checkout;
  - a 793-line skill that tells Claude to run Python snippets and spawn subagents.
- **Writes outside the repo:** `~/.claude/CLAUDE.md` and `~/.claude/skills/`, unless you pass `--project`.
- **SECURITY.md** documents these defences:
  - SSRF guards on `add <url>`;
  - prompt-injection mitigation, by wrapping files in `<untrusted_source sha256=…>` and defanging chat-template tokens; it says this "does not make injection impossible";
  - no `shell=True`;
  - the HTTP MCP transport is opt-in and binds to 127.0.0.1.
- **Package-name risk.** The double-y name `graphifyy` invites typosquats. Search snippets of third-party skill audits (skills.sh "agent trust hub") flag the name mismatch and the per-commit git hook. Verify that the PyPI project links back to `Graphify-Labs/graphify`; as of 2026-10-30 it does.
- **Freshness.** The graph goes stale as soon as code changes. The hook's "STALE" nudge depends on `graphify update` or the git hooks.

### 1.3 Other projects with the same name

| Project | What it is | Notes |
|---|---|---|
| gmh5225/graphify, belinwu/graphify, Nectrion/graphify, missflash/graphify, Thizeidler/graphify, 8silvergun/graphify | Forks or copies of the main repo | Don't install from forks. |
| esmatcm/setup-graphify, chencore/graphify-knowledge-graph-docs, lucasrosati/claude-code-memory-setup | Setup scripts and guides around graphify. The last pairs it with Obsidian and claims "up to 71.5x fewer tokens". | Third party. |
| yasinyaman/graphify-mcp, renamed **graphlore** | An MCP server wrapping the graphify CLI: 28 tools, MIT, about 5 stars, 59 commits, `pip install graphlore`. It needs `graphifyy`. | Tiny project; self-reported benchmarks. |
| `stden/graphify` (Docker Hub), `@audsiui/pi-graphify` (npm, Pi agent extension) | Third-party packagings | Search snippets only. |

---

## 2. Alternatives and complements for code understanding

### 2.1 Official clangd plugin for Claude Code (`clangd-lsp`) — recommended

- **Source:** the Claude Code docs, "Code intelligence plugins" (https://code.claude.com/docs/en/plugins/code-intelligence.md), and the `claude-plugins-official` marketplace manifest (https://github.com/anthropics/claude-plugins-official, last commit 2026-10-27, 315 plugins).
- **Manifest entry:** `clangd-lsp` version 1.0.0, author Anthropic, license Apache-2.0. It contains only a README and an inline `lspServers` entry:

  ```json
  "lspServers": { "clangd": { "command": "clangd", "args": ["--background-index"],
    "extensionToLanguage": { ".c": "c", ".h": "c", ".cpp": "cpp", ".cc": "cpp", ".cxx": "cpp",
                             ".hpp": "cpp", ".hxx": "cpp", ".C": "cpp", ".H": "cpp" } } }
  ```

- **What Claude gains:**
  - Diagnostics after every edit: "Found N new diagnostic issues…".
  - A read-only `LSP` tool: go to definition, find references, type info at a position, document symbols, workspace symbol search, implementations, and call hierarchy (tools reference, "LSP tool behavior").
- **Limits:**
  - It doesn't work in cloud sessions: "Claude Code doesn't start plugin language servers" there.
  - You install the `clangd` binary yourself.
  - `.h` is mapped to C, and `.cppm`, `.ixx`, `.ipp`, `.tpp` and `.inl` aren't mapped. Trellis should use `.hpp`. To cover `.cppm`, write a local plugin with its own `.lsp.json` (the docs show the format).
  - clangd needs `compile_commands.json`. A header gets its flags from a translation unit that includes it, so every public header must be included by at least one test TU. clangd also reads a `.clangd` file, for example to point `CompilationDatabase` at `build/dev`.
- **Install:**

  ```bash
  sudo apt install clangd            # or: brew install llvm / winget install LLVM.LLVM (plugin README)
  # in Claude Code:
  /plugin install clangd-lsp@claude-plugins-official
  ```

- **Value for templates.** Real compiler front-end diagnostics on headers that are being edited are the main win. Claude sees a broken `requires` clause or a failed substitution right after the edit, without waiting for a full build. Navigation inside *uninstantiated* templates relies on clangd heuristics; dependent names resolve partially. This is still far more accurate than tree-sitter or grep for C++.

### 2.2 Serena (oraios/serena)

| Field | Value |
|---|---|
| Repo | https://github.com/oraios/serena (Oraios AI: Dominik Jain and Michael Panchenko) |
| License | Per component. SolidLSP (the LSP client library) is MIT. **The Serena application has been GPL-3.0-or-later since v2.** The last MIT release was v1.7.0 (marker tag `mit-final`, 2026-09-14). It began under GPL-2 and switched to MIT on 2025-05-01. Contributions need a CLA. |
| First / latest | First commit 2025-03-23; first tag 2025-04-01. v2.0.0 on 2026-10-13, **v2.1.0 on 2026-10-19**, last commit 2026-10-28. |
| Activity | 3,620 commits. Monthly in 2026: Jan 181, Feb 192, Mar 177, Apr 372, May 123, Jun 105, Jul 190, Aug 102, Sep 129, Oct 171. |
| Stars | About 30.9k, with 2.1k forks (GitHub page, 2026-10-30). PyPI `serena-agent` 2.1.0 is GPL-3.0-or-later. |
| What it does | An MCP server that gives the agent IDE-like symbolic tools: find symbol, find referencing symbols, symbol overview, replace symbol body, insert before or after a symbol, rename. It also has memories and a dashboard. The backend is LSP, which is free, or a paid JetBrains plugin; the JetBrains plugin doesn't support CLion. |
| C++ | clangd by default, with ccls optional. **Serena downloads clangd 19.1.2 itself and checks it against pinned SHA-256s.** It needs `compile_commands.json` at the repo root and rewrites relative paths into `.serena/compile_commands.json` (`docs/03-special-guides/cpp_setup.md`). |
| Claude Code integration | MCP server, plus optional "reminder hooks" (alpha) to counter "agent drift". |
| Install (README and docs, verbatim) | `uv tool install -p 3.13 serena-agent`, then `serena init`, then `serena setup claude-code`, or manually: `claude mcp add serena -- serena start-mcp-server --context claude-code --project "$(pwd)"` (per project) or `claude mcp add --scope user serena -- serena start-mcp-server --context claude-code --project-from-cwd`. The README warns: "**Do not install Serena via an MCP or plugin marketplace!** They contain outdated and suboptimal installation commands." Anthropic's official marketplace entry runs `uvx --from git+https://github.com/oraios/serena serena start-mcp-server`, which is **unpinned**: it fetches the default branch head. |
| Security model | It assumes the machine, the client (LLM), the repository and the package-manager config are all trusted. It has shell-execution and file-editing tools. It recommends Docker or a sandbox. "Trusted projects" (since v1.6) is "a functionality boundary, not a containment boundary" (`docs/02-usage/070_security.md`). |
| Verdict for Trellis | Its retrieval mostly duplicates `clangd-lsp` now that Claude Code has a native LSP tool. Its extra is symbol-level *editing*. Adopting it would add another MCP server, a GPL application, more tool definitions in context, and its own clangd download. **Optional; not in the minimal set.** |

### 2.3 Code-graph and RAG tools: code-graph-rag and GitNexus

| | code-graph-rag | GitNexus |
|---|---|---|
| Repo | https://github.com/vitali87/code-graph-rag (vitali87) | https://github.com/abhigyanpatwari/GitNexus (Abhigyan Patwari; commercial licences through akonlabs.com) |
| License | MIT | **PolyForm Noncommercial 1.0.0**; the npm `license` field agrees. Commercial use needs a paid licence. |
| Stars (2026-10-30) | About 5.3k (708 forks) | About 48.1k (5.2k forks) |
| Activity and releases | 10,998 commits; last commit 2026-10-30. PyPI `code-graph-rag` has 36 releases: the first, 0.0.79, on 2026-02-21; the latest, 0.4.5, on 2026-10-30. | About 2,300 commits; last commit 2026-10-30. npm `gitnexus` was created 2026-02-06; the latest, 1.6.12, came out 2026-09-12. |
| What it does | Parses code with tree-sitter into a graph, stored by default in embedded SQLite (`~/.cgr/graph.sqlite`), with optional Memgraph or Neo4j plus Qdrant (`cgr daemon up`). Natural-language queries go through an LLM that writes Cypher. Also has editing tools. | A tree-sitter index of symbols, calls, dependencies and execution flows. MCP tools: `query`, `context`, `impact`, `trace`, `detect_changes`, `rename`. |
| Claude Code integration | An MCP server; the setup guide is not on the README page. | `npx gitnexus analyze` builds the index **and also installs agent skills, registers Claude Code hooks, and writes AGENTS.md and CLAUDE.md**; `--index-only` skips that. `npx gitnexus setup` configures MCP. `gitnexus mcp --http --port 3111` runs a shared server. |
| Install (README) | `uv tool install "code-graph-rag[treesitter-full,ast-grep,semantic]"` | `npx gitnexus analyze` or `npm install -g gitnexus` |
| C++ | tree-sitter, plus an **optional libclang layer** for "compiler-grade facts" where a toolchain is available (opt-in semantic mode) | tree-sitter; "support varies by construct" |
| Token claims | None in the README | None in the README |
| Privacy | An LLM provider is needed for Cypher generation; the README page doesn't name it. | Indexing is local with no LLM. Optional remote embeddings, wiki and chat use whichever providers you configure. |

**Verdict.** Both are tree-sitter graphs with graphify's blind spot for template metaprogramming. code-graph-rag's opt-in libclang mode is the exception, but how well it handles template metaprogramming is untested. GitNexus has two strikes for Trellis: its non-commercial licence, and by default it writes CLAUDE.md and hooks. Neither belongs in the minimal set.

### 2.4 LSP bridges over MCP

**isaacphi/mcp-language-server** (https://github.com/isaacphi/mcp-language-server):

- **Facts.** BSD-3-Clause, about 1.6k stars, 104 commits. The **last commit was on 2025-06-03**, a dependency bump, so the project is effectively dormant. The README calls it "beta software", and it has 36 open issues and 34 open PRs.
- **Tools:** `definition`, `references`, `diagnostics`, `hover`, `rename_symbol`, `edit_file`.
- **clangd setup.** The README configures clangd as `mcp-language-server --workspace <project> --lsp <clangd path> -- --compile-commands-dir=<dir>`. This is a paraphrase of the README's structure, not a verbatim copy.
- **Verdict.** It is **superseded** by Claude Code's native LSP support (`clangd-lsp`, §2.1), which also pushes diagnostics automatically after each edit. Don't add it.

Serena (§2.2) is the other LSP bridge.

### 2.5 Documentation MCPs and cppreference

**Context7** (https://github.com/upstash/context7):

- **Facts.** MIT, about 64.0k stars. The npm package `@upstash/context7-mcp` was created 2025-04-08; its latest release is 4.4.4, on 2026-10-19.
- **Setup.** The remote server is `https://mcp.context7.com/mcp`. There is also a CLI setup, `npx ctx7 setup` (with `--claude`).
- **What runs where.** The README says "the API backend, parsing engine and crawling engine are private". Its disclaimer says projects are "community-contributed" and that Upstash cannot guarantee their "accuracy, completeness, or security". So the content is third-party text injected into Claude's context.

Context7 in the official marketplace is a **remote HTTP MCP** at `https://mcp.context7.com/mcp?client=claude-code-plugin`. The plugin is by Upstash and tagged "community-managed". It works anonymously, and `CONTEXT7_API_KEY` raises the rate limits (manifest).

- **Install:** `/plugin install context7@claude-plugins-official`.
- **cppreference.**
  - **No cppreference MCP server was found.** cppreference.com and context7.com were both blocked from this session, so whether Context7 indexes cppreference couldn't be checked.
  - Offline options that need no third-party server:
    - the **offline cppreference HTML archive**, https://github.com/PeterFeicht/cppreference-doc. Its latest tag is `v20250209`, so it lags the live site by about 20 months.
    - the **C++ working-draft sources**, https://github.com/cplusplus/draft (LaTeX; checked with `git ls-remote`). Claude can grep the normative wording, for example [temp.constr] or [over.match].
- **Value for Trellis: low to moderate.** The hard questions in Trellis are about what each compiler actually does with a construct, and compiling answers those better than docs do. Context7 helps most with third-party APIs: GoogleTest, Google Benchmark, CMake. **Optional.**

### 2.6 Compiler Explorer (godbolt) MCP

**compiler-explorer/ce-mcp** (https://github.com/compiler-explorer/ce-mcp):

- **Facts.** Its description is "Experimental official ce-mcp". BSD-2-Clause, 8 stars, 59 commits, no releases; the **last commit was 2026-03-21**.
- **Install** (README):

  ```bash
  git clone https://github.com/compiler-explorer/ce-mcp && cd ce-mcp && ./setup-claude-code.sh
  ```

  The manual route is `uv venv` and then `uv pip install -e ".[dev]"`.
- **Tools (12):**
  - `compile_check_tool`, `compile_and_run_tool`, `compile_with_diagnostics_tool`
  - `analyze_optimization_tool`, `compare_compilers_tool`, `lookup_instruction_tool`
  - `generate_share_url_tool`, `find_compilers_tool`
  - `get_libraries_tool`, `get_library_details_tool`, `get_languages_tool`, `download_shortlink_tool`
- **Privacy.** The default endpoint is `https://godbolt.org/api`, so code is sent to the **public service**, and the share-URL tool publishes it.

**Other servers** (search snippets only):

- `ce-analyzer-mcp` on PyPI: alpha; versions 0.1.0 and 0.2.1 were both uploaded 2026-08-12; no license in the metadata.
- benestar's `godbolt-mcp` (an mcp.so listing).
- zzuse/mcp_compiler_explorer, a demo.
- MCPBundles lists a hosted `godbolt.org/mcp` endpoint. It is unverified, because godbolt.org is blocked here.

**A lighter option needs no MCP at all.** The REST API is documented in `compiler-explorer/compiler-explorer/docs/API.md`, which I checked: `GET /api/compilers/<language-id>` and `POST /api/compiler/<compiler-id>/compile` with `{"source": "...", "options": {"userArguments": "-std=c++20 -O2"}}`. Claude can call it with `curl`.

**Value for Trellis: real but narrow.**

- Quick checks across compilers that aren't on a Linux dev box, especially MSVC and many GCC and Clang versions.
- Side-by-side diagnostics when writing golden tests.
- Codegen checks for the plan's §8.2 test that asserts no calls into `trellis::` remain.

For an open-source library, sending code to the public service is acceptable. The CI matrix stays the source of truth. **Optional.**

### 2.7 GitHub tooling

The official marketplace `github` plugin is a **remote HTTP MCP** at `https://api.githubcopilot.com/mcp/`, authenticated with `Authorization: Bearer ${GITHUB_PERSONAL_ACCESS_TOKEN}` (manifest).

**GitHub MCP server** (https://github.com/github/github-mcp-server):

- **Facts.** MIT, about 34.1k stars, latest tag `v2.0.3`.
- **Read-only and toolset-scoped endpoints.**
  - Remote read-only URL: `https://api.githubcopilot.com/mcp/readonly`.
  - Per-toolset URLs: `https://api.githubcopilot.com/mcp/x/<toolset>[/readonly]` (`docs/remote-server.md`).
  - The local server accepts `--read-only` and `--toolsets` (or `GITHUB_TOOLSETS`).
- **Claude Code commands** (`docs/installation-guides/install-claude.md`, verbatim):

  ```bash
  claude mcp add-json github '{"type":"http","url":"https://api.githubcopilot.com/mcp","headers":{"Authorization":"Bearer YOUR_GITHUB_PAT"}}'
  claude mcp add --transport http github https://api.githubcopilot.com/mcp -H "Authorization: Bearer YOUR_GITHUB_PAT"   # Claude Code <= 2.1.0
  ```

**gh CLI** (https://github.com/cli/cli):

- **Facts.** MIT, latest tag `v2.103.3`; 2.89.0 is installed here.
- **Why it's enough.** Claude drives it through Bash, which adds no MCP tool definitions to the context. It covers the CI loop that matters for MSVC, whose failures show up only in CI: `gh pr checks`, `gh run list --workflow ci.yml`, `gh run view <id> --log-failed`, `gh run download <id>`.

**Verdict.** **Use the gh CLI.** Use the GitHub MCP only if gh isn't available, and then through the `/readonly` URL with a fine-grained PAT scoped to the one repository. Neither tool helps with templates; both help close the CI feedback loop.

---

## 3. C++ tools to wire into hooks and CI

**How this was checked.** Snippets marked *verified* were run in a throwaway project with CMake 3.28.3, Ninja 1.11.1, ccache 4.9.1, GCC 13.3 and Clang 18.1.3. The others come from the tools' READMEs.

**Latest upstream tags** (`git ls-remote`, 2026-10-30):

| Area | Tools and tags |
|---|---|
| Test frameworks | googletest `v1.18.0`, Catch2 `v3.16.1`, doctest `v2.5.3` |
| Benchmarking | google/benchmark `v1.10.0`, nanobench `v4.6.0`, hyperfine `v2.0.0` (and `v1.21.0`) |
| Build analysis and includes | ClangBuildAnalyzer `v1.6.0`, IWYU `0.27` |
| Compiler caches | ccache `v4.14.2`, sccache `v0.19.0` |
| pre-commit | pre-commit `v4.7.0`, pre-commit-hooks `v6.0.0`, mirrors-clang-format `v23.1.3` (= PyPI `clang-format` 23.1.3), cpp-linter-hooks `v1.8.1` |

### 3.1 CMake presets, Ninja and ccache *(verified)*

`CMakePresets.json`:

```json
{
  "version": 6,
  "cmakeMinimumRequired": { "major": 3, "minor": 25, "patch": 0 },
  "configurePresets": [
    { "name": "dev", "generator": "Ninja", "binaryDir": "${sourceDir}/build/${presetName}",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Debug", "CMAKE_EXPORT_COMPILE_COMMANDS": "ON",
                          "CMAKE_CXX_COMPILER_LAUNCHER": "ccache", "TRELLIS_BUILD_TESTS": "ON" } },
    { "name": "asan", "inherits": "dev",
      "cacheVariables": { "CMAKE_CXX_FLAGS": "-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all" } },
    { "name": "tsan", "inherits": "dev", "cacheVariables": { "CMAKE_CXX_FLAGS": "-fsanitize=thread" } },
    { "name": "timetrace", "inherits": "dev",
      "cacheVariables": { "CMAKE_CXX_COMPILER": "clang++", "CMAKE_CXX_COMPILER_LAUNCHER": "", "CMAKE_CXX_FLAGS": "-ftime-trace" } }
  ],
  "buildPresets": [ { "name": "dev", "configurePreset": "dev" } ],
  "testPresets": [ { "name": "dev", "configurePreset": "dev", "output": { "outputOnFailure": true } } ],
  "workflowPresets": [ { "name": "dev", "steps": [
    { "type": "configure", "name": "dev" }, { "type": "build", "name": "dev" }, { "type": "test", "name": "dev" } ] } ]
}
```

**What I checked:**

- `cmake --list-presets` listed all four presets.
- `cmake --workflow --preset dev` configured, built and tested, and wrote `build/dev/compile_commands.json`.
- `ccache -s` counted the compile as cacheable.
- The `asan` build ran clean.

**Notes:**

- **CMake versions.** Presets file `version` 6, needed for workflow presets, requires CMake 3.25 or later on developer machines. The library's own `cmake_minimum_required(VERSION 3.21)` (plan §10) is unaffected.
- **Turn the cache off when measuring.** Disable ccache for compile-time measurements and for `-ftime-trace` builds; the `timetrace` preset sets an empty launcher. Otherwise you measure cache hits. Compile-fail tests that invoke `${CMAKE_CXX_COMPILER}` directly (§3.4) never go through the launcher.
- **MSVC in CI.** sccache is the usual cache for MSVC. It needs embedded debug info (`/Z7`), which you get with `CMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded`, available from CMake 3.25 (policy CMP0141). *This is from the tools' documentation and was not checked here.*

### 3.2 clang-format and clang-tidy *(verified)*

**Commands:**

```bash
# format check, whole tree / changed lines only
clang-format --dry-run --Werror $(git ls-files '*.hpp' '*.cpp')
git clang-format --diff origin/main
# clang-tidy on changed lines only (headers included; exits 1 on a new violation)
git diff -U0 --no-color origin/main...HEAD | clang-tidy-diff -p1 -path build/dev -quiet -j 8
# full run (nightly); the trailing regex restricts it to real TUs (see the gotcha below)
run-clang-tidy -p build/dev -header-filter='include/trellis/.*' -quiet -j 8 'tests/'
```

**Gotcha, verified.** If you don't filter the files, `misc-include-cleaner` fails on the stub TUs that `VERIFY_INTERFACE_HEADER_SETS` generates (§3.4): "included header meta.hpp is not used directly". With the `'tests/'` file regex, the run exits 0.

`.clang-tidy`, the plan's §9.1 check families:

```yaml
Checks: "-*,bugprone-*,performance-*,modernize-*,-modernize-use-trailing-return-type,misc-include-cleaner"
WarningsAsErrors: "*"
HeaderFilterRegex: "include/trellis/.*"
```

**What I checked.** `clang-tidy-diff` ran clang-tidy directly on an edited `include/trellis/*.hpp`, borrowing compile flags from `compile_commands.json`. It reported only the changed lines and **exited 1** on new `readability-implicit-bool-conversion` and `modernize-use-trailing-return-type` errors.

**Notes:**

- `modernize-use-trailing-return-type` is noisy, so it is disabled above.
- Ubuntu installs the script as `/usr/bin/clang-tidy-diff`; upstream it is `clang-tidy-diff.py`.
- Its default `-iregex` covers `.h`, `.hpp`, `.cpp` and a few others, but not `.cppm`.
- **Caveat for header-only code.** Headers are analysed through the translation units that include them. Every public header must therefore be included by some test TU, which §3.4 enforces.

**Claude Code hook: format on edit.** This uses the docs' PostToolUse `Edit|Write`, `jq` and `$CLAUDE_PROJECT_DIR` pattern (hooks-guide). The `jq` extraction was checked against a PostToolUse payload.

`.claude/hooks/format-cpp.sh`:

```bash
#!/usr/bin/env bash
# PostToolUse: format the C++ file Claude just edited. Never blocks.
f=$(jq -r '.tool_input.file_path // empty')
case "$f" in *.hpp|*.cpp|*.cppm|*.h) clang-format -i -- "$f" ;; esac
exit 0
```

`.claude/settings.json`:

```json
{ "hooks": { "PostToolUse": [ { "matcher": "Edit|Write",
  "hooks": [ { "type": "command", "command": "\"$CLAUDE_PROJECT_DIR\"/.claude/hooks/format-cpp.sh" } ] } ] } }
```

**Don't build or run clang-tidy on every edit.** A template-heavy TU takes seconds to minutes, and the clangd plugin already returns diagnostics after each edit. Run `clang-tidy-diff` in pre-commit and CI, or in a `Stop` hook.

### 3.3 Compile-time profiling: `-ftime-trace`, ClangBuildAnalyzer and hyperfine

- **`-ftime-trace`** *(verified with Clang 18)*. The `timetrace` preset writes `build/timetrace/CMakeFiles/unit.dir/tests/unit/main.cpp.json` next to each object file. Clang also accepts `-ftime-trace-granularity=<µs>`. Open the file in Perfetto or `chrome://tracing`.
- **ClangBuildAnalyzer** (https://github.com/aras-p/ClangBuildAnalyzer; Unlicense, about 1.3k stars, `v1.6.0`; needs Clang 9 or later). It ranks the costliest template instantiations and headers, which gives the plan's C9 "top-10 list":

  ```bash
  ClangBuildAnalyzer --start build/timetrace
  cmake --build build/timetrace
  ClangBuildAnalyzer --stop build/timetrace build/cba.bin
  ClangBuildAnalyzer --analyze build/cba.bin
  ```

- **hyperfine** (https://github.com/sharkdp/hyperfine). Use it for quick A/B checks while Claude iterates on a template change. The flags come from the README:

  ```bash
  hyperfine -N --warmup 1 --runs 5 --export-json ct.json \
    'clang++ -std=c++20 -Iinclude -fsyntax-only benchmarks/compile/graph_100.cpp'
  ```

  It doesn't report memory, so keep the plan's `measure.py` (median of 3, peak RSS through `wait4`) as the CI gate.
- **GCC and MSVC.** GCC has `-ftime-report`, which reports per compiler pass rather than per template. MSVC has C++ Build Insights (vcperf). *Neither was checked here.*

### 3.4 Tests: GoogleTest, compile-fail via CTest, header self-containment

**GoogleTest/gMock** is the plan's choice, and v1.18.0 keeps it. The archive's SHA-256 was computed here from the GitHub tarball:

```cmake
include(FetchContent)
FetchContent_Declare(googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.18.0.tar.gz
  URL_HASH SHA256=b3e03aaaff81d68730d67392a135ac3fdfdf022880c66632ab188d2fe084cda3)
# CORRECTION (2026-10-30, found during T0.1): this hash is the SHA-256 of the network proxy's
# 403 error page, not of the GoogleTest tarball, so the snippet above can never succeed.
# Trellis instead fetches GoogleTest with git, pinned to the commit of tag v1.18.0
# (see tests/CMakeLists.txt).
FetchContent_MakeAvailable(googletest)
target_link_libraries(unit PRIVATE GTest::gtest_main GTest::gmock)
include(GoogleTest)
gtest_discover_tests(unit)
```

Catch2 (`v3.16.1`) and doctest (`v2.5.3`) bring nothing Trellis needs. Consider doctest only if the test binaries' own compile time becomes a bottleneck.

**Compile-fail tests through CTest** *(verified)*. With `PASS_REGULAR_EXPRESSION` set, CTest ignores the exit code and passes the test when the output matches. Put the error ID only in `static_assert` messages, so a snippet that compiles can't pass by accident. For MSVC, use `/Zs` in place of `-fsyntax-only`.

```cmake
add_test(NAME compile_fail.E003
         COMMAND ${CMAKE_CXX_COMPILER} -std=c++20 -fsyntax-only -I${CMAKE_CURRENT_SOURCE_DIR}/include
                 ${CMAKE_CURRENT_SOURCE_DIR}/tests/compile_fail/E003_duplicate.cpp)
set_tests_properties(compile_fail.E003 PROPERTIES PASS_REGULAR_EXPRESSION "TRELLIS E003")
```

The golden-diagnostics layer extends this: capture the output, normalise it, and diff it against a snapshot per compiler, using the Python runner from plan §9.1.

**Header self-containment** *(verified with CMake 3.28; the feature dates from CMake 3.24)*. The `all_verify_interface_header_sets` target compiles each public header on its own. That is cheaper than IWYU at catching missing includes:

```cmake
target_sources(trellis INTERFACE FILE_SET HEADERS BASE_DIRS include FILES include/trellis/meta.hpp ...)
set_target_properties(trellis PROPERTIES VERIFY_INTERFACE_HEADER_SETS ON)
# cmake --build build/dev --target all_verify_interface_header_sets
```

### 3.5 include-what-you-use vs include-cleaner

**IWYU** (https://github.com/include-what-you-use/include-what-you-use, `0.27`):

- It must be built against the matching Clang. Its README says "IWYU master follows Clang main", and it keeps a `clang_<version>` branch for each Clang release.
- Usage: `cmake -DCMAKE_CXX_INCLUDE_WHAT_YOU_USE=include-what-you-use ...` or `iwyu_tool.py -p .`.
- Its handling of template-heavy headers is imperfect.

**Recommendation.** Use `misc-include-cleaner` in clang-tidy (`clang-include-cleaner-18` is installed here) together with the header-set check in §3.4. Run IWYU at most as an occasional nightly job.

### 3.6 Runtime benchmarks

- **Google Benchmark** is the plan's choice. Flags from `docs/user_guide.md`:

  ```bash
  ./runtime_bench --benchmark_repetitions=10 --benchmark_report_aggregates_only=true \
    --benchmark_out=bench.json --benchmark_out_format=json
  ```

  Use `benchmark::DoNotOptimize(x)` to stop the compiler eliding results.
- **nanobench** (`v4.6.0`, single header) is lighter: `ankerl::nanobench::Bench().run("resolve", [&]{ ...; ankerl::nanobench::doNotOptimizeAway(x); });`. Optional.
- **Measuring at this scale.** Differences of 2–9 ns per resolution, as in the research, need a pinned CPU (`taskset`), repetitions and medians. Tell Claude that noise inside the run-to-run spread is not a regression.

### 3.7 Sanitizers *(verified with GCC 13)*

The `asan` and `tsan` presets are in §3.1.

```bash
cmake --preset asan && cmake --build build/asan && ctest --test-dir build/asan --output-on-failure
```

`-fno-sanitize-recover=all` makes UBSan findings fatal, so CI fails on them. MSVC offers only `/fsanitize=address`, so TSan and UBSan jobs must use GCC or Clang. *(The MSVC point is from Microsoft's documentation and was not checked here.)*

### 3.8 pre-commit

This config was **not executed** here; the revs are the latest tags on 2026-10-30.

```yaml
# .pre-commit-config.yaml
repos:
  - repo: https://github.com/pre-commit/pre-commit-hooks
    rev: v6.0.0
    hooks:
      - id: trailing-whitespace
      - id: end-of-file-fixer
      - id: check-merge-conflict
      - id: check-yaml
      - id: check-json
  - repo: https://github.com/pre-commit/mirrors-clang-format
    rev: v23.1.3            # pins clang-format itself (PyPI wheel), same on every machine
    hooks:
      - id: clang-format
        types_or: [c++, c]
  - repo: local
    hooks:
      - id: clang-tidy-diff
        name: clang-tidy (changed lines)
        language: system
        pass_filenames: false
        entry: bash -c 'git diff -U0 --no-color --cached | clang-tidy-diff -p1 -path build/dev -quiet'
```

**Notes:**

- **Pin to SHAs.** `pre-commit autoupdate --freeze` replaces the tags with commit SHAs.
- **One clang-format version everywhere.** The format hook in §3.2 should call the same version as pre-commit, for example by installing it with `uv tool install clang-format==23.1.3`. This machine has 18.1.3, and different versions format differently.

---

## 4. Supply chain and safety for third-party MCP servers, skills and plugins

### 4.1 What the Claude Code docs say

- **MCP** (https://code.claude.com/docs/en/mcp.md): "Verify you trust each server before connecting it. Servers that fetch external content can expose you to prompt injection risk."
- **Plugins** (https://code.claude.com/docs/en/plugins/security.md):
  - "A plugin can carry content that runs code on your machine with your user privileges and content that enters Claude's context as instructions."
  - **Auto-update** can change the files you reviewed.
  - The review steps: `claude plugin marketplace list`; the `/plugin` details pane and its "Will install" section; reading `hooks/hooks.json`, `.mcp.json` and `bin/`; and `claude --plugin-dir <dir> plugin details <name>` for a component inventory.
  - The official and community marketplace names are accepted only from `github.com/anthropics/` repositories.
- **Project MCP approvals.** A cloned repository cannot approve its own servers. A committed `enableAllProjectMcpServers` or `enabledMcpjsonServers` is ignored in an untrusted folder.
- **Permissions:**
  - Deny `mcp__*` to remove every MCP tool from the context.
  - Allow rules must name a specific server, as in `mcp__<server>__*`.
  - `Read(...)` deny rules also apply to the LSP tool.
- **Organisations.** Managed `allowedMcpServers` with `allowManagedMcpServersOnly: true` gives an approved catalogue; `deniedMcpServers` gives a denylist (managed-mcp.md).

### 4.2 Concrete risks found in this research

1. **graphify:**
   - It writes to user-global `~/.claude/CLAUDE.md` and `~/.claude/skills/`.
   - It can install PreToolUse hooks that run before **every** Bash, Grep, Read and Glob call, and git hooks that run on every commit.
   - It releases almost daily (245 tags in seven months) and pulls about 30 native wheels.
   - Its skill tells Claude to spawn subagents with Bash and Write access.
   - Its PyPI name, `graphifyy` with a double y, invites typosquats.
   - In headless mode it **auto-selects an LLM backend from whichever API key is set**, possibly Kimi, whose servers are in China.
2. **The official marketplace's `serena` entry is unpinned** (`uvx --from git+https://github.com/oraios/serena`), so every fresh resolution runs the default branch head. Serena's own README says not to install it from marketplaces.
3. **Remote MCPs send your queries to third parties.** Context7 and the GitHub remote MCP are examples. Context7's docs are community-contributed, which is a prompt-injection channel.
4. **Compiler Explorer MCPs send code to public godbolt.org**, and their share tools publish it.
5. **Plain `npx -y pkg` and `uvx pkg` resolve to the latest version on each fresh install.**

### 4.3 Mitigations

- **Pin everything:**
  - Python tools: `uv tool install graphifyy==0.9.97`, `uv tool install -p 3.13 serena-agent==2.1.0`.
  - npm packages: `npx -y @upstash/context7-mcp@4.4.4`.
  - Git-sourced servers: a tag or SHA, as in `uvx --from git+https://github.com/oraios/serena@v2.1.0 ...`.
  - pre-commit: `--freeze`.
  - Plugin marketplaces: turn auto-update off for third-party marketplaces.
- **Review before installing.** Clone as data, then read the hook commands, the MCP launch lines and anything the skill tells Claude to run. Diff `.claude/settings.json` and `.mcp.json` in code review, because hooks are code.
- **Least privilege:**
  - Prefer project scope (`--project`, `claude mcp add` with the default local scope) to user scope.
  - Use GitHub's `/readonly` endpoint with a fine-grained PAT limited to the Trellis repo.
  - Keep API keys out of MCP server environments unless a server needs them. For graphify, set `GRAPHIFY_BACKEND=none`.
  - Deny the MCP tools you don't use.
- **Isolate:**
  - Claude Code's sandboxed Bash, a dev container, or a VM for anything that executes code.
  - Serena's own docs recommend Docker.
- **Prefer first-party or zero-install routes:** the gh CLI over the GitHub MCP, clangd through the official plugin over MCP bridges, and the Compiler Explorer REST API through `curl` over an experimental MCP.

---

## 5. Recommended minimal tool set

| # | Tool | Why (one line) |
|---|---|---|
| 1 | **clangd plus the official `clangd-lsp` plugin** | Real Clang front-end diagnostics after every edit, plus symbol navigation; the only code-understanding tool here that knows C++ semantics. |
| 2 | **CMake presets plus Ninja plus ccache** (sccache for MSVC CI), exporting `compile_commands.json` | One reproducible command per configuration for Claude and CI, fast rebuilds, and the compile database that clangd and clang-tidy need. |
| 3 | **clang-format (pinned) as a PostToolUse hook and in pre-commit** | Formatting gets fixed deterministically outside the model, so no tokens go to style and diffs stay clean. |
| 4 | **clang-tidy-diff on changed lines** (pre-commit and PR CI), with `run-clang-tidy` nightly | Enforces the plan's §9.1 clang-tidy policy without per-edit latency. |
| 5 | **Sanitizer presets** (ASan plus UBSan, TSan) | The plan's memory-safety and concurrency layers; one preset name for Claude to run. |
| 6 | **`-ftime-trace` plus ClangBuildAnalyzer**, with hyperfine for ad-hoc A/B checks | Compile-time budgets and the C9 top-10 template list are the project's core performance risk. |
| 7 | **CTest compile-fail and golden tests, plus `VERIFY_INTERFACE_HEADER_SETS`**, alongside GoogleTest | Checks error IDs and self-contained headers with nothing beyond CMake. |
| 8 | **gh CLI** | PR and CI-log loop, especially MSVC failures, with no MCP. |

**Optional:** Compiler Explorer (REST API or `ce-mcp`) for checking snippets on MSVC and many compiler versions; Context7 for third-party library docs; Serena for symbol-level refactors; Google Benchmark is already in the plan.

**Not recommended for Trellis:** graphify, GitNexus, code-graph-rag and mcp-language-server.

- **Size.** A header-only library of tens of headers is well within normal Claude Code exploration. graphify's own old README conceded that small corpora "fit in a context window anyway".
- **Wrong level of analysis.** tree-sitter graphs can't see concepts, specialisations or instantiation.
- **Overlap.** The useful part overlaps with clangd.
- **Cost.** They add hooks, latency and supply-chain surface.

**If the user still wants to try graphify,** this is the safe setup. The syntax was checked against `graphify/cli.py` v0.9.97.

```bash
uv tool install graphifyy==0.9.97                      # pinned; double-y PyPI name
cd trellis && echo 'graphify-out/' >> .gitignore
GRAPHIFY_BACKEND=none GRAPHIFY_QUERY_LOG_DISABLE=1 graphify extract . --code-only   # AST only, offline
graphify query "what connects ctor_traits to the injector?"
graphify install --project      # optional: project-scoped /graphify skill instead of ~/.claude
# avoid: `graphify claude install` (PreToolUse hooks on every Bash/Grep/Read/Glob) and `graphify hook install` (git hooks)
```
