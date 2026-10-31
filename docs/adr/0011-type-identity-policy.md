# ADR-0011: Type identity as a policy in the runtime engine

- **Status:** Accepted (plan baseline)
- **Plan:** [§4](../plan.md#4-architecture-decisions) ADR-11; §7.1 (`type_name`, `type_key`), §7.9; risk K9
- **Spikes:** none changed it. S3 measured `type_name` spellings. S6, which would test plugins across shared libraries, is deferred (ADR-0002).

## Context

The static engine works on types directly. The runtime engine needs a value per type, a `type_key`, and the right choice depends on whether keys must agree across shared libraries: plugins need that, a single linked image doesn't (risk K9).

## Decision

- The static engine uses types directly and has no IDs.
- In the runtime engine, the `key_policy` option chooses how each `type_key` is made:
  - `address` (the default): the address of an inline variable template. Fastest, but valid only within one linked image.
  - `name`: a hash of the compile-time type name. It is stable across shared libraries built with the same compiler.
  - `rtti`: `std::type_index`.
- Plugins require `name` or `rtti`, and CI covers that with a test that loads a plugin at run time.

## Alternatives

- **One policy for everything:** `address` alone rules out plugins; `rtti` alone rules out `-fno-rtti` builds (NFR-11); `name` alone gives up the fastest key in single-image programs.

## Consequences

- **Type names depend on the compiler and the standard library** (S3 §3.4). `std::string` is `std::__cxx11::basic_string<char>` on GCC, `std::basic_string<char>` on Clang with libstdc++, and `std::string` with libc++; GCC writes `trellis::v1::singleton` where Clang writes `trellis::singleton`. So the `name` policy is stable only between binaries built with the same compiler and standard library, which the plugin rules already require (§3.3; the ABI tag check in §7.9). On MSVC, `__FUNCSIG__` names need their `struct ` and `class ` prefixes stripped (S3 §3.8).
- **R004 across plugins** (S4 §5): the thread-local frame stack behind R004 is one object per program only if its symbol isn't hidden. Re-entry through a lazy that crosses a `-fvisibility=hidden` boundary would deadlock instead of reporting R004 (ADR-0012). The plugin tests should cover it once S6 runs.

## History

- **2026-10-30, plan draft 1:** drafted as ADR-11 in plan §4.
- **2026-10-31, T0.8:** recorded in this file, unchanged; S3's and S4's findings added as consequences.
