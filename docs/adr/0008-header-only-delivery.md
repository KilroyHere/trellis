# ADR-0008: Header-only, no dependencies, several delivery forms

- **Status:** Accepted (plan baseline)
- **Plan:** [§4](../plan.md#4-architecture-decisions) ADR-08; §10; NFR-11; R14, R15
- **Spikes:** none changed it. S4's choice of slot keeps the core's includes to standard headers it needs anyway.

## Context

Hypodermic needs Boost, and Fruit is a compiled library whose RTTI and build settings must match the application (R14). Wrapping Boost.DI in a C++20 module produced warnings, and a digit separator inside an `#if` broke Clang's module scanning (R15).

## Decision

- Headers, plus a generated single header, plus an optional C++20 module (`import trellis;`).
- A CMake package, and a vcpkg port and a Conan recipe from v0.2.
- No Boost or other dependency: `include/` uses only the standard library.
- Both engines work with `-fno-exceptions` and `-fno-rtti` (NFR-11).
- The module build and the plain-header build are both tested in CI from Phase 1 (R15, §10).

## Alternatives

- **A compiled library** (Fruit): its RTTI and build settings must match the application's.
- **Depending on Boost** (Hypodermic): a heavy dependency for users who don't otherwise need it.

## Consequences

- **Every header's cost lands in every translation unit that includes Trellis.** S4 measured it for the slot designs (S4 §3.3): `<mutex>` and `<condition_variable>` add 12% more preprocessed lines and 0.07–0.09 s per translation unit, a quarter of NFR-03's +0.3 s budget. That is one reason ADR-0012 chose a slot that needs only `<atomic>`.
- **Not yet covered by the spikes:** modules and `-fno-exceptions` with deduced discovery (S1 §5; S1's form tracer throws, though its injector stub doesn't). T1.8 covers the modules: deduced constructors are found through `import trellis;` in `tests/module/`. How R004 is reported without exceptions is open (ADR-0012).
- The single header and the module need the same tests as the headers (§9.2; T1.8).
- **The module needs more than the headers** (T1.8): CMake 3.28, Ninja or Visual Studio 2022, and Clang 16 or later with `clang-scan-deps`. GCC 14 builds it, but in importers `type_name<T>()` gives wrong names and class template argument deduction ignores the deduction guides, so GCC before 15 is refused. An importer gets no macros: it includes `<trellis/config.hpp>` for them ([internals note](../internals/delivery.md)).

## History

- **2026-10-30, plan draft 1:** drafted as ADR-08 in plan §4.
- **2026-10-30, T0.1:** the CMake INTERFACE target `trellis::trellis`, with install rules and a package config.
- **2026-10-31, T0.8:** recorded in this file, unchanged.
- **2026-11-01, T1.8:** the module `trellis` (`modules/trellis.cppm`, target `trellis::module` with `TRELLIS_ENABLE_MODULE`) exports every public name, `trellis::error` included, with using-declarations in the declaring namespace over a global module fragment that includes the headers; `tools/module_exports.py` keeps the list equal to the headers' public names. `tools/amalgamate.py` generates the single header, and the unit tests also run against it. Tested with Clang 18 and 20; GCC 14 refused, as above. The module isn't installed yet. "The same tests" holds for the single header, which runs every unit test. The modules run their smoke tests and the unit tests that use only public names: at T1.8, six files of `trellis_unit_tests` and the scenario program. The rest test `trellis::detail`, which a module doesn't export (`create`, `explain` and `override` among them), or set a configuration macro, which only the module's own build can (`TRELLIS_HAS_ATOMIC_WAIT=0`). The testing helpers, a header that the umbrella header leaves out, are the module `trellis.testing`, which re-exports `trellis`. Follow-up: tests of public features should use public names where they can, so that they also run through the modules.
