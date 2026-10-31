# ADR-0009: The runtime error model

- **Status:** Accepted (plan baseline)
- **Plan:** [§4](../plan.md#4-architecture-decisions) ADR-09; §7.7; R8; NFR-11
- **Spikes:** none changed it. S4's decision to detect R004 in all builds (ADR-0012) gives the static engine a runtime error too.

## Context

Fruit's runtime injection errors call `exit(1)`, which a caller can't catch (R8). Trellis must work with and without exceptions (NFR-11).

## Decision

- `try_get<T>()` returns `trellis::result<T>`: `std::expected` when it is available (`TRELLIS_HAS_EXPECTED`), otherwise a small internal type.
- `get<T>()` throws `trellis::resolution_error` when exceptions are enabled. Otherwise it calls a handler installed with `trellis::set_error_handler`, and calls `std::abort()` only if that handler returns.
- The library never calls `exit()`.

## Alternatives

- **`exit()` on error** (Fruit): the process ends where no caller can react.
- **Exceptions only:** rules out `-fno-exceptions` builds.
- **Result types only:** makes composition roots that would rather fail fast handle every error by hand.

## Consequences

- **R004 is a new runtime error in the static engine.** Plan §7.7 said the static engine has no failure path at run time with `-fno-exceptions`; it now names R004 as the exception (amended by ADR-0012). Re-entrant construction through a lazy is detected in every build (ADR-0012), and S4's prototype reports it by throwing `TRL-R004`. How it is reported without exceptions is open; this ADR's handler is the obvious channel. It is decided when `lazy` and `provider` arrive in Phase 2.
- Runtime messages reuse the compile-time type names of plan §7.1 (`type_name<T>()`), which S3 showed are constant expressions on GCC and Clang, including with libc++ (S3 §3.4).

## History

- **2026-10-30, plan draft 1:** drafted as ADR-09 in plan §4.
- **2026-10-31, T0.8:** recorded in this file, unchanged; the open question about R004 without exceptions added.
