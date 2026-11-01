# Deduced constructor discovery

The C++20 default for a type that declares no constructor ([ADR-0003](../adr/0003-constructor-discovery.md), [plan §7.2](../plan.md#72-constructor-discovery-confirm-in-s1-and-s5)): Trellis deduces the constructor with placeholder arguments, as spike S1 designed it ([S1](../spikes/S1-deduced-discovery.md)), without macros or changes to the type. The code is [`include/trellis/discovery/deduced.hpp`](../../include/trellis/discovery/deduced.hpp); the explicit strategy that comes first is in [constructor-discovery.md](constructor-discovery.md). This note explains each technique, gives S1's form table with what Trellis deduces, and lists every limit with what it produces.

The engine's entry points, all in `trellis::detail`:

| Entity | What it gives | Assertion |
|---|---|---|
| `discovered_ctor_t<T, BindingCtor, MaxArity>` | the constructor in every mode: a declared `ctor<Args...>`, or the deduced `ctor<probe_arg<T>, ...>` | TRL-E002, TRL-E004 or TRL-E010, once per root cause |
| `discoverable_v<T, BindingCtor, MaxArity>` | whether `discovered_ctor_t` gives a constructor | none |
| `deduction<T, MaxArity>` | deduction's `status` and `arity`, memoized per (T, bound) | none |
| `deduced_constructible_v<T, Arity, Ctx>` | whether T builds with every parameter resolved by `Ctx` | none |
| `construct_deduced<T, Arity>(ctx)` | the built T, a prvalue | none |

`BindingCtor` is a binding's `.with_ctor<Args...>()` (T1.4), and `MaxArity` the injector's `max_ctor_arity`, 16 by default (`default_max_ctor_arity`) and at most 32; T1.5 connects both. Every entry point strips `T`'s cv-qualifiers, and `not_self` compares unqualified types, so a `const` or `volatile` `T` is the same class with the same constructors.

## The placeholders

**What it does.** Two placeholder classes have conversion operator templates, so that overload resolution of `T(placeholder, ...)` deduces each parameter's type. `probe_arg<T>` only finds the arity; `any_arg<T, Ctx>` resolves, and is S1's `min` set exactly (ADR-0003 item 1):

```cpp
template <class Ctx, class R>
concept can_resolve = Ctx::template can_resolve<R>;

template <class Consumer, class Ctx>
struct any_arg {
  Ctx& ctx;
  template <class T> requires not_self<Consumer, T> && (Ctx::template can_resolve<T>)
  operator T() { return ctx.template resolve<T>(); }           // non-const: value forms
  template <class T> requires not_self<Consumer, T> && (!can_resolve<Ctx, T>)
  operator T() = delete;                                        // deleted twin
  template <class T> requires not_self<Consumer, T> && (Ctx::template can_resolve<T&>)
  operator T&() const { return ctx.template resolve<T&>(); }    // const: T& and const T&
  template <class T> requires not_self<Consumer, T> && (!can_resolve<Ctx, T&>)
  operator T&() const = delete;                                 // deleted twin
};
```

The plan writes every constraint with the concept. The two real operators name `Ctx::can_resolve<T>` directly instead, which saves Clang one level of instantiation depth per hop of a dependency chain (see "Depth per hop"); for them the two forms are equivalent. The deleted twins must keep the concept. A context's `can_resolve<R>` may not exist for some requests, if it is constrained or uses `enable_if`. There, the atomic constraint `!Ctx::can_resolve<R>` fails to substitute and is not satisfied, so neither the real operator nor its twin would be viable, and the other operator would bind a temporary to a `const X&`. The concept-id `can_resolve<Ctx, R>` is false there instead, so its negation holds and the twin is selected. T1.3's review found this; the `sfinae_ctx` and `enable_if_ctx` cases in the tests guard it.

The request a parameter makes is the return type of the operator that overload resolution selects:

- A by-value parameter (`X`, `X*`, smart pointers, `std::function`, `std::span`, ...) has both templates as candidates, and partial ordering can't separate `T` from `T&`. The implicit object parameter decides: the placeholder is an rvalue, and binding it to a non-const object parameter beats binding it to a const one ([over.ics.rank]/3.2.6), so `operator T()` wins.
- `X&` and `const X&` can only bind to the lvalue-yielding `operator T&()` ([dcl.init.ref]/5.1.2), with `T` deduced as `const X` for `const X&`.
- `X&&` and `const X&&` deduce the referred type ([temp.deduct.conv]), so they are the request `X`, as the request vocabulary says in every mode (ADR-0003 item 3, plan §6.3).
- `not_self<Consumer, T>` keeps the target from being the class under construction, which removes its copy and move constructors: with one argument, they would be viable through both operators, and every copyable class would seem to have a one-parameter constructor. It compares both types without cv-qualifiers, through `TRELLIS_IS_SAME`, the `__is_same` builtin where the compiler has one, which instantiates nothing per pair of types. (Comparing with a `const` consumer as it was let `construct_deduced<const X, 1>` ask for `X` itself, through the move constructor; T1.3's review found it.)
- **The deleted twins.** Constrained on `can_resolve` alone, an unresolvable request wouldn't fail: overload resolution would pick the other operator, binding a temporary to a `const X&` of a unique `X`, or copying a singleton requested by value. A deleted function still takes part in overload resolution, so the operator that matches the form stays selected, and the construction becomes ill-formed (S1 §3.1).

`Ctx` is the engine's context, which answers `Ctx::can_resolve<R>` and `ctx.resolve<R>()`. The engine memoizes the first per (key, request form, consumer lifetime) (plan §7.3, ADR-0014): `Ctx` stands for an injector and the lifetime of the object being built, `R` for the key and the form. The calls are member calls, so argument-dependent lookup never searches the namespaces of `Ctx` or of the request.

**Why.** S1 measured 8 operator sets on 35 forms (S1 §3.3). This is the smallest that gives the same verdict on GCC and Clang for every form, with no silent fallback, and MSVC agrees (S1's MSVC addendum); `min3` and `refq`, which add or change an operator, agree too and stay the measured fallbacks (ADR-0003).

**Tests:** `tests/unit/deduced_test.cpp`, "Forms" (every form, at compile time and at run time) and "Limits" (the `values_only` and `references_only` contexts: no fallback, no copy of a singleton).

## Discovery before resolution

**What it does.** Discovery fixes the arity with the context-free `probe_arg<T>`, which has `any_arg`'s two shapes and no `can_resolve` constraint; resolution then uses `any_arg` at exactly that arity (ADR-0003 item 2). A class with `C()` and `C(const X&)`, where `const X&` can't be served, is an error, not `C()`.

**Why.** Searching with the resolving placeholder would let an unresolvable parameter silently select a shorter constructor. A context-free probe also makes discovery a property of the type alone, memoized once per (T, bound) for every injector.

**Tests:** `tests/unit/deduced_test.cpp` (`needs_ref_to_unique`, `defaulted_param`); `tests/compile_fail/discovery/deduced_no_fallback.cpp`.

## The arity search

**What it does.** The arity is the largest *N* up to the bound such that T is constructible from *N* probes, searched from the top down by a flat `if constexpr` chain (S1 §3.4, ADR-0003 item 5):

```cpp
template <class T, std::size_t N, std::size_t MaxArity>
concept probes_at =
    (N <= MaxArity) && constructible_from_n<T, probe_arg<T>, std::make_index_sequence<N>>;

if constexpr (probes_at<T, 16, MaxArity>) { return 16; }
else if constexpr (probes_at<T, 15, MaxArity>) { return 15; }
// ... down to 0
```

- The discarded branches of the chain are never instantiated, so a class with *k* parameters costs 17 − *k* probes and one instantiation level.
- `probes_at` is a concept because a conjunction of constraints stops at the first one that isn't satisfied: arities above the bound are never probed. In a plain `&&` inside the `if constexpr` condition, both sides would be instantiated.
- Each probe expands `repeat_t<Is, probe_arg<T>>...` over `std::make_index_sequence<N>`, which the compilers build with a builtin, so no arity recurses. `constructible_from_n` is a variable template, memoized per (type, placeholder, arity). On MSVC, `repeat_t` goes through a class template (`TRELLIS_REPEAT_THROUGH_CLASS`, under "The compiler workarounds").
- The chain is written out for arities 0 to 16, and again for 17 to 32, which only a bound above 16 instantiates. `deduce` refuses a bound above 32 with a plain `static_assert`: a longer constructor can be declared explicitly, without a limit.
- On the failure path only, `probes_above` probes the 16 arities above the bound with a fold of 16 operands, so that a constructor just above the bound is TRL-E010 rather than TRL-E002.

**Why.** S1 measured the alternatives (S1 §3.4): a recursive search costs about the same but nests up to 17 instantiations per class on the resolution path, against NFR-09, and a fold over all the arities is 6–41% slower, as it can't stop at the first hit.

**Tests:** `tests/unit/deduced_test.cpp`, "Limits": the arities of `plain0`, `plain3`, `seventeen` and `five` under bounds of 0, 4, 5, 16, 17 and 32, and `variadic_template`, which takes the bound itself.

## The construction rule and aggregates

**What it does.** A probe uses the one construction rule, `builds_from<T, Args...>()` in `ctor_traits.hpp`, by which explicit declarations are checked too (ADR-0003 item 5): an aggregate with braces, so that each placeholder initializes one field, and anything else with parentheses, through `std::is_constructible`, so that an `initializer_list` constructor never captures the placeholders. Its `if constexpr` instantiates one of the two checks per type. A default member initializer is overridden, as its field is injected, and a base class is the first field.

An aggregate's field count can still go wrong in two ways, and `deduce` refuses both, so that an aggregate deduces the same fields on every compiler or not at all (TRL-E004):

- **Array members.** GCC and Clang elide the braces around an array member and give each element its own placeholder; MSVC initializes the whole array from one, through `operator T&()` with `T = int[2]` (T1.13, S1's MSVC addendum). `no_array_arg<T>` is the probe without a conversion to an array reference. In parentheses, which never elide braces, it can't initialize an array member on any compiler, so `std::is_constructible_v<T, no_array_arg<T>...>`, with as many as the deduced fields, is false exactly when the aggregate has an array member, however the compiler counted, and true otherwise, as parentheses then initialize the same fields as braces.
- **Members a placeholder can't initialize,** such as `std::optional`, `std::tuple` and `std::any` (S1 §3.2 note c). Such a member ends the deduced field list, and the members after it would be value-initialized without a word. `T{probes..., {}}` is well-formed exactly when a field follows the deduced ones: the empty braces initialize it as leaving it out did. The one gap is a field with a default member initializer whose type has no default constructor, which leaving out accepts and `{}` doesn't.

A member that is itself an aggregate with an array is one field, requested as its own type; deducing that type then reports the array.

**MSVC checks with braces.** The platforms CI showed that cl 19.51's parenthesized aggregate initialization fails for every member that is itself an aggregate class (`std::array`, an empty struct, a struct with one `int`), although the placeholder converts to it, in `std::is_constructible` and in a requires-expression alike. So the parenthesized check took `agg`, `agg_base`, `agg_nested`, `with_array` and `agg_tuple_last` for aggregates with arrays. With braces, MSVC counts the same fields as GCC and Clang for every aggregate without an array, with the probe and with `no_array_arg` alike. So where `TRELLIS_ARRAY_CHECK_WITH_BRACES` is 1 (MSVC), the check is `T{no_array_arg<T>...}`, at MSVC's count, where each array is one field, and `no_array_arg` declares its conversion to an array reference, `operator T&()` with an array `T`, deleted. MSVC initializes an array member through exactly that conversion, so it selects the deleted one and the initialization fails, for arrays of any size, `int[1]` and `int[2][2]` included; for any other member `no_array_arg` converts as the probe does. GCC and Clang elide the braces around an array instead, so they keep the parenthesized check. Measured on cl 19.51 with S1's and T1.3's aggregates: the arrays give `array_member`, the aggregates with aggregate members `deduced`, and the `std::optional` and `std::tuple` members `undeducible_member`, as on GCC and Clang.

**Why.** S1 found array members compiler-dependent and left them as a residual limit (ADR-0003 item 7); T1.13 confirmed that, with `int` bound, GCC and Clang build `{int a[2]; X x;}` and MSVC doesn't. Refusing them on every compiler gives one result everywhere, and the fix, declaring one parameter per element, works on every compiler because declarations are checked with braces too. The member check closes the silent case that S1's `shorter_viable` flag only noted. Each check costs one probe, for aggregates only.

**Tests:** `tests/unit/deduced_test.cpp`, "Limits": `agg`, `agg_defaults`, `agg_base`, `agg_nested`, `agg_of_aggregate`, `agg_rvalue_member`, `agg_empty`, the five `agg_array*` and `agg_matrix` cases, `std::array<int, 2>`, and the three member cases; `TEST(DeducedLimits, AggregateFieldsAreInjected)`; `tests/compile_fail/discovery/deduced_aggregate_array.cpp` and `deduced_aggregate_optional_member.cpp`.

## The limits

**What it does.** `deduce<T, MaxArity>()` returns a status and the arity, each limit checked only where the step before leaves the question open, so that a well-behaved class pays for the search and one inert probe:

1. not a class (`std::is_class_v`: fundamental types, pointers, references, enumerations, unions): `not_a_class`;
2. incomplete (`requires { sizeof(T); }`), before anything that needs the definition: `incomplete`. Every trait and probe below needs a complete class, and without this check an incomplete one gave 20 errors on Clang and 77 on GCC. Like any memoized answer about completeness, it holds from the first discovery of `T` on, so the message asks for the definition where `T` is discovered;
3. abstract (`std::is_abstract_v`), before probing: `abstract`;
4. no arity within the bound: `above_bound` if one of the 16 above works, otherwise `not_constructible`;
5. the arity also accepts `inert_arg`, which converts to nothing: a constructor template or a variadic constructor accepts anything, `greedy`;
6. an aggregate: `array_member`, then `undeducible_member`, as above;
7. otherwise `deduced`.

`deduction<T, MaxArity>` holds the result, memoized per (T, bound), for the engine's normal pass, which only asks (ADR-0006 item 2).

**Tests:** `tests/unit/deduced_test.cpp`, "Limits", one `static_assert` per case of S1's `limits.cpp`.

## The entry point

**What it does.** `discovered_ctor_t<T, BindingCtor, MaxArity>` is `decltype(detail::discover<std::remove_cv_t<T>, BindingCtor, MaxArity>())`. `discover` returns the explicit strategy's checked declaration when one applies (`explicit_ctor_t`, TRL-E002), and otherwise deduction's result: `ctor<probe_arg<T>, ...>`, one placeholder per parameter, or one `static_assert` per failed status, each in its own `if constexpr` branch, after which the type is `no_ctor_declared`.

- Deduction can't name the parameter types (ADR-0003, plan §5.3: "an arity in deduced mode"), so each placeholder stands for the request that it deduces when the engine replaces it with `any_arg`. The result has the same shape as a declaration, and `ctor_matches_v` checks it by the same rule: true, as the probes are exactly what discovery found T constructible from.
- The assertions sit in a function whose `auto` return type makes `decltype` instantiate its body; the function is memoized per (T, BindingCtor, MaxArity), so a type is reported once however often it is used. A failed `static_assert` in a class template would leave the class invalid, and Clang reports each later use of it as a second error ("no type named 'type'"); a function body leaves nothing behind.
- cv-qualifiers are stripped, so `const X` and `X` share one deduction and one report.
- The branch for a declaration is taken first and discards the deduction branches, so a declared type is never probed.

**Why.** One entry point lets the engine handle every mode the same way: walk `args`, and build each argument from the declared type or from the placeholder. `discoverable_v` gives the same answer without an assertion.

**Tests:** `tests/unit/deduced_test.cpp`, "Entry point"; every `tests/compile_fail/discovery/deduced_*.cpp` that uses `discovered_ctor_t` names its type twice, once with `const`, or uses it twice, and allows no second error line.

## Building a deduced type

**What it does.** `construct_deduced<T, Arity>(ctx)` returns `T{any_arg<T, Ctx>{ctx}...}` for an aggregate and `T(any_arg<T, Ctx>{ctx}...)` otherwise, as discovery probed it, with `T` stripped of cv-qualifiers. It returns a prvalue, so T needn't be movable and the engine can initialize storage from it directly. `deduced_constructible_v<T, Arity, Ctx>` is the same check with `any_arg`: the engine's resolvable test for a deduced type (T1.5).

`deduced_constructible_v` is on the resolution path once per hop, so it is itself the specialization that expands the arity and makes the check, through parameters callers never pass (`T` without cv-qualifiers, the arity as an index sequence, and a `bool` that selects braces or parentheses). The check is `builds_from`'s rule written out, not a call, which would cost a level per hop; a test checks that the two agree.

**Tests:** `tests/unit/deduced_test.cpp`: `EachFormResolvesToTheRequestS1Recorded`, `ResolutionDeliversTheServedObjects`, `DefaultedParameterIsInjected`, `AggregateFieldsAreInjected`, `ConstConsumerRequestsItsParameterNotItself`, and the `rule_agrees` static_asserts.

## Depth per hop

**What it costs.** Resolving a deduced type nests one level of template instantiation per template between one hop's request and the next: the context's `can_resolve<R>`, `deduced_constructible_v<R, ...>`, the constructor's overload resolution, and the selected operator's constraint on the next request. NFR-09 asks for chains of 200 hops at the compilers' default limits (`-ftemplate-depth` 900 on GCC, 1024 on Clang), so each level per hop counts.

Measured on a chain of classes each taking the next by value, resolved through `deduced_constructible_v` from a context whose `can_resolve<R>` is `deduced_constructible_v<R, deduction<R>::arity, ctx>`: the lean shape T1.5 should keep. The smallest `-ftemplate-depth` that compiles:

| Chain of N | gcc-13, gcc-14 | clang-18, clang-20 |
|---|---|---|
| T1.3 | 4N + 9 (409 at N = 100) | 5N + 11 (511) |
| T1.3 before its review, through `builds_from` and the `can_resolve` concept | 6N + 10 (610) | 8N + 11 (811) |
| S1's prototype, in the same shape | about 5N (509) | about 7N (715) |

So T1.3's share leaves T1.5 about 222 hops on GCC and 204 on Clang at the default limits. Every level that T1.5's context adds per hop, such as a separate class for the binding lookup, costs about 40 to 50 hops at N = 200. T1.11 measures the deepest chain the engine compiles (NFR-09).

**Tests:** none in the suite yet; T1.11 adds the chain test. The numbers come from compiling the chain with `-fsyntax-only` at N = 50 and 100 and bisecting `-ftemplate-depth`.

## Notes for the engine

- T1.5 must ask `discoverable_v` before it reads `discovered_ctor_t<...>::args`: after a failed discovery the type is `no_ctor_declared`, which has no `args`, so reading it adds a second error.
- A failure is reported once per (T, binding declaration, bound): one greedy type used by two injectors with different `max_ctor_arity` gives two TRL-E004s, one per bound. That is acceptable, since each injector's options are its own.

## The compiler workarounds

All four live in `config.hpp` (ADR-0003 item 6): two for GCC and two for MSVC.

- **`TRELLIS_PROBE_DECLARES_TWINS`, GCC before 14.** GCC 13 picks the probe's `operator T&() const` for a parameter of type `X&&`, `const X&&`, `std::unique_ptr<X>&&` or `std::reference_wrapper<X>` and then rejects it ("invalid user-defined conversion ... near match"), so discovery would miss those constructors. T1.3 measured that GCC 13 chooses correctly when the probe also declares `any_arg`'s two deleted twins, constrained on `twin_tag`, an internal class that no constructor takes, so that no parameter selects them. (A constraint that nothing satisfies would leave the twin templates without a valid specialization, which [temp.res.general]/6 makes ill-formed.) `any_arg` itself needs nothing: it has its twins anyway, and GCC 13 resolves all four forms with it (S1 had tested `X&&` only).
  - S1 recommended a probe-only `operator T&&() const` instead. It made GCC 13's discovery accept `std::optional<T>` and `std::tuple<T>` parameters, and `std::tuple` aggregate members, which resolution and the other compilers reject; the twins don't, so every compiler gives the same verdicts on every form and limit. Recorded in ADR-0003's history for the owner.
  - Test: `tests/unit/deduced_test.cpp`, "Workarounds": the macro is 1 exactly where S1's plain probe misses the four forms, so the test fails once a GCC 13 release is fixed.
- **`TRELLIS_PLACEHOLDER_CONVERSIONS_BEGIN` and `_END`, GCC.** GCC's `-Wconversion` warns at every by-value parameter that it chose `operator T()` over `operator T&() const`, which is the tie-break the design relies on, in Trellis's code. The pair switches that warning off around `construct_deduced`; Clang doesn't warn. Test: `tests/unit/deduced_test.cpp` builds by-value parameters through `construct_deduced` under the project warnings and `-Werror`, which GCC refuses without the pair.
- **`TRELLIS_REPEAT_THROUGH_CLASS`, MSVC.** `repeat_t<I, A>` is an alias for `A` that ignores `I`. cl 19.51 loses the pack when `repeat_t<Is, A>...` expands over a single index: `ctor<repeat_t<Is, probe_arg<T>>...>`, the deduced declaration of a constructor with one parameter, failed with C3546 ("there are no parameter packs available to expand") inside `ctor`, at the arities 0, 2 and 3 it compiled (platforms CI). Through a class template, `typename repeat<I, A>::type`, the pattern depends on `I`, and every arity compiles. GCC and Clang keep the plain alias, which instantiates nothing; the class is a leaf, so it adds no depth per hop on MSVC either. Test: `tests/unit/deduced_test.cpp`, "Workarounds" (the macro is on exactly on MSVC), and every one-parameter form under "Forms".
- **`TRELLIS_ARRAY_CHECK_WITH_BRACES`, MSVC.** The array-member check uses braces and a deleted conversion to an array reference, because cl 19.51's parentheses refuse members of aggregate class type (see "The construction rule and aggregates"). Test: "Workarounds" pins the macro to MSVC and checks that parentheses still refuse `agg_nested` there and accept it elsewhere, so the test fails once a cl release is fixed; the aggregates under "Limits" test the check.

## Forms

S1's 35 parameter forms (S1 §3.2), each with the request Trellis deduces, on gcc-13, gcc-14, clang-18, clang-20 and clang-18 with libc++, and in CI on Apple clang 21 and cl 19.51. `X` is a class, `I` an abstract class. Every row is a check in `tests/unit/deduced_test.cpp`, "Forms": at compile time, discovery finds one parameter and a context that serves only the request builds the consumer, while one that serves only the other operator's request doesn't; at run time, `construct_deduced` with a context that serves every request records exactly that request.

| # | Parameter | Operator | Request |
|---|---|---|---|
| 1 | `X` | `T()` | `X` |
| 2 | `const X` | `T()` | `X` (top-level const isn't part of the function type) |
| 3 | `X&` | `T&()` | `X&` |
| 4 | `const X&` | `T&()`, `T = const X` | `const X&` |
| 5 | `X&&` | `T()` | `X` (ADR-0003 item 3) |
| 6 | `const X&&` | `T()` | `X` |
| 7 | `X*` | `T()` | `X*` |
| 8 | `const X*` | `T()` | `const X*` |
| 9 | `X* const` | `T()` | `X*` |
| 10 | `std::unique_ptr<X>` | `T()` | `std::unique_ptr<X>` |
| 11 | `std::unique_ptr<X>&&` | `T()` | `std::unique_ptr<X>` |
| 12 | `std::shared_ptr<X>` | `T()` | `std::shared_ptr<X>` |
| 13 | `const std::shared_ptr<X>&` | `T&()` | `const std::shared_ptr<X>&` |
| 14 | `std::shared_ptr<const X>` | `T()` | `std::shared_ptr<const X>` |
| 15 | `std::weak_ptr<X>` | `T()` | `std::weak_ptr<X>` |
| 16 | `std::function<int()>` | `T()` | `std::function<int()>` |
| 17 | `std::function<std::unique_ptr<I>(int)>` | `T()` | the same |
| 18 | `std::span<X>` | `T()` | `std::span<X>` |
| 19 | `std::span<const X>` | `T()` | `std::span<const X>` |
| 20 | `std::optional<X>` | — | not deducible: explicit route |
| 21 | `int` | `T()` | `int` |
| 22 | `const int&` | `T&()` | `const int&` |
| 23 | `std::string` | `T()` | `std::string` |
| 24 | `std::string_view` | `T()` | `std::string_view` |
| 25 | `std::vector<X>` | `T()` | `std::vector<X>` |
| 26 | `I&` | `T&()` | `I&` |
| 27 | `const I&` | `T&()` | `const I&` |
| 28 | `I*` | `T()` | `I*` |
| 29 | `std::unique_ptr<I>` | `T()` | `std::unique_ptr<I>` |
| 30 | `std::shared_ptr<I>` | `T()` | `std::shared_ptr<I>` |
| 31 | `std::reference_wrapper<X>` | `T()` | `std::reference_wrapper<X>` |
| 32 | `lazy<X&>` (a wrapper Trellis owns) | `T()` | `lazy<X&>` |
| 33 | `std::tuple<X>` | — | not deducible: explicit route |
| 34 | `std::any` | — | not deducible: explicit route |
| 35 | `std::variant<X, int>` | `T()` | `std::variant<X, int>` |

Rows 20, 33 and 34: the type's converting constructor template accepts the placeholder itself, which competes with the conversion operator. Discovery finds no constructor (TRL-E002, `deduced_optional_param.cpp`), and with a context that serves every request, as S1's placeholder did, the construction is ambiguous, on every compiler. `std::any` is ambiguous in every context; `std::optional<X>` and `std::tuple<X>` only when `X` itself is resolvable, which is the usual case, since their constructor needs `X` constructible from the placeholder. Wrappers that Trellis owns (`lazy`, `provider`, `factory`) must therefore never have a converting constructor template that accepts any type (ADR-0007); row 32's stand-in shows that such a wrapper deduces exactly.

On gcc-13, rows 5, 6, 11 and 31 need `TRELLIS_PROBE_DECLARES_TWINS` to be discovered; S1 recorded them as failing there with the plain set.

## What each limit produces

S1's limits (S1 §3.5, ADR-0003 items 5 and 7), plus the aggregate member check. "Documented" means this note and the error page.

| Case | Outcome | Test |
|---|---|---|
| Constructor template that accepts anything, `template <class U> C(U&&)` | TRL-E004 | `deduced_greedy_template.cpp` |
| Variadic constructor template | TRL-E004, at the bound | `deduced_variadic_template.cpp` |
| C variadic constructor, `C(...)` | TRL-E004, at the bound | `deduced_c_variadic.cpp` |
| Aggregate with an array member | TRL-E004 on every compiler | `deduced_aggregate_array.cpp` |
| Aggregate member a placeholder can't initialize (`std::optional`, `std::tuple`, `std::any`) | TRL-E004 | `deduced_aggregate_optional_member.cpp` |
| Constructor only above the bound (within 16) | TRL-E010 | `deduced_above_bound.cpp`, `deduced_above_lowered_bound.cpp` |
| Same-arity overloads, `C(X, Y)` and `C(Y, X)` | TRL-E002, whose message names same-arity overloads: deduction can't tell ambiguity from absence | `deduced_same_arity.cpp` |
| Private constructor | TRL-E002 | `deduced_private_ctor.cpp` |
| Deleted constructor, or the `template <class T> C(T) = delete` idiom | TRL-E002 | `deduced_deleted_ctor.cpp`; unit test `deleted_template_guard` |
| Only copy and move constructors | TRL-E002 (they are excluded) | `deduced_only_copy_ctor.cpp` |
| Parameter of type `std::optional`, `std::tuple` or `std::any` | TRL-E002 | `deduced_optional_param.cpp`; unit tests for all three |
| Abstract class | TRL-E002, "it is abstract". The injector reports TRL-E001 for an unbound abstract key before it asks discovery (T1.6) | `deduced_abstract.cpp` |
| Not a class (`int`, a pointer, an enumeration, a union) | TRL-E002, "deduction needs a class type" | `deduced_not_a_class.cpp` |
| Incomplete class | TRL-E002, "it is incomplete here" | `deduced_incomplete.cpp` |
| Unresolvable parameter of the discovered constructor | an error, never a shorter constructor; documented message below | `deduced_no_fallback.cpp` |
| Defaulted parameter | injected, never defaulted; documented message below when it can't be resolved | `deduced_defaulted_param.cpp`; `DefaultedParameterIsInjected` |
| Same-arity overloads next to a shorter constructor | residual: the shorter one is chosen silently | unit test `same_arity_plus_default` |
| Constructor template whose constraint the probe satisfies and an inert argument doesn't | residual: deduced, with the placeholder itself as the argument | unit test `probe_satisfies_constraint` |
| Private, deleted and absent constructors | residual: indistinguishable, all TRL-E002 | as above |
| A private constructor with more parameters next to a public one | the public one is chosen | unit test `private_longer_public_shorter` |

The fix for every row is an explicit declaration (a `ctor_traits` specialization, a member alias, or a binding's `.with_ctor<...>()`), or for TRL-E010 a higher `max_ctor_arity`; the error pages give the details.

**Resolution failures, until T1.6.** When a parameter of the discovered constructor can't be resolved, the engine's normal pass sees `deduced_constructible_v` false and doesn't build (T1.5); its explain pass will name the parameter with an ID (T1.6, ADR-0006). Building directly, as the two tests above do, gives the compiler's error on the deleted twin, which names the request: on GCC, `use of deleted function '... any_arg<Consumer, Ctx>::operator T() [with T = int; ...]'`; on Clang, `conversion function from '...any_arg<...>' to 'int' invokes a deleted function`. That is the documented message the two tests check.

## Cost

S1 measured full discovery at 1.5–1.6 ms per class on GCC and 2.9–3.2 ms on Clang, with about 0.4 MB of compiler memory per class (S1 §3.4). On S1's benchmark (100 classes of 0 to 8 parameters, front end only, median of 5, the two headers compiled in turn), T1.3's header costs less than S1's prototype on every compiler measured, from the builtin comparison in `not_self`, variable templates instead of class templates, and no `shorter_viable` probe:

| Per class, above the baseline | gcc-13 | gcc-14 | clang-18 | clang-20 |
|---|---|---|---|---|
| S1's prototype (`ctor_info`) | 4.2 ms | 4.6 ms | 6.8 ms | 6.1 ms |
| T1.3 (`deduction`) | 2.2 ms | 1.8 ms | 5.0 ms | 5.6 ms |
| Peak memory, S1 / T1.3, 100 classes | 126 / 104 MB | 124 / 101 MB | 153 / 147 MB | 164 / 156 MB |

The machine was shared, with a load average of about 3 on 4 CPUs, so the absolute numbers are slower than S1's; the comparison between the two, measured in the same runs, is what counts. Probing through `builds_from`, a function, cost gcc-13 0.4–0.7 ms per class more than the same rule written as a disjunction of constraints, and Clang nothing measurable (A/B, median of 7); deduction uses `builds_from` all the same, so that declarations and deduction share one definition of the rule. T1.11 checks the bound against NFR-04 and NFR-05 with the injector.

## Open points

- The engine (T1.5) builds a deduced type with `construct_deduced` and checks it with `deduced_constructible_v`, from a context that carries the consumer's lifetime and adds as few levels per hop as it can (see "Depth per hop"). It connects `options.max_ctor_arity` (T1.4) to `MaxArity`, and should check once that it is at most 32: `deduce` asserts it once per type, without an error ID, as plan §7.8 has none for a misconfigured option.
- T1.6 decides whether the assertions stay in `discover` or move into `create<T>()` with the explain pass, and how the pass reports a deduced type's failing parameter (TRL-E005 with its index, ADR-0006), which deduction can't name.
- MSVC and Apple Clang run only in CI. `tests/unit/deduced_test.cpp` passes on cl 19.51 and Apple clang 21, and so do the compile-fail tests on Apple clang. On MSVC the compile-fail tests give the intended ID first, but their checks of the error type fail, because cl doesn't print a `static_assert`'s condition (ADR-0006's MSVC risk), and two expect GCC's and Clang's wording for a deleted function; T1.6 owns the MSVC rendering.
