# ADR-0004: Explicit lifetimes and an enforced request-form table

- **Status:** Accepted (plan baseline)
- **Plan:** [§4](../plan.md#4-architecture-decisions) ADR-04; §6.2, §6.3; FR-02, FR-03, FR-04
- **Spikes:** none changed it. S1, S2 and S4 confirmed parts of it. S1's rule that `T&&` is the request `T` is recorded in [ADR-0003](0003-constructor-discovery.md) and amends §6.3.

## Context

Boost.DI infers a lifetime from how each consumer spells its parameter: `T&` gets a singleton and `T` gets a new object. Its issue tracker has reports of dangling references and of sharing nobody expected (R5). Hypodermic silently constructs any unregistered concrete type, so a forgotten registration becomes a fresh object instead of an error (R11).

## Decision

- **Lifetimes:** `unique`, `singleton`, `singleton_inline`, `scoped<Tag>`, external (`to_ref`, `to_ptr`, `to_shared`) and `to_value` (§6.2).
- **Each request form is allowed or rejected per lifetime,** as the table in §6.3 says, and a rejected form is a compile error (E005).
- **Unbound concrete types are implicitly `unique`.** The strict option turns this off.
- **Requesting an unbound type by reference, pointer or `shared_ptr` is error E012,** because nobody would own the object.
- **The captive-dependency rule (E006)** orders lifetimes: external ≈ value ≈ singleton > `scoped<Outer>` > `scoped<Inner>` > unique (§6.3).

What the spikes confirmed:

- **Deduced mode enforces the table without fallbacks.** S1's deleted twins make a request the binding can't satisfy ill-formed, instead of letting overload resolution pick another form. E005 for a reference to a unique binding, and for a singleton requested by value, each give the ID as the first and only error on five configurations (S1 §3.1, §3.5).
- **The consumer's lifetime belongs in the memoization key,** because the captive rule depends on it (S2 §2; ADR-0014).
- **Inline storage rejects `shared_ptr<T>` requests at compile time** (S4 §3.4; ADR-0005).
- **`T&&` and `const T&&` parameters are the request `T`** (ADR-0003; the note in §6.3).

## Alternatives

- **Boost.DI's "deduce" scope,** where the consumer's parameter spelling chooses the lifetime: it produced dangling references and unexpected sharing (R5).
- **Building any unregistered concrete type,** as Hypodermic does: a forgotten registration goes unnoticed (R11). Trellis builds an unbound concrete type only for owning requests (by value or as `unique_ptr`), and the strict option disables even that.

## Consequences

- The rules may feel strict (risk K11). The escape hatches are `.allow_copy()` and explicit external bindings, and every rejection names its reason (E005, E006, E012).
- **Open question from S1 §5:** should `const shared_ptr<T>&` and `const T&` parameters of unique objects be allowed, with the placeholder holding the object until the constructor returns? Today the table rejects them (E005). The binding-vocabulary work (T1.4) or the API review decides.
- The table is encoded as a `constexpr` table, with checks and tests (§13, week 6).

## History

- **2026-10-30, plan draft 1:** drafted as ADR-04 in plan §4.
- **2026-10-31, T0.8:** recorded in this file, unchanged. §6.3 gained ADR-0003's note on `T&&` parameters.
