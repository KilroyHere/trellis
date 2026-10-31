# ADR-0002: A static injector core and a separate runtime engine, sharing one vocabulary

- **Status:** Accepted (plan baseline)
- **Plan:** [§4](../plan.md#4-architecture-decisions) ADR-02; §5.1, §6.11, §7.4, §7.9; §16 D5
- **Spikes:** none changed it. Spike S6 (runtime dependency recording) is deferred to Phase 5.

## Context

Compile-time wiring was measured at hand-wired speed (R1), while the containers that look dependencies up at run time cost 4–25× per resolution (R2). Plugin hosts and configuration-driven graphs still need registration at run time (FR-14). One vocabulary for both means one set of concepts to learn and to test.

## Decision

- `make_injector(...)` builds a fully static injector.
- `runtime::builder` builds a type-erased container.
- Both accept the same binding expressions (`bind<I>.to<C>().in(scope)`), modules and options.
- A static injector can be imported into a runtime builder, so plugins can depend on core services that are wired statically.

## Alternatives

- **Runtime only:** slower resolution, and errors arrive later.
- **Static only:** no plugins.
- **Fruit's model** (types checked at compile time, injector built at run time): pays the runtime cost even where nothing needs it.

## Consequences

- Two engines to maintain. Mitigated by a shared discovery layer, a shared vocabulary and one test suite parameterized over both engines.
- **D5 is decided:** the runtime container ships in 1.0 (owner, 2026-10-31), so Phase 5 is part of the 1.0 scope and T5.0 expands it normally.
- **Spike S6 is deferred to Phase 5.** It decides how the runtime engine learns the dependency lists of classes found by deduced discovery (§7.4): self-registration from the placeholder's conversion operators, or a dry run inside `validate(roots...)`, including how either behaves across shared libraries. Reasons:
  - nothing before Phase 5 builds the runtime engine, and the static engine doesn't need dependency lists;
  - the plan allows it: §12.2's Phase 0 row says S5 and S6 can be shortened, and §13 (week 3) that S6 can be postponed to Phase 5;
  - when S6 was deferred, D5 could still have taken the runtime engine out of 1.0. D5 has since kept it in, and the first two reasons still hold.

  T5.0 should schedule S6 before the runtime engine's discovery work. Inputs so far:
  - S1 §5: self-registration can hook the bodies of the placeholder's conversion operators, which know the consumer, the request and the parameter position.
  - S4 §5: the thread-local frame stack behind R004 (ADR-0012) is one object per program only if its symbol isn't hidden, so re-entry through a lazy that crosses a `-fvisibility=hidden` shared-library boundary would deadlock instead of reporting R004.

## History

- **2026-10-31, T1.12:** the Consequences updated for the D5 decision; S6 stays deferred to Phase 5.
- **2026-10-31, owner decision (plan D5):** the runtime engine ships in 1.0, so Phase 5 is part of the 1.0 scope.
- **2026-10-30, plan draft 1:** drafted as ADR-02 in plan §4.
- **2026-10-31, T0.8:** recorded in this file, unchanged. S6 recorded as deferred, with the reasons above.
