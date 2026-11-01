# Plan: building a C++ dependency injection framework from scratch

> The background research this plan builds on is in [`research/`](../research/README.md).

> **Status:** draft 1, 2026-10-30
> **Working name:** Trellis (`namespace trellis`; the examples write `namespace di = trellis;`). Rename it before any public release.
> **Builds on:** [the research report](../research/reports/Cpp%20dependency%20injection%20frameworks.md), [its notes](../research/research_notes/Cpp%20dependency%20injection%20frameworks/) and [the experiment harness](../research/experiments/README.md).
> **About the numbers:** figures quoted as "measured" come from that harness: GCC 13.3 and Clang 18.1, `-O2`, one translation unit, a shared 4-vCPU VM. Read them as ratios against hand-written wiring, not as absolute values.

## Contents

0. [Summary](#0-summary)
1. [Should you build one? Scope options](#1-should-you-build-one-scope-options)
2. [Lessons from existing frameworks, turned into design rules](#2-lessons-from-existing-frameworks-turned-into-design-rules)
3. [Requirements and measurable targets](#3-requirements-and-measurable-targets)
4. [Architecture decisions](#4-architecture-decisions)
5. [Architecture](#5-architecture)
6. [Public API sketches](#6-public-api-sketches)
7. [Internal design](#7-internal-design)
8. [Performance engineering](#8-performance-engineering)
9. [Quality strategy](#9-quality-strategy)
10. [Build, packaging and releases](#10-build-packaging-and-releases)
11. [Documentation plan](#11-documentation-plan)
12. [Roadmap](#12-roadmap)
13. [The first nine weeks, week by week](#13-the-first-nine-weeks-week-by-week)
14. [Risk register](#14-risk-register)
15. [Governance and sustainability](#15-governance-and-sustainability)
16. [Open decisions](#16-open-decisions)
17. [Appendices](#17-appendices)

---

## 0. Summary

**What gets built.** A header-only C++20 dependency injection (DI) library with two engines:

- **The static injector (the core).**
  - It wires objects with direct constructor calls, so nothing is looked up at run time.
  - It checks the graph at compile time and reports each mistake as one readable error that names the type and the path that needed it.
  - Lifetimes are declared explicitly and enforced, so dangling references and "captive" dependencies become compile errors instead of production bugs.
- **The runtime container (optional).** It uses the same binding vocabulary and covers plugins and configuration-driven graphs.

A C++26 reflection backend replaces the constructor-discovery workarounds on compilers that support it. It stays optional until Clang and MSVC catch up.

**Why there is room for it.** The research found that usable frameworks exist but none is a safe default:

1. **Maintenance.** Every candidate depends on one maintainer, and fixes sit unreleased. Boost.DI's 2026 compiler fixes are untagged, and Fruit has 26 commits unreleased since January 2023.
2. **Silent missing bindings.** Three of six frameworks compiled a missing binding without complaint and failed only at run time. Boost.DI under GCC only warns, then fails at link time unless `-Werror` is set.
3. **Surprising lifetimes.** Boost.DI infers a lifetime from how each consumer spells the parameter, and its singletons are shared by every injector in the process. Fruit cannot inject `std::unique_ptr`, and the `shared_ptr` it injects does not own its object.
4. **Compile-time cliffs.** A flat Fruit component was killed for running out of memory at about 13.9 GB with only 25 components. qlibs/di took 518 s and 10.8 GB at 25 components.
5. **Runtime cost.** The containers that look dependencies up at run time paid 4–25× the hand-written cost per resolution.

Boost.DI shows that the core idea works. It resolved objects within 2–9 ns of hand-written code, and on GCC each extra component cost about the same compile time as hand-wiring. The opening is a compile-time injector at that level of performance, with better diagnostics, safer lifetimes, real releases and more than one maintainer.

**Key decisions** (details in [§4](#4-architecture-decisions)):

1. **Language baseline:** C++20. C++23 and C++26 features are used when the compiler has them.
2. **Two engines:** a static injector core and a separate runtime container, sharing one binding vocabulary.
3. **Constructor discovery:** three strategies behind one trait: explicit declaration, C++26 reflection, and deduction (the C++20 default, which needs no macros).
4. **Lifetimes:** declared explicitly, with an enforced compatibility table for how a dependency may be requested. A lifetime is never inferred from how a parameter is spelled.
5. **State:** all of it lives in the injector or scope object. There are no process-wide statics.
6. **Diagnostics:** one hard error per root cause, with stable error IDs and the dependency path. No tricks that depend on warnings or the linker.
7. **Non-intrusive:** domain classes never need to include a framework header.
8. **Packaging:** header-only with no dependencies, working with `-fno-exceptions` and `-fno-rtti`. Also shipped as a single header and a C++20 module.
9. **Runtime errors:** never `exit()`. They come back as result types, exceptions, or a call to a user-installed handler.
10. **Compile-time budgets:** enforced in CI from week one, using the research harness.
11. **Governance:** two maintainers, and every merged fix released promptly.

**Timeline.** Estimates assume one experienced C++ library engineer working full-time. With two engineers it drops to about 28 weeks, because the spikes, the Phase 3 and 4 work, and phases 5 and 6 can run in parallel.

| Phase | Weeks | Release | Outcome |
|---|---|---|---|
| 0 Foundations and spikes | 1–3 | — | Decisions confirmed by spikes; CI and benchmark baseline in place |
| 1 Core static injector (MVP) | 4–9 | v0.1 | Bindings, unique/singleton/external lifetimes, automatic constructor discovery, compile-time errors |
| 2 Lifetimes and safety | 10–13 | v0.2 | Scopes, captive-dependency and cycle detection, eager initialization, thread-safety options, lazy and provider injection |
| 3 Binding features | 14–18 | v0.3 | Qualifiers, multibindings, factories with runtime arguments, decorators, module requirements |
| 4 Diagnostics | 19–21 | v0.4 | Dependency paths in errors, an error catalog, golden tests, C++26 formatted messages |
| 5 Runtime container | 22–27 | v0.5 | Runtime registration, validation at `build()`, plugins, error model |
| 6 Reflection backend | 28–32 | v0.6 (experimental) | Reflection-based discovery, annotations, namespace scanning |
| 7 Ecosystem and docs | 33–36 | v0.9 beta | Packages, module, docs site, examples, migration guides |
| 8 Beta to 1.0 | 37–44 | v1.0 | API freeze, design-partner feedback, fuzzing, governance in place |

**This week:**

1. Create the `trellis` repository with CMake, a CI matrix (Linux GCC and Clang, macOS, Windows MSVC) and `-Werror`.
2. Port the research harness (`experiments/common/domain.hpp`, `scripts/gen_scaling.py`, `scripts/measure.py`) into `benchmarks/`.
3. Add `trellis` to the `FRAMEWORKS` lists in `measure.py` and `gen_scaling.py`, at first as a hand-written stub.
4. Start spikes S1 (deduced discovery) and S3 (diagnostics rendering).

---

## 1. Should you build one? Scope options

The research recommends hand-written constructor injection with a composition root in `main` for most projects. It recommends a framework only once maintaining the wiring of dozens of long-lived, context-scoped services becomes the bottleneck. Chromium, userver and Unreal each built their own component system at that point. Building a framework is worthwhile if at least one of these holds:

- You have, or expect, such a codebase.
- You want a reusable or open-source library and accept the maintenance commitment, which the research identified as the field's weak point.
- Learning modern metaprogramming and reflection is itself a goal.

Choose the scope deliberately:

| Option | What it is | Effort | Right when |
|---|---|---|---|
| **0. Don't build: co-maintain** | Become the second maintainer of Kangaru or dingo, the two active projects | Ongoing | You need a framework, not ownership of one. This directly fixes the bus-factor problem. |
| **A. Composition helper** | 300–800 lines: a typed registry for long-lived services with ordered construction and teardown. `get<T>()` is called only from wiring code, and there is no constructor discovery. | 1–2 weeks | One application that wants Chromium/userver-style lifecycle management without template machinery |
| **B. Static injector** | Phases 0–4, plus packaging and docs from phases 7–8 | ~5–6 months | Many services, compile-time safety, zero runtime overhead |
| **C. Full framework** (this plan) | B, plus the runtime container, plugins, the reflection backend and the ecosystem work | ~10–11 months | A public library, or plugin- and configuration-driven hosts |

**Recommendation:** plan for C, but structure the work so that B ships on its own if time runs short. The runtime engine and the reflection backend are additive and can land in 1.x without breaking the static API.

**Positioning statement:** *the DI library for teams that already practice constructor injection and want their wiring checked at compile time, as fast as hand-written code, and easy to remove.* "Easy to remove" is a design requirement. Ordinary classes never depend on the framework, so dropping it means rewriting only the composition root.

---

## 2. Lessons from existing frameworks, turned into design rules

Every rule below traces to a finding in the research. Later sections refer to the rules by number (R1–R20).

| # | Evidence (research) | Design rule |
|---|---|---|
| R1 | Boost.DI and qlibs/di resolve within 1–9 ns of hand-wiring (29.0/35.7 ns and 27.3/27.6 ns, against 26.6/26.5 ns). Their binaries are within 0–8 KB of the hand-wired one. | Static resolution compiles down to direct constructor calls. There is no type erasure or lookup on the static path. |
| R2 | The lookup-based containers cost about 4× (Fruit), 10–12× (dingo) and 22–25× (Hypodermic) the hand-wired resolve, because of hash maps and type erasure. | The runtime container gives each type a dense integer ID when it is frozen, and its hot path indexes flat arrays rather than searching a node-based hash map. |
| R3 | Kangaru, dingo and Hypodermic compiled a missing binding silently. Boost.DI under GCC only warned, then failed at link time unless `-Werror` was set. | Every error that can be detected statically is a hard `static_assert` error on every compiler, whatever the warning flags. |
| R4 | Fruit's first error line names the problem in plain English. dingo prints the resolution path (`required by demo::App -> demo::Service`). Kangaru's message names nothing. | Every error names the type, the form it was requested in, the path from the root, and a suggested fix. |
| R5 | Boost.DI infers lifetime from the parameter spelling: `T&` gets a singleton and `T` gets a new object. Its issue tracker has reports of dangling references. | Lifetime is declared on the binding. Requested forms are checked against it ([§6.3](#63-the-request-form-compatibility-table)), and a reference to an object nobody owns is a compile error. |
| R6 | Boost.DI singletons are function-local statics shared by every injector (issue #459), so tests leak state into each other. | All state lives in injector and scope objects. |
| R7 | Fruit cannot inject `unique_ptr` parameters, the `shared_ptr` it injects does not own its object, and its providers must be lambdas without captures. | `unique_ptr` and `shared_ptr` are first-class, with honest ownership. Providers may be any callable, including capturing lambdas. |
| R8 | Fruit's runtime injection errors call `exit(1)`, which a caller cannot catch. | The library never terminates the process on its own initiative. Errors are returned, thrown, or passed to a user handler. |
| R9 | A flat Fruit component ran out of memory at 25 components. qlibs/di wraps each nested resolution in a distinct provider type and reached 518 s and 10.8 GB at 25 components. | The type system never accumulates state through fluent chains. Results are memoized per type, never per path. Compile-time scaling tests with hard budgets run in CI. |
| R10 | dingo's own documentation says GCC handles a dependency chain of only about 52 links at the default template depth. | At most 4 template-instantiation levels per dependency hop, so chains of 200 or more fit within default limits. |
| R11 | By default Hypodermic silently constructs any unregistered concrete type, so a forgotten registration produces a fresh object instead of an error. | Unbound concrete types are built automatically only when requested in an owning form (by value or as `unique_ptr`), and a strict option disables even that. |
| R12 | Every project has one maintainer, fixes go untagged, and Fruit's Windows CI fails and its macOS CI is cancelled every month without anyone noticing. | Two maintainers; every merged fix released within 7 days; failing CI alerts people. |
| R13 | Jusiak advises "consider not coupling code to a specific DI framework". Chromium and userver keep ordinary classes unaware of their component systems. | Domain classes need no framework includes. Framework types in constructors are optional conveniences, and each has a standard-library equivalent. |
| R14 | Hypodermic needs Boost. Fruit is a compiled library whose RTTI and build settings must match the application. | No dependencies; header-only; works with `-fno-exceptions` and `-fno-rtti`. |
| R15 | Wrapping Boost.DI in a C++20 module produced warnings (PR #585). A digit separator inside an `#if` broke Clang's module scanning (#568/#572). | The module build and the plain-header build are both tested in CI from the start. |
| R16 | Kangaru emits three `-Wunused-variable` warnings under `-Wall -Wextra`, still present on its master branch. | Warning-free at high warning levels on all compilers, enforced with `-Werror` in CI. |
| R17 | Kangaru's maintainer is mostly working on an unreleased rewrite. dingo merged several large internal refactors in October 2026. | A stability promise after 1.0: deprecate before removing, and never rewrite without a compatibility layer. |
| R18 | Thread safety is undocumented in Kangaru and dingo. inversify-cpp's singleton cache appears to have a race. | Thread-safety guarantees are documented per operation and tested under ThreadSanitizer. |
| R19 | Only GCC 16 implements C++26 reflection, behind an experimental flag. Clang began merging it in October 2026, and MSVC has none. | Reflection is an optional backend behind feature-test macros and is never required. |
| R20 | Fruit's author reports faster incremental rebuilds at 1,000 classes with components (6.1 s, against 8 s and 20 s for hand-wiring), because components split the wiring across files. | Supported patterns keep wiring in one translation unit or split it along module boundaries, so incremental builds stay fast. |

---

## 3. Requirements and measurable targets

### 3.1 Functional requirements

Priority: **P0** = required for 1.0, **P1** = expected in 1.0, **P2** = after 1.0 or if time allows.

| ID | Pri | Requirement |
|---|---|---|
| FR-01 | P0 | Bind interface → implementation; bind concrete types; wire constructors automatically |
| FR-02 | P0 | Lifetimes: `unique` (new object per request), `singleton` (one per injector), external instance (by reference, value or `shared_ptr`) |
| FR-03 | P0 | Request forms `T`, `T&`, `const T&`, `T*`, `const T*`, `std::unique_ptr<T>`, `std::shared_ptr<T>`, checked against lifetimes |
| FR-04 | P0 | Compile-time detection of missing bindings, ambiguous constructors, duplicate bindings, and form/lifetime mismatches |
| FR-05 | P0 | Modules (composable binding sets) and test overrides that must replace an existing binding |
| FR-06 | P0 | Constructor discovery without macros, plus an explicit escape hatch |
| FR-07 | P1 | Scoped lifetimes with nested scopes (request, session, world), and per-scope seed values |
| FR-08 | P1 | Captive-dependency detection and dependency-cycle detection |
| FR-09 | P1 | Qualifiers: annotated types and contextual bindings (`when_injected_into<C>`) |
| FR-10 | P1 | Multibindings: `std::vector<std::unique_ptr<I>>`, `std::span<I* const>` and similar |
| FR-11 | P1 | Factories with runtime ("assisted") arguments; `std::function`, `std::move_only_function` and `di::factory` forms |
| FR-12 | P1 | `di::lazy<T>` and `di::provider<T>`; deferred edges that break cycles |
| FR-13 | P1 | Eager initialization; deterministic teardown in reverse order of construction |
| FR-14 | P1 | Runtime container: registration at run time, plugins, validation at `build()`, errors that include the path |
| FR-15 | P1 | Decorators that wrap a binding with layers (caching, logging, metrics) |
| FR-16 | P2 | Implementation selected at run time from configuration |
| FR-17 | P2 | Graph export (DOT or JSON) and construction-tracing hooks for startup timing |
| FR-18 | P2 | Experimental C++26 reflection backend in 1.0, stabilized in 1.x |
| FR-19 | P2 | Module "requirements" (Fruit's `Required<...>`), so a module can be checked in isolation |
| FR-20 | P2 | Compiling a static injector into a type-erased bundle that another translation unit can use as a parent |
| FR-21 | P2 | Construction aware of allocators and `std::pmr` |
| FR-22 | P2 | Concurrent startup of independent singletons, as userver does |

### 3.2 Non-functional targets

The baselines come from the research harness and are quoted as GCC / Clang where both were measured.

| ID | Metric | Target | Research baseline |
|---|---|---|---|
| NFR-01 | Resolve the scenario root from a warm injector | ≤ 1.1× hand-wired | Hand-wired 26.6 / 26.5 ns; Boost.DI 29.0 / 35.7; Kangaru 39.8 / 40.8; Fruit 102 / 104 |
| NFR-02 | Build the injector and graph (per-injector singletons) | ≤ 1.5× hand-wired | Hand-wired 58 / 53 ns; Boost.DI per-injector scope 164 / 179; Kangaru 579 / 551 |
| NFR-03 | Extra compile time for the scenario program, C++20 | ≤ +0.3 s | Boost.DI +0.27 / +0.23 s; Fruit +1.00 / +1.02; dingo +2.34 / +2.39 |
| NFR-04 | Compile time for a generated 100-component graph | ≤ 1.3× hand-wired; linear growth to 1,000 components, ≤ 1.5× there | Hand-wired 5.5 / 4.2 s; Boost.DI 6.8 / 6.3; Fruit 18.1 / 16.3 |
| NFR-05 | Peak compiler memory at 100 components | ≤ 1.5× hand-wired; no blow-ups that depend on how bindings are laid out | Hand-wired 363 / 232 MB; Boost.DI 487 / 341; Fruit flat layout ran out of memory at 25 |
| NFR-06 | Stripped binary size of the scenario | ≤ hand-wired + 8 KB | Hand-wired 18.7 KB; Boost.DI 22.8 KB; Kangaru 35 KB |
| NFR-07 | Missing binding | Compile error on GCC, Clang and MSVC without `-Werror`; the first error line names the error ID; the same diagnostic names the type (amended by ADR-0006, 2026-10-30); ≤ 40 lines of output on GCC and Clang, not counting the lines the user's own layout produces: the include chain and the excerpt of the user's call site (amended by ADR-0006, 2026-11-01) | Fruit 5–10 KB; qlibs/di up to 30 KB; Boost.DI on GCC fails only at link time |
| NFR-08 | Runtime container: resolve the scenario root, warm | ≤ 2× the static injector, i.e. faster than Fruit | Fruit 102 ns; dingo 273–326; Hypodermic 598–657 |
| NFR-09 | Longest dependency chain at default compiler limits | ≥ 200 hops; ≥ 150 on MSVC before 19.50, an assumed boundary (amended by ADR-0014, 2026-10-31, provisional) | dingo about 52 |
| NFR-10 | Warnings | 0 under `-Wall -Wextra -Wpedantic -Wshadow -Wconversion` (GCC/Clang) and `/W4` (MSVC) | Kangaru 3 |
| NFR-11 | `-fno-exceptions` and `-fno-rtti` | Both engines build and pass the tests in both modes | — |
| NFR-12 | Maintainability | Core headers ≤ 6,000 lines, every metaprogramming trick commented and tested | Hypodermic's headers total 4,854 lines |
| NFR-13 | Platforms | GCC ≥ 12, Clang ≥ 16, Apple Clang ≥ 15, MSVC ≥ 19.38 (VS 2022 17.8); libstdc++, libc++ and MS STL; Linux, macOS, Windows | Ubuntu 24.04 ships GCC 13 and Clang 18 |

**Measuring:** each target is a CI check ([§8.4](#84-benchmark-suite-and-ci-gates)). The runtime targets are ratios against the hand-written baseline built in the same job, which cancels most of the noise from shared runners.

### 3.3 Non-goals (1.0)

- Field or setter injection. Constructor injection only; a reflection backend could add field injection after 1.0.
- A public service-locator style. The runtime container's `get` is meant for composition roots, and the docs discourage passing containers into classes.
- Plugin interfaces that work across compilers or ABIs. Plugins must use the same compiler, standard library and key settings as the host.
- Wiring from configuration files (XML/JSON, as Wallaroo does). The library integrates with the user's own configuration objects.
- Aspect-oriented interception beyond decorators.
- Support for C++17 or older.
- External code generators. Reflection covers that role.
- Asynchronous or coroutine-aware construction (P2: FR-22).

---

## 4. Architecture decisions

These are written as short architecture decision records (ADRs) and will move into `docs/adr/` in the new repository. "Confirm in S*n*" means a phase-0 spike must confirm the decision before Phase 1 depends on it.

> **Now in [`docs/adr/`](adr/README.md)** (T0.8, 2026-10-31): ADR-0001 to ADR-0013 are the records below, with the spike results folded in, and ADR-0014 adds §7.3's decision. Where they differ from this section, the files there take precedence.

### ADR-01: C++20 baseline; C++23 and C++26 used when available

- **Decision:** require C++20. Use C++23 (`std::expected`, `std::move_only_function`) and C++26 (reflection, user-generated `static_assert` messages, pack indexing) behind feature-test macros.
- **Why:**
  - Concepts give constraints that fail cleanly and readable errors.
  - Class-type template parameters allow string qualifiers (`named<"replica">`) and an options struct.
  - `consteval` and better `constexpr` let graph analysis run as ordinary code at compile time.
  - Every target compiler supports C++20, including the Ubuntu 24.04 defaults.
- **Rejected:**
  - C++17: wider reach, but diagnostics would fall back to SFINAE (substitution failure), which is the field's main pain point.
  - C++26 only: just GCC 16 implements reflection, behind an experimental flag.
- **Consequences:** legacy toolchains are out of scope; Boost.DI (C++14) and Kangaru v4 (C++11) remain options there.

### ADR-02: a static injector core and a separate runtime engine, sharing one vocabulary

- **Decision:**
  - `make_injector(...)` builds a fully static injector.
  - `runtime::builder` builds a type-erased container.
  - Both accept the same binding expressions (`bind<I>.to<C>().in(scope)`), modules and options.
  - A static injector can be imported into a runtime builder, so plugins can depend on core services that are wired statically.
- **Why:** compile-time wiring measured at hand-wired speed, while runtime lookups cost 4–25×. But plugin hosts and configuration-driven graphs need registration at run time. One vocabulary means one set of concepts to learn and test.
- **Rejected:**
  - Runtime only: worse performance and later errors.
  - Static only: no plugins.
  - Fruit's model (types checked at compile time, injector built at run time): pays the runtime cost even when it isn't needed.
- **Consequences:** two engines to maintain. Mitigated by a shared discovery layer, a shared vocabulary and one shared test suite parameterized over both engines.

### ADR-03: three constructor-discovery strategies behind one trait (confirm in S1, S5)

- **Decision:** `trellis::ctor_traits<T>` resolves, in this order:
  1. **Explicit**, from any of three sources:
     - a specialization `template<> struct trellis::ctor_traits<T> : trellis::ctor<Args...> {};`
     - a member alias `using trellis_ctor = trellis::ctor<Args...>;` (opt-in, intrusive)
     - the binding itself: `.with_ctor<Args...>()`.
  2. **Reflection (C++26):** the constructor marked `[[=di::inject]]`, or else the unique public, non-copy, non-move constructor with the most parameters. Parameter types come from `std::meta::parameters_of`.
  3. **Deduced (the C++20 default):** probe the arity with conversion-operator placeholder arguments (Boost.DI's technique), up to `max_ctor_arity` (default 16).
- **Why:**
  - The explicit and reflection strategies yield the dependency type list, which makes complete static graph analysis possible: cycles, captive dependencies, unused bindings and graph export.
  - Deduction gives zero intrusion on today's compilers. Automatic, macro-free wiring is Boost.DI's best-liked trait.
- **Consequences:**
  - Some checks are stronger in the explicit and reflection modes ([§7.4](#74-what-is-validated-where)).
  - Deduction has known edge cases: constructor templates, variadic constructors, overloads with the same arity, and defaulted parameters. The explicit route is the escape hatch, and compile-fail tests document each case.

### ADR-04: explicit lifetimes and an enforced request-form table

- **Decision:**
  - Lifetimes: `unique`, `singleton`, `singleton_inline`, `scoped<Tag>`, external (`to_ref`, `to_ptr`, `to_shared`) and `to_value`.
  - Each request form is allowed or rejected per lifetime ([§6.3](#63-the-request-form-compatibility-table)).
  - Unbound concrete types are implicitly `unique`. The strict option turns this off.
  - Requesting an unbound type by reference, pointer or `shared_ptr` is error E012, because nobody would own the object.
- **Rejected:** Boost.DI's "deduce" scope, where the consumer's parameter spelling chooses the lifetime. It produced dangling references and sharing nobody expected (R5).

### ADR-05: state lives only in injector and scope objects; storage is honest

- **Decision:**
  - Singleton storage defaults to `std::shared_ptr<Impl>` held by the injector, so a `shared_ptr<T>` request returns real shared ownership.
  - `singleton_inline` stores the object inside the injector with no heap allocation, and makes `shared_ptr<T>` requests a compile error.
- **Rejected:**
  - Function-local statics (Boost.DI #459).
  - Aliasing `shared_ptr`s that don't own their object (Fruit).
- **Consequences:**
  - The default costs one heap allocation per singleton, which is negligible.
  - Embedded users opt into inline storage.

### ADR-06: the diagnostics contract (confirm in S3)

- **Decision:**
  - Exactly one hard error per root cause.
  - It names the error ID, the type, the requested form, the path from the root and a suggested fix.
  - Error IDs (TRL-E001...) are stable and documented in an error catalog.
  - Output is golden-tested on every compiler.
  - No deprecation, undefined-function or link-time tricks.
- **Rendering:**
  - C++20: a `static_assert` whose condition names an error type.
  - Where `__cpp_static_assert >= 202306L` (P2741R3): a formatted message string.

### ADR-07: non-intrusive by default

- **Decision:** domain classes never have to include Trellis. Every optional type has a standard-library spelling that works too:
  - `di::lazy<T>`, `di::provider<T>` and `di::factory<Sig>`: `std::function`, or `std::move_only_function` on C++23.
  - `di::annotated<Tag, T>`: contextual bindings, or wrapper types the user defines.

### ADR-08: header-only, no dependencies, several delivery forms

- **Decision:**
  - Headers plus a generated single header, plus an optional C++20 module (`import trellis;`).
  - A CMake package, a vcpkg port and a Conan recipe from v0.2.
  - No Boost or other dependency.
  - Both engines work with `-fno-exceptions` and `-fno-rtti`.

### ADR-09: the runtime error model

- **Decision:**
  - `try_get<T>()` returns `trellis::result<T>`: `std::expected` when available, otherwise a small internal type.
  - `get<T>()` throws `trellis::resolution_error` when exceptions are enabled. Otherwise it calls a handler installed with `trellis::set_error_handler`, and calls `std::abort()` only if that handler returns.
  - The library never calls `exit()`.

### ADR-10: composition through variadic modules, never fluent type accumulation

- **Decision:** `make_injector(xs...)` and `make_module(xs...)` take bindings and modules as a pack and flatten them once into one binding map.
- **Why:** a chain like `.bind().bind().bind()`, where each call returns a new type holding everything before it, grows the type size quadratically. Fruit's flat-component blow-up is consistent with this, though not proven to be the cause.
- **Consequence:** modules are plain values, cheap to compose.

### ADR-11: type identity as a policy in the runtime engine

- **Decision:** the static engine uses types directly and has no IDs. In the runtime engine, the `key_policy` option chooses how each `type_key` is made:
  - `address` (default): the address of an inline variable template. Fastest, but valid only within one linked image.
  - `name`: a hash of the compile-time type name. It is stable across shared libraries built with the same compiler.
  - `rtti`: `std::type_index`.
- **Plugins** require `name` or `rtti`, and CI covers that with a test that loads a plugin at run time.

### ADR-12: threading model (confirm in S4)

- **Decision:**
  - The default is `threading::safe`: lazy singleton slots are initialized exactly once, even under concurrent `create()`.
  - `threading::none` removes the synchronization.
  - `eager = true` constructs all singletons inside `make_injector`, in dependency order, so later resolution is just reading initialized storage.
  - Scopes are owned by a single thread unless configured otherwise.
- **Documentation recommends eager mode for applications:** it fails fast at startup and behaves the same regardless of which services get created first. Chromium's design principles ban lazy instantiation for this reason.

### ADR-13: governance and release discipline

- See [§15](#15-governance-and-sustainability). Recorded as an ADR because R12 is the single largest risk the research identified.

---

## 5. Architecture

### 5.1 Layers

```
 user code    composition root (main, or one wiring .cpp)
              make_injector(modules...)            runtime::builder / container
                     │                                       │
   ┌─────────────────▼────────────────┐   ┌─────────────────▼─────────────────┐
   │ Static engine                    │   │ Runtime engine                     │
   │  binding map (types)             │   │  registry → frozen table (dense ID)│
   │  resolver (memoized concepts)    │   │  type-erased creators              │
   │  validator + explain pass        │   │  build()-time validator            │
   │  storage: slots, scopes, teardown│   │  scopes, plugins, error model      │
   └─────────────────┬────────────────┘   └─────────────────┬─────────────────┘
                     └──────────────┬────────────────────────┘
   ┌────────────────────────────────▼──────────────────────────────────────────┐
   │ Binding vocabulary: bind / to / in, lifetimes, qualifiers, multibind,     │
   │ factories, lazy / provider, modules, overrides, decorators, options       │
   └────────────────────────────────┬──────────────────────────────────────────┘
   ┌────────────────────────────────▼──────────────────────────────────────────┐
   │ Discovery: ctor_traits<T>  ←  explicit | reflection (C++26) | deduced     │
   └────────────────────────────────┬──────────────────────────────────────────┘
   ┌────────────────────────────────▼──────────────────────────────────────────┐
   │ Meta toolkit: type_list, type_map, fixed_string, type_name, type_key,     │
   │ result<T>, feature detection, compiler workarounds (config.hpp)           │
   └───────────────────────────────────────────────────────────────────────────┘
```

### 5.2 Repository layout

```
trellis/
├── include/trellis/
│   ├── trellis.hpp            # umbrella header
│   ├── config.hpp             # version, feature detection, compiler workarounds
│   ├── meta/                  # type_list, type_map, fixed_string, type_name, type_key, result
│   ├── discovery/             # ctor_traits, explicit, deduced, reflect (C++26 only)
│   ├── dsl/                   # bind, lifetimes, qualifiers, multibind, factory, lazy,
│   │                          # module, override, decorate, options
│   ├── static/                # injector, resolve, storage, scope, validate
│   ├── runtime/               # builder, container, registry, plugin, errors
│   ├── diagnostics/           # error ids and types, explain pass, formatting
│   └── testing/               # override helpers, validate<Roots...>, gMock helpers
├── modules/trellis.cppm       # C++20 module interface
├── tests/
│   ├── unit/                  # behaviour tests (GoogleTest)
│   ├── compile_fail/          # snippets that must not compile, with expected-output rules
│   ├── diagnostics_golden/    # normalized compiler output, per compiler
│   ├── property/              # random-graph generator and equivalence oracle
│   ├── plugins/               # dlopen / LoadLibrary tests for the runtime engine
│   └── module/                # `import trellis;` smoke tests
├── benchmarks/
│   ├── compile/               # graph generator (from the research harness), budgets.json
│   └── runtime/               # resolve, build, scope and container microbenchmarks
├── examples/                  # CLI app, HTTP server with request scope, plugin host,
│                              # game subsystems, testing with gMock
├── docs/                      # guide, reference, error catalog, cookbook, migration, adr/
├── tools/amalgamate.py        # single-header generator
├── cmake/                     # package config, warnings, sanitizers
└── .github/workflows/         # ci.yml, nightly.yml, bench.yml, release.yml
```

### 5.3 How `injector.create<App>()` compiles (static engine)

1. **Flattening.** `make_injector(xs...)` flattens modules into one `binding_map<B1, ..., Bn>`. This is an inheritance-based map keyed by (type, qualifier). Duplicate keys are found here and reported as E003. Storage is then built: one slot per binding the injector owns.
2. **Request.** `create<App>()` requests `App` by value from the root context.
3. **Resolving each requested form:**
   1. Find the binding for the key, either explicit or an implicit `unique` for concrete types.
   2. Check the request form against the binding's lifetime (E005 or E012), and the consumer's lifetime against the dependency's (captive check, E006).
   3. Discover the implementation's constructor: a type list in the explicit and reflection modes, an arity in deduced mode.
   4. Satisfy each parameter recursively through the memoized concept `resolvable<Ctx, Key, Form, ConsumerLifetime>`.
4. **On success:** the generated code is a nest of direct constructor calls plus slot accesses for shared objects. Nothing remains at run time except what the requested ownership requires.
5. **On failure:** exactly one `static_assert` fires, after the explain pass has located the first failing edge and built the path ([§7.8](#78-diagnostics-engine)). Creation code for a failing graph is never instantiated, so errors don't cascade.

---

## 6. Public API sketches

These sketches set direction; spikes and API review will change details. The domain types match the research scenario, so the same program can be measured against the six frameworks already benchmarked.

### 6.1 Hello world

```cpp
#include <trellis/trellis.hpp>
namespace di = trellis;

struct ILogger     { virtual ~ILogger() = default;     virtual void log(std::string_view) = 0; };
struct IRepository { virtual ~IRepository() = default; virtual void save(int) = 0; };
struct IClock      { virtual ~IClock() = default;      virtual long now() const = 0; };
struct Config      { std::string db_url; };

class ConsoleLogger final : public ILogger     { /* ... */ };
class SystemClock   final : public IClock      { /* ... */ };
class SqlRepository final : public IRepository {
 public:
  SqlRepository(const Config& cfg, ILogger& log);           // discovered, no macro
};

class Service {
 public:
  Service(std::shared_ptr<IRepository> repo, ILogger& log, std::unique_ptr<IClock> clock);
};
class App {
 public:
  explicit App(Service& service);
  void run();
};

int main() {
  const Config cfg = load_config();
  auto injector = di::make_injector(
      di::bind<ILogger>.to<ConsoleLogger>().in(di::singleton),
      di::bind<IRepository>.to<SqlRepository>().in(di::singleton),
      di::bind<IClock>.to<SystemClock>(),                    // unique by default
      di::bind<Service>.in(di::singleton),                   // App takes Service&, so it needs an owner
      di::bind<Config>.to_ref(cfg));                         // external: never owned; constness kept
  injector.create<App>().run();
}
```

None of the domain classes includes Trellis. Deleting the `di::bind<Service>` line is error E012 (Service is requested by reference but nothing owns it), not a silent new object.

### 6.2 Lifetimes

| Lifetime | Spelling | Owner | Instances | Typical use |
|---|---|---|---|---|
| unique (default) | `.in(di::unique)` | the consumer | one per request | Value objects, per-call helpers |
| singleton | `.in(di::singleton)` | the injector (`shared_ptr` storage) | one per injector | Services, clients, caches |
| singleton_inline | `.in(di::singleton_inline)` | the injector (inline, no heap) | one per injector | Embedded or no-heap builds; `shared_ptr` requests rejected |
| scoped | `.in(di::scoped<Tag>)` | a scope object | one per scope instance | Per-request, per-session or per-world state |
| external | `.to_ref(x)`, `.to_ptr(p)`, `.to_shared(sp)` | the caller | the given object | Configuration and objects from other systems |
| value | `.to_value(x)` | the injector (a copy) | one | Constants and small configuration structs |

### 6.3 The request-form compatibility table

✓ allowed; ✗ compile error, with the reason shown. Constness carries through: a `const` external binding can't be requested as `T&`.

| Requested as | unique | singleton | singleton_inline | scoped | external (`to_ref`/`to_ptr`) | `to_shared` | value |
|---|---|---|---|---|---|---|---|
| `T` | ✓ new object | ✗ copies hide sharing; opt in with `.allow_copy()` | ✗ (same) | ✗ (same) | ✗ (same) | ✗ (same) | ✓ copy |
| `T&`, `const T&` | ✗ E005: nothing would keep it alive | ✓ | ✓ | ✓ (captive rule applies) | ✓ | ✓ | ✓ |
| `T*`, `const T*` (non-owning) | ✗ E005: ambiguous ownership; use `unique_ptr` | ✓ | ✓ | ✓ (captive rule) | ✓ | ✓ | ✓ |
| `std::unique_ptr<T>` | ✓ | ✗ would duplicate ownership | ✗ | ✗ | ✗ | ✗ | ✗ |
| `std::shared_ptr<T>` | ✓ sole owner | ✓ real shared ownership | ✗ not heap-owned | ✓ (captive rule) | ✗ | ✓ | ✗ |
| `std::weak_ptr<T>` | ✗ | ✓ | ✗ | ✓ | ✗ | ✓ | ✗ |
| `di::lazy<…>` / `di::provider<…>` | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |

**Rvalue-reference parameters** (amended by ADR-0003, 2026-10-30). A parameter of type `X&&` or `const X&&` is the request `X`, allowed and rejected exactly as `X` is: `T&&` asks for a fresh `T` that the consumer owns, and `std::unique_ptr<T>&&` for a `std::unique_ptr<T>`. Deduced discovery can't tell `T&&` from `T` on any compiler (spike S1). The rule holds in every discovery mode, so a class wires the same way whichever mode discovers it.

**Deleting through a base class** (amended by ADR-0004, 2026-11-01, provisional). Where the table allows `std::unique_ptr<T>`, a binding whose implementation is a class derived from `T` needs `T`'s destructor to be virtual: otherwise `std::default_delete<T>` would delete the implementation through `T`, undefined behaviour, and the request is E014. `std::shared_ptr<T>`, whose control block deletes the class it was made for, and the other allowed forms are unaffected.

**The captive-dependency rule (E006).**

- A binding with lifetime L may hold a dependency with lifetime L′ through a sharing form (`T&`, `T*`, `shared_ptr`, `weak_ptr`) only if L′ lives at least as long as L.
- Lifetimes are ordered: external ≈ value ≈ singleton > `scoped<Outer>` > `scoped<Inner>` > unique.
- Owning forms (`T`, `unique_ptr`) are always allowed.
- Every static mode can check this, because each edge knows its consumer's lifetime.

**Documented precondition** (as in every DI container): objects created by an injector must not outlive it if they hold its singletons by reference. Holding them by `shared_ptr` is the way to outlive it.

### 6.4 Scopes

```cpp
struct request_scope {};                                     // a tag type the user defines

auto injector = di::make_injector(
    di::bind<IUserRepo>.to<PgUserRepo>().in(di::singleton),
    di::bind<RequestContext>.in(di::scoped<request_scope>),
    di::bind<Handler>.in(di::scoped<request_scope>));

void on_request(Request& req) {
  auto scope = injector.begin_scope<request_scope>(di::seed<Request&>(req));  // per-request input
  scope.create<Handler&>().handle();
}  // scoped objects are destroyed here, in reverse order of creation
```

- Scopes nest: `scope.begin_scope<transaction_scope>()`.
- A singleton that depends on `RequestContext&` is error E006.
- Requesting a scoped binding from the injector outside any scope is error E011.

### 6.5 Qualifiers

```cpp
// (a) Contextual bindings: non-intrusive.
di::bind<IDatabase>.to<PgPrimary>().in(di::singleton),                                   // default
di::bind<IDatabase>.to<PgReplica>().in(di::singleton).when_injected_into<Reporting>(),

// (b) Annotated types: explicit at the consumer (an opt-in framework type).
struct Primary {};
struct Replica {};
class Reporting {
 public:
  Reporting(di::annotated<Primary, IDatabase&> primary, di::annotated<Replica, IDatabase&> replica);
};
di::bind<di::annotated<Replica, IDatabase>>.to<PgReplica>().in(di::singleton),

// (c) String names, which are tags too: di::named<"replica">.
```

Contextual binding handles the common case without touching the class. Annotation handles one consumer needing two implementations of the same interface. Targeting a constructor parameter by position (`.when_injected_into<C>(di::at<1>)`) is possible but brittle when parameters are reordered, so it is P2 at most.

### 6.6 Multibindings

```cpp
auto injector = di::make_injector(
    di::multibind<IPlugin>.to<MetricsPlugin>().in(di::singleton),
    di::multibind<IPlugin>.to<AuditPlugin>().in(di::singleton),
    di::multibind<IMiddleware>.to<Auth, Gzip, Logging>());          // unique; order preserved

class Host     { public: explicit Host(std::span<IPlugin* const> plugins); };                  // shared elements
class Pipeline { public: explicit Pipeline(std::vector<std::unique_ptr<IMiddleware>> chain); }; // owned elements
```

- **Collection forms:** `std::vector<std::unique_ptr<I>>`, `std::vector<std::shared_ptr<I>>`, `std::vector<I*>`, `std::span<I* const>` and `std::vector<std::reference_wrapper<I>>`.
- **Validation:** each element is checked against the request-form table.
- **Ordering:** registration order across modules, which is deterministic and documented.

### 6.7 Factories and assisted injection

```cpp
class Connection final : public IConnection {
 public:
  Connection(std::string host, int port, ILogger& log, IClock& clock);   // host and port are runtime arguments
};

class Pool {
 public:
  // The framework type spelling:
  explicit Pool(di::factory<std::unique_ptr<IConnection>(std::string, int)> make);
  // ...or the standard-library spelling, with no framework type in the class:
  // explicit Pool(std::function<std::unique_ptr<IConnection>(std::string, int)> make);
};
```

- **The rule:** the factory's parameters bind, in order, to the implementation constructor's leading parameters. A parameter matches only if its type is the same once references and `const` are removed. The remaining parameters are injected.
- **Mismatches** are error E008, with both signatures shown.
- **Explicit bindings come first:** a `std::function` the user bound explicitly wins over the automatic factory.
- **Lifetime:** a factory holds a reference to its injector and must not outlive it. A version holding shared ownership is P2.

### 6.8 Lazy and provider injection

```cpp
class Exporter {
 public:
  explicit Exporter(di::lazy<IS3Client&> s3);                     // resolved on first use, then cached
  void flush() { s3->put(buffer_); }
};
class Worker {
 public:
  explicit Worker(di::provider<std::unique_ptr<ITask>> next);     // a new task on every call
};
```

- **Deferred edges.** `lazy` and `provider` edges are not followed during cycle analysis, so `A(lazy<B&>)` with `B(A&)` is legal.
- **Re-entry.** If `A`'s constructor dereferences the lazy `B`, it re-enters `A`'s own construction. That is caught at run time as error R004, in all builds and under every threading mode (amended by ADR-0012, 2026-10-30; the draft said debug builds and `threading::safe`).

### 6.9 Modules and composition roots

```cpp
inline auto storage_module(const Config& cfg) {
  return di::make_module(
      di::bind<Config>.to_ref(cfg),
      di::bind<IRepository>.to<SqlRepository>().in(di::singleton));
}
inline auto logging_module() {
  return di::make_module(di::bind<ILogger>.to<ConsoleLogger>().in(di::singleton));
}

auto injector = di::make_injector(storage_module(cfg), logging_module(),
                                  di::bind<IClock>.to<SystemClock>());
```

**Recommended pattern for build times (R20).** Put the composition root in one `wiring.cpp` that exposes a plain function, such as `std::unique_ptr<App> make_app(const Config&)`. Only that file includes Trellis and pays the template cost, so editing a domain class doesn't recompile the wiring's templates.

**Very large graphs** have two P2 features:

- FR-19 module requirements: `di::make_module(di::requires<ILogger>, ...)` lets a module be compiled and checked in isolation.
- FR-20 type-erased bundles: `di::export_bundle<Keys...>(injector)` lets one translation unit use a subsystem's injector as a parent without seeing its template internals.

### 6.10 Testing

```cpp
TEST(Service, SavesThroughRepository) {
  testing::StrictMock<MockRepository> repo;
  auto injector = di::make_injector(
      app_module(test_config()),
      di::override<IRepository>.to_ref(repo));       // E009 if no IRepository binding exists to override
  EXPECT_CALL(repo, save(42));
  injector.create<Service>().handle(42);
}

static_assert(di::testing::wireable<decltype(make_app_injector(std::declval<const Config&>())), App>);
```

- **Isolation.** Each test's injector owns its own singletons, so tests can't leak state into each other (R6).
- **`di::testing::create_all(injector)`** constructs every singleton. It suits a smoke test that fails fast.
- **`di::testing::unused_bindings<Roots...>()`** lists bindings no root reaches. It works in the explicit and reflection modes.
- **Overrides** (amended by ADR-0010, 2026-11-01, T1.7, provisional: awaits the owner's confirmation). `di::override<K>` has `di::bind<K>`'s vocabulary and is an argument of `make_injector` only, never of `make_module`. It replaces the binding of `K` in place, wherever the other arguments hold it, modules included. Two overrides of one key are E003. An override is a binding like any other for the request-form table: `.to_ref()` serves references and pointers, so the example's `Service` takes `IRepository&`; one that takes a `std::shared_ptr<IRepository>`, as §6.1's does, needs `.to_shared()`. `wireable` and `create_all` live in `<trellis/testing/wiring.hpp>`, which tests include themselves.

### 6.11 Runtime container and plugins

```cpp
di::runtime::builder b{di::options{.key_policy = di::key_policy::name}};
b.import(core_injector);                                   // statically wired services become available
b.add<ILogger, ConsoleLogger>(di::singleton);              // constructor discovered at compile time
b.add<IRepository>(di::singleton, [](di::runtime::resolver& r) {
  return std::make_unique<SqlRepository>(r.get<const Config&>(), r.get<ILogger&>());
});
for (auto& plugin : load_plugins(dir)) plugin.register_services(b);   // extern "C" entry point

di::runtime::container c = b.build();                      // validates, then freezes into flat tables
IService& svc = c.get<IService&>();                        // throws, or calls the handler, on failure
auto maybe = c.try_get<IMetrics&>();                       // di::result<IMetrics&>
```

Example error, matching NFR-07 and R4:

```
trellis::resolution_error [TRL-R001]: no registration for 'IRepository' (requested as 'IRepository&')
  required by: App -> Service
  registered nearby: IRepository [named "replica"]; did you mean di::named<"replica">?
```

### 6.12 C++26 reflection mode (experimental)

This mode turns on automatically when `__cpp_impl_reflection` and `__cpp_lib_reflection` (P2996) are defined. Today that means GCC 16 with `-std=c++26 -freflection`, and Bloomberg's clang-p2996 fork for testing.

```cpp
namespace myapp::services {
struct [[=di::component]] [[=di::singleton]] [[=di::provides<IRepository>]]
SqlRepository final : IRepository {
  [[=di::inject]] SqlRepository(const Config& cfg, ILogger& log);   // optional: picks this constructor
};
struct [[=di::component]] [[=di::provides<IClock>]] SystemClock final : IClock { /* ... */ };
}

auto injector = di::make_injector(
    di::scan<^^myapp::services>(),        // registers every [[=di::component]] in the namespace
    di::bind<Config>.to_ref(cfg));
```

- **No arity probing.** Constructor discovery uses `members_of`, `is_constructor` and `parameters_of`.
- **Readable messages.** Errors use `display_string_of` inside formatted `static_assert` messages.
- **Scanning only sees what is included.** It finds components declared in headers that the wiring translation unit includes, and the docs say so.

### 6.13 Options

```cpp
auto injector = di::make_injector<di::options{
    .strict = true,                     // no implicit unique for unbound concrete types
    .eager = true,                      // construct singletons inside make_injector
    .threading = di::threading::none,   // single-threaded application
    .max_ctor_arity = 12,
}>(app_module(cfg));
```

`max_ctor_arity` bounds deduced discovery's arity search: 16 by default and at most 32, as a longer constructor can be declared explicitly, with no limit (amended by ADR-0003, 2026-10-31, T1.3, provisional).

**Per-injector, not global.** Options are a C++20 class-type template argument with designated initializers. Making them per-injector avoids configuration through global macros, which is fragile under the one-definition rule (ODR). Macros remain only for things that must be global, such as disabling the reflection backend.

---

## 7. Internal design

### 7.1 Meta toolkit

- **`type_list<Ts...>`.** Its algorithms use flat pack expansions and inheritance-based sets, never recursive variadic templates, and fold expressions only over packs that can't exceed 256 operands, as Clang rejects longer ones (amended by ADR-0010, 2026-10-31).
  - `contains` asks `std::is_base_of` whether a tag for `T` is a base of the list's indexed set, a class built once per list (amended by ADR-0010, 2026-10-31).
  - `index_of` resolves one non-template overload per element on a per-list class, and scans a `constexpr` array of booleans only for a repeated or missing type (amended by ADR-0010, 2026-10-31).
  - `at<I>` uses compiler intrinsics where available (such as `__type_pack_element`), then C++26 pack indexing (`Ts...[I]`), then indexed base classes.
- **`type_map`.** It is a single struct inheriting from `entry<K_i, V_i>...`. Each entry declares one non-template lookup overload for its key, and the map gathers them, so each lookup is one overload resolution, with no search and no deduction (amended by ADR-0010, 2026-10-31: the draft deduced the value from `entry<K, V>*`, and GCC 13 and 14 match `T&` keys against `T&&` bases when deducing).
- **`fixed_string<N>`.** Used for named qualifiers and for building messages at compile time.
- **`type_name<T>()`.** Computed at compile time from `__PRETTY_FUNCTION__` or `__FUNCSIG__` on C++20, and from `std::meta::display_string_of(^^T)` on C++26. It feeds runtime messages, C++26 `static_assert` messages and the name-based `type_key`.
- **`result<T>`.** `std::expected<T, failure>` when `__cpp_lib_expected` is defined, otherwise a minimal internal version (amended by ADR-0006, 2026-10-31: the error type is `trellis::failure`, because `trellis::error` is the compile-error namespace).
- **`config.hpp`.** All compiler workarounds live here, behind named macros, each with a link to the compiler bug and a test.

### 7.2 Constructor discovery (confirm in S1 and S5)

**Explicit strategy**

- The primary `ctor_traits<T>` falls through to the next strategy.
- Specializations, member aliases and `.with_ctor<Args...>()` give `ctor<Args...>`.
- A `static_assert` checks the declaration the way the object will be built, so an explicit declaration can't lie: an aggregate must brace-initialize from `Args...` (`T{std::declval<Args>()...}`, so narrowing is rejected and brace elision allowed), and any other type must satisfy `std::is_constructible_v<T, Args...>` and is built with parentheses (amended by ADR-0003, 2026-10-31, T1.2, provisional).

**Reflection strategy (C++26)**

- **Candidates:** `members_of(^^T, std::meta::access_context::current())`, keeping members where `is_constructor` is true, and dropping copy, move and deleted constructors and non-public ones.
- **Choice:** a constructor annotated `[[=di::inject]]` must be unique and wins. Otherwise the unique constructor with the most parameters is chosen. A tie is error E004.
- **Parameter types:** `parameters_of(ctor)` mapped through `type_of`, then spliced into `ctor<...>` with `substitute`.
- **Constructor templates** are skipped and need the explicit strategy.

**Deduced strategy (C++20 default)**

The trick, as spike S1 chose it (amended by ADR-0003, 2026-10-30). It replaces the draft sketch, whose two `const` operators S1 found ambiguous for every by-value parameter:

```cpp
template <class Consumer, class T>
concept not_self = !std::same_as<std::remove_cvref_t<T>, Consumer>;  // drops copy/move ctors

template <class Consumer, class Ctx>
struct any_arg {                                   // the resolving placeholder argument
  Ctx& ctx;
  template <class T> requires not_self<Consumer, T> && can_resolve<Ctx, T>
  operator T() { return ctx.template resolve<T>(); }               // non-const: value forms
  template <class T> requires not_self<Consumer, T> && (!can_resolve<Ctx, T>)
  operator T() = delete;                                            // deleted twin
  template <class T> requires not_self<Consumer, T> && can_resolve<Ctx, T&>
  operator T&() const { return ctx.template resolve<T&>(); }        // const: T& and const T&
  template <class T> requires not_self<Consumer, T> && (!can_resolve<Ctx, T&>)
  operator T&() const = delete;                                     // deleted twin
};
// Discovery runs first, with probe_arg<Consumer>: the same two shapes, without can_resolve.
// arity = largest N <= max_ctor_arity (default 16) such that
//         std::is_constructible_v<Impl, probe_arg x N>,
// found by a flat, top-down if constexpr chain and memoized per class. Resolution then uses
// any_arg at exactly that arity, so an unresolvable parameter can't select a shorter constructor.
```

- **Why this set** (amended by ADR-0003, 2026-10-30). A by-value parameter selects the non-const `operator T()`: the placeholder is an rvalue, and a non-const object parameter ranks better than a const one. `T&` and `const T&` can only use the lvalue-yielding `operator T&()`, with `T` deduced as `const U` for a `const U&`. The deleted twins keep an unresolvable request from falling back to the other operator, which would bind a temporary to a `const X&` of a unique `X`, or copy a singleton requested by value. S1's prototype keys `can_resolve` on the request alone; Trellis also keys it on the consumer's lifetime, as [§7.3](#73-resolution-and-memoization)'s memoization key requires.
- **Forms** (amended by ADR-0003, 2026-10-30). S1 tested 35 parameter forms on gcc-13, gcc-14, clang-18, clang-20 and clang-18 with libc++. 29 are deduced exactly everywhere except gcc-13, which deduces 28 and needs the workaround below. `T&&`, `const T&&` and `unique_ptr<T>&&` are reported as their by-value forms, which [§6.3](#63-the-request-form-compatibility-table) makes the rule. `std::optional<T>`, `std::tuple<T>` and `std::any` are ambiguous on every compiler, because their unconstrained converting constructors accept the placeholder; they need the explicit route, and Trellis's own wrapper types must not have such constructors. The table goes into `docs/internals/deduction.md`. MSVC is untested; ADR-0003 names the fallback operator sets.
- **Workarounds in `config.hpp`** (amended by ADR-0003, 2026-10-30). On GCC < 14 the probe also declares the deleted twins of `any_arg`'s operators, constrained so that no parameter selects them, because gcc-13 otherwise mis-selects `operator T&()` for rvalue-reference parameters; S1's `operator T&&() const` did the same but also accepted `std::optional<T>` and `std::tuple<T>` there (amended by ADR-0003, 2026-10-31, T1.3, provisional). A `#pragma GCC diagnostic` push and ignore around construction silences GCC's `-Wconversion` warning about the object-parameter tie-break. On MSVC, the placeholder-repeating alias goes through a class template, which cl 19.51 needs to expand it over one index, and the array-member check uses braces with the probe's conversion to an array reference deleted, because cl's parenthesized aggregate initialization refuses members of aggregate class type (amended by ADR-0003, 2026-11-01, platform fixes, provisional).
- **Copy and move constructors** are excluded by the constraint that the conversion's target is not `Consumer`.
- **Aggregates** are probed with braces only when the type has no user-declared constructor. Configuration structs are normally bound as values anyway. An aggregate with an array member, or with a member that no placeholder can initialize, such as a `std::optional`, is refused with E004 on every compiler, rather than counted differently by each or silently cut short (amended by ADR-0003, 2026-10-31, T1.3, provisional). Discovery's greedy check and the engine's check of a deduced aggregate's members use parentheses, which never elide braces, except on MSVC, and the creation code keeps braces (amended by ADR-0003, 2026-11-01, the Apple Clang fix, provisional).
- **Known limits, each with a compile-fail test and an error that points to the explicit route:**
  - "Greedy" constructor templates such as `template<class U> X(U&&)`.
  - Variadic constructors.
  - Overloads with the same arity.
  - Defaulted parameters, which get injected. This is documented, and the explicit route avoids it.
- **Residual limits** (amended by ADR-0003, 2026-10-30), which discovery can't detect, so they are documented and point to the explicit route: same-arity overloads next to a shorter constructor, which is then chosen silently; constructor templates whose constraints the probe satisfies; and private, deleted and absent constructors, which look the same. Aggregate array members, whose count depends on the compiler (GCC and Clang elide the braces and count each element, cl counts the array as one field: T1.13), are E004 instead (above; amended by ADR-0003, 2026-10-31, T1.3, provisional).

### 7.3 Resolution and memoization

- **Memoization key:** (key, request form, consumer lifetime). Each triple is evaluated once through a concept or a class-template instantiation, and compilers cache both.
- **Not in the key:** the path or the depth, on the normal pass. This is the qlibs/di lesson (R9): a path in the type multiplies instantiations by the number of paths, which grows exponentially in graphs with many shared dependencies.
- **The path is built only by the explain pass**, which runs only after a failure and follows only the first failing edge ([§7.8](#78-diagnostics-engine)), so it costs O(depth × arity).
- **Instantiation depth budget:** at most 4 levels per hop (NFR-09). The S2 spike measures it with `-ftemplate-depth` sweeps on chains of 50–500. The budget covers the creation code as well as the check (amended by ADR-0014, 2026-10-31): S2 measured 2 (GCC) or 4 (Clang) levels per hop for the check and 3–4 for the creation code, and 221–298 hops on GCC and 251 on Clang at the default limits, with a lean per-hop shape.

**Cycles in deduced mode are the main open design problem.** In explicit and reflection modes the graph is known, so a `consteval` depth-first search over dense type indices reports E007 with the full cycle. In deduced mode, parameter types are discovered only by instantiating conversion operators, so a cycle makes the compiler hit its own recursion error ("satisfaction of constraint ... depends on itself" or an instantiation-depth limit) before the library can report one. Spike S2 compares three options:

1. **Depth-bounded resolution.** Add a depth counter to the key, only while a cycle check is enabled. Cost is roughly O(nodes × distinct depths); the spike measures it at N = 100 and 1,000 with fan-in 3 and 10. Make it the default if it costs ≤ 1.2×.
2. **Opt-in checking.** Keep option 1 behind `options{.cycle_check = true}`, used for CI builds and when debugging a cryptic recursion error.
3. **Rely on full-validation builds.** Rely on explicit or reflection mode in a test build (`di::testing::validate`). Document that a cycle in deduced mode produces the compiler's recursion error, and list the signatures to look for.

**Chosen option** (amended by ADR-0014, 2026-10-31): option 3 by default, option 2 as the opt-in, and option 1 rejected. S2 measured option 1 at 1.20–2.41× the hand-wired compile time at N = 100, growing quadratically, and out of memory at N = 250 on Clang with fan-in 10, while memoization stayed at 0.62–1.10× up to N = 1,000. Under memoization a cycle never reaches a recursion limit: the compiler stops with a prompt hard error where the recursion re-enters a node (2–6 errors), and the error catalog documents those messages. The opt-in check is bounded at 200 hops (150 on MSVC before 19.50, an assumed boundary; amended by ADR-0014, 2026-10-31, provisional) and adds a relaxed second pass on failure, so it reports exactly one TRL-E007; it costs 1.2–2.4× at N = 100, only when enabled. Path tracking, which names the cycle, stays in the explicit and reflection modes; in deduced mode it ran out of memory from N = 20–50.

### 7.4 What is validated where

| Check | Deduced (C++20) | Explicit / reflection | Runtime engine |
|---|---|---|---|
| Missing binding / not constructible (E001, E002) | compile time | compile time | `build()` if dependencies are known; otherwise first resolve, or `validate(roots...)` |
| Duplicate binding (E003) | compile time | compile time | `build()` |
| Ambiguous constructor (E004) | compile time: constructor templates and variadic constructors that accept anything, and aggregates with an array member or a member no placeholder can initialize; same-arity overloads look like a missing constructor there, so they are E002 (amended by ADR-0003, 2026-10-31, T1.3, provisional) | compile time | — (discovered at compile time) |
| Request form vs lifetime (E005, E012) | compile time | compile time | `build()` or resolve |
| Captive dependency (E006) | compile time | compile time | `build()` |
| Cycle (E007) | see [§7.3](#73-resolution-and-memoization) (spike S2) | compile time, with the full cycle | `build()` |
| Factory signature (E008) | compile time | compile time | compile time (at `add<>`) |
| Override without a target (E009) | compile time | compile time | `build()` |
| Unused bindings (lint) | needs roots: `testing::unused_bindings<Roots...>` | compile time | `build()` warning hook |

The runtime engine needs dependency lists for classes discovered by deduction. Spike S6 evaluates two ways to get them:

- **Self-registration.** The conversion operator touches a variable template whose dynamic initializer records `(class, parameter index, type key)` at startup. This is legal C++, but initialization order is unspecified and the behaviour across shared libraries needs checking.
- **A dry run** inside `validate(roots...)`.

### 7.5 Storage, lifetimes and teardown

- **Injector layout:** a slot for each binding the injector owns, held as indexed base classes, one per binding position and empty for bindings the injector doesn't own, rather than in a `std::tuple<slot<B>...>`, whose recursive implementation nests a class per element (amended by ADR-0005, 2026-11-01, T1.5); a teardown stack; and the options.
- **`slot<B>`** holds a `std::shared_ptr` to the `Impl` (in raw bytes, as a `std::shared_ptr<void>`, its control block's type shared with every class of the same size and alignment unless the class shares from this; amended by ADR-0005, 2026-11-01, T1.14), or, for inline storage, the object in aligned raw bytes, plus its one-byte initialization state, which also serves as the inline object's engaged flag (amended by ADR-0005, 2026-10-30; the draft said `std::optional<Impl>`). An inline slot costs `sizeof(Impl)` plus 1 byte, rounded up to `alignof(Impl)`: 2, 16 and 65 bytes for objects of 1, 8 and 64 bytes, as measured in S4 (the 64-byte case was `char[64]`, alignment 1; with 8-byte alignment it would be 72 bytes).
- **Teardown stack:** a `std::array` of `{destroy function, object}` pairs. Its capacity is the number of owned bindings, known at compile time, so it needs no heap. Each slot pushes itself when constructed, and the injector's destructor pops in reverse, so teardown runs in reverse order of construction for each injector.
  - **Ordering rules** (amended by ADR-0005, 2026-10-30). A slot pushes its entry after its constructor succeeds and before it publishes `ready`. A direct dependency is ready before its dependent's constructor runs, so its entry always lands below its dependents', even when different threads build them. Under `threading::safe` the counter is an `atomic<size_t>` with relaxed `fetch_add`. Failed attempts push nothing, so the capacity can't overflow.
  - The stack is unwound before the slots' storage is destroyed: it is declared after the slots, or `~injector` unwinds it (amended by ADR-0005, 2026-10-30).
- **Eager mode:** `make_injector` constructs every singleton in dependency order. If a constructor throws, everything already built is torn down in reverse order and the exception propagates, so `make_injector` gives the strong guarantee.
- **Scopes:** `scope<Tag, Parent>` holds slots for bindings scoped to `Tag`, plus its seeds and its own teardown stack, and refers to its parent.
  - Injectors and scopes are non-movable, because factories and lazies hold pointers into them.
  - Guaranteed copy elision still allows `auto x = make_injector(...)` and `auto s = injector.begin_scope<T>()`.
  - `make_unique_injector(...)` is for storing one in a member.
- **Versioning:** an inline namespace (`trellis::v1`) lets two major versions coexist in one program without breaking the one-definition rule.

### 7.6 Thread safety (confirm in S4)

- **`threading::safe`:**
  - Each lazy slot has a small atomic state, `empty → constructing → ready`.
  - **The slow path** (amended by ADR-0012, 2026-10-30; this settles the draft's choice between a per-injector mutex and `std::call_once`). The first thread moves the state from `empty` to `constructing` with a compare-exchange and constructs. Others re-read the state up to 128 times with a CPU pause, then set a waiter bit and block in `std::atomic::wait` on the state byte. The constructing thread publishes with an exchange, and calls `notify_all` only if the waiter bit was set. Where `__cpp_lib_atomic_wait` is missing, waiters use a per-injector mutex and condition variable instead. `std::call_once` is rejected: on libstdc++ it is 6–10× slower warm, it can't report R004, and its retry after a throw hangs under TSan with libstdc++.
  - If the constructor throws, the slot returns to `empty`, waiters wake, and the next caller retries (amended by ADR-0012, 2026-10-30).
  - Nested construction of different slots is fine. Re-entering the same slot can only happen through a lazy dereference inside a constructor, which reports R004 in all builds: each thread keeps a stack of the slots it is constructing and checks it before waiting (amended by ADR-0012, 2026-10-30; the draft said debug builds).
- **`threading::none`:** plain state flags.
- **`eager`:** after `make_injector` returns, every access is a plain read.
- **Runtime container:** after `build()` its tables are immutable, and its singleton slots use the same mechanism. The builder is single-threaded.
- **Documented guarantees**, one table per operation (construct, `create`, `begin_scope`, `get`, teardown), each backed by a ThreadSanitizer stress test.

### 7.7 Exceptions and RTTI

- **Exceptions from constructors:**
  - `create<T>()` gives the basic guarantee: singletons already built stay valid, and any unique objects built for the failed request are destroyed.
  - `make_injector` in eager mode gives the strong guarantee.
- **With `-fno-exceptions`:** the static engine is unaffected, because it has no failure path at run time, except R004 (re-entrant construction through a lazy), whose reporting under `-fno-exceptions` is an open point (amended by ADR-0012, 2026-10-30). The runtime engine's `get` uses the handler from ADR-09.
- **No RTTI in the static engine.** The runtime engine needs RTTI only for the `type_key::rtti` policy, and never uses `dynamic_cast`.

### 7.8 Diagnostics engine

**Error catalog (initial)**

| ID | Meaning | Example fix hint |
|---|---|---|
| E001 | No binding for an abstract type | `add di::bind<IRepository>.to<Impl>()` |
| E002 | Type not constructible: no viable constructor for the available bindings | Lists each constructor parameter and whether it resolved |
| E003 | Duplicate binding for a key | Names both modules |
| E004 | Ambiguous constructor | `use di::ctor_traits or [[=di::inject]]` |
| E005 | Requested form incompatible with the lifetime | `T& from a unique binding: bind T in di::singleton, or request T by value` |
| E006 | Captive dependency | `singleton 'Cache' holds scoped 'RequestContext&'` |
| E007 | Dependency cycle | Prints the cycle; suggests `di::lazy` |
| E008 | Factory signature does not match the constructor | Both signatures shown |
| E009 | Override without an existing binding | `did you mean di::bind?` |
| E010 | Constructor arity above `max_ctor_arity` | `raise max_ctor_arity or use di::ctor_traits` |
| E011 | Scoped binding requested outside its scope | `call injector.begin_scope<Tag>()` |
| E012 | Unbound concrete type requested by reference, pointer or `shared_ptr` | `bind it with an owning lifetime` |
| E013 | Unbound concrete type requested in strict mode (amended by ADR-0006, 2026-11-01, provisional) | `bind it, such as di::bind<T>` |
| E014 | Owning request would delete the implementation through a base class whose destructor isn't virtual (amended by ADR-0006, 2026-11-01, provisional) | `give I a virtual destructor, or request std::shared_ptr<I>` |
| R001–R005 | Runtime: missing registration, cycle, captive dependency, re-entrant construction, type-key mismatch across shared libraries | — |

**C++20 rendering**

- `create<T>()` checks the memoized analysis. On failure, the branch for the failing graph instantiates exactly one `static_assert(trellis::error::E001<IRepository, request<std::shared_ptr<IRepository>>, path<App, Service>>::reported, "TRL-E001: no binding for an abstract type");` and nothing else (amended by ADR-0006, 2026-10-30: the literal was added). (Amended by ADR-0006, 2026-11-01, T1.6, provisional: the condition is `trellis::error::E001<IRepository, request<std::shared_ptr<IRepository>>>::reported`, raised once per injector and request by one function template, `detail::raise`, whatever the root; the path, `path<App, Service>` with the parameter index, is in the instantiation note of the explain step that reports, Clang's second note and GCC's first "required from" line.)
- What each compiler prints (amended by ADR-0006, 2026-10-30; the draft said all three print the condition's template arguments on the first lines). Clang puts the condition, with the type, the request and the path, on the first error line. GCC puts the assertion's literal on the error line and the condition in the attached note. MSVC is expected to print only the literal (untested), so the type would appear only in its "see reference" notes. That is why the literal carries the ID. (Amended by ADR-0006, 2026-11-01, T1.6, provisional: the condition names the type and the request; the path is in an instantiation note, Clang's second and GCC's first "required from" line, and MSVC's first note names the error type, through `detail::raise`.)

**C++26 rendering** (`__cpp_static_assert >= 202306L`, P2741R3)

Also used in C++20 and C++23 modes on GCC 14 or later and Clang 20 or later, where P2741 works as an extension; `config.hpp` gates it on compiler versions rather than on the feature-test macro alone (amended by ADR-0006, 2026-10-30).

The message is built at compile time in a `fixed_string` buffer:

```
error: static assertion failed: trellis [TRL-E001]: no binding for abstract type 'IRepository'
  requested as 'std::shared_ptr<IRepository>' by 'Service' (parameter 1)
  path: App -> Service -> IRepository
  fix: add di::bind<IRepository>.to<YourImplementation>() (docs: errors/TRL-E001)
```

**The explain pass**

1. Start from the root.
2. For each constructor parameter, look up the memoized `resolvable` result (already computed on the normal pass) to find the first failing parameter.
3. Descend into it with the path extended, and stop at the deepest failure.
4. Classify that failure into one of the error IDs above.

(Amended by ADR-0006, 2026-11-01, T1.6.) Step 2 reads the memoized results of a declared constructor's parameters; a deduced constructor's parameters can't be listed, so the pass probes each position with the resolving placeholder and builds the class from placeholders to reach the failing request ([docs/internals/explain-pass.md](internals/explain-pass.md)).

**Golden tests**

- For every `tests/compile_fail/*.cpp` and every compiler, normalize file paths, line numbers and compiler noise, then check:
  1. compilation fails;
  2. the first error line matches the expected regular expression and contains the error ID;
  3. the output stays within the line and byte budgets (NFR-07).
- Golden files are reviewed whenever a compiler is upgraded.

**Usability check in Phase 4.** Five developers who don't know the library each fix five seeded wiring mistakes using only the compiler output. Target: at least 90% fixed in under two minutes each.

### 7.9 Runtime engine internals

- **`builder`.** Each registration records: the key (type and qualifier), the lifetime, a creator function pointer, the dependency keys if known, and its position within a multibinding.
- **`build()`:**
  1. Assign dense IDs.
  2. Resolve dependency keys to IDs.
  3. Validate, using a coloured depth-first search for cycles and a lifetime order for captive dependencies.
  4. Compute a topological order for eager mode.
  5. Freeze into flat arrays.
- **Lookup by key.** The common case is an open-addressing table built at freeze time and keyed by `type_key`. Small containers use a sorted array with binary search. Target: under 10 ns for a cached singleton.
- **Creators** are `void* (*)(resolver&, void* storage)`. They are generated at compile time:
  - deduced classes: `Impl(any_rt_arg<Impl, I>{&r}...)`;
  - explicit and reflection classes: from the known parameter list.
- **Plugins.** A plugin exports `extern "C" void trellis_register(trellis::runtime::builder&)`. The host and plugin must agree on compiler and settings, which `trellis_abi_tag()` checks when the plugin loads. CI covers Linux (`dlopen`), macOS and Windows (`LoadLibrary`).
- **Importing a static injector.** Each of its owned bindings is wrapped as a registration that refers to the injector's slot. The static injector must outlive the container, and debug builds check this.

### 7.10 Reflection backend internals

- **Isolated** in `discovery/reflect.hpp` and `dsl/annotations.hpp`, compiled only when the reflection macros are defined. A C++20 build never parses it.
- **What it provides:**
  - `ctor_traits<T>` through reflection;
  - the annotation types `component`, `singleton`, `scoped<Tag>`, `provides<I>`, `inject` and `qualifier<"name">`;
  - `scan<NS>()`;
  - formatted diagnostics.
- **Tested** on GCC 16 (`-std=c++26 -freflection`) and the clang-p2996 fork, with Clang trunk added when it gains support. EDG on Compiler Explorer is useful for manual cross-checks.
- **Risks noted in the research:**
  - Reflection details were still being refined in 2026 (P4101R1, "Consteval-only values", was applied in July 2026).
  - GCC 16 has friction when reflection and modules are combined, so the reflection backend stays out of the module build at first.
  - clangd can't parse reflection syntax yet.

---

## 8. Performance engineering

### 8.1 Compile time

| # | Technique | Why |
|---|---|---|
| C1 | Memoize per (key, form, lifetime); never per path or depth on the normal pass | qlibs/di's explosion (R9) |
| C2 | Inheritance-based maps; lookups by overload resolution | Avoid O(N) recursive searches per lookup |
| C3 | Flat pack expansions and pack-indexing intrinsics; folds only over packs that can't exceed 256 operands; no recursive variadic templates (amended by ADR-0010, 2026-10-31) | Instantiation count and depth; Clang rejects a fold over more than 256 operands |
| C4 | Flatten modules once; no fluent type accumulation | Fruit's flat-component blow-up (ADR-10) |
| C5 | Short, flat type names: `binding<K, Impl, L>`, not stacks of nested policies | Symbol length drives compiler memory and debug-info size |
| C6 | Instantiate creation code only for roots that are actually requested | Unused bindings cost only their binding type |
| C7 | ≤ 4 instantiation levels per hop, counting the creation code as well as the check (amended by ADR-0014, 2026-10-31) | NFR-09; dingo's 52-link limit. S2: the creation code nests 3–4 levels per hop and limits GCC's chains |
| C8 | Single-TU wiring pattern; optional explicit instantiation of `create<Root>` (P2) | R20; incremental builds |
| C9 | Profile every release with `-ftime-trace` and ClangBuildAnalyzer; keep a top-10 list of the costliest templates | Catches regressions early |

### 8.2 Run time

- **The static path is direct construction (R1).** A disassembly test (P1) compiles `create<App>()` at `-O2` and asserts that no calls into `trellis::` functions remain, apart from slot initialization in non-eager mode.
- **Heap use.** No allocations beyond what the requested ownership needs, plus one per default singleton (ADR-05).
- **Runtime engine.** Dense IDs, flat arrays, no `std::function` on the hot path, and an atomic acquire load for singletons that are already initialized.

### 8.3 Binary size

- **Keep heavy machinery out of the static engine:** no `std::function`, no iostreams or `<format>` in headers. Formatting lives only in the runtime engine's error path, behind a separate header.
- **No exception machinery** when exceptions are disabled.
- **Measured** with `bloaty` and stripped sizes for the scenario program and the 100-component graph (NFR-06).

### 8.4 Benchmark suite and CI gates

Start from the research harness (`experiments/`): the scenario program, `gen_scaling.py` and `measure.py`.

**Extensions:**

- Graph sizes 10, 25, 50, 100, 250, 500 and 1,000.
- Fan-in profiles: 1–3 dependencies (the research setting) and 10 dependencies for 90% of classes (Fruit's published setting).
- Layouts: one module, one module per 10 components, and several translation units.
- Request forms: `shared_ptr` (the research setting), references and `unique_ptr`.
- MSVC in addition to GCC and Clang.

**Metrics:** compile wall time (median of 3), peak memory, object and stripped binary sizes; and for run time, resolve, build, scope and container operations, as medians over repeats.

**Gates:**

- Pull requests: the scenario plus N = 100 on GCC and Clang, compared with the hand-written baseline built in the same job.
- Nightly: the full sweep on a dedicated runner, failing on regressions of more than 10% against the last release.

**Publishing:** raw data and scripts go into the docs alongside the hand-written baseline, so anyone can rerun them. The research found that the only published benchmarks were run by competing authors on workloads they chose, and contradicted each other.

---

## 9. Quality strategy

### 9.1 Test layers

| Layer | What it proves | Tooling |
|---|---|---|
| Unit | Lifetimes, sharing (identity checks), teardown order (recorded destructor calls), scopes, factories, multibindings, overrides, lazy and provider, eager mode, exceptions thrown from constructors, options | GoogleTest, for consistency with the gMock examples |
| Compile-fail | Every error ID has at least one snippet that must fail with the expected message | A CMake and Python runner, adapted from the research's `measure.py` |
| Golden diagnostics | First line, error ID and size budget, per compiler | Normalized snapshots |
| Property-based equivalence | For random graphs, Trellis and generated hand-written wiring agree on sharing, construction-order constraints and teardown order | The graph generator emits both programs plus an identity checksum. The research harness already checked every framework's checksum against the manual wiring. |
| Concurrency | No data races and exactly-once initialization under concurrent `create` and `get` | ThreadSanitizer stress tests |
| Memory safety | No leaks, no use-after-free, no undefined behaviour | AddressSanitizer and UndefinedBehaviorSanitizer on every unit test |
| Fuzzing | Random registration sequences never crash `build()`, and errors stay well-formed | libFuzzer on the runtime builder |
| Plugins | Loading at run time, type-key policies, ABI tag checks | Shared-library tests on all three OSes |
| Modules | `import trellis;` works | CMake 3.28+ with Ninja; MSVC |
| Documentation | Every code block in the docs compiles | Snippet extraction in CI |
| Static analysis | Common bug patterns, modern idioms | clang-tidy (bugprone, performance, modernize), with `-Werror` and high warning levels |

### 9.2 CI matrix

| Axis | Values |
|---|---|
| OS | Ubuntu 24.04, Ubuntu 22.04, macOS 14 and 15, Windows Server 2022 and 2025 |
| Compilers | GCC 12, 13, 14, 15, 16; Clang 16, 18, 20, 21, plus trunk nightly; Apple Clang 15 and 16; latest MSVC (19.51 on `windows-latest`, Visual Studio 2026, in October 2026); clang-cl |
| Standard libraries | libstdc++, libc++, MS STL |
| Language modes | C++20, C++23, C++26; GCC 16 with `-freflection`; the clang-p2996 fork nightly |
| Flags | Default; `-fno-exceptions`; `-fno-rtti`; `-O0` and `-O2`; ASan and UBSan; TSan |
| Delivery forms | Headers, the single-header amalgamation, the C++20 module |

**Pull requests** run a representative subset of about 12 jobs (each compiler family once, each flag set once). **Nightly** runs the full matrix. A failed nightly run opens an issue automatically and notifies both maintainers, because Fruit's CI failed for months without anyone noticing (R12). A red nightly blocks releases. (Amended by owner decision, 2026-11-01: NFR-03 and NFR-06 on Clang are accepted for v0.1, so the nightly reports the clang-18 scenario's numbers without failing on them; a Phase 2 task brings NFR-03 within budget, and `benchmarks/compile/budgets.json` stays strict.)

### 9.3 Definition of done for a feature

1. A design note, either a new ADR or an update to one.
2. Public API documented, with a snippet that compiles in CI.
3. Unit, compile-fail and golden tests passing on the pull-request matrix.
4. Benchmarks within budget.
5. A changelog entry.
6. For a new error, an error-catalog page.

---

## 10. Build, packaging and releases

- **CMake:**
  - an INTERFACE target `trellis::trellis`, with `find_package(trellis CONFIG)`, install rules and `FetchContent` support;
  - options `TRELLIS_BUILD_TESTS`, `TRELLIS_BUILD_BENCHMARKS`, `TRELLIS_BUILD_EXAMPLES` and `TRELLIS_ENABLE_MODULE`;
  - CMake 3.21 minimum, or 3.28 for the module.
- **Single header:** generated by `tools/amalgamate.py`, attached to each release and tested in CI.
- **Module:** `modules/trellis.cppm` uses `export module trellis;` and re-exports the public API. It is tested from Phase 1 (R15).
- **Package managers:** vcpkg port and Conan Center recipe at v0.2, updated automatically on each release. Boost.DI's packages still lack its 2026 fixes, which is the failure this avoids. A pkg-config file serves projects that don't use CMake.
- **Versioning:**
  - Semantic versioning. Releases 0.x may break the API, with migration notes. 1.0 freezes the public API.
  - Removals are deprecated for at least one minor release first.
  - The library provides version macros and the inline namespace `trellis::v1` (§7.5).
- **Release automation:** a tag triggers CI to build the amalgamation and docs, publish a GitHub release, and open pull requests updating the vcpkg and Conan recipes.
- **Release cadence:**
  - A patch release within 7 days of any merged fix.
  - A minor release every 6–8 weeks during 0.x.
  - No fix waits untagged for more than 14 days.

---

## 11. Documentation plan

| Document | Content |
|---|---|
| README | 30-second pitch, hello world, the hand-wired version side by side, and "when not to use a framework" (from the research) |
| Getting started | 10 minutes: install, first injector, first error message |
| Concepts | Bindings, keys, lifetimes, the request-form table, scopes, modules, composition roots |
| Guides | Testing with gMock; request scopes in servers; plugins and the runtime container; reflection mode; build-time performance (wiring in one translation unit, eager mode); thread-safety guarantees; exception safety |
| Error catalog | One page per E/R code: cause, minimal example, fix, related errors |
| Reference | Every public entity, generated from the headers and checked for completeness in CI |
| Cookbook | Configuration-driven selection; decorators; multi-tenant scopes; game subsystems; CLI tools |
| Migration guides | From hand-written DI (adopt one module at a time); from Boost.DI (syntax mapping and lifetime differences); from Fruit (components → modules, `INJECT` → deduction); from Kangaru (service definitions → bindings) |
| Internals | Deduction tricks (from S1), memoization, the explain pass, storage layout; aimed at the second maintainer |
| Benchmarks | Method, raw data, how to rerun, and comparison with the hand-written baseline |
| ADRs | `docs/adr/0001-...` onwards |

---

## 12. Roadmap

### 12.1 Spikes (Phase 0)

Each spike is a throwaway branch with a one- or two-page write-up and a decision.

| Spike | Question | Decides | Done when |
|---|---|---|---|
| S1 Deduced discovery | Which set of conversion operators resolves all 12 parameter forms without ambiguity on GCC, Clang and MSVC? What does arity probing cost? Can greedy constructors be detected? | ADR-03, §7.2 | A form × compiler table with every cell resolved or explained |
| S2 Memoization and cycles | Per-key memoization versus depth-bounded checking at N = 100 and 1,000, fan-in 3 and 10; instantiation depth per hop | §7.3 option, NFR-09 | Measurements recorded in the benchmark harness format |
| S3 Diagnostics rendering | E001 and E005 as C++20 error types and as C++26 messages, on three compilers; output size | ADR-06 | Golden outputs within budget |
| S4 Storage and threading | `std::call_once` versus an atomic state machine: size, speed, TSan results | ADR-12, §7.6 | Microbenchmarks and a clean TSan run |
| S5 Reflection prototype | Constructor discovery, annotations and namespace scanning on GCC 16 and clang-p2996 | §7.10 | The scenario program wired by reflection |
| S6 Runtime dependency recording | Self-registration versus a dry run for deduced classes, including across shared libraries | §7.4, Phase 5 | A working prototype plus a plugin test |

### 12.2 Phases

**Order of Phases 3 and 4** (amended by owner decision, 2026-10-30): diagnostics come before binding features, so Phase 4's tasks are expanded and implemented before Phase 3's. The table keeps the original numbers and weeks.

| Phase | Weeks | Main tasks | Exit criteria |
|---|---|---|---|
| **0 Foundations** | 1–3 | Repository, CMake, CI on 3 OSes; port the research harness; baselines for hand-written and Boost.DI wiring; spikes S1–S6 (S5 and S6 can be shortened); ADRs finalized; API review of §6 | Decisions recorded; CI green; baseline numbers committed |
| **1 Core MVP → v0.1** | 4–9 | Meta toolkit; explicit and deduced discovery; `bind/to/in/to_ref/to_value/to_shared`; lifetimes unique, singleton, singleton_inline, external, value; request forms and the table; storage and teardown; `create<T>`; modules and duplicate detection; overrides; errors E001–E005, E009, E010 and E012 (C++20 rendering); the module build | Scenario warning-free on the pull-request matrix; NFR-01, 03, 06, 07 and 10 met; N = 100 compile ≤ 1.3× hand-written. On Clang, NFR-03 (+0.38 s) and NFR-06 (+8,240 B) are accepted for v0.1, a Phase 2 task brings NFR-03 within budget, and `budgets.json` stays strict (amended by owner decision, 2026-11-01) |
| **2 Lifetimes and safety → v0.2** | 10–13 | Scopes, seeds and nesting; E006 captive dependencies; E007 cycles (per S2); eager mode; threading options with TSan tests; `lazy` and `provider`; `weak_ptr`; exception-safety tests; options and strict mode; vcpkg and Conan | Property oracle green on 1,000 random graphs; TSan clean; lifetimes and scopes documented |
| **3 Binding features → v0.3** | 14–18 | Qualifiers (contextual, annotated, named); multibindings; factories and assisted injection (`di::factory`, `std::function`, `move_only_function`); decorators; module requirements (P2) | All P0 and P1 static requirements done; budgets hold at N = 1,000, fan-in 10 |
| **4 Diagnostics → v0.4** | 19–21 | Explain pass with paths; full catalog; C++26 messages; "did you mean" suggestions for near-miss keys, such as a binding that differs only by qualifier; golden tests on every compiler; usability check | Every ID has a catalog page and a golden test; NFR-07 met on all compilers; usability target met |
| **5 Runtime engine → v0.5** | 22–27 | Builder and container; creators for every discovery mode; `build()` validation and freezing; runtime scopes and multibindings; plugins with type-key policies; error model; importing static injectors; thread safety; fuzzing | Plugin-host example working; 24 h of fuzzing without crashes; NFR-08 met |
| **6 Reflection → v0.6 (experimental)** | 28–32 (in parallel with 5 if two engineers) | Reflection discovery; annotations; `scan<NS>`; formatted diagnostics; CI on GCC 16 and clang-p2996 | Scenario and random graphs pass in reflection mode; no effect on C++20 builds |
| **7 Ecosystem → v0.9 beta** | 33–36 | Single header; finished docs site; five examples; migration guides; benchmarks page | Installs from vcpkg and Conan work in fresh projects on 3 OSes; every doc snippet compiles |
| **8 Beta → v1.0** | 37–44 | API review; a programme of 2–3 design partners using the beta in real code; revalidation on the newest compilers; sanitizer and fuzzing sweep; governance in place | No open P0 or P1 bugs; budgets green; at least 2 external users; at least 2 maintainers; 1.0 tagged |

**Early in Phase 1** (amended by ADR-0012 and ADR-0004, 2026-11-01): eager mode, `threading::none` with a ThreadSanitizer test of `threading::safe`, and strict mode took effect with storage and `create<T>()` (T1.5), as each is a small branch of that code. Phase 2 keeps their remaining work: the property oracle and the per-operation thread-safety guarantees of §7.6.

### 12.3 Requirement coverage by phase

| Phase | Requirements |
|---|---|
| 1 | FR-01, 02, 03, 04, 05, 06; NFR-01, 03, 06, 07, 10, 11, 13 |
| 2 | FR-07, 08, 12, 13; NFR-02, 09 |
| 3 | FR-09, 10, 11, 15, 19 (P2) |
| 4 | NFR-07 on all compilers; diagnostics quality |
| 5 | FR-14, 16 (P2), 20 (P2); NFR-08 |
| 6 | FR-18 |
| 7–8 | Packaging, docs, NFR-12, governance |
| After 1.0 | FR-17, 21, 22 |

---

## 13. The first nine weeks, week by week

- **Week 1.**
  - Create the repository, CMake skeleton and warning flags, with clang-format and clang-tidy configuration.
  - Set up CI on Ubuntu (GCC and Clang), macOS and Windows (MSVC).
  - Port the research harness into `benchmarks/` and commit the hand-written and Boost.DI baselines as CI runners measure them.
- **Week 2.**
  - Spike S1 (deduced discovery across 12 forms and 3 compilers) and spike S3 (diagnostics rendering).
  - Finalize ADR-03 and ADR-06.
- **Week 3.**
  - Spike S2 (memoization and cycles) and spike S4 (storage and threading).
  - Prototype S5 (reflection) for one or two days. S6 can be postponed to Phase 5.
  - Finalize ADR-02, 04, 05 and 12. Review the API in §6 and freeze the v0.1 surface.
- **Week 4.**
  - Build `type_list`, `type_map`, `fixed_string`, `type_name`, `result` and `config.hpp`, each with unit tests.
  - Add the explicit `ctor_traits`.
- **Week 5.**
  - Implement deduced discovery: `any_arg`, arity probing and the copy/move exclusion.
  - Add compile-fail tests for every known limit.
- **Week 6.**
  - Build the binding vocabulary (`bind`, `to`, `in`, `to_ref`, `to_value`, `to_shared`), modules and duplicate detection.
  - Encode the request-form table as a `constexpr` table, with checks and tests.
- **Week 7.**
  - Add slots, the teardown stack, `create<T>`, and the unique, singleton, singleton_inline, external and value lifetimes.
  - Write identity and teardown-order tests.
  - Get the scenario program running inside the harness.
- **Week 8.**
  - Implement E001–E005, E009, E010 and E012 with C++20 rendering, plus golden tests on three compilers.
  - Run the first comparison against the budgets: scenario and N = 100.
- **Week 9.**
  - Fix MSVC-specific problems, add overrides for tests, build the module, write the getting-started guide.
  - Tag v0.1.0 and publish the measurements next to the research baselines.

---

## 14. Risk register

L = likelihood, I = impact (H, M, L).

| # | Risk | L | I | Mitigation | Early warning |
|---|---|---|---|---|---|
| K1 | Compile-time blow-ups for some graph shapes | M | H | Memoization per key (C1); every layout in the benchmark suite; budgets as CI gates; spike S2 | N = 1,000 with fan-in 10 exceeds 1.5× hand-written |
| K2 | MSVC template and concept bugs block deduced discovery | H | H | MSVC in CI from week 1; S1 covers MSVC; explicit-mode fallback; workarounds isolated in `config.hpp` | Cells in the S1 table that can't be resolved on MSVC |
| K3 | Cycles in deduced mode produce cryptic compiler errors | H | M | Spike S2 options; full checks in explicit and reflection modes; docs listing error signatures | Depth-bounded mode costs more than 1.2× |
| K4 | Diagnostic quality varies between compilers | H | M | Golden tests per compiler; C++26 messages where supported | Golden output over budget |
| K5 | Reflection API or compiler support shifts; ISO publication of C++26 delayed (the final ballot reportedly failed in October 2026, over contracts) | M | M | Optional, isolated backend; feature-test macros; follow GCC and Clang trunk | Reflection tests break on compiler updates |
| K6 | Scope creep: interception, configuration files, async | H | M | Non-goals (§3.3); phase gates; a P2 list | A phase slips by more than 2 weeks |
| K7 | Bus factor 1, the field's common failure | H | H | A second maintainer before v0.5; internals docs; the code-size budget (NFR-12) | One person committing alone for 2 months |
| K8 | Low adoption: the long-tail libraries all have under 30 stars | H | M | Design partners; packages from v0.2; migration guides; reproducible benchmarks; clear differentiators (diagnostics, safety, speed) | No outside issue reporters by the beta |
| K9 | Type identity or one-definition-rule problems with plugins (shared libraries, hidden visibility) | M | H | Name-based key policy; plugin tests on 3 OSes; ABI tag check; documented constraints | Plugin test failures |
| K10 | Thread-safety bugs in lazy initialization | M | H | ThreadSanitizer stress tests; a simple once-only state design; eager mode | TSan reports |
| K11 | Lifetime rules feel too strict | M | M | Escape hatches (`.allow_copy()`, explicit external bindings); clear errors | Design-partner feedback |
| K12 | Name collision or trademark | L | L | Check GitHub, vcpkg, Conan and search before the first public tag | — |

---

## 15. Governance and sustainability

This is the main lesson of the research: every existing framework depends on one person.

- **Maintainers:** two with merge rights before v0.5, plus CODEOWNERS, branch protection and required reviews. Keep a short `MAINTAINERS.md` with a release and triage rota.
- **Release discipline:** the cadence in [§10](#10-build-packaging-and-releases): patch releases within 7 days, nothing untagged beyond 14 days, minor releases every 6–8 weeks during 0.x. Package recipes update automatically.
- **CI hygiene:** a nightly full matrix; failures open issues and notify both maintainers; a red nightly blocks releases.
- **Triage:** first response within 7 days; labels for bug, enhancement, diagnostics and compiler-specific issues; a monthly review of open issues.
- **Simplicity budget:** core headers stay at or under 6,000 lines (NFR-12). Every metaprogramming trick gets a comment, a test and an entry in `docs/internals/`. A capable C++ engineer should be able to understand the core in a week.
- **Compatibility promise:**
  - The list of supported compilers changes only in minor releases, with one release of notice.
  - The C++20 baseline holds for all of 1.x.
  - Public API changes after 1.0 follow semantic versioning.
- **Community files:** CONTRIBUTING.md, CODE_OF_CONDUCT.md and SECURITY.md (a header-only library still receives untrusted input in plugins). Sponsorship is optional.

---

## 16. Open decisions

These change the plan most. Each has a recommended default, so work can start without waiting.

| # | Decision | Recommended default | Alternatives and their effect |
|---|---|---|---|
| D1 | Name | Keep "Trellis" while building; check for clashes before the first public tag | Any name, after a GitHub, vcpkg and Conan search |
| D2 | C++ baseline | C++20 | C++17: wider reach, but SFINAE diagnostics and no class-type template parameters, about +4 weeks |
| D3 | License | Boost Software License 1.0: permissive, and needs no attribution in binaries, which suits header-only code | MIT (also fine); Apache-2.0 (patent grant, more ceremony) |
| D4 | Main audience | Servers and applications: thread-safe default, scopes, plugins | Games or embedded: `singleton_inline`, `threading::none`, no exceptions by default |
| D5 | Runtime engine in 1.0? | Yes, since plugins are a main reason to use a framework at all (decided by the owner, 2026-10-31: yes) | Ship a static-only 1.0 around week 26 and add the runtime engine in 1.1 |
| D6 | Default for lazy versus eager | Lazy and thread-safe by default; docs recommend eager for applications | Eager by default, as Chromium's practice suggests: simpler threading, slower startup for unused services |
| D7 | Test framework | GoogleTest and gMock | Catch2 or doctest (faster builds, but the gMock examples still need GoogleTest) |
| D8 | Team size | 2 engineers (about 28 weeks) | 1 engineer (about 44 weeks) |
| D9 | Where to develop | A new `trellis` repository; this research repository stays as background | Develop inside this repository under `framework/` |

---

## 17. Appendices

### A. Glossary

- **Composition root:** the one place, usually `main` or a single wiring file, where the object graph is assembled.
- **Binding:** a rule mapping a key to how it is produced: an implementation, an instance or a factory, together with a lifetime.
- **Key:** a type plus an optional qualifier (annotation, name or context).
- **Lifetime or scope:** who owns an object and how many instances exist (unique, singleton, scoped, external, value).
- **Request form:** how a consumer asks for a dependency (`T`, `T&`, `T*`, `unique_ptr<T>`, `shared_ptr<T>` and so on).
- **Captive dependency:** a longer-lived object holding a shorter-lived one by reference, so the reference outlives its target or keeps a stale object.
- **Assisted injection:** a factory whose callers supply some constructor arguments at run time while the injector supplies the rest.
- **Multibinding:** several bindings for one interface, injected as a collection.
- **Seed:** a value supplied when a scope opens, such as the current request.
- **Deduced discovery:** finding constructor arity by probing with placeholder arguments whose conversion operators resolve dependencies.
- **Explain pass:** the instantiation that runs only after a failure and walks the first failing path to build the error message.

### B. Research inputs this plan relies on

- [Report: hand-wiring still beats most C++ DI frameworks](../research/reports/Cpp%20dependency%20injection%20frameworks.md): verdict, landscape table, measurements, diagnostics comparison, C++26 reflection status, guidance on when to use which approach.
- [Notes: Boost.DI](../research/research_notes/Cpp%20dependency%20injection%20frameworks/boost_di.md): deduction rules, scopes, process-wide singletons, benchmarks.
- [Notes: Google Fruit](../research/research_notes/Cpp%20dependency%20injection%20frameworks/google_fruit.md): components, compile-time versus runtime checks, `exit(1)` errors, benchmarks.
- [Notes: runtime containers](../research/research_notes/Cpp%20dependency%20injection%20frameworks/runtime_containers.md): Kangaru, dingo, Hypodermic, inversify-cpp.
- [Notes: long tail and reflection](../research/research_notes/Cpp%20dependency%20injection%20frameworks/long_tail_and_emerging.md): P2996 and companion papers, compiler support, reflection-based prototypes (Skirnir, ctorium, libctdi).
- [Notes: patterns and practice](../research/research_notes/Cpp%20dependency%20injection%20frameworks/patterns_and_practice.md): pure DI, Chromium, userver, folly, BDE; expert opinion.
- [Notes: hands-on evaluation](../research/research_notes/Cpp%20dependency%20injection%20frameworks/hands_on_evaluation.md) and [experiment harness](../research/experiments/README.md): the baselines behind every NFR.

### C. External references

- WG21 papers:
  - P2996R13 Reflection for C++26;
  - P3394R4 annotations;
  - P3096R12 function-parameter reflection;
  - P1306R5 expansion statements;
  - P3491R3 `define_static_array`;
  - P2741R3 user-generated `static_assert` messages;
  - P4101R1 consteval-only values.
- Mark Seemann, "Pure DI" (2014) and "When to use a DI Container" (2012).
- Kris Jusiak, "Dependency Injection – a 25-dollar term for a 5-cent concept" (C++Now 2019).
- Peter Muldoon, "Dependency Injection in C++: A Practical Guide" (CppCon 2024).
- Chromium browser design principles and the KeyedService documentation; the userver component system documentation.
