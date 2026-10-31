# ADR-0003: Three constructor-discovery strategies behind one trait

- **Status:** Accepted (owner, 2026-10-30)
- **Plan:** [§4](../plan.md#4-architecture-decisions) ADR-03; §6.3, §7.2, §7.10; FR-06
- **Spikes:** [S1 deduced discovery](../spikes/S1-deduced-discovery.md), whose recommendation the owner accepted. Spike S5 (reflection prototype) is deferred, so the reflection strategy below is accepted as planned but not yet prototyped.
- **Amends:** plan §6.3 (`T&&` parameters) and §7.2 (the operator set, the workarounds and the residual limits).

## Context

FR-06 asks for constructor discovery without macros, plus an explicit escape hatch. Automatic, macro-free wiring is Boost.DI's best-liked trait, and it rests on a trick: a placeholder argument whose conversion operators are templates, so that overload resolution deduces each parameter's type. The explicit and reflection strategies instead yield the dependency type list, which makes complete static graph analysis possible: cycles, captive dependencies, unused bindings and graph export.

Plan §7.2 sketched the placeholder with two `const` operators, `operator T()` and `operator T&()`, and asked spike S1 three things: which operator set resolves every parameter form without ambiguity on GCC, Clang and MSVC; what arity probing costs; and how each limit is detected. S1 tested 35 parameter forms against 8 operator sets on gcc-13, gcc-14, clang-18, clang-20 and clang-18 with libc++, all in C++20 mode (S1 §2). MSVC wasn't available.

## Decision

`trellis::ctor_traits<T>` resolves, in this order:

1. **Explicit**, from any of three sources: a specialization `template<> struct trellis::ctor_traits<T> : trellis::ctor<Args...> {};`, a member alias `using trellis_ctor = trellis::ctor<Args...>;` (opt-in, intrusive), or the binding itself, `.with_ctor<Args...>()`. A `static_assert` checks the declaration the way the object will be built, so a declaration can't lie: an aggregate must brace-initialize from `Args...`, which rejects narrowing and allows brace elision, and any other type must satisfy `std::is_constructible_v<T, Args...>`, and is built with parentheses (amended in T1.2; History). Its cost isn't measurable: −0.12 to +0.23 ms per class, within noise (S1 §3.4).
2. **Reflection (C++26):** the constructor marked `[[=di::inject]]`, or else the unique public, non-copy, non-move constructor with the most parameters, with the parameter types taken from `std::meta::parameters_of` (plan §7.2). Not prototyped, because S5 is deferred (Consequences).
3. **Deduced (the C++20 default)**, as S1 recommends (S1 §4):

   1. **The operator set** is S1's `min` set (S1 §3.1):

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

      - **By-value parameters** (`T`, `T*`, smart pointers, `std::function`, `std::span`, …) select `operator T()`. Partial ordering can't separate the two templates, but the placeholder is an rvalue, and binding it to a non-const implicit object parameter beats binding it to a const one ([over.ics.rank]/3.2.6).
      - **`T&` and `const T&` parameters** can only use lvalue-yielding conversion functions ([dcl.init.ref]/5.1.2), so they select `operator T&()`, with `T` deduced as `const U` for a `const U&`.
      - **The deleted twins** keep an unresolvable request from silently using the other operator. With constraints alone, a `const X&` of a unique `X` would bind a temporary that dies when the constructor returns, and a singleton requested by value would be copied; S1's `stub_test.cpp` shows both hazards at run time. A deleted function still takes part in overload resolution, so the construction becomes ill-formed instead.
      - S1's prototype keys `can_resolve` on the request alone. Trellis also keys it on the consumer's lifetime, as plan §7.3 and ADR-0014 require.
      - **Result** (S1 §3.2): on gcc-14, clang-18, clang-20 and clang-18 with libc++, 29 of the 35 forms are deduced exactly, the 3 rvalue-reference forms are reported as `T` (item 3) and 3 forms are ambiguous (item 4). gcc-13 needs the workaround in item 6.

   2. **Discovery is separate from resolution** (S1 §4.2). A context-free probe, `probe_arg<Consumer>`, with the same two operator shapes and no `can_resolve` constraint, fixes each class's arity first, memoized per class. Resolution then uses `any_arg` at exactly that arity. Otherwise an unresolvable parameter would silently select a shorter constructor: with `C()` and `C(const X&)` and `X` unique, the result is an error, not `C()` (S1 §3.5).

   3. **`T&&` is the request `T`** (S1 §3.2 note a, §4.3). For an rvalue-reference target, deduction uses the referred type ([temp.deduct.conv]), so no operator set can tell more than two of `T`, `T&` and `T&&` apart. A parameter of type `X&&` or `const X&&` is therefore the request `X`: a fresh object that the consumer owns. This is a rule of the request vocabulary (plan §6.3, ADR-0004), so it holds in every discovery mode.

   4. **Parameters of type `std::optional<T>`, `std::tuple<T>` and `std::any` need the explicit route** (S1 §3.2 note c). Their unconstrained converting constructor templates accept the placeholder itself, which is ambiguous on every compiler. For the same reason, Trellis's own wrappers (`lazy`, `provider`, `factory`) must not have converting constructor templates that accept arbitrary types (ADR-0007); S1's `lazy<T&>` stand-in shows that such a wrapper deduces exactly.

   5. **Arity search** (S1 §3.4, §4.4): the arity is the largest *N* ≤ `max_ctor_arity` for which the class is constructible from *N* probes, searched from the top down with a flat `if constexpr` chain whose discarded branches are never instantiated. `max_ctor_arity` defaults to **16** and is an option (§6.13). The limits are detected as S1 §3.5 shows:

      | Case | Detection | Error |
      |---|---|---|
      | Abstract type | `std::is_abstract_v`, before probing | E001 |
      | Greedy constructor template, `template<class U> C(U&&)` | the chosen arity also accepts an inert argument that converts to nothing | E004 |
      | Variadic constructor (template or C `...`) | arity = `max_ctor_arity`, and the inert probe is accepted | E004 |
      | Same-arity overloads | the probe is ambiguous at that arity | E002, with a hint |
      | Arity above the limit | probes *max* + 1 to 2·*max*, only on the failure path | E010 |
      | Private or deleted constructors | invisible to `is_constructible` | E002, or the next public arity |
      | Copy and move constructors | `not_self` on every operator | none: excluded |
      | Aggregates | `std::is_aggregate_v`: probe and build with braces; otherwise parentheses, so `initializer_list` constructors never capture the placeholders | none |

      The compile-fail cases E001, E002 (private), E002 (no fallback), E004, E005 (reference to a unique binding), E005 (singleton by value) and E010 each give the intended ID as the first and only error on all five configurations, in 7 to 11 lines (S1 §3.5).

   6. **Two workarounds in `config.hpp`**, each with a test (S1 §3.2 note b, §3.3, §4.5):
      - **gcc-13** (fixed in gcc-14) selects `operator T&()` for an rvalue-reference parameter and then rejects it. On GCC < 14 the probe, and only the probe, also declares `operator T&&() const`, so discovery still finds the constructor. This was verified for `C(X&&)`. `const T&&`, `unique_ptr<T>&&` and `reference_wrapper<T>` take the same path, but their resolution on gcc-13 wasn't tested separately. Side effect, on gcc-13 only: discovery accepts `optional<T>` parameters, which resolution still rejects.
      - **GCC's `-Wconversion`** warns at every by-value conversion, because it flags exactly the object-parameter tie-break this design relies on. A `#pragma GCC diagnostic` push and ignore around the construction functions, behind `config.hpp` macros, gives zero warnings under `-Werror` on all five configurations (NFR-10).

   7. **Residual limits,** documented and pointing to the explicit route (S1 §4.6):
      - same-arity overloads next to a shorter constructor: the shorter one is chosen silently;
      - defaulted parameters, which are injected (S1's `shorter_viable` flag sees them, but a shorter overload gives the same signal);
      - constructor templates whose constraints the probe satisfies;
      - aggregate array members, whose count depends on the compiler: GCC and Clang elide the braces and count each element, while cl counts the whole array as one field (T1.13, S1's MSVC addendum). Once the element type is bound, GCC and Clang build such an aggregate and cl doesn't;
      - private, deleted and absent constructors, which look the same.

Cycle handling in deduced mode is a separate decision, [ADR-0014](0014-resolution-memoization-and-cycles.md).

### MSVC risk (S1 §5: medium; tested by T1.13)

**T1.13 tested it** on cl 19.51 (Visual Studio 2026) and cl 19.44 (Visual Studio 2022 17.14), in S1's "Addendum: MSVC". The operator set behaves there as on GCC and Clang, all six behaviours below hold, and the fallbacks weren't needed. The rest of this section is the assessment as S1 left it.

The design depends on six behaviours that couldn't be tested here:

1. the tie-break between a non-const and a const implicit object parameter, for conversion function templates on an rvalue;
2. lvalue-reference binding considering only lvalue-yielding conversion functions;
3. `operator T&` deducing `T = const U` for a `const U&` target;
4. a selected deleted conversion function making `is_constructible` false rather than falling through;
5. constraints on conversion function templates;
6. `__is_constructible` honouring access.

Boost.DI relies on points 1 and 2 and lists MSVC 2015 to 2019 support; points 3 to 5 have no track record. Mitigation:

- The first MSVC CI job runs S1's `forms.cpp`, `limits.cpp` and `stub_test.cpp` unchanged; they need only the standard library. Done by T1.13 (`.github/workflows/msvc-probe.yml`).
- If cells differ, try `min3`, which adds `operator const T&() const` and so removes point 3, then `refq` (`operator T() &&` and `operator T&() const&`), which replaces point 1 with ref-qualifiers. Both give the same verdicts as `min` on GCC and Clang.
- The operator shapes live in one internal header, so a `config.hpp` switch can pick the set.
- Phase 1 builds with `/permissive-`.

## Alternatives

Operator sets S1 measured (S1 §3.3):

- **The plan's sketch** (`T() const`, `T&() const`): ambiguous for every by-value form. 25 of 35 cells fail on Clang, 27 on gcc-13 and 28 on gcc-14, and a `std::any` parameter "resolves" by storing the placeholder itself.
- **Boost.DI's `any_type_ref_fwd` shape** (four operators): compiler-dependent. GCC reports `T&&` and accepts `optional<T>` and `tuple<T>`; Clang reports `T` and rejects them, so code would compile on one compiler and not the other.
- **Value only** (`T()`): `T&` fails, and `const T&` silently becomes a temporary.
- **Constraints without deleted twins:** silent dangling references and copies, as in Boost.DI.
- **`rref_first`** (prefers `operator T&&()`): reports `T&&` for every by-value form.
- **`min_rr`** (`min` plus `T&&() const`): fixes gcc-13 but makes GCC and Clang diverge; it is used only in gcc-13's probe.

Other choices:

- **Explicit declarations everywhere:** remove every limit above, but lose the zero-intrusion default (ADR-0007).
- **A recursive arity search:** costs about the same as the flat chain (within ±20%), but nests up to 17 instantiations per class on the resolution path, against NFR-09; the flat chain adds one. **Probing every arity with a fold** is 6–41% slower than stopping at the first hit.
- **A lower `max_ctor_arity`:** the failed probes above the arity dominate discovery. A maximum of 8 costs 46–51% of 16, and Boost.DI's default of 10 would cut a typical 4-parameter class from 13 probes to 7. 16 stays until task T1.11 measures whether it matters against NFR-04 and NFR-05, using T1.9's benchmarks.

## Consequences

- **Discovery cost** (S1 §3.4): full discovery costs 1.5–1.6 ms per class on GCC and 2.9–3.2 ms on Clang, plus about 0.4 MB of compiler memory per class, close to linear up to 1,000 classes. At 100 components that is about 10% (GCC) to 25% (Clang) of NFR-04's compile-time headroom, and a quarter to a third of NFR-05's memory headroom.
- **Some checks are stronger in the explicit and reflection modes** (§7.4). Deduced mode can't enumerate a class's parameters, which limits the explain pass (ADR-0006) and cycle reports (ADR-0014).
- **T1.3 implements this decision.** Every row of S1's form table gets a resolution test, and every limit a compile-fail test or a documented message. The form table goes into `docs/internals/deduction.md` (§7.2).
- **Untested** (S1 §5): Apple Clang, the NFR-13 minimums GCC 12 and Clang 16 (GCC 12 presumably needs the gcc-13 workaround), `-std=c++23`, modules and `-fno-exceptions`. MSVC was tested by T1.13 on cl 19.51 and 19.44, not on NFR-13's minimum, 19.38 (S1's MSVC addendum).
- **Open questions for Phase 1** (S1 §5):
  - The explain pass should report E005 with the parameter index rather than a generic E002 (ADR-0006).
  - Whether `const shared_ptr<T>&` and `const T&` parameters of unique objects should be allowed, with the placeholder holding the object until the constructor returns (ADR-0004).
- **Spike S5 is deferred.** It would prototype reflection-based discovery, annotations and namespace scanning (§7.10), and needs GCC 16 (`-std=c++26 -freflection`) or Bloomberg's clang-p2996 fork. Ubuntu 24.04's archives carry GCC 9 to 14 and Clang 14 to 20 only (checked with `apt-cache` on 2026-10-31), and the network here blocks Compiler Explorer, so neither toolchain is reachable from this environment. The reflection strategy stays as planned until T6.0 schedules S5 for Phase 6, or CI gains such a toolchain.

## History

- **2026-10-30, plan draft 1:** drafted as ADR-03 in plan §4, "confirm in S1, S5", with the operator sketch in §7.2.
- **2026-10-30, spike S1 (T0.4):** recommended amendments 1–6 (S1 §4): the `min` set with deleted twins, a separate probe, `T&&` as the request `T`, the explicit route for `optional`, `tuple` and `any`, the flat search with `max_ctor_arity` 16, and two `config.hpp` workarounds.
- **2026-10-30, owner:** accepted S1's recommendation (docs/PROGRESS.md, "Owner decisions").
- **2026-10-31, T0.8:** recorded in this file, with S1's outcome folded into the Decision. Plan §6.3 and §7.2 amended. S5 recorded as deferred.
- **2026-10-31, T1.13 (MSVC check):** S1's tests ran on cl 19.51 and 19.44 in GitHub Actions (S1's "Addendum: MSVC"). No decision changes.
  - The operator set behaves as on GCC and Clang, in all 35 forms, and all six behaviours under "MSVC risk" hold.
  - One residual limit turned out compiler-dependent: cl counts an aggregate's array member as one field, where GCC and Clang count each element. Its wording changed here and in plan §7.2.
  - The MSVC risk section and the "Untested" list now point to the addendum.
- **2026-10-31, T1.2 (explicit declarations):** the check of an explicit declaration follows the construction rule, not `is_constructible` alone. Aggregates are checked by brace-initialization, as they are built, so a declaration that narrows into an aggregate is rejected and one that relies on brace elision is accepted. Every other type is checked with `is_constructible` and built with parentheses, which also keeps `initializer_list` constructors from capturing a declaration. Plan §7.2 amended. Awaits the owner's confirmation, as a refinement of the accepted Decision.
