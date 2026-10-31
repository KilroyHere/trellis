# Spike S1: deduced constructor discovery

> **Decision:** accepted by the owner on 2026-10-30; recorded in [ADR-0003](../adr/0003-constructor-discovery.md). The status line below is as the spike left it.

Status: **waiting for owner sign-off** (task T0.4). Code: `spikes/S1-deduced-discovery/`. Reproduce every number with `bash spikes/S1-deduced-discovery/run.sh` from the repository root; it exits non-zero if any expectation below stops holding. Its outputs go to `out/`, which is ignored by version control. `results/` holds a committed snapshot of the final run: tables, program outputs and the first error line of every failing cell.

## 1. Question

From plan §12.1: which set of conversion operators on a placeholder argument resolves every constructor-parameter form without ambiguity on GCC, Clang and MSVC, so that the operator template deduces exactly the form the parameter asks for (`T`, `T&`, `const T&`, `T&&`, `T*`, `unique_ptr<T>`, `shared_ptr<T>`, …)? What does arity probing cost? How are copy and move constructors excluded, how are aggregates handled, and how is each limit detected: greedy constructor templates, variadic constructors, same-arity overloads, defaulted parameters, private or deleted constructors, abstract types? Decides ADR-03 and §7.2.

## 2. Method

- **Compilers:** GCC 13.3.0 and 14.2.0, each with its own libstdc++; Clang 18.1.3 and 20.1.2 with GCC 14's libstdc++; Clang 18.1.3 with libc++ 18.1. All in `-std=c++20`. `out/versions.txt` records the exact versions and the machine load. MSVC is not available (see §5).
- **Forms × operator sets** (`forms.cpp`). For each of 35 parameter forms `P`, a class `consumer<P>` has the single constructor `consumer(P)`. Eight placeholder operator sets are tested. If `std::is_constructible_v<consumer<P>, Set>` holds, the program constructs the consumer with a tracing placeholder: the operator that overload resolution selects records its kind and the request type it deduced (`T`, `T&`, …, with `T` deduced), then throws, so no value is needed. The request is compared with the declared parameter type. Failing cells are recompiled one at a time (`-DS1_DIAG_SET/FORM`) to record the compiler's reason. The recommended set's expected result is encoded per form, so any deviation fails `run.sh`.
- **Limits** (`limits.cpp`): one class per limit, with discovery's verdict asserted by `static_assert`.
- **Injector stub** (`injector_stub.hpp`, `stub_test.cpp`): resolves a small graph and checks that form/lifetime mismatches are errors, not fallbacks. `compile_fail/*.cpp` check that each intended error ID is the first error.
- **Warnings:** `limits.cpp` and `stub_test.cpp` build with `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror` (NFR-10).
- **Probing cost** (`bench/gen_bench.py`): 100 classes, class *i* with *i* mod 9 parameters (0 to 8, average 4), the parameters cycling through `T`, `const T&`, `T&`, `T*`, `unique_ptr<T>` and `shared_ptr<T>` over ten dependency types. The translation units differ only in what they assert per class. Front end only (`-fsyntax-only`), wall time and peak RSS of the compiler's process tree, median of 5 runs, compiles run one at a time. The machine is shared with other agents, so expect ±10% noise.

## 3. Results

### 3.1 The operator set

Recommended (`min`, prototype in `deduce.hpp`):

```cpp
template <class Consumer, class T>
concept not_self = !std::same_as<std::remove_cvref_t<T>, Consumer>;  // drops copy/move ctors

template <class Consumer, class Ctx>
struct any_arg {
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
```

The request is the return type of the selected operator. Why it is unambiguous:

- **By-value parameters** (`T`, `T*`, `unique_ptr<T>`, `function<…>`, `span<T>`, …): both templates are candidates, and partial ordering cannot separate `T` from `T&`. The implicit object parameter decides instead. The placeholder is an rvalue, and binding it to a non-const object parameter beats binding it to a const one ([over.ics.rank]/3.2.6), so `operator T()` wins.
- **`T&` and `const T&`**: only lvalue-yielding conversion functions are candidates ([dcl.init.ref]/5.1.2, [over.match.ref]). For `const U&`, `T` deduces as `const U`, so the request `T&` is exactly `const U&` with no second operator.
- **Discovery** uses a context-free twin, `probe_arg<Consumer>`, with the same two shapes and no `can_resolve` constraint. The arity is fixed per class before any resolution.

