# ADR-0004: Explicit lifetimes and an enforced request-form table

- **Status:** Accepted (plan baseline; its TRL-E014 amendment by owner decision, 2026-11-02, see History)
- **Plan:** [§4](../plan.md#4-architecture-decisions) ADR-04; §6.2, §6.3; FR-02, FR-03, FR-04
- **Spikes:** S1, S2 and S4 confirmed parts of it without changing it. S1 also led to a new rule about how a parameter's spelling maps to a request form, `T&&` as the request `T`. [ADR-0003](0003-constructor-discovery.md) decides that rule and adds a note to §6.3; this ADR's lifetimes and table are unchanged.
- **Amends:** plan §6.3 (from T1.5's review, confirmed by the owner on 2026-11-02: `std::unique_ptr<T>` of a derived implementation needs `T`'s destructor to be virtual) and §12.2 (strict mode takes effect in Phase 1).

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

A related rule from ADR-0003: a parameter of type `X&&` or `const X&&` is the request `X` (the note in §6.3). It says how a parameter's spelling maps to a form; the forms, the lifetimes and the table above are unchanged.

## Alternatives

- **Boost.DI's "deduce" scope,** where the consumer's parameter spelling chooses the lifetime: it produced dangling references and unexpected sharing (R5).
- **Building any unregistered concrete type,** as Hypodermic does: a forgotten registration goes unnoticed (R11). Trellis builds an unbound concrete type only for owning requests (by value or as `unique_ptr`), and the strict option disables even that.

## Consequences

- The rules may feel strict (risk K11). The escape hatches are `.allow_copy()` and explicit external bindings, and every rejection names its reason (E005, E006, E012).
- **Open question from S1 §5:** should `const shared_ptr<T>&` and `const T&` parameters of unique objects be allowed, with the placeholder holding the object until the constructor returns? Today the table rejects them (E005). The binding-vocabulary work (T1.4) or the API review decides. T1.4 kept the rejection and left the question to the API review: `const T&` of a unique object is E005, and `const std::shared_ptr<T>&` is a reference to a `std::shared_ptr<T>` object, a key of its own, so without a binding of that type it is E012 (History).
- The table is encoded as a `constexpr` table, with checks and tests (§13, week 6).

## History

- **2026-10-30, plan draft 1:** drafted as ADR-04 in plan §4.
- **2026-10-31, T0.8:** recorded in this file, unchanged. §6.3 gained ADR-0003's note on `T&&` parameters.
- **2026-10-31, T0.8 review:** the Spikes line and the list of spike findings made consistent: the `T&&` rule belongs to ADR-0003 and leaves this ADR's lifetimes and table unchanged.
- **2026-10-31, T1.4:** the table is encoded cell for cell as `constexpr` data (`include/trellis/dsl/request_forms.hpp`), with the scoped column and the `lazy`/`provider` row although both arrive in Phase 2, and an eighth column for unbound concrete types, whose sharing forms are E012. No decision changes; details for review:
  - `.allow_copy()` exists and turns exactly the "copies hide sharing" cells into allowed ones. Constness carries through for `to_ref`, `to_ptr` and `to_shared` bindings.
  - Cells that §6.3 marks ✗ without a reason take their row's reason (`unique_ptr`: would duplicate ownership), or that of the matching `shared_ptr` cell (`weak_ptr` of `singleton_inline` and `to_value`: not heap-owned); `shared_ptr` and `weak_ptr` of a `to_ref` or `to_ptr` binding say "the caller owns the object", and `weak_ptr` of a unique object "nothing would keep it alive". The `lazy`/`provider` row allows every column, as §6.3 says; whether those wrappers check their inner form is for Phase 2.
  - The open question above stays open for the API review.
- **2026-11-01, T1.5:** `create<T>()` serves every allowed cell and reports every rejected one, at any depth of the graph. No decision changes; readings for review:
  - **Unbound non-class types,** such as `int`, are implicitly unique for the table, as T1.4 encodes them, but have no constructor to discover: a by-value request is TRL-E002 ("deduction needs a class type; bind the type to a value"), never a silently value-initialized 0, and a reference is TRL-E012 (`[NEEDS CLARIFICATION]` in the progress log).
  - **Strict mode** turns the implicit unique binding off, so an owning request (by value or `std::unique_ptr`) whose type has no binding is TRL-E013 there; the other requests of unbound types are TRL-E001 or TRL-E012, as without it (see ADR-0006's History). Strict mode, like eager mode and the threading options (ADR-0012), takes effect in Phase 1 (plan §12.2 amended).
  - **A by-value request** of a unique binding whose implementation is a derived class, `bind<Base>.to<Derived>()`, gets a new `Derived` moved into a `Base`, sliced, as a by-value request of a `.to_value()` of a derived object gets a sliced copy (T1.4); a key that can't be constructed from its implementation is TRL-E002.
- **2026-11-01, T1.5 review:** where the table allows `std::unique_ptr<T>`, a binding whose implementation derives from `T` needs `T`'s destructor to be virtual, since `std::default_delete<T>` would otherwise delete the implementation through `T`, which is undefined behaviour; such a request is TRL-E014 (plan §6.3 amended, provisional, `[NEEDS CLARIFICATION]` in the progress log). `std::shared_ptr<T>`, singletons and by-value requests stay allowed: they destroy the implementation itself (`tests/unit/create_test.cpp`).
- **2026-11-02, owner decision (Phase 1 gate):** confirmed the T1.5 review's amendment above, by confirming TRL-E014 as a new ID with exactly this meaning, as recommended (docs/PROGRESS.md, "Owner decisions"; ADR-0006): where the table allows `std::unique_ptr<T>`, an implementation derived from `T` needs `T`'s destructor to be virtual, or the request is TRL-E014. Plan §6.3's marker says "confirmed by the owner, 2026-11-02". The open question on `const std::shared_ptr<T>&` parameters (Consequences) wasn't decided at the gate: it stays under "Waiting for owner", for the API review.
