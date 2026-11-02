# ADR-0010: Composition through variadic modules, never fluent type accumulation

- **Status:** Accepted (plan baseline; its overrides amendment by owner decision, 2026-11-02, see History)
- **Plan:** [§4](../plan.md#4-architecture-decisions) ADR-10; §5.3, §6.9, §6.10, §7.1; §8.1 C2, C4, C5; R9
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
  - S2's recommendation, which the owner accepted on 2026-10-31 (ADR-0014), has the injector take a named binding map, such as `struct app : trellis::module<...> {}`. Taking the binding list as `injector<Bindings...>` would put all N bindings into the name of every node and every creation-code symbol (S2 §2, §4.4; not measured).
  - S3 measured the diagnostic side: with the injector parameterized by module names, GCC's "In instantiation of ... [with R = App; Module = app_module]" banner stays one short line instead of listing every binding (S3 §3.2).
  - `make_injector(xs...)` deduces its pack, so the API of §6.1 and §6.9 needs a way to give the result a short name. That is for T1.4 and the API review. T1.4's answer, which the API review may revisit: the injector is `injector<Opts, Parts...>`, named by `make_injector`'s argument types rather than by the flattened bindings, and a struct derived from a module is a module, so `struct app : decltype(make_module(...)) {}` passed alone gives `injector<options{}, app>` (History).
- **No fold over a pack that can exceed 256 operands** (measured in T1.1): Clang 18 and 20 reject a fold expression over more than 256 operands, fatally (`instantiating fold expression with 300 arguments exceeded expression nesting limit of 256`; the limit is `-fbracket-depth`). GCC 13 and 14 have no such limit. S2 first hit it with a fold over a 257-element path (S2 §3.3). The rule: fold only over packs that can't exceed 256 operands; over a list's elements or a module's bindings, use a flat pack expansion (a braced list of booleans, scanned in a constexpr loop) or an inheritance-based map or set. The meta toolkit (T1.1, `docs/internals/meta-toolkit.md`) folds over no pack at all.

## History

- **2026-10-30, plan draft 1:** drafted as ADR-10 in plan §4.
- **2026-10-31, T0.8:** recorded in this file, unchanged; S2's and S3's findings on type names and the fold limit added as consequences.
- **2026-10-31, T0.8 review:** the fold consequence narrowed to "no single fold over more than 256 operands" and marked inferred and untested, consistent with plan §7.1 and §8.1 C3. S2's recommendation is now owner-accepted.
- **2026-10-31, T1.1:** the fold limit measured on clang-18 and clang-20 (fatal above 256 operands; GCC has none), and the consequence restated as a rule. The meta toolkit therefore uses flat pack expansions and inheritance-based sets, and its type map looks keys up through one non-template overload per entry instead of deducing against base classes, which GCC 13 and 14 get wrong for keys that differ only in `T&` and `T&&`. Plan §7.1 and §8.1 C3 amended.
- **2026-10-31, T1.4:** implemented as decided. `make_module` and `make_injector` flatten their arguments in one pass that is linear in the number of bindings, with no fold and no recursion over a pack: 1,000 bindings, passed directly or as one module, build an injector in 3.4–3.5 s on gcc-13 and clang-18 (`docs/internals/bindings-and-modules.md`). The injector's type names its arguments, which answers the open point above for now.
- **2026-11-01, T1.7, provisional (awaits the owner's confirmation):** overrides (plan §6.10, amended) are applied as `make_injector` flattens its arguments, still without a fold or recursion over a pack: the other arguments' bindings are flattened as before, then each binding whose key an override names is replaced in place, by one lookup per binding in a `type_map` of the overrides' keys (`docs/internals/overrides-and-testing.md`). Overrides are arguments of `make_injector` only, so modules stay sets of bindings; two overrides of one key are TRL-E003, and an override with no binding to replace is TRL-E009.
- **2026-11-02, owner decision (Phase 1 gate):** confirmed T1.7's overrides amendment above, as recommended (docs/PROGRESS.md, "Owner decisions"): `trellis::override<K>` has `bind<K>`'s vocabulary; only `make_injector` takes it, and `make_module` rejects it, rather than letting a test module bundle overrides; it replaces the binding of `K` in place as `make_injector` flattens its arguments, with no fold or recursion over the pack; two overrides of one key are TRL-E003, and one with nothing to replace is TRL-E009. Plan §6.10's marker says "confirmed by the owner, 2026-11-02". The owner also kept the public class's name, `trellis::module`, and the API review may revisit it. In C++20 a line that starts with `module` and a name is a module directive, so after `using namespace trellis;` a declaration `module m = make_module(...);` at global scope doesn't compile with Clang 18 and 20; the [getting-started guide](../getting-started.md#4-bindings-and-lifetimes) says to write the name qualified, or `auto`.