**Why the deleted twins.** If the operators were merely constrained on `can_resolve`, an unresolvable request would not fail: overload resolution would pick the other operator. A `const X&` of a unique `X` would fall back to `operator T()` and bind a temporary that dies when the constructor returns. A singleton requested by value would fall back to `operator T&()` and be copied. A deleted function still takes part in overload resolution, so the form-matching operator stays selected and the construction becomes ill-formed. `stub_test.cpp` shows both hazards with a naive placeholder: the constraint-only design compiles, and the held reference points to a destroyed temporary at run time. It also shows both refused with the twins, on all five configurations.

### 3.2 Form by compiler, recommended set

Summary:

- **gcc-14, clang-18, clang-20 and clang-18 + libc++:** 29 of 35 forms are deduced exactly, the 3 rvalue-reference forms are reported as `T`, and 3 forms are ambiguous.
- **gcc-13, with the plain set:** 28 forms are exact and 7 fail.

"exact" means the request equals the declared type; by-value parameters lose top-level `const`, as in the function type. Columns: gcc-13, gcc-14, clang-18, clang-20, clang-18 + libc++. Full per-cell output: `results/summary.md` (a committed snapshot of `out/summary.md` from the final run).

| # | Parameter | Operator → request | g13 | g14 | c18 | c20 | c18 libc++ |
|---|---|---|---|---|---|---|---|
| 1 | `T` | `T()` → `T` exact | ✓ | ✓ | ✓ | ✓ | ✓ |
| 2 | `const T` (by value) | `T()` → `T` exact | ✓ | ✓ | ✓ | ✓ | ✓ |
| 3 | `T&` | `T&()` → `T&` exact | ✓ | ✓ | ✓ | ✓ | ✓ |
| 4 | `const T&` | `T&()` [T = const U] → `const T&` exact | ✓ | ✓ | ✓ | ✓ | ✓ |
| 5 | `T&&` | `T()` → `T` (a) | ✗ (b) | ✓ | ✓ | ✓ | ✓ |
| 6 | `const T&&` | `T()` → `T` (a) | ✗ (b) | ✓ | ✓ | ✓ | ✓ |
| 7 | `T*` | `T()` → `T*` exact | ✓ | ✓ | ✓ | ✓ | ✓ |
| 8 | `const T*` | `T()` → `const T*` exact | ✓ | ✓ | ✓ | ✓ | ✓ |
| 9 | `T* const` | `T()` → `T*` exact | ✓ | ✓ | ✓ | ✓ | ✓ |
| 10 | `unique_ptr<T>` | `T()` → exact | ✓ | ✓ | ✓ | ✓ | ✓ |
| 11 | `unique_ptr<T>&&` | `T()` → `unique_ptr<T>` (a) | ✗ (b) | ✓ | ✓ | ✓ | ✓ |
| 12 | `shared_ptr<T>` | `T()` → exact | ✓ | ✓ | ✓ | ✓ | ✓ |
| 13 | `const shared_ptr<T>&` | `T&()` → exact | ✓ | ✓ | ✓ | ✓ | ✓ |
| 14 | `shared_ptr<const T>` | `T()` → exact | ✓ | ✓ | ✓ | ✓ | ✓ |
| 15 | `weak_ptr<T>` | `T()` → exact | ✓ | ✓ | ✓ | ✓ | ✓ |
| 16 | `function<int()>` | `T()` → exact | ✓ | ✓ | ✓ | ✓ | ✓ |
| 17 | `function<unique_ptr<I>(int)>` (factory) | `T()` → exact | ✓ | ✓ | ✓ | ✓ | ✓ |
| 18 | `span<T>` | `T()` → exact | ✓ | ✓ | ✓ | ✓ | ✓ |
| 19 | `span<const T>` | `T()` → exact | ✓ | ✓ | ✓ | ✓ | ✓ |
| 20 | `optional<T>` | ✗ ambiguous (c) | ✗ | ✗ | ✗ | ✗ | ✗ |
| 21 | `int` | `T()` → exact | ✓ | ✓ | ✓ | ✓ | ✓ |
| 22 | `const int&` | `T&()` → exact | ✓ | ✓ | ✓ | ✓ | ✓ |
| 23 | `string` | `T()` → exact | ✓ | ✓ | ✓ | ✓ | ✓ |
| 24 | `string_view` | `T()` → exact | ✓ | ✓ | ✓ | ✓ | ✓ |
| 25 | `vector<T>` | `T()` → exact | ✓ | ✓ | ✓ | ✓ | ✓ |
| 26 | `I&` (abstract) | `T&()` → exact | ✓ | ✓ | ✓ | ✓ | ✓ |
| 27 | `const I&` | `T&()` → exact | ✓ | ✓ | ✓ | ✓ | ✓ |
| 28 | `I*` | `T()` → exact | ✓ | ✓ | ✓ | ✓ | ✓ |
| 29 | `unique_ptr<I>` | `T()` → exact | ✓ | ✓ | ✓ | ✓ | ✓ |
| 30 | `shared_ptr<I>` | `T()` → exact | ✓ | ✓ | ✓ | ✓ | ✓ |
| 31 | `reference_wrapper<T>` | `T()` → exact | ✗ (b) | ✓ | ✓ | ✓ | ✓ |
| 32 | `lazy<T&>` (library wrapper stand-in) | `T()` → exact | ✓ | ✓ | ✓ | ✓ | ✓ |
| 33 | `tuple<T>` | ✗ ambiguous (c) | ✗ | ✗ | ✗ | ✗ | ✗ |
| 34 | `any` | ✗ ambiguous (c) | ✗ | ✗ | ✗ | ✗ | ✗ |
| 35 | `variant<T, int>` | `T()` → exact | ✓ | ✓ | ✓ | ✓ | ✓ |

