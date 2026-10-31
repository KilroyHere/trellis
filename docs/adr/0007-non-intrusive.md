# ADR-0007: Non-intrusive by default

- **Status:** Accepted (plan baseline)
- **Plan:** [§4](../plan.md#4-architecture-decisions) ADR-07; §1 (the positioning statement); R13
- **Spikes:** none changed it. S1 adds a constraint on Trellis's own wrapper types.

## Context

Jusiak advises against coupling code to a specific DI framework, and Chromium and userver keep ordinary classes unaware of their component systems (R13). Trellis's positioning makes "easy to remove" a requirement: dropping the library should mean rewriting only the composition root (§1).

## Decision

Domain classes never have to include Trellis. Every optional framework type has a standard-library spelling that works too:

- `di::lazy<T>`, `di::provider<T>` and `di::factory<Sig>`: `std::function`, or `std::move_only_function` on C++23;
- `di::annotated<Tag, T>`: contextual bindings (`when_injected_into<C>`), or wrapper types the user defines.

## Alternatives

- **Intrusive markers,** such as Fruit's `INJECT` macro or a required base class: domain classes would depend on the framework, and removing it would touch every class.
- **Framework types as the only spelling** for lazy, provider and factory parameters: less to implement and test, but it couples classes to Trellis.

## Consequences

- **Trellis's wrapper types must deduce cleanly** (S1 §3.2 note c). `lazy`, `provider` and `factory` must not have converting constructor templates that accept arbitrary types, or deduced discovery finds their parameters ambiguous, as it does for `std::optional<T>`, `std::tuple<T>` and `std::any`. S1's `lazy<T&>` stand-in deduces exactly. The same reasoning covers `annotated`.
- **Where deduction can't work** (ADR-0003: `optional`, `tuple` and `any` parameters, and the residual limits), a `ctor_traits` specialization outside the class keeps the class untouched. The member alias `trellis_ctor` is the intrusive opt-in.
- Each standard-library spelling bypasses the framework type, so it needs its own tests.

## History

- **2026-10-30, plan draft 1:** drafted as ADR-07 in plan §4.
- **2026-10-31, T0.8:** recorded in this file, unchanged; S1's constraint on wrapper types added to the consequences.
