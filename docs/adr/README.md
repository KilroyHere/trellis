# Architecture decision records

Each file records one decision: its context, the decision itself, the alternatives and what they would cost, the consequences, and a dated history. ADR-0001 to ADR-0013 are the thirteen decisions in [plan §4](../plan.md#4-architecture-decisions), numbered the same way (ADR-01 there is ADR-0001 here), with the results of the Phase 0 spikes folded in. ADR-0014 is new: it records spike S2's answer to plan §7.3, which §4 had no ADR for. Where an ADR and the plan disagree, the ADR wins.

## Index

| ADR | Title | Status | Spikes that informed it |
|---|---|---|---|
| [0001](0001-cpp20-baseline.md) | C++20 baseline; C++23 and C++26 used when available | Accepted (plan baseline) | S3 (the P2741 gate, decided in 0006) |
| [0002](0002-static-and-runtime-engines.md) | A static injector core and a separate runtime engine, sharing one vocabulary | Accepted (plan baseline) | S6 (deferred) |
| [0003](0003-constructor-discovery.md) | Three constructor-discovery strategies behind one trait | Accepted (owner, 2026-10-30; its T1.2, T1.3, MSVC, Apple Clang and T1.15 amendments by owner decision, 2026-11-02) | **S1**; S5 (deferred) |
| [0004](0004-lifetimes-and-request-forms.md) | Explicit lifetimes and an enforced request-form table | Accepted (plan baseline; its TRL-E014 amendment, T1.5, by owner decision, 2026-11-02) | S1, S2, S4 (confirmations; S1's `T&&` rule is decided in 0003) |
| [0005](0005-state-and-storage.md) | State lives only in injector and scope objects; storage is honest | Accepted (owner, 2026-10-30) | **S4** |
| [0006](0006-diagnostics-contract.md) | The diagnostics contract | Accepted (owner, 2026-10-30; NFR-07's count by owner decision 1A, 2026-11-01; its T1.5 IDs and T1.6 amendments by owner decision, 2026-11-02) | **S3**; S1, S2 |
| [0007](0007-non-intrusive.md) | Non-intrusive by default | Accepted (plan baseline) | S1 (wrapper types) |
| [0008](0008-header-only-delivery.md) | Header-only, no dependencies, several delivery forms | Accepted (plan baseline) | S4 (include cost) |
| [0009](0009-runtime-error-model.md) | The runtime error model | Accepted (plan baseline) | S4 (R004) |
| [0010](0010-module-composition.md) | Composition through variadic modules, never fluent type accumulation | Accepted (plan baseline; its overrides amendment, T1.7, by owner decision, 2026-11-02) | S2, S3 (an open point on type names) |
| [0011](0011-type-identity-policy.md) | Type identity as a policy in the runtime engine | Accepted (plan baseline) | S3, S4; S6 (deferred) |
| [0012](0012-threading-model.md) | Threading model | Accepted (owner, 2026-10-30) | **S4** |
| [0013](0013-governance-and-releases.md) | Governance and release discipline | Accepted (plan baseline) | none |
| [0014](0014-resolution-memoization-and-cycles.md) | Resolution: memoization, depth per hop and cycles in deduced mode | Accepted (owner, 2026-10-31; its MSVC amendment, T1.13, by owner decision, 2026-11-02) | **S2** |

A spike in bold changed or settled the decision; the others confirmed it, or added consequences and open points.

**Status values:**

- **Accepted (owner, *date*):** the owner confirmed the decision, including the spike results it records (docs/PROGRESS.md, "Owner decisions").
- **Accepted (plan baseline):** the decision as plan §4 states it; no spike changed it.
- **Accepted (provisional, awaiting owner review):** a spike, a task or a review settled the decision or an amendment to it, and the owner hasn't confirmed it yet. Work follows it as the default until then, and it stays under "Waiting for owner" in docs/PROGRESS.md. No ADR and no amendment has this status now. At the Phase 1 gate, on 2026-11-02, the owner confirmed every amendment that had it, and each ADR's History records the decision:
  - **ADR-0003:** T1.2's construction rule for explicit declarations (an aggregate is checked with braces, any other type with `is_constructible`); T1.3's amendments to items 5 to 7 (an abstract or incomplete type is TRL-E002, TRL-E010 probes the 16 arities above the bound, `max_ctor_arity` is at most 32, gcc-13's probe declares the deleted twins, and an aggregate with an array member or a member no placeholder can initialize is TRL-E004); item 6's two MSVC workarounds (platform fixes, 2026-11-01); the Apple Clang fix's refinements of item 5 (the aggregate checks in parentheses, braces on cl, and TRL-E004 naming the inner aggregate, 2026-11-01); and T1.15's four readings and its system header for the module built by Clang before 20 (2026-11-01).
  - **ADR-0004:** `std::unique_ptr<T>` of a binding whose implementation derives from `T` needs `T`'s destructor to be virtual, or the request is TRL-E014 (T1.5 review).
  - **ADR-0006:** the IDs TRL-E013 and TRL-E014 (T1.5 review), and T1.6's changes to items 2, 3 and 5: every assertion in one function template, `detail::raise`, so that a report is raised once per request and injector whatever the root; the path in the instantiation note of the step that reports, not in the condition; and, from T1.6's review, the explain pass's cycle stop and one note per deduced class.
  - **ADR-0010:** overrides, which replace a binding in place as `make_injector` flattens its arguments (T1.7).
  - **ADR-0014:** NFR-09 and the opt-in cycle check's bound at 150 hops on MSVC before 19.50, an assumed boundary (T1.13).

  ADR-0004's open question on `const std::shared_ptr<T>&` parameters isn't an amendment: it stays open under "Waiting for owner", for the API review.

## Spikes

| Spike | Task | Outcome | ADRs |
|---|---|---|---|
| [S1 Deduced discovery](../spikes/S1-deduced-discovery.md) | T0.4 | Done. Recommendation accepted by the owner, 2026-10-30. | 0003; also 0004, 0006, 0007 |
| [S2 Memoization and cycles](../spikes/S2-memoization-cycles.md) | T0.7 | Done. Recommendation accepted by the owner, 2026-10-31. | 0014; also 0006, 0010 |
| [S3 Diagnostics rendering](../spikes/S3-diagnostics.md) | T0.5 | Done. Recommendation accepted by the owner, 2026-10-30, with NFR-07 reworded. | 0006; also 0001, 0010, 0011 |
| [S4 Storage and threading](../spikes/S4-storage-threading.md) | T0.6 | Done. Recommendation accepted by the owner, 2026-10-30. | 0012, 0005; also 0008, 0009, 0011 |
| S5 Reflection prototype | none | **Deferred.** It needs GCC 16 (`-std=c++26 -freflection`) or the clang-p2996 fork. Neither is packaged for Ubuntu 24.04, whose archives carry GCC 9 to 14 and Clang 14 to 20, and the network here blocks Compiler Explorer. Revisit when T6.0 expands Phase 6, or when CI can install such a toolchain. Until then the reflection strategy stands as planned, unprototyped. | 0003 (reflection strategy); plan §7.10 |
| S6 Runtime dependency recording | none | **Deferred to Phase 5.** It decides how the runtime engine gets the dependency lists of deduced classes (plan §7.4), and nothing before Phase 5 builds the runtime engine. Plan §13 already allows the postponement, and D5 (whether the runtime container ships in 1.0) is still open. T5.0 should schedule it. | 0002; also 0011 |

## Where the ADRs change the plan

Each change to docs/plan.md is a minimal edit in place, marked "(amended by ADR-00xx, *date*)" with the date of the decision, often with the task that made it, and "provisional" while the owner hasn't confirmed it, which becomes "confirmed by the owner, *date*" once the owner has. The §4 pointer is marked as the table says, and an owner decision with no ADR of its own reads "(amended by owner decision, *date*)".

| Plan section | Change | By |
|---|---|---|
| §3.2, NFR-07 | The first error line names the error ID; the same diagnostic names the type. The 40 lines don't count the lines the user's own layout produces, the include chain and the excerpt of the user's call site (owner decision 1A, 2026-11-01). | 0006 |
| §3.2, NFR-09 | ≥ 150 hops on MSVC before 19.50, an assumed boundary; ≥ 200 hops elsewhere (T1.13). Confirmed by the owner, 2026-11-02. | 0014 |
| §3.2, NFR-09 | The exceptions accepted for v0.1, each with a Phase 2 task: `std::unique_ptr` chains reach 198 hops on cl 19.51, aggregate chains stop at 15 on cl (C1054), and Apple Clang's aggregate chain passes with a 6% margin, at 213 hops (owner decision, 2026-11-02). | 0014 |
| §3.2, NFR-12 | The core headers' lines are counted as lines that are neither blank nor comments: 4,406 of the 6,000 at the Phase 1 gate (owner decision, 2026-11-02). | owner |
| §4 | A pointer to this directory. | this index |
| §6.3 | `T&&` and `const T&&` parameters are the request `T`. | 0003 |
| §6.3 | Where the table allows `std::unique_ptr<T>`, an implementation derived from `T` needs `T`'s destructor to be virtual; otherwise the request is E014 (T1.5 review). Confirmed by the owner, 2026-11-02. | 0004 |
| §6.8 | R004 is detected in all builds. | 0012 |
| §6.10 | Overrides: `override<K>` has `bind<K>`'s vocabulary, is an argument of `make_injector` only, and replaces the binding of `K` in place, modules included; two overrides of one key are E003 (T1.7). Confirmed by the owner, 2026-11-02. | 0010 |
| §6.13 | `max_ctor_arity` is at most 32; a longer constructor is declared explicitly (T1.3). Confirmed by the owner, 2026-11-02. | 0003 |
| §7.1 | `type_list` algorithms use flat pack expansions and an inheritance-based set, folding only over packs that can't exceed 256 operands; `type_map` lookups resolve one non-template overload per entry, without deduction (T1.1). | 0010 |
| §7.1 | `result<T>`'s error type is `trellis::failure`, as `trellis::error` is the compile-error namespace (T1.1). | 0006 |
| §7.2 | S1's operator set replaces the sketch: deleted twins, a separate probe, the explicit route for `optional`, `tuple` and `any`, two `config.hpp` workarounds for GCC and the residual limits. T1.13 found the aggregate-array count compiler-dependent, which only rewords that limit. Two MSVC workarounds join them (platform fixes, 2026-11-01), confirmed by the owner, 2026-11-02. | 0003 |
| §7.2 | The check of an explicit declaration follows the construction rule: an aggregate must brace-initialize from the declared arguments, any other type satisfies `is_constructible` and is built with parentheses (T1.2). gcc-13's probe declares the deleted twins instead of S1's `operator T&&()`, and an aggregate with an array member or a member no placeholder can initialize is E004 (T1.3). Confirmed by the owner, 2026-11-02. | 0003 |
| §7.2 | Discovery's greedy check and the engine's check of a deduced aggregate's members use parentheses, which never elide braces, except on MSVC, and the creation code keeps braces (the Apple Clang fix, 2026-11-01). Confirmed by the owner, 2026-11-02. | 0003 |
| §7.3 | The cycle option: option 3 by default, option 2 as the opt-in. The depth budget covers the creation code. The opt-in check's bound is 150 hops on MSVC before 19.50, an assumed boundary (T1.13); the owner confirmed this part, 2026-11-02. | 0014 |
| §7.4 | E004's row: aggregates with an array member or a member no placeholder can initialize; same-arity overloads look like a missing constructor, so they are E002 (T1.3). Confirmed by the owner, 2026-11-02. | 0003 |
| §7.5 | Inline storage in aligned raw bytes, with the state byte as its engaged flag; the two teardown ordering rules. | 0005 |
| §7.5 | The injector holds its slots as indexed base classes, not in a `std::tuple` (T1.5), and a shared slot holds a `std::shared_ptr<void>` whose control block's type every class of the same size and alignment shares (T1.14). | 0005 |
| §7.5 | `make_unique_injector` moves to Phase 2; until then, a member initialized from `make_injector(...)`, or a heap object made by `new auto(make_injector(...))`, holds an injector (owner decision, 2026-11-02). | owner |
| §7.6 | The slot design (spin, then `atomic::wait`; a mutex fallback; `std::call_once` rejected), the retry after a throw, and R004 in all builds. | 0012 |
| §7.7 | R004 is the static engine's one failure path at run time; how it is reported under `-fno-exceptions` is open. | 0012 |
| §7.8 | The example's literal, what each compiler prints for the C++20 rendering, and when the C++26 message is used. | 0006 |
| §7.8 | The catalog's E013, an unbound concrete type requested in strict mode, and E014, an owning request that would delete the implementation through a base class whose destructor isn't virtual (T1.5 review). Confirmed by the owner as new IDs, 2026-11-02. | 0006 |
| §7.8 | The condition names the type and the request without the path, raised once per injector and request by `detail::raise`, and the path is in the instantiation note of the explain step that reports; confirmed by the owner, 2026-11-02. The explain pass's step 2 probes a deduced constructor's parameters and builds the class from placeholders (T1.6). | 0006 |
| §8.1, C3 | Flat pack expansions; folds only over packs that can't exceed 256 operands, as Clang rejects longer ones (T1.1). | 0010 |
| §8.1, C7 | The per-hop budget covers the creation code as well as the check. | 0014 |
| §9.2 | The rule that a red nightly blocks releases: NFR-03 and NFR-06 on Clang are accepted for v0.1, so the nightly reports the clang-18 scenario's numbers without failing on them, and `budgets.json` stays strict (owner decisions 2a and 3a, 2026-11-01). | owner |
| §12.2 | Phase 4 (diagnostics) comes before Phase 3 (binding features). This is an owner decision with no ADR of its own, so its marker reads "(amended by owner decision, 2026-10-30)". | owner |
| §12.2 | Eager mode, `threading::none` and strict mode took effect in Phase 1, with storage and `create<T>()` (T1.5). | 0012, 0004 |
| §12.2 | Phase 1's exit criteria: NFR-03 (+0.38 s) and NFR-06 (+8,240 B) on Clang are accepted for v0.1, a Phase 2 task brings NFR-03 within budget, and `budgets.json` stays strict (owner decisions 2a and 3a, 2026-11-01). | owner |

## How ADRs are amended

An ADR changes only on evidence, such as a spike, a benchmark or a compiler finding, or on an owner decision, and never silently: the Decision section (and the Alternatives and Consequences it affects) is edited in place, so the file always states the current decision; a History entry records the date, what changed, why, and the evidence; and the Status line changes when the authority behind the decision changes, for example from "Accepted (provisional, awaiting owner review)" to "Accepted (owner, *date*)" once the owner's confirmation is recorded in docs/PROGRESS.md. A decision that is replaced outright gets a new ADR, and the old one's Status becomes "Superseded by ADR-00xx". When an ADR changes what docs/plan.md says, the plan gets a minimal edit in the section concerned, marked "(amended by ADR-00xx, *date*)" with the date of the decision rather than of the edit, and the surrounding text is left alone. While an ADR or an amendment awaits the owner's sign-off, its plan markers also say "provisional", and it is listed under "Waiting for owner" in docs/PROGRESS.md. Once the owner confirms it, its History records the decision, and its plan markers say "confirmed by the owner, *date*" in place of "provisional".
