# Changelog

All notable changes to Trellis are recorded in this file. The format follows [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/), and versions follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html). Before 1.0, a minor release may break the API, with migration notes (plan §10).

Each user-visible change adds a line under "Unreleased", in the group that fits: Added, Changed, Deprecated, Removed, Fixed or Security. A release renames "Unreleased" to its version and date, and starts a new, empty one.

## [Unreleased]

Phase 0 (foundations) is complete. There is no injector API yet; the first one comes with v0.1.

### Added

- **Package:** the header-only CMake target `trellis::trellis`, with install rules and `find_package(trellis CONFIG)`. `<trellis/trellis.hpp>` is the umbrella header. `<trellis/config.hpp>` holds the version (0.1.0, in development) and the `TRELLIS_HAS_*` macros that detect C++23 and C++26 features. Licensed under the Boost Software License 1.0.
- **Meta toolkit** (`include/trellis/meta/`, T1.1): `trellis::fixed_string<N>`, a string usable as a template argument and as a compile-time message buffer; `trellis::type_name<T>()`, a type's name as a constant expression; and `trellis::result<T>`, which is `std::expected<T, trellis::failure>` where the standard library provides it (`__cpp_lib_expected`) and otherwise a fallback with the same core interface, with `trellis::make_unexpected` to return a failure in both. Internally, type lists and an inheritance-based type map whose lookups never recurse, fold over a pack or instantiate the types they hold ([notes](docs/internals/meta-toolkit.md)).
- **Configuration macros** in `<trellis/config.hpp>`: `TRELLIS_HAS_EXCEPTIONS`, `TRELLIS_HAS_TYPE_PACK_ELEMENT`, `TRELLIS_IS_SAME` (the `__is_same` builtin where there is one), and `TRELLIS_FUNCTION_SIGNATURE` with `TRELLIS_FUNCTION_SIGNATURE_CLASS_KEYS`.
- **Explicit constructor declarations** (`<trellis/discovery/ctor_traits.hpp>`, T1.2): `trellis::ctor<Args...>` lists the parameter types of the constructor that Trellis calls. A class declares it with a specialization of `trellis::ctor_traits<T>`, which leaves the class untouched, or with a public member alias `using trellis_ctor = trellis::ctor<...>;`. The specialization wins when both exist. A declaration that matches no constructor, or that doesn't brace-initialize an aggregate, fails to compile with [TRL-E002](docs/errors/TRL-E002.md), the first error ID that the library emits.
- **Compilers:** GCC 13 and 14, Clang 18 and 20, and Clang 18 with libc++ 18, built and tested on every push; Apple Clang and MSVC, built and tested weekly and on demand. All warning-free under `-Werror` or `/WX`.
- **Build presets and scripts:** CMake presets for each compiler in debug mode, two release builds, ASan with UBSan, TSan and a `-ftime-trace` build. `scripts/init.sh`, `check-fast.sh`, `check-full.sh` and `tidy.sh` print a short summary and keep the full logs in `build/logs/`; `scripts/ci-status.sh` shows the latest CI runs and the first errors of failed jobs.
- **CI:** six Linux presets on every push, TSan and the quick compile-time benchmark nightly, and macOS and Windows weekly or on demand. A failed scheduled run opens an issue.
- **Test tooling:** unit tests with GoogleTest and gMock; a check that every public header compiles on its own; compile-fail tests, which check the error ID on the first error line, regexes and a line budget (`tools/compile_fail.py`); golden diagnostics, compared after normalizing paths and compiler noise (`tools/golden.py`); and property tests that check object sharing on random dependency graphs against a model.
- **Benchmark harness:** a deterministic graph generator, compile-time runs that check the budgets of NFR-03 to NFR-05 against hand-written wiring (`scripts/bench-compile.sh`), and runtime benchmarks of the research scenario with Google Benchmark (`scripts/bench-runtime.sh`).
- **Design decisions,** recorded as ADR-0001 to ADR-0014 in [`docs/adr/`](docs/adr/README.md), informed by spikes S1 to S4:
  - 0001: C++20 baseline; C++23 and C++26 features only behind `TRELLIS_HAS_*` macros.
  - 0002: a static injector and a runtime container, sharing one binding vocabulary; both ship in 1.0.
  - 0003: constructors declared explicitly, found by C++26 reflection, or deduced without macros.
  - 0004: explicit lifetimes, and a table of the request forms each lifetime allows.
  - 0005: objects live only in injectors and scopes, and are destroyed in reverse order of construction.
  - 0006: one hard error per mistake, naming a stable `TRL-` ID, the type, the path and a fix.
  - 0007: domain classes never need to include Trellis.
  - 0008: header-only and standard library only; a single header and a C++20 module; works without exceptions or RTTI.
  - 0009: runtime errors as `result<T>`, an exception or an error handler; never `exit()`.
  - 0010: composition through variadic modules, never fluent type accumulation.
  - 0011: type identity in the runtime engine as a policy: address, name or RTTI.
  - 0012: thread-safe lazy singletons by default, an eager mode, and re-entrant construction reported in every build.
  - 0013: governance and release discipline.
  - 0014: resolution memoized per key, request form and consumer lifetime; how dependency cycles are reported in deduced mode.
- **Documentation:** the error catalog in [`docs/errors/`](docs/errors/README.md), with a check that every error ID used in `include/` has a page; maintainer notes and the doc-comment convention for public headers in [`docs/internals/`](docs/internals/README.md); and this changelog.
