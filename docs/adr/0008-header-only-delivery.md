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
- **Not yet covered by the spikes:** modules and `-fno-exceptions` with deduced discovery (S1 §5; S1's form tracer throws, though its injector stub doesn't). How R004 is reported without exceptions is open (ADR-0012).
- The single header and the module need the same tests as the headers (§9.2; T1.8).

## History

- **2026-10-30, plan draft 1:** drafted as ADR-08 in plan §4.
- **2026-10-30, T0.1:** the CMake INTERFACE target `trellis::trellis`, with install rules and a package config.
- **2026-10-31, T0.8:** recorded in this file, unchanged.