Notes:

- **(a) `T&&` is reported as `T`, and no operator set can do better.** For an rvalue-reference target, deduction uses the referred type ([temp.deduct.conv]), so every operator deduces the same `T` as for a by-value parameter.
  - The `T&&` candidates (prvalue- and xvalue-yielding operators) are a subset of the by-value candidates, and two candidates rank the same way in both contexts. So the by-value winner also wins for `T&&`, unless that winner is the lvalue operator, and then `T` and `T&` coincide instead.
  - At most two of `T`, `T&` and `T&&` can be told apart. The `rref_first` set shows the other side: preferring `operator T&&()` reports `T&&` for every by-value parameter.
  - This is harmless if Trellis defines `T&&` as the same request as `T`, a fresh object the consumer owns. §6.3 should say so.
- **(b) gcc-13 bug, fixed in gcc-14.** With the plain set, gcc-13 selects `operator T&()` for an rvalue-reference parameter and then rejects it ("invalid user-defined conversion … near match"). The workaround is in the prototype: on GCC < 14 the *probe* also declares `operator T&&() const`, so discovery still finds the constructor. Resolution then works because, for a unique `X`, `operator T&()` is the deleted twin, which gcc-13 no longer mis-selects. Verified for `C(X&&)`, which `limits.cpp` discovers, resolves and builds on gcc-13. `const T&&` and `unique_ptr<T>&&` take the same path but were not tested separately. Side effect: on gcc-13 only, discovery accepts `optional<T>` parameters; resolution still rejects them. On `reference_wrapper<T>`, the same bug fires inside its constructor's constraint. The workaround probe accepts it (set `min_rr`), but its resolution on gcc-13 was not tested.
- **(c) Parameter types with an unconstrained converting constructor template** compete with the conversion function and are ambiguous on every compiler. `optional(U&&)`, `tuple(Us&&...)` and `any(T&&)` all accept the placeholder itself. Such parameters need the explicit route. Library wrappers (`lazy`, `provider`, `factory`) must therefore not have converting constructor templates that accept arbitrary types. The stand-in `lazy<T&>` row shows that such a wrapper works.

### 3.3 Other operator sets (all five configurations unless noted)

| Set | Operators | Result |
|---|---|---|
| `plan` (§7.2 sketch) | `T() const`, `T&() const` | **Ambiguous for every by-value form**: 25 of 35 cells fail on Clang, 27 on gcc-13 and 28 on gcc-14 (`T`, `T*`, smart pointers, `function`, `span`, `int`, …). The same object parameter leaves only partial ordering, which cannot order `T` against `T&`. Worse, a `std::any` parameter "resolves" by storing the placeholder itself. |
| `boost` (Boost.DI `any_type_ref_fwd` shape) | `T()`, `T&() const`, `T&&() const`, `const T&() const` | **Compiler-dependent.** GCC reports `T&&` exactly and accepts `optional<T>` and `tuple<T>`; Clang reports `T` and rejects them. Code would compile on one compiler and not the other. |
| `min3` | `min` plus `const T&() const` | Same verdicts as `min`; `const T&` goes through `const T&()` by partial ordering. A fallback if MSVC mis-deduces `T = const U`. |
| `min_rr` | `min` plus `T&&() const` | Fixes gcc-13 but makes GCC and Clang diverge, like `boost`. Used only in the gcc-13 probe. |
| `refq` | `T() &&`, `T&() const&` | Same as `min`, including the gcc-13 failure. A fallback if MSVC mis-ranks cv-qualified object parameters. |
| `rref_first` | `T() const`, `T&() const`, `T&&()` | Reports `T&&` for every by-value form: T and T&& are indistinguishable. |
| `value` | `T()` only | `T&` fails; `const T&` silently becomes a temporary `T`. |

