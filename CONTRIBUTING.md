# Contributing to Trellis

Trellis is pre-alpha. The design is in [`docs/plan.md`](docs/plan.md), and the working rules for every change, including the C++ conventions, are in [`CLAUDE.md`](CLAUDE.md). Contributions are accepted under the [Boost Software License 1.0](LICENSE), the project's license.

## Tools

- CMake 3.21 or later, Ninja and Python 3.8 or later. ccache is used when it is installed.
- For the full local matrix: GCC 13 and 14, Clang 18 and 20, and libc++ 18.
- clang-format-18 and clang-tidy-18. Other versions format and lint differently.
- GoogleTest and gMock 1.11 or later as an installed package (on Ubuntu, `libgtest-dev` and `libgmock-dev`). Without one, the build fetches v1.18.0 with git. If your package was built against a different standard library, configure with `-DCMAKE_DISABLE_FIND_PACKAGE_GTest=ON`, as the libc++ preset does.
- For the runtime benchmarks: Google Benchmark 1.8 or later (on Ubuntu, `libbenchmark-dev`).

## Build and test

| Command | What it does |
|---|---|
| `scripts/init.sh` | Checks the tools, then configures, builds and tests `clang-18-debug` |
| `scripts/check-fast.sh` | Checks the format of changed files, then builds and tests `clang-18-debug`. Run it before every commit. |
| `scripts/check-full.sh [preset...]` | Every preset in `CMakePresets.json` that this host can run, one at a time, including the sanitizers |
| `scripts/tidy.sh [file...]` | clang-tidy on the given files, or on the lines you changed |
| `scripts/ci-status.sh [--wait] [branch]` | The latest GitHub Actions runs of a branch (default: the current one) and the first errors of failed jobs. Exits 0 only when the `ci` run for HEAD passed. |
| `scripts/bench-compile.sh [--quick]` | Compile-time benchmarks: the scenario program and generated graphs, hand-wired, against `benchmarks/compile/budgets.json`. `--quick` is N = 10, 50 and 100 with gcc-13 and clang-18. |
| `scripts/bench-runtime.sh [preset...]` | Runtime benchmarks of the scenario with Google Benchmark, built with the release presets |
| `cmake --preset P && cmake --build --preset P && ctest --preset P` | One preset by hand |

The scripts print one line per step and keep the full output in `build/logs/`. Builds and tests use 2 parallel jobs. To change that, set `TRELLIS_JOBS` for the scripts, or pass `-j N` to `cmake --build` and `ctest`.

Test labels: `unit`, `headers`, `compile_fail`, `golden`, `property` and `tools`, so `ctest --preset clang-18-debug -L compile_fail` runs one kind. A selection that matches no test is an error rather than a silent pass.

`appleclang-debug` (macOS) and `msvc-debug` (Windows, from a Visual Studio developer prompt, with Ninja) are enabled only on their own host system.

## Continuous integration

The repository is private and GitHub Actions minutes are limited, so only Linux runs on every push:

| Workflow | When | What |
|---|---|---|
| `.github/workflows/ci.yml` | Every push to a branch; pull requests from forks | The presets `gcc-13-debug`, `gcc-14-debug`, `clang-18-debug`, `clang-20-debug`, `clang-18-libcxx-debug` and `asan-ubsan` on Ubuntu 24.04 |
| `.github/workflows/nightly.yml` | Daily on `main`; `gh workflow run nightly.yml --ref <branch>` | `tsan`, and `scripts/bench-compile.sh --quick` |
| `.github/workflows/platforms.yml` | Weekly on `main`; `gh workflow run platforms.yml --ref <branch> [-f os=macos\|windows]` | `appleclang-debug` on macOS and `msvc-debug` on Windows |

CI gives the property tests seeds that local runs never use, through `TRELLIS_PROPERTY_SEEDS` and from the run number: run N of `ci.yml` tries the 64 seeds from N × 1,000,000, and run N of `nightly.yml` the 256 from N × 1,000,000 + 500,000. A failed scheduled run of `nightly.yml` or `platforms.yml` opens an issue. Actions are pinned to full commit SHAs, with the release tag in a comment. The problem matchers in `.github/problem-matchers.json` turn error lines into annotations, which `scripts/ci-status.sh` shows when the job logs can't be downloaded.

