# Trellis

A header-only C++20 dependency injection library. It builds and connects the objects in a C++ program for you, checks the wiring when the program compiles, and aims to cost no more at run time than wiring by hand.

> **Status: pre-alpha, not usable yet.** Phase 0 (foundations) is complete. Phase 1 (the core injector, v0.1) is in progress. There is no public injector API yet.

## Goals

- **Compile-time checks.** Wiring is checked at compile time and resolves as fast as hand-written code.
- **Clear errors.** One error per mistake, naming the missing type and the path that needs it.
- **Lifetime safety.** Lifetime rules turn dangling references and captive dependencies into compile errors.
- **Plugins.** Plugins and run-time registration come through a runtime container, planned for 1.0.

The design is in [`docs/plan.md`](docs/plan.md), and the decisions behind it are in [`docs/adr/`](docs/adr/README.md).

## Repository map

| Path | What it holds |
|---|---|
| [`docs/plan.md`](docs/plan.md) | The design and implementation plan |
| [`docs/adr/`](docs/adr/README.md) | Architecture decision records |
| [`docs/PROGRESS.md`](docs/PROGRESS.md) | Progress log: what is done, the evidence, decisions, open items |
| `docs/tasks.json` | The task list, managed with `scripts/tasks.py` |
| `docs/spikes/`, `spikes/` | The Phase 0 experiments behind the decisions |
| `include/trellis/` | The library (header-only) |
| `tests/` | Unit, compile-fail, golden-diagnostics and property tests |
| `benchmarks/` | Compile-time and runtime benchmarks, and their budgets |
| `tools/`, `scripts/` | Test runners and developer scripts |
| [`research/`](research/README.md) | Background research on existing C++ DI libraries |
| [`docs/agent/PROMPTS.md`](docs/agent/PROMPTS.md) | How this project is built with Claude Code |

## Building and testing

You need:

- CMake 3.21 or later (3.28 for the C++20 module);
- Ninja;
- GCC 13 or 14, or Clang 18 or 20 (libc++ optional).

GoogleTest is found on the system or fetched. The benchmarks need Google Benchmark 1.8 or later.

To check tools, configure, build and test the clang-18-debug preset:

```bash
scripts/init.sh
```

To build and test every local preset, including the sanitizer builds:

```bash
scripts/check-full.sh
```

CI runs Linux on every push. macOS and Windows (MSVC) run weekly and on demand.

## Contributing

See [`CONTRIBUTING.md`](CONTRIBUTING.md).

## License

[Boost Software License 1.0](LICENSE). Copyright (c) 2026 Aryan Patel.