**GCC `-Wconversion`:** the recommended set triggers "choosing `operator T()` over `operator T&() const` … because conversion sequence for the argument is better" at every by-value conversion, because GCC flags exactly the object-parameter tie-break the design relies on. A `#pragma GCC diagnostic` push and ignore around the construction functions in the header silences it (verified: zero warnings under `-Werror` on all five configurations). Trellis needs that pair behind macros in `config.hpp` to meet NFR-10.

### 3.4 Arity probing cost

Arity is the largest *N* ≤ `max_ctor_arity` with `T` constructible from *N* probes. It is searched from the top down, with an `if constexpr` chain whose discarded branches are never instantiated. "Search" variants run only the search. "Discover" runs the full `ctor_info`: the abstract check, the search, the greedy check and the shorter-arity check.

Milliseconds per class above a baseline translation unit of the same size (median of 5 runs), with peak front-end memory:

| Variant (100 classes, 0 to 8 parameters) | gcc-13 | gcc-14 | clang-18 | clang-20 | clang-18 libc++ |
|---|---|---|---|---|---|
| baseline TU: seconds, MB | 0.29, 82 | 0.30, 81 | 0.37, 114 | 0.37, 123 | 0.30, 109 |
| explicit: `is_constructible<C, declared types...>` | −0.12 | −0.08 | 0.23 | 0.20 | 0.21 |
| search, flat chain, max 16 | 1.27 | 1.24 | 2.70 | 2.63 | 2.69 |
| search, recursive specializations, max 16 | 1.42 | 1.35 | 2.44 | 3.17 | 2.95 |
| search, fold over all 17 arities, max 16 | 1.79 | 1.70 | 2.87 | 3.31 | 3.43 |
| search, flat chain, max 8 | 0.63 | 0.63 | 1.23 | 1.23 | 1.34 |
| **full discovery, flat, max 16** | **1.64** | **1.49** | **2.92** | **3.24** | **3.06** |
| full discovery, max 16, 1,000 classes | 2.23 | 2.00 | 3.04 | 3.30 | 3.30 |
| peak MB, full discovery, 100 / 1,000 classes | 126 / 534 | 124 / 522 | 154 / 503 | 164 / 526 | 149 / 505 |

- **Time.** Full discovery costs 1.5–1.6 ms per class on GCC and 2.9–3.2 ms on Clang, so 0.15–0.32 s for 100 classes. At 1,000 classes it costs 2.0–3.3 ms per class: close to linear, though GCC's per-class cost rises by about a third while Clang's stays flat.
- **Memory.** About 0.4 MB of compiler memory per class: +39 to +44 MB at 100 classes, +387 to +438 MB at 1,000.
- **NFR budgets.** At 100 components, that is roughly 10% (GCC) to 25% (Clang) of NFR-04's compile-time budget (1.3× the hand-wired 5.5 / 4.2 s, so +1.65 / +1.26 s). It is about a quarter to a third of NFR-05's memory headroom (1.5× the hand-wired 363 / 232 MB).
- **The failed probes above the arity dominate.** The search tries 17 − *k* arities. With max 8, the search costs 46–51% of max 16 (on average 5 probes instead of 13).
- **Explicit declarations cost nothing measurable** (−0.12 to +0.23 ms, within noise). The machine was shared with other agents: the load average was about 2 to 3 on 4 vCPUs.
- **Search strategy.** The flat and recursive chains cost about the same (within ±20%, either way), but the recursive one nests up to 17 instantiations per class on the resolution path (NFR-09), and the flat one adds one. Probing all arities with a fold is 6–41% slower than stopping at the first hit.

### 3.5 Copy/move constructors, aggregates and limits

