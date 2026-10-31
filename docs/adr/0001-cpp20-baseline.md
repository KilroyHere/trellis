# ADR-0001: C++20 baseline; C++23 and C++26 used when available

- **Status:** Accepted (plan baseline)
- **Plan:** [§4](../plan.md#4-architecture-decisions) ADR-01; §3.2 NFR-13; §15 (compatibility promise); §16 D2
- **Spikes:** none changed it. Spike S3's gate for P2741 messages is an exception recorded in [ADR-0006](0006-diagnostics-contract.md).

## Context

Trellis needs language features that C++17 lacks:

- concepts, so constraints fail cleanly and errors are readable;
- class-type template parameters, for string qualifiers (`named<"replica">`) and the options struct (§6.13);
- `consteval` and the wider `constexpr` rules, so graph analysis can run as ordinary code at compile time.

Every compiler NFR-13 lists supports C++20, including Ubuntu 24.04's defaults, GCC 13 and Clang 18. Newer features help where they exist: `std::expected` and `std::move_only_function` (C++23), and reflection, user-generated `static_assert` messages and pack indexing (C++26). They aren't available everywhere, and a compiler may report a feature in an older language mode where using it only works as an extension, with a warning.

## Decision

- **Require C++20,** and keep that baseline for all of 1.x (§15).
- **Use C++23 and C++26 features only behind the `TRELLIS_HAS_*` macros** in `include/trellis/config.hpp`, the only header that defines macros. Code never tests a feature-test macro directly.
- **How the macros are defined** (T0.1):
  - a library feature relies on its `__cpp_lib_*` macro alone, because a standard library defines it only in modes where the feature is usable;
  - a core-language feature also requires the language mode it was standardized in. Clang 20 defines `__cpp_static_assert` as 202306L and `__cpp_pack_indexing` in C++20 mode, then warns that both are C++26 extensions, and `-Werror` turns the warnings into errors.
- **One deliberate exception,** decided in ADR-0006 from spike S3: user-generated `static_assert` messages (P2741) are also used in C++20 and C++23 modes on GCC 14 or later and Clang 20 or later, where they work as an extension. That gate tests compiler versions, and the extension warning is silenced around the one assertion that uses it.
- **Compiler workarounds also live in `config.hpp`,** each behind a named `TRELLIS_` macro with a comment and a test. The spikes call for:
  - the gcc-13 probe operator and the GCC `-Wconversion` pragma pair (ADR-0003);
  - the P2741 gate (ADR-0006);
  - `TRELLIS_HAS_ATOMIC_WAIT`, `TRELLIS_CPU_RELAX()` and a no-inline attribute (ADR-0012);
  - fallbacks for the `__is_constructible` and `__is_same` builtins on MSVC (ADR-0014).

## Alternatives

- **C++17.** Wider reach, but diagnostics would fall back to SFINAE (substitution failure), the field's main pain point, and there are no class-type template parameters. Plan §16 D2 puts the cost at about 4 more weeks.
- **C++26 only.** Only GCC 16 implements reflection, behind an experimental flag, and Ubuntu 24.04 packages neither GCC 16 nor a reflection-capable Clang (see S5 in the [index](README.md#spikes)).

## Consequences

- Legacy toolchains are out of scope; Boost.DI (C++14) and Kangaru v4 (C++11) remain options there.
- Reflection lives in an optional, isolated backend (ADR-0003, plan §7.10) that a C++20 build never parses.
- Each `TRELLIS_HAS_*` macro has a test, and plan §9.2 puts C++20, C++23 and C++26 modes in the CI matrix.
- Keeping every workaround in one header is what lets a later compiler fix remove it cleanly.

## History

- **2026-10-30, plan draft 1:** drafted as ADR-01 in plan §4.
- **2026-10-30, T0.1:** `config.hpp` defines `TRELLIS_HAS_STATIC_ASSERT_MESSAGE`, `TRELLIS_HAS_EXPECTED`, `TRELLIS_HAS_PACK_INDEXING` and `TRELLIS_HAS_REFLECTION`, tying core-language features to their language mode. Plan §16's default D2 (C++20) stands (docs/PROGRESS.md).
- **2026-10-31, T0.8:** recorded in this file. The decision is unchanged; the P2741 exception from ADR-0006 and the `config.hpp` macros the spikes call for are listed above.
