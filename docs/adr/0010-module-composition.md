# ADR-0010: Composition through variadic modules, never fluent type accumulation

- **Status:** Accepted (plan baseline)
- **Plan:** [§4](../plan.md#4-architecture-decisions) ADR-10; §5.3, §6.9, §7.1; §8.1 C2, C4, C5; R9
- **Spikes:** none changed it. S2 and S3 raise an open point about how the injector's type names its bindings.

## Context

A chain like `.bind().bind().bind()`, where each call returns a new type holding everything before it, grows the type size quadratically. Fruit's flat-component blow-up, out of memory at about 13.9 GB with 25 components, is consistent with this, though not proven to be the cause (R9).

## Decision

- `make_injector(xs...)` and `make_module(xs...)` take bindings and modules as a pack and flatten them once into one binding map (§5.3).
- The map is a single struct inheriting from one entry per binding, so each lookup is one overload resolution (§7.1, §8.1 C2).
- Modules are plain values, cheap to compose.

## Alternatives

- **Fluent accumulation in the type:** quadratic type growth (R9).
- **Recursive or linear lookup in a type list:** O(N) instantiations per lookup (§8.1 C2).

## Consequences

- **Open point: the injector's type should name a module, not list the bindings.**
  - S2 recommends, provisionally (ADR-0014), that the injector take a named binding map, such as `struct app : trellis::module<...> {}`. Taking the binding list as `injector<Bindings...>` would put all N bindings into the name of every node and every creation-code symbol (S2 §2, §4.4; not measured).
  - S3 measured the diagnostic side: with the injector parameterized by module names, GCC's "In instantiation of ... [with R = App; Module = app_module]" banner stays one short line instead of listing every binding (S3 §3.2).
  - `make_injector(xs...)` deduces its pack, so the API of §6.1 and §6.9 needs a way to give the result a short name. That is for T1.4 and the API review.
- **No fold over all of a module's bindings:** Clang limits fold expressions to 256 operands (`-fbracket-depth=256`). S2 hit the limit with a fold over a 257-element path and infers it for binding lists, untested (S2 §3.3). The inheritance-based map doesn't fold, and checks over the whole binding list must not fold either.

## History

- **2026-10-30, plan draft 1:** drafted as ADR-10 in plan §4.
- **2026-10-31, T0.8:** recorded in this file, unchanged; S2's and S3's findings on type names and the fold limit added as consequences.