| Limit | Detection in the prototype | Intended error | Verified case → verdict |
|---|---|---|---|
| Copy/move constructors | `not_self<Consumer, T>` on every operator | none (excluded) | copy + move + default → arity 0; only a copy constructor → E002 |
| Aggregates | `std::is_aggregate_v` (C++20: no user-declared constructor) → probe and build with braces; otherwise parentheses, so `initializer_list` constructors never capture the placeholders | none | `{X, const Y&, unique_ptr<X>}` → 3 fields, each a dependency. Default member initializers are overridden (`{int port = 80; X}` → 2); arrays overcount through brace elision (`{int[2], X}` → 3); a base class counts as the first field. `shorter_viable` is always set for aggregates, because trailing fields may be omitted. |
| Greedy constructor template `template<class U> C(U&&)` | the chosen arity also accepts `inert_arg`, a type that converts to nothing | E004 | → E004. With a longer non-template constructor (`C(X, Y)`) the template is irrelevant → arity 2, OK. A template constrained so the probe fails (`std::integral U`) is ignored. |
| Variadic constructors (template or C `...`) | arity = `max_ctor_arity` and the inert probe is accepted | E004 | both → E004 at arity 16 |
| Same-arity overloads | ambiguity makes the probe fail at that arity | E002 (hint: same-arity overloads) | `C(X,Y)` + `C(Y,X)` → E002. **Undetectable** when a shorter constructor exists: `C()` + the pair → arity 0, silently. Explicit route only. |
| Defaulted parameters | counted: they are injected. `shorter_viable` (arity − 1 also viable) flags them, but a shorter overload gives the same signal | none (documented); a strict option could make it E004 | `C(X, int = 3)` → arity 2, flagged; `C(X)` + `C(X, Y)` → arity 2, flagged |
| Private or deleted constructors | invisible to `is_constructible`; private, deleted and absent are indistinguishable | E002, or the next public arity | private `C(X)` → E002; `C(X) = delete` → E002; private `C(X,Y)` + public `C(X)` → 1. The `template<class T> C(T) = delete` idiom catches the probe → E002. |
| Abstract types | `std::is_abstract_v`, checked before probing | E001 | → E001 |
| Arity above the limit | probe *max*+1 … 2·*max*, only on the failure path | E010 | 17 parameters → E010 |
| Unresolvable parameter of the chosen constructor | arity fixed by discovery, so never a fallback | E002 (or E005 from the explain pass) | `C()` + `C(const X&)` with `X` unique → error, not `C()` |

All five configurations agree on every row (`limits.cpp` `static_assert`s), except the gcc-13 `optional<T>` discovery side effect in note (b). The compile-fail cases E001, E002 (private), E002 (no fallback), E004, E005 (reference to unique), E005 (singleton by value) and E010 give the intended ID as the first and only error on all five configurations, in 7 to 11 lines of output. `create()` stops after the failed check, so nothing cascades.

### 3.6 Injector stub for S2

`injector_stub.hpp` provides `s1::injector<singleton<Ts>...>`. Unbound classes are unique, and `singleton<T>` makes one lazily built object per injector. It supports `T`, `T&`, `const T&`, `T*`, `unique_ptr<T>`, `shared_ptr<T>` (sole owner) and aggregates. `can_resolve<R>` is a variable template, memoized per (injector, request type); the consumer lifetime is not yet in the key. `create<R>()` reports one `static_assert` per root cause. `stub_test.cpp` resolves `App(Service, shared_ptr<Repo>, const Logger&, Settings)` → `Service(Repo, const Config&, Logger*, Clock)` → `Repo(Logger&, unique_ptr<Clock>)`, with `Config` and `Logger` singletons and `Settings` an aggregate. The checks: one `Logger` and one `Config`; every reference and pointer aliases them; unique objects are distinct; everything is destroyed. Cycles are not handled: they produce the compiler's own recursion error (S2's question).

## 4. Recommendation

**Confirm ADR-03** (explicit, then reflection, then deduced, behind `ctor_traits<T>`), **with these amendments to §7.2:**

1. **Replace the sketched operator pair.** Both operators `const`, as sketched, are ambiguous for every by-value parameter. Use the non-const `operator T()` and the const `operator T&()`, each constrained by `not_self`, and give each a deleted twin for requests the injector cannot satisfy. Keep `min3` and `refq` as measured fallbacks if MSVC disagrees (§5).
2. **Separate discovery from resolution.** Use a context-free probe, memoized per class, then the resolving placeholder at that fixed arity. Without this, an unresolvable parameter silently selects a shorter constructor.
3. **Request vocabulary (§6.3).** `T&&` and `const T&&` are the request `T`. Parameters of type `optional<T>`, `tuple<T>` and `any` are not deducible and need the explicit route. A wrapper Trellis owns must not have an unconstrained converting constructor template.
4. **Arity search.** Use the flat top-down `if constexpr` chain, with E001, E004 and E010 detected as in §3.5 and `max_ctor_arity` = 16. Discovery costs 1.5–3.2 ms and about 0.4 MB per class, mostly in failed probes above the arity. Keep 16 for now, and revisit it against the NFR-04 and NFR-05 budgets in Phase 1's benchmark. Boost.DI's default of 10 would cut a typical 4-parameter class from 13 probes to 7.
5. **config.hpp gets two workarounds,** each with a test: the gcc-13 probe-only `operator T&&() const`, and the GCC `-Wconversion` push/ignore around construction.
6. **Known residual limits, documented with compile-fail tests and pointing to the explicit route:**
   - same-arity overloads next to a shorter constructor, which are silently skipped;
   - defaulted parameters, which are injected;
   - constructor templates whose constraints the probe satisfies;
   - aggregate array members;
   - private versus deleted versus absent constructors, which are indistinguishable.

