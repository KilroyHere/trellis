# Architecture decision records

Each file records one decision: its context, the decision itself, the alternatives and what they would cost, the consequences, and a dated history. ADR-0001 to ADR-0013 are the thirteen decisions in [plan §4](../plan.md#4-architecture-decisions), numbered the same way (ADR-01 there is ADR-0001 here), with the results of the Phase 0 spikes folded in. ADR-0014 is new: it records spike S2's answer to plan §7.3, which §4 had no ADR for. Where an ADR and the plan disagree, the ADR wins.

## Index

| ADR | Title | Status | Spikes that informed it |
|---|---|---|---|
| [0001](0001-cpp20-baseline.md) | C++20 baseline; C++23 and C++26 used when available | Accepted (plan baseline) | S3 (the P2741 gate, decided in 0006) |
| [0002](0002-static-and-runtime-engines.md) | A static injector core and a separate runtime engine, sharing one vocabulary | Accepted (plan baseline) | S6 (deferred) |
| [0003](0003-constructor-discovery.md) | Three constructor-discovery strategies behind one trait | Accepted (owner, 2026-10-30) | **S1**; S5 (deferred) |
| [0004](0004-lifetimes-and-request-forms.md) | Explicit lifetimes and an enforced request-form table | Accepted (plan baseline) | S1, S2, S4 (confirmations; S1's `T&&` rule is decided in 0003) |
| [0005](0005-state-and-storage.md) | State lives only in injector and scope objects; storage is honest | Accepted (owner, 2026-10-30) | **S4** |
| [0006](0006-diagnostics-contract.md) | The diagnostics contract | Accepted (owner, 2026-10-30) | **S3**; S1, S2 |
| [0007](0007-non-intrusive.md) | Non-intrusive by default | Accepted (plan baseline) | S1 (wrapper types) |
| [0008](0008-header-only-delivery.md) | Header-only, no dependencies, several delivery forms | Accepted (plan baseline) | S4 (include cost) |
| [0009](0009-runtime-error-model.md) | The runtime error model | Accepted (plan baseline) | S4 (R004) |
| [0010](0010-module-composition.md) | Composition through variadic modules, never fluent type accumulation | Accepted (plan baseline) | S2, S3 (an open point on type names) |
| [0011](0011-type-identity-policy.md) | Type identity as a policy in the runtime engine | Accepted (plan baseline) | S3, S4; S6 (deferred) |
| [0012](0012-threading-model.md) | Threading model | Accepted (owner, 2026-10-30) | **S4** |
| [0013](0013-governance-and-releases.md) | Governance and release discipline | Accepted (plan baseline) | none |
| [0014](0014-resolution-memoization-and-cycles.md) | Resolution: memoization, depth per hop and cycles in deduced mode | Accepted (owner, 2026-10-31); its MSVC amendment is provisional (T1.13) | **S2** |

A spike in bold changed or settled the decision; the others confirmed it, or added consequences and open points.

**Status values:**

- **Accepted (owner, *date*):** the owner confirmed the decision, including the spike results it records (docs/PROGRESS.md, "Owner decisions").
- **Accepted (plan baseline):** the decision as plan §4 states it; no spike changed it.
- **Accepted (provisional, awaiting owner review):** a spike settled the decision, and the owner hasn't confirmed it yet. Work follows it as the default until then, and it stays under "Waiting for owner" in docs/PROGRESS.md. At present only one amendment has this status: ADR-0014's, for MSVC before 19.50 (T1.13).

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

Each change to docs/plan.md is a minimal edit in place, marked "(amended by ADR-00xx, *date*)" with the date of the decision: 2026-10-30, or 2026-10-31 for ADR-0014. The §4 pointer and the §12.2 note are marked as the table says.

| Plan section | Change | By |
|---|---|---|
| §3.2, NFR-07 | The first error line names the error ID; the same diagnostic names the type. | 0006 |
| §3.2, NFR-09 | ≥ 150 hops on MSVC before 19.50, an assumed boundary; ≥ 200 hops elsewhere. Provisional, awaiting the owner (T1.13). | 0014 |
| §4 | A pointer to this directory. | this index |
| §6.3 | `T&&` and `const T&&` parameters are the request `T`. | 0003 |
| §6.8 | R004 is detected in all builds. | 0012 |
| §7.2 | S1's operator set replaces the sketch: deleted twins, a separate probe, the explicit route for `optional`, `tuple` and `any`, two `config.hpp` workarounds and the residual limits. T1.13 found the aggregate-array count compiler-dependent, which only rewords that limit. | 0003 |
| §7.1 | `type_list` algorithms use flat pack expansions and an inheritance-based set, folding only over packs that can't exceed 256 operands; `type_map` lookups resolve one non-template overload per entry, without deduction (T1.1). | 0010 |
| §7.1 | `result<T>`'s error type is `trellis::failure`, as `trellis::error` is the compile-error namespace (T1.1). | 0006 |
| §7.3 | The cycle option: option 3 by default, option 2 as the opt-in. The depth budget covers the creation code. The opt-in check's bound is 150 hops on MSVC before 19.50, an assumed boundary; this part is provisional, awaiting the owner (T1.13). | 0014 |
| §7.5 | Inline storage in aligned raw bytes, with the state byte as its engaged flag; the two teardown ordering rules. | 0005 |
| §7.6 | The slot design (spin, then `atomic::wait`; a mutex fallback; `std::call_once` rejected), the retry after a throw, and R004 in all builds. | 0012 |
| §7.7 | R004 is the static engine's one failure path at run time; how it is reported under `-fno-exceptions` is open. | 0012 |
| §7.8 | The example's literal, what each compiler prints for the C++20 rendering, and when the C++26 message is used. | 0006 |
| §8.1, C3 | Flat pack expansions; folds only over packs that can't exceed 256 operands, as Clang rejects longer ones (T1.1). | 0010 |
| §8.1, C7 | The per-hop budget covers the creation code as well as the check. | 0014 |
| §12.2 | Phase 4 (diagnostics) comes before Phase 3 (binding features). This is an owner decision with no ADR of its own, so its marker reads "(amended by owner decision, 2026-10-30)". | owner |

## How ADRs are amended

An ADR changes only on evidence, such as a spike, a benchmark or a compiler finding, or on an owner decision, and never silently: the Decision section (and the Alternatives and Consequences it affects) is edited in place, so the file always states the current decision; a History entry records the date, what changed, why, and the evidence; and the Status line changes when the authority behind the decision changes, for example from "Accepted (provisional, awaiting owner review)" to "Accepted (owner, *date*)" once the owner's confirmation is recorded in docs/PROGRESS.md. A decision that is replaced outright gets a new ADR, and the old one's Status becomes "Superseded by ADR-00xx". When an ADR changes what docs/plan.md says, the plan gets a minimal edit in the section concerned, marked "(amended by ADR-00xx, *date*)" with the date of the decision rather than of the edit, and the surrounding text is left alone. While an ADR or an amendment awaits the owner's sign-off, its plan markers also say "provisional", and it is listed under "Waiting for owner" in docs/PROGRESS.md.
