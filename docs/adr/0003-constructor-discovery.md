# ADR-0003: Three constructor-discovery strategies behind one trait

- **Status:** Accepted (owner, 2026-10-30). Four amendments are provisional, awaiting owner review (see History): T1.2's construction rule for explicit declarations (2026-10-31), T1.3's amendments to items 5, 6 and 7 (2026-10-31), item 6's two MSVC workarounds (platform fixes, 2026-11-01), and the Apple Clang fix's refinements of item 5 for deduced aggregates (2026-11-01).
- **Plan:** [§4](../plan.md#4-architecture-decisions) ADR-03; §6.3, §6.13, §7.2, §7.4, §7.10; FR-06
- **Spikes:** [S1 deduced discovery](../spikes/S1-deduced-discovery.md), whose recommendation the owner accepted. Spike S5 (reflection prototype) is deferred, so the reflection strategy below is accepted as planned but not yet prototyped.
- **Amends:** plan §6.3 (`T&&` parameters), §6.13 (the bound on `max_ctor_arity`), §7.2 (the operator set, the workarounds, the aggregate checks and the residual limits) and §7.4 (what deduced mode reports as E004).

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

   5. **Arity search** (S1 §3.4, §4.4): the arity is the largest *N* ≤ `max_ctor_arity` for which the class is constructible from *N* probes, searched from the top down with a flat `if constexpr` chain whose discarded branches are never instantiated. `max_ctor_arity` defaults to **16** and is an option (§6.13); deduction supports a bound of at most **32**, and longer constructors are declared explicitly, which has no limit (T1.3, 2026-10-31, provisional). The limits are detected as S1 §3.5 shows:

      | Case | Detection | Error |
      |---|---|---|
      | Abstract type | `std::is_abstract_v`, before probing | E002 from discovery, which sees an abstract type only as the implementation of a binding; the engine reports E001 for an abstract key without a binding before it asks discovery (T1.6). S1's stub reported E001 here because it had no bindings to tell the two apart (T1.3, 2026-10-31, provisional) |
      | Incomplete type | `requires { sizeof(T); }`, before anything that needs the definition | E002, asking for the definition where the type is discovered (T1.3, 2026-10-31, provisional) |
      | Greedy constructor template, `template<class U> C(U&&)` | the chosen arity also accepts an inert argument that converts to nothing; an aggregate takes the inert arguments in parentheses, which never elide braces, except on MSVC (Apple Clang fix, 2026-11-01, provisional) | E004 |
      | Variadic constructor (template or C `...`) | arity = `max_ctor_arity`, and the inert probe is accepted | E004 |
      | Same-arity overloads | the probe is ambiguous at that arity | E002, with a hint |
      | Arity above the limit | probes the 16 arities above the bound, only on the failure path; S1 probed *max* + 1 to 2·*max* (T1.3, 2026-10-31, provisional) | E010 |
      | Private or deleted constructors | invisible to `is_constructible` | E002, or the next public arity |
      | Copy and move constructors | `not_self` on every operator | none: excluded |
      | Aggregates | `std::is_aggregate_v`: probe and build with braces; otherwise parentheses, so `initializer_list` constructors never capture the placeholders. The engine checks a deduced aggregate's members' requests with parentheses, except on MSVC (Apple Clang fix, 2026-11-01, provisional) | none |
      | Aggregate with an array member, or with a member no placeholder can initialize (T1.3, 2026-10-31, provisional) | parentheses with a probe that can't yield arrays, or on MSVC braces with that probe's conversion to an array reference deleted (platform fixes, 2026-11-01, provisional); a trailing `{}` after the deduced fields | E004 |

      The compile-fail cases E001, E002 (private), E002 (no fallback), E004, E005 (reference to a unique binding), E005 (singleton by value) and E010 each give the intended ID as the first and only error on all five configurations, in 7 to 11 lines (S1 §3.5).

   6. **Four workarounds in `config.hpp`**, each with a test: two for GCC (S1 §3.2 note b, §3.3, §4.5) and two for MSVC (platform fixes, 2026-11-01, provisional; see History):
      - **gcc-13** (fixed in gcc-14) selects `operator T&()` for an rvalue-reference parameter and then rejects it. On GCC < 14 the probe, and only the probe, also declares the two deleted twins of `any_arg`'s operators, constrained so that no parameter selects them; gcc-13 then chooses as gcc-14 does, so discovery finds the constructor (T1.3, 2026-10-31, provisional; see History). Measured on gcc-13 for `X&&`, `const X&&`, `std::unique_ptr<X>&&` and `std::reference_wrapper<X>`, and `any_arg` resolves all four there without help. S1's own workaround, a probe-only `operator T&&() const`, also made gcc-13's discovery accept `optional<T>` and `tuple<T>` parameters, which resolution still rejects; the twins don't.
      - **GCC's `-Wconversion`** warns at every by-value conversion, because it flags exactly the object-parameter tie-break this design relies on. A `#pragma GCC diagnostic` push and ignore around the construction functions, behind `config.hpp` macros, gives zero warnings under `-Werror` on all five configurations (NFR-10).
      - **cl 19.51** loses the pack when `repeat_t<Is, A>...`, an alias that ignores its index, expands over one index into a class template's arguments: the deduced `ctor<probe_arg<T>>` of every one-parameter constructor failed with C3546. On MSVC, `TRELLIS_REPEAT_THROUGH_CLASS` routes `repeat_t` through a class template, which keeps the pattern dependent on the index.
      - **cl 19.51's parenthesized aggregate initialization** fails for every member of aggregate class type, such as `std::array` or an empty struct, so item 5's parenthesized array check refused aggregates without arrays. On MSVC, `TRELLIS_ARRAY_CHECK_WITH_BRACES` checks with braces instead, at cl's count, with the probe's conversion to an array reference deleted: cl initializes an array member through that conversion, so it selects the deleted one exactly when there is an array member.

   7. **Residual limits,** documented and pointing to the explicit route (S1 §4.6):
      - same-arity overloads next to a shorter constructor: the shorter one is chosen silently;
      - defaulted parameters, which are injected (S1's `shorter_viable` flag sees them, but a shorter overload gives the same signal);
      - constructor templates whose constraints the probe satisfies;
      - aggregate array members, whose count depends on the compiler: GCC and Clang elide the braces and count each element, while cl counts the whole array as one field (T1.13, S1's MSVC addendum). Once the element type is bound, GCC and Clang build such an aggregate and cl doesn't. T1.3 detects them on every compiler and reports E004, which no longer leaves this limit silent (T1.3, 2026-10-31, provisional);
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
- **A lower `max_ctor_arity`:** the failed probes above the arity dominate discovery. A maximum of 8 costs 46–51% of 16, and Boost.DI's default of 10 would cut a typical 4-parameter class from 13 probes to 7. Task T1.11 measured what that saves in a whole program, against NFR-04 and NFR-05 with T1.9's benchmarks: 1–3% of the compile's instructions at 100 components, while every deduced constructor of 9 to 16 parameters would need a declaration. 16 stays (History).
- **A higher `max_ctor_arity`,** such as the cap of 32 (T1.11): 4% (GCC) and 11% (Clang) more instructions and 8% and 25% more memory at 100 components, for constructors that can be declared. Clang's fan-in 10 graph then takes 1.18× to 1.31× the hand-written compile time (NFR-04: 1.3×), depending on the machine's load.

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
- **2026-10-31, T1.3 (implementation):** the decision is implemented in `include/trellis/discovery/deduced.hpp`, with S1's form table and every limit in [docs/internals/deduction.md](../internals/deduction.md). Its amendments, marked provisional above, are for the owner to confirm:
  - **The gcc-13 workaround changed (item 6).** The probe's twins, declared on GCC < 14 only, replace S1's probe-only `operator T&&() const`. Measured on gcc-13.3: the four rvalue-reference forms are discovered and resolved, and every form and limit gets the same verdict as on gcc-14 and Clang, `optional<T>` and `tuple<T>` parameters and aggregate members included, which `operator T&&()` accepted. The twins' constraint names an internal tag type, so that the templates have valid specializations.
  - **Two aggregate checks (items 5 and 7).** An aggregate with an array member is refused with E004 on every compiler, as S1's addendum suggested, so the count no longer depends on the compiler; the fix, declaring one parameter per element, works everywhere because declarations are checked with braces. An aggregate member that no placeholder can initialize, such as `std::optional`, would have ended the deduced field list and left the later members value-initialized without a word; it is refused with E004 too.
  - **An abstract type is E002 from discovery, not E001 (item 5).** Discovery sees an abstract type only as the implementation of a binding, which can't be built; a missing binding for an abstract key is the engine's E001, reported before discovery is asked (T1.6). S1's stub reported E001 because it had no bindings to tell the two apart.
  - **An incomplete type is one E002 (item 5),** checked before anything that needs the definition, which otherwise gave 20 errors on Clang and 77 on GCC.
  - **E010 probes the 16 arities above the bound (item 5),** not *max* + 1 to 2·*max*, so the failure path costs the same for every bound, and a bound of 0 still finds a constructor just above it.
  - **The bound is at most 32 (item 5, plan §6.13).** The arity search is written out up to 32; a longer constructor is declared explicitly. The check is a plain `static_assert` without an ID, as plan §7.8 has none for a misconfigured option.
  - E004 and E010 are emitted as item 5 says. Same-arity overloads stay E002 with a hint, as item 5's table says: deduction can't tell them from a private or deleted constructor. Plan §7.2 and §7.4's E004 row amended to match; §7.4 had listed same-arity overloads under E004.
- **2026-11-01, platform fixes (provisional, for the owner to confirm):** the first macOS and Windows CI run since T1.3 failed to build `tests/unit/deduced_test.cpp` on cl 19.51. Two MSVC workarounds join item 6, and item 5's array row names the MSVC check. No decision changes: an aggregate with an array member is still refused with E004 on every compiler, and GCC and Clang compile exactly what they did before.
  - **`repeat_t` through a class (item 6).** cl lost the pack in `ctor<repeat_t<Is, probe_arg<T>>...>` over one index (C3546), so no one-parameter constructor could be deduced.
  - **The MSVC array check (items 5 and 6).** cl's parentheses refuse members of aggregate class type, so the parenthesized check took `agg`, `agg_defaults`, `agg_base`, `agg_nested`, `with_array` and `agg_tuple_last` for aggregates with arrays. With braces and the array conversion deleted, cl gives every aggregate in the tests the verdict GCC and Clang give it.
  - Measured on cl 19.51 only. The gates cover every MSVC from NFR-13's 19.38; cl 19.44 was measured only for what T1.13 covered, which includes the array-reference initialization the brace check relies on. Plan §7.2 amended.
- **2026-11-01, T1.11 (the bound measured):** `max_ctor_arity` at 8, 16 and 32, against NFR-04 and NFR-05, on T1.9's graphs at N = 100: `scripts/bench-compile.sh --quick`'s fan-in 1-3 graph, and the fan-in 10 graph, whose classes take up to 10 parameters ([docs/benchmarks/v0.1.md](../benchmarks/v0.1.md#max_ctor_arity)).
  - At 16 both budgets hold in both profiles: at most 1.20× the hand-written compile time (Clang, fan-in 10; NFR-04's 1.3×) and 1.11× its memory (NFR-05's 1.5×).
  - 32 adds 16 failed probes to every deduced class: 3.9% more instructions with GCC and 11.1% with Clang on the fan-in 1-3 graph, and 27 MB and 69 MB more memory. With Clang, the fan-in 10 graph then took 1.31× the hand-written compile time in rounds at a load of up to 4, within their 10% variation of NFR-04's 1.3×, and 1.18× to 1.22× in two quiet rounds of T1.11's review; its memory, 1.29×, doesn't depend on the load.
  - 8 saves 1.3% and 3.2% of the instructions, and refuses every deduced constructor of 9 to 16 parameters with TRL-E010, the fan-in 10 graph's among them.
  - So the default stays 16, as item 5 decided. No decision changes.
- **2026-11-01, Phase 1 gate:** the Status line names all three provisional amendments; it named only T1.3's, though the T1.2 and platform-fix entries above say theirs await the owner too. The plan's markers for T1.2's and T1.3's amendments now say provisional as well. No decision changes.
- **2026-11-01, T1.15 (the binding's declaration):** item 1's third source is in the binding DSL: `bind<I>.to<C>().with_ctor<Args...>()`, `bind<C>.with_ctor<Args...>()`, and the same on `override<K>`. It wins over the class's specialization and member alias and over deduction, for its binding only, as T1.2 ranked it. The binding carries it in the implementation's place, `detail::declared_impl<C, ctor<Args...>>`, so that a binding stays `binding<Key, Impl, Lifetime, AllowCopy>` and one without a declaration keeps its type and its spelling in diagnostics (a fifth template parameter changed both; [bindings-and-modules.md](../internals/bindings-and-modules.md#a-constructor-declared-on-the-binding)). `create<T>()` passes it to discovery as `BindingCtor`, on the normal pass and in the explain pass, which checks it by item 1's construction rule where the injector uses it and reports a mismatch as one TRL-E002 naming the binding's declaration. Two readings, for the owner to confirm: `.to<C>()` after `.with_ctor<Args...>()` moves the declaration to `C`, since a declaration is for the class the binding builds; and a second `.with_ctor` is an error, as a second `.to<C>()` is. No decision changes.
- **2026-11-01, T1.15 follow-up (partial aggregate declarations):** a declaration that lists fewer members than its aggregate has, which item 1's brace rule allows, made GCC and Clang warn about each member left out without a default member initializer (`-Wmissing-field-initializers`, part of `-Wextra`) in the creation code, for all three sources: an error under `-Werror`, against NFR-10. No construction keeps the meaning of braces without the warning (parentheses warn on GCC too, and would accept narrowing and refuse brace elision), so a pragma pair in `config.hpp`, `TRELLIS_PARTIAL_AGGREGATE_INIT_BEGIN` and `_END`, switches that one warning off around that construction, on GCC and Clang, as item 6's `-Wconversion` pair does for deduction; every other warning stays, Clang's `-Wdangling-field` included (`tests/warnings/`). Such a pragma doesn't reach an importer of the C++20 module built by Clang before 20 (clang-18 warns there, clang-20 doesn't), so in that build only, the construction's header, `include/trellis/static/declared_aggregate.hpp`, is a system header (`TRELLIS_MODULE_SYSTEM_HEADER`; Apple Clang is treated as before 20). No decision changes, but one residual cost, for the owner: in the module built by Clang before 20, no warning located in that header reaches importers, `-Wdangling-field` for a reference member bound to a temporary included. Tested by `tests/unit/declared_aggregate_test.cpp`, through the headers, the single header and the module, `tests/warnings/` and `single_header.main_file`.
- **2026-11-01, the Apple Clang fix (provisional, awaiting owner review):** two refinements of item 5 for deduced aggregates. `TRELLIS_AGGREGATE_CHECKS_WITH_BRACES` (`config.hpp`) keeps braces on MSVC, whose parenthesized aggregate initialization fails for members of aggregate class type.
  - **The greedy check takes the inert arguments in parentheses,** which never elide braces. With braces, an inert argument that converted to no member of aggregate class type was tried on that member's first member, and so on down. So an aggregate holding an aggregate that holds a member that accepts anything was itself TRL-E004. Now E004 names the inner aggregate, whose own deduction is greedy, when a request reaches it: `holds_greedy_aggregate` in `tests/unit/deduced_test.cpp` was greedy before and is now deduced, with one member. The elision also nested one level of the frontend's stack per aggregate below, and that is what stopped Apple Clang 21's chain of 200 aggregates (platforms runs 41338913950 and 41339845579: 751 hops without the greedy check, 195 with it). With the greedy check and the node in parentheses, the chain reaches 213 hops there (run 41342943535); parentheses cost Apple Clang nearly as much as the elision did.
  - **The engine checks a deduced aggregate's members with parentheses,** in its check and in the explain pass, and builds it with braces, which then initialize each member directly. With braces, Clang took the deleted twin that a member's failing request selects for no conversion, and elided braces into the member, building it from its own members' requests. So a member that deduction refuses (TRL-E004), or a singleton held by value without `.allow_copy()` (TRL-E005), was built anyway, and its error lost. GCC reported the error, as [dcl.init.aggr] reads since CWG2149, adopted as a defect report by P3106R1: an initializer-clause stays with its element when an implicit conversion sequence to the element can be formed, and only otherwise are braces elided. The conversion through the deleted function is such a sequence; it makes the initialization ill-formed ([dcl.fct.def.delete]), and is no reason to elide. Now every compiler reports the member's own error, through the aggregate's path: `tests/compile_fail/explain/e004_aggregate_member_two_roots.cpp`, and "members whose request fails" in `tests/unit/create_test.cpp`.