**Cost of the alternatives.**

- Boost's four-operator shape gives different answers on GCC and Clang.
- A value-only placeholder binds temporaries to `const T&` parameters.
- Constraint-only resolution (no twins) reintroduces Boost.DI-style silent dangling references and copies.
- Requiring the explicit strategy everywhere would remove every limit above, but loses the zero-intrusion default that is Boost.DI's best-liked trait (ADR-03, "Why").

## 5. Limits and open questions

**MSVC risk assessment (untested here; medium risk).** The design depends on:

1. the cv tie-break between a non-const and a const implicit object parameter for conversion function templates on an rvalue object;
2. lvalue-reference binding considering only lvalue-yielding conversion functions;
3. `operator T&` deducing `T = const U` for a `const U&` target;
4. a selected deleted conversion function making `is_constructible` false, not falling through;
5. constraints on conversion function templates;
6. `__is_constructible` honouring access.

For context:

- Boost.DI relies on points 1 and 2 in its `any_type` family, and its changelog lists MSVC 2015 to 2019 support, so the core trick has a track record there.
- Points 3 to 5 have none.
- Boost.DI has open compile-failure reports against VS 2019 (#498, #468, #462; research notes). Whether any of them touches this mechanism is unknown.
- MSVC's concepts support is younger than GCC's and Clang's, which matters for points 4 and 5. This is a general caution, not a measured result. Phase 1 should build with `/permissive-`.

Mitigation:

- Make the first MSVC CI job run `forms.cpp`, `limits.cpp` and `stub_test.cpp` unchanged; they need only the standard library.
- If cells differ, try `min3`, which removes point 3, then `refq`, which replaces point 1 with ref-qualifiers. Both give identical results on GCC and Clang.
- Keep the operator shapes in one internal header so a `config.hpp` switch can pick the set.

**Not tested:**

- Apple Clang;
- the NFR-13 minimums (GCC 12 and Clang 16), where GCC 12 presumably needs the gcc-13 workaround;
- `-std=c++23`, modules, `-fno-exceptions` (the forms tracer throws; the stub does not);
- cycles and per-hop instantiation depth, which belong to S2.

The recursive search adds up to 17 nested instantiations per class on the resolution path. The flat chain adds a constant number, which matters for NFR-09.

**Open questions for Phase 1:**

- **Explain pass.** The deleted twin already names the failing operator and `T` in the compiler's note. The explain pass should still report E005 with the parameter index, not a generic E002.
- **`const shared_ptr<T>&` and `const T&` of unique objects.** Should these be allowed by holding the object in the placeholder until the constructor returns?
- **Self-registration (§7.4, S6).** It can hook the operator bodies, since each knows the consumer, the request and the position.

## Addendum: MSVC (2026-10-31, task T1.13)

S1 couldn't test MSVC, and rated the risk medium (§5; ADR-0003, "MSVC risk"). Task T1.13 ran the spike's tests, unchanged, on the only MSVC this project can reach, GitHub's `windows-latest` runner, before T1.3 and T1.5 build on them.

**Method.**

- **Compilers.** cl 19.51.36260 (toolset 14.51.36231, Visual Studio 2026 18.11) is the runner's latest, which T1.13 asks about. cl 19.44.35229 (toolset 14.44.35207, Visual Studio 2022 17.14) comes from the same image: it is the newest compiler in the series that NFR-13's minimum, 19.38, belongs to. The image was `win25-vs2026` 20261021.278.1.
- **Flags.** `/std:c++20 /permissive- /W4 /EHsc`, without `/WX`; warnings are counted instead.
- **What runs.** `msvc.py`, next to `run.sh`, builds and runs `forms.cpp`, `limits.cpp`, `stub_test.cpp` and `compile_fail/*.cpp`, as `run.sh` does on GCC and Clang. It compares each of the 280 form cells (8 sets × 35 forms) with the committed GCC 14 and Clang 20 snapshot (`results/forms-gcc14.txt`, `results/forms-clang20.txt`).
- **Fallbacks.** The two fallbacks, `min3` and `refq`, are among `forms.cpp`'s eight sets. `msvc.py` also builds `limits.cpp` and `stub_test.cpp` with each fallback patched into a copy of `deduce.hpp`, deleted twins included. Both patched copies pass on gcc-14 and clang-18.
- **Runs.** `.github/workflows/msvc-probe.yml`. Every number below comes from [run 41235251688](https://github.com/KilroyHere/trellis/actions/runs/41235251688). The first run, [41234196060](https://github.com/KilroyHere/trellis/actions/runs/41234196060), gave the same results for the cells it covered, and led to the form-by-form build described under "`forms.cpp`" below. Results come back as check-run annotations (`gh api repos/{owner}/{repo}/check-runs/<job id>/annotations`). To reproduce them, run `gh workflow run msvc-probe.yml`.

**Result: the recommended set behaves on MSVC exactly as on GCC 14 and Clang 20.** Both cl versions give the same result in every cell, so the table has one MSVC column. ✓ means the same operator, the same request and the same verdict as GCC 14 and Clang 20 (§3.2).

| # | Parameter | Operator → request (GCC 14, Clang 20) | cl 19.51 and 19.44 |
|---|---|---|---|
| 1 | `T` | `T()` → `T` exact | ✓ |
| 2 | `const T` (by value) | `T()` → `T` exact | ✓ |
| 3 | `T&` | `T&()` → `T&` exact | ✓ |
| 4 | `const T&` | `T&()` [T = const U] → `const T&` exact | ✓ |
| 5 | `T&&` | `T()` → `T` (a) | ✓ |
| 6 | `const T&&` | `T()` → `T` (a) | ✓ |
| 7 | `T*` | `T()` → `T*` exact | ✓ |
| 8 | `const T*` | `T()` → `const T*` exact | ✓ |
| 9 | `T* const` | `T()` → `T*` exact | ✓ |
| 10 | `unique_ptr<T>` | `T()` → exact | ✓ |
| 11 | `unique_ptr<T>&&` | `T()` → `unique_ptr<T>` (a) | ✓ |
| 12 | `shared_ptr<T>` | `T()` → exact | ✓ |
| 13 | `const shared_ptr<T>&` | `T&()` → exact | ✓ |
| 14 | `shared_ptr<const T>` | `T()` → exact | ✓ |
| 15 | `weak_ptr<T>` | `T()` → exact | ✓ |
| 16 | `function<int()>` | `T()` → exact | ✓ |
| 17 | `function<unique_ptr<I>(int)>` (factory) | `T()` → exact | ✓ |
| 18 | `span<T>` | `T()` → exact | ✓ |
| 19 | `span<const T>` | `T()` → exact | ✓ |
| 20 | `optional<T>` | ✗ ambiguous (c) | ✓ (b) |
| 21 | `int` | `T()` → exact | ✓ |
| 22 | `const int&` | `T&()` → exact | ✓ |
| 23 | `string` | `T()` → exact | ✓ |
| 24 | `string_view` | `T()` → exact | ✓ |
| 25 | `vector<T>` | `T()` → exact | ✓ |
| 26 | `I&` (abstract) | `T&()` → exact | ✓ |
| 27 | `const I&` | `T&()` → exact | ✓ |
| 28 | `I*` | `T()` → exact | ✓ |
| 29 | `unique_ptr<I>` | `T()` → exact | ✓ |
| 30 | `shared_ptr<I>` | `T()` → exact | ✓ |
| 31 | `reference_wrapper<T>` | `T()` → exact | ✓ |
| 32 | `lazy<T&>` (library wrapper stand-in) | `T()` → exact | ✓ |
| 33 | `tuple<T>` | ✗ ambiguous (c) | ✓ (b) |
| 34 | `any` | ✗ ambiguous (c) | ✓ (b) |
| 35 | `variant<T, int>` | `T()` → exact | ✓ |

(a) and (c) as in §3.2. (b) Not constructible, as on GCC and Clang. cl's error is C2665, "no overloaded function could convert all the argument types", where GCC and Clang say the conversion is ambiguous.

**All eight sets.** On both cl versions, out of 35 cells per set:

| Set | Cells equal to GCC 14 | Cells equal to Clang 20 |
|---|---|---|
| `min` | 35 | 35 |
| `min3` | 35 | 35 |
| `refq` | 35 | 35 |
| `min_rr` | 30 | 35 |
| `boost` | 30 | 35 |
| `plan` | 32 | 35 |
| `rref_first` | 29 | 35 |
| `value` | 34 | 34, and 1 hard error |

So MSVC equals Clang 20 in 279 of the 280 cells, and GCC 14 wherever GCC and Clang agree. In the 19 cells where they disagree (`boost`, `min_rr`, `plan` and `rref_first` on the rvalue-reference, `optional`, `tuple` and `const T&` forms, §3.3), MSVC sides with Clang. The 280th cell, `value` × `const I&`, is a hard error; see "`forms.cpp`" below.

**The five behaviours** that ADR-0003's MSVC risk section and §5 name all hold, on both cl versions:

| # | Behaviour | Evidence |
|---|---|---|
| 1 | The tie-break between a non-const and a const implicit object parameter, for conversion function templates on an rvalue | The 23 by-value forms (rows 1, 2, 7–10, 12, 14–19, 21, 23–25, 28–32 and 35) select `operator T()`. |
| 2 | Lvalue-reference binding considers only lvalue-yielding conversion functions | Rows 3, 4, 13, 22, 26 and 27 select `operator T&()`. None binds a temporary from `operator T()`. |
| 3 | `operator T&` deduces `T = const U` for a `const U&` target | Rows 4, 13, 22 and 27 report `const U&` exact, through `operator T&()`. |
| 4 | A selected deleted conversion function makes `is_constructible` false, without falling through | In `limits.cpp`: `const X&` and `X*` of a unique `X`, no fallback to `C()` for `C(const X&)`, and `X&&` of a singleton. In `stub_test.cpp`: `HoldsClockRef` and `CopiesLogger` are refused, while the naive placeholder accepts both. Also the two TRL-E005 compile-fail cases. |
| 5 | Constraints on conversion function templates | `not_self` excludes copy and move constructors (a class with copy, move and default constructors has arity 0; one with only a copy constructor is E002). The twins' `can_resolve` constraints pick the real or the deleted operator (behaviour 4's cases). |

Point 6 of §5, `__is_constructible` honouring access, holds too: a private `C(X)` gives E002, and a private `C(X, Y)` beside a public `C(X)` gives arity 1.

**`limits.cpp`, `stub_test.cpp`, the compile-fail cases and `forms.cpp`.**

- **`limits.cpp` doesn't compile unchanged** on either cl version, because one of its 36 `static_assert`s fails: **an aggregate `{int a[2]; X x;}` has arity 2 on MSVC, 3 on GCC and Clang** (§3.5, the aggregates row).
  - The other 35 hold. cl reports every failing `static_assert`, and with its assertions removed, `limits.cpp` prints the other 28 `LIMIT` rows exactly as GCC and Clang do.
  - A small program in `msvc.py` shows why. cl initializes the whole array member from one placeholder, through `operator T&()` with `T = int[2]`, and the second placeholder initializes `x`. GCC and Clang elide the braces and take one placeholder per element. cl does reject `{arr, 3}` for an `int arr[2]` lvalue, so the cause is how cl treats a conversion function that yields an array reference, not a general extension that copies arrays.
  - This is a residual limit ADR-0003 already lists ("aggregate array members, which brace elision overcounts"). Both counts end in an error, because neither an `int(&)[2]` request nor an `int` one is resolvable. Only the count differs, so T1.3's test for this limit should check the error, not the arity.
- **`stub_test.cpp` builds and runs** on both: 10 PASS, and all 13 `static_assert`s hold. It has three C4702 warnings (unreachable code) at `injector_stub.hpp:166`: the `std::terminate()` after `create()`'s `if constexpr` chain is unreachable whenever the chain returns. Under `/WX` that would fail NFR-10, so T1.5's `create()` must not leave code after a returning branch.
- **The compile-fail cases:** all 7 give the intended ID as the first and only error on both versions, in 9 lines each (7 to 11 on GCC and Clang).
- **`forms.cpp` doesn't compile as a whole,** because one cell is a hard error.
  - In set `value` (`operator T()` alone), for `const I&` with `I` abstract, cl's `is_constructible` reports true, and the construction then fails with C2259 ("cannot instantiate abstract class"). GCC and Clang report the cell not constructible.
  - `min` never reaches this case. A reference parameter has only `operator T&()` as a candidate. A by-value request of an abstract type is unresolvable, so the operator `any_arg` selects for it is the deleted twin, which has no body to instantiate.
  - `msvc.py` therefore builds `forms.cpp` form by form: each form with all 8 sets, from a copy of everything before `main`, and the failing form cell by cell. That gives the 279 other cells.
- **Warnings under `/W4`:** none in `forms.cpp` on 19.51, and 4 on 19.44 (C4127, "conditional expression is constant", in the tracer). None in `limits.cpp`. In `stub_test.cpp`, the three C4702 above.

**Fallbacks.** They aren't needed, because `min` matches. For the record, `min3` and `refq` also match GCC and Clang in all 35 forms on MSVC, and pass `stub_test.cpp`. With `limits.cpp` they stop at the same `int[2]` assertion, which doesn't depend on the operator set.

**Conclusion.** The accepted operator set holds on MSVC 19.51 and 19.44, so ADR-0003's decision stands unchanged. For Phase 1:

- T1.3: the aggregate-array count depends on the compiler (2 on MSVC, 3 on GCC and Clang), so the test checks the error, not the arity.
- T1.5: keep code out of the path after a returning `if constexpr` branch in `create()`, or C4702 breaks `/WX`.
- Not tested on MSVC: clang-cl, `/std:c++latest`, modules, and MSVC goldens for the error messages (the IDs do come first).