## Tests

- **Unit tests** live in `tests/unit/` and use GoogleTest and gMock. Test names are CamelCase, because GoogleTest asks for names without underscores.
- **Public headers** are checked automatically: the build compiles each header in `include/trellis/` on its own, with the project warnings and `-Wundef` as errors. `-Wundef` catches a header that tests a `TRELLIS_HAS_*` macro without including `config.hpp`.
- **Compile-fail tests** are snippets in `tests/compile_fail/<area>/<name>.cpp` that must not compile. `tools/compile_fail.py` compiles each one with the preset's compiler and flags (without `-Werror`: misuse must be a hard error), with `LC_ALL=C` and a clean environment, so that inherited variables such as `CPATH` or `GCC_URLS` change nothing. The file starts with directives:

  ```cpp
  // compile-fail: App needs IRepository, which has no binding
  // expect-id: TRL-E001
  // expect-regex: no binding for .*IRepository
  // expect-max-lines: 40
  ```

  `compile-fail` is required, and so is an `expect-id` or at least one `expect-regex`. The first error line must contain the `expect-id` as a whole token. Every `expect-regex` must match the diagnostics: the quoted source and caret lines don't count. The whole output may have at most `expect-max-lines` lines (60 by default).

  The test fails if the snippet compiles, or if the compiler crashes, runs out of memory, dies by a signal or times out, even after printing the expected error. A misspelt directive (`expect_regex:`), or one after the first line of code, is an error.
- **Golden diagnostics** in `tests/diagnostics_golden/<compiler>-<major>/<name>.txt` hold the normalized output for the compile-fail test `<name>`. `tools/golden.py` compares them. It compiles the way `tools/compile_fail.py` does, but with the plainest output: `-fdiagnostics-plain-output` on GCC, `-fno-color-diagnostics -fno-caret-diagnostics` on Clang. Normalization follows spike S3's rules ([docs/spikes/S3-diagnostics.md](docs/spikes/S3-diagnostics.md), section 3.7). It drops colors, hyperlinks, quoted source, carets, include chains, `N errors generated.` and `compilation terminated.`. Paths become `<src>/`, `<build>/`, `<std>/` (the standard library) or `<external>/<file name>`, always with `/`. Line and column numbers become `:L:C`, or `(L,C)` on MSVC.

  [`tests/golden_manifest.txt`](tests/golden_manifest.txt) lists which tests have goldens, and for which compilers. CTest registers the golden tests from it, so a missing golden fails. To add one, add a line there, then run `tools/golden.py --preset <preset> --update --reason "<why>" <name>` with a preset of that compiler (such as `clang-18-debug` for clang-18). It records the reason in [`docs/golden-changes.md`](docs/golden-changes.md). Goldens change only at the owner's request. The libc++ preset runs no golden tests, because libc++ spells standard-library names differently from the libstdc++ that the goldens were made with.
- **Property tests** (`tests/property/check_sharing.py`) wire random dependency graphs from `benchmarks/compile/gen_graph.py` by hand, compile them with the preset's compiler and flags, and check through an identity checksum, and a count of the objects built, that each one shares objects exactly as its lifetimes say. Local runs use the fixed seeds 1-32; set `TRELLIS_PROPERTY_SEEDS` (such as `1000000+64`) to try others, as CI does. A failure prints the command that reproduces it.
- Never weaken, delete or rewrite an existing test, golden or budget to make a change pass. If one looks wrong, say why in the pull request.

## Style and commits

- Formatting is clang-format-18 with `.clang-format`; editors pick up `.editorconfig`. Lint with `scripts/tidy.sh`.
- Every public header compiles on its own and is warning-free with `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual` (`/W4 /permissive-` on MSVC).
- Commit subjects are imperative and at most 72 characters. The body says what changed and why.
