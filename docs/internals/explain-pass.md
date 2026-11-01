# The explain pass and the reports

How Trellis reports a graph that can't be built (T1.6): the explain pass, which follows the first failing request from the root to its cause, and the one function that holds every TRL `static_assert`. Headers: [`include/trellis/diagnostics/explain.hpp`](../../include/trellis/diagnostics/explain.hpp) (the pass) and [`include/trellis/diagnostics/errors.hpp`](../../include/trellis/diagnostics/errors.hpp) (the error types, `raise`). Decisions: [ADR-0006](../adr/0006-diagnostics-contract.md) (the contract, as amended by T1.6), plan [§7.8](../plan.md#78-diagnostics-engine); the design follows spike [S3](../spikes/S3-diagnostics.md), with the changes below.

`create<R>()` is `if constexpr (resolvable) { creation code } else { explain pass }` ([resolution-and-storage.md](resolution-and-storage.md)). The second branch is:

```cpp
detail::explain_root_t<injector, R>::run();   // the first step of the explain pass
detail::unreachable();
```

## One report per root cause: `raise`

**What it does.** Every check that finds a TRL mistake, `create<T>()`'s explain pass, discovery (`checked_ctor`, `discover`), T1.4's request check (`report_request_form`), `make_injector` (TRL-E003) and the override check (TRL-E009), calls one function template with the error type, the message and the scope of the report:

```cpp
template <class Error, error_message Message, class Scope = void>
constexpr void raise() noexcept {
  using args = error_args<Error>;
  if constexpr (Message == error_message::no_binding) {
    static_assert(trellis::error::E001<typename args::first, typename args::second>::reported,
                  "TRL-E001: no binding for an abstract type: ...");
  } else if constexpr (...)
}
```

Each message has its own branch and literal (ADR-0006 item 2). Scope is the injector for the mistakes `create<T>()` finds, and `void` for discovery's and `make_injector`'s, which are unique to a class or to a set of bindings.

**Why.**

- *Once per root cause.* A function template's body is instantiated once per specialization, so a report is raised once however many roots, consumers or entry points reach it: two roots that need the same missing request are one error, and so are two injector types that repeat the same two bindings. A `static_assert` in a class template would not do: Clang marks a class whose assertion failed as invalid, and every later use is an error of its own (T1.2's lesson). The condition can't name the path from the root for the same reason: a path is different for each root, so a condition that named it would make one error per root, as the S3 spike's design did (ADR-0006, amended).
- *The error type for MSVC.* MSVC prints a failed `static_assert`'s message, not its condition (ADR-0006, MSVC risk). Its first instantiation note names `raise<trellis::error::E0xx<...>, ...>`, so the error type appears there. `tools/compile_fail.py` searches every diagnostic line with each `expect-regex`, so a test whose regex on the error type has no `, ` (MSVC writes template arguments without the space) matches there; untested locally, as MSVC runs only in CI.
- *The type spelled out.* The condition spells the error template out with Error's arguments (`error_args`), rather than `Error::reported`: Clang prints a nested-name-specifier without its scope, so `Error::reported` would print as `E012<...>::reported`, while the spelled-out type prints as `trellis::error::E012<...>`, as GCC prints it (checked on Clang 18 and 20).
- *Cost.* One instantiation note more than an assertion written in the check (S3 measured 1 to 3 lines on GCC); the explain pass more than saves it, below. T1.2's and T1.3's own tests, which drive discovery's entry points directly, grew by 3 to 4 lines (19 of their 20 on gcc-14).

**Tests:** every compile-fail test that raises a TRL ID; `tests/unit/explain_test.cpp`, `ErrorArgumentsAreReadBack`, `RequestFormMessagesFollowTheTable`, `DeductionMessagesFollowTheStatus`.

## The steps

**What it does.** The pass starts at the root request and follows the first failing request down, one class at a time, building `trellis::error::path<Consumers...>`, the classes whose constructors lead to the failure, root first:

| At | The pass | Instantiation notes |
|---|---|---|
| a request whose own plan fails (no binding, strict mode, an unsafe delete, the request-form table, a copy, discovery) | runs `explain_cause<Path, Parameter, R, Injector>::run()`, which raises the request's report (`own_report`): the check's own, so discovery's report from `create<T>()` and from discovery's entry points is one report | one: `explain_cause`, whose note names the path and the parameter |
| a class with a declared constructor, `ctor<Args...>` | reads `Ctx::can_resolve<Args>...`, the checks the normal pass memoized, and continues with the first that fails | none: class templates (`explain_stop`) that finish before anything is reported |
| a class whose constructor deduction found | finds the first failing parameter and builds the class from placeholders, inside the operator that met its request | one: that operator, from one line of `explain.hpp` |
| a class already on the path, or whose constructor requests one: a dependency cycle | stops, and reports nothing (`explain_cycle`) | none |

*Deduced classes.* Deduction can't list a constructor's parameter types (ADR-0006, "Deduced mode limits the explain pass"), and no type-level trick names them without stateful metaprogramming. The pass gets around it in three parts:

1. `failing_parameter<Node>` probes the class once per parameter, with the resolving placeholder `any_arg` at one position and discovery's probe at the others:

   ```cpp
   TRELLIS_IS_CONSTRUCTIBLE(Impl, std::conditional_t<Is == K, any_arg<Impl, Ctx>, probe_arg<Impl>>...)
   ```

   Both placeholders are viable for any parameter (`any_arg` through its deleted twins), so overload resolution selects discovery's constructor and the same operator at every position; only the operator at K can be deleted, exactly when its request can't be served. The first K for which the class isn't constructible is the first failing parameter. A failing class always has one: with every parameter resolvable, the constructor discovery found is viable and still the best.
2. The class is built, in a function that is instantiated and never called, from an `explain_arg` at that position and `explain_filler`s, inert placeholders with the probe's shapes, at the others: the first deduced class by `explain_build<...>::run()`, from `create<T>()`, and every later one by the body of the `explain_arg` operator that met the request leading to it. Overload resolution selects the operator that discovery's probe selects, with the request that parameter makes, and instantiates its body, which asks `Ctx::can_resolve` for that request, as `any_arg`'s constraint does, and, as it fails, runs the stop's `explain_cause` or builds the stop's deduced class itself. (The memoized check is what lets `tests/unit/resolve_forms_test.cpp` see which request each operator deduced.)
3. To build the next class in its body, the operator needs that class's arity as a pack before it knows the class: only overload resolution finds the request. The lookahead finds it one class ahead, when the placeholder is made: `next_indices_t` probes the class whose placeholder it is with `query_arg` at the placeholder's position, whose operators are deleted unless one bit of the stop code holds for their request (0 for a stop that reports, else 1 + the arity of the class to build): six probes, one per bit, as `failing_parameter` probes. The placeholder carries the result, `std::make_index_sequence<arity>`, as its last template argument.

*One note per class, folded.* Every deduced class after the first is built by the same construction in `explain_arg`, one per operator shape (`T` and `T&`) and one for an aggregate, so a chain of them instantiates its operators from one line of `explain.hpp`. Clang prints the source line once for a run of notes at one location, and GCC folds the run into one "recursively required from" line that names the outermost operator of the run:

```text
explain.hpp:L:C:   recursively required from 'trellis::v1::detail::explain_arg<Path, Parameter, Ctx, std::integer_sequence<long unsigned int, __indices ...> >::operator T() [with T = app::c2; Path = trellis::error::path<app::c0, app::c1>; ...]'
```

That fold drops the innermost operator's note, so the report goes through `explain_cause`, whose note names the path and the parameter on every compiler: Clang's second note below the error, the first "required from" line on GCC. Clang 18 on `explain/e013_strict_two_roots.cpp`, where `car`'s parameter 1 is `engine&`, a singleton with a declared constructor, which prints no note, and the failing request is `engine`'s parameter 1 (`gear`, unbound in strict mode):

```text
note: in instantiation of member function 'trellis::detail::explain_cause<trellis::error::path<app::car, app::engine>, 1, app::gear, trellis::injector<...>>::run' requested here
```

*Dependency cycles.* Under memoization the normal pass reports a cycle with the compiler's own errors: the check of a request on the cycle re-enters itself, and its memoized value, `Ctx::can_resolve`, can't be read as a constant afterwards, every read one more error (ADR-0014 item 2). The pass stops at a class that is already on the path (`on_path`), and at a class whose constructor requests one (`cycles_back`, one probe with `cycle_arg`, whose operators ask only which class a request's node builds; `declared_cycles_back` for a declared constructor), before it reads that class's checks, and each operator checks its stop for a cycle before it reads its own request's check. A request counts only when its node is a deduced or declared class node (`request_class_t`), as only then did the normal pass's check recurse into the class; `cycle_arg` asks it of both forms, `T` and `T&`, as overload resolution checks the constraints of both operators for a parameter by value. A request that its own plan rejects first, such as `std::unique_ptr` of a singleton above, never recursed, so the compiler reported no cycle, and the pass goes on and reports it: T1.6's first cycle stop counted any request of a class on the path, and such a graph compiled without an error and aborted at run time (T1.6 recheck; `explain/e005_unique_ptr_back_to_path.cpp`, `e005_declared_unique_ptr_back_to_path.cpp`, `eager_e005_back_to_path.cpp`, and `cycle_with_rejected_request_back.cpp`, where a real cycle beside such a request still stops the pass). So a deduced cycle gets the normal pass's errors and nothing more: 3 on Clang 18 and 20 and 1 on GCC 13 and 14 by reference (`explain/cycle_by_reference.cpp`, `cycle_eager.cpp`), 4 and 3 by value (`cycle_by_value.cpp`), within ADR-0014's 2 to 6. T1.6's first explain pass followed a cycle round and round, as the path made every step a new specialization: 20 errors and Clang's "too many errors emitted". A cycle of declared constructors gives 7 errors on Clang, one fewer than T1.5's failure pass: its root's own check is read once more by `create<T>()`, as before, until the explicit mode's cycle check (ADR-0014).

*An owner of an incomplete class.* A deduced class that takes `std::unique_ptr<X>` by value, with `X` only declared, can't be built even from placeholders: the parameter's destructor asserts that `X` is complete, a second error beside the request's own, discovery's TRL-E002 for `X` (found in T1.5's review). Such a parameter always fails, so the pass reports it rather than the first failing parameter (`owner_parameter`, one probe for the class and one per parameter only when it has such a parameter), and builds the class only in an unevaluated requires-expression, where nothing is destroyed, with an `owner_reporter` at that position: the constraint of its by-value operator raises the report, and its note names the path. The class's other failing parameters are reported once `X` is complete.

*Eager mode.* `make_injector` with `options{.eager = true}` checks every singleton first (`eager_fails`, `static/injector.hpp`) and, if one can't be built, runs the explain pass for the first, in binding order, before the injector exists. So `make_injector` is the only frame outside the pass, as `create<T>()` is for a request, and the injector's constructor only ever builds.

**Why.** S3's mock had hand-written parameter lists, so its pass was all class templates and the `static_assert` fired from `create<T>()` itself: 7 to 12 lines at any depth. Deduced classes make the pass instantiate functions, whose notes are part of the error. T1.5's failure pass descended through every failing request with three functions per class and reached 41 lines three classes down on Clang; T1.6's first pass had two per class, and reached the compilers' backtrace limit of ten contexts five classes down, at 37 lines on Clang and 39 on GCC 14 with the user's call on one line, more with each level of include and each line of the call (T1.6 review). With one note per class from one location, a chain of deduced classes costs the error a few lines whatever its length.

Measured on gcc-13, gcc-14, clang-18 and clang-20 (lines):

| Failure | gcc-13 | gcc-14 | clang-18, clang-20 |
|---|---|---|---|
| at the root (`create/e005_root_reference_to_unique.cpp`) | 10 | 16 | 17 |
| one deduced class down (`create/e012_hello_world_without_service.cpp`) | 12 | 22 | 21 |
| two deduced classes down (`explain/e012_three_forms_one_root.cpp`) | 13 | 25 | 24 |
| any depth through declared classes only | 10 | 16 | 15 |
| 14 classes, deduced and declared (`explain/deep_chain_through_declared.cpp`) | 13 | 23 | 29 |
| 21 deduced classes in an application header, the call over three lines (`explain/deep_chain_call_over_three_lines.cpp`) | 14 | 26 | 30 |
| the same behind four application headers (`explain/deep_chain_four_headers.cpp`), and with the call over three lines too | 17 | 27, 29 | 33 |
| 20 deduced classes whose requests alternate between by value and by reference, the header included directly (`explain/alternating_chain_one_header.cpp`), and with the call over three lines | 20 | 40, 42 | 38 |
| the same behind four headers (`explain/alternating_chain_four_headers.cpp`), and with the call over three lines too | 23 | 43, 45 | 41 |
| the same with aggregates mixed in, behind four headers with the call over three lines (`explain/alternating_aggregates_four_headers_call_over_three_lines.cpp`) | 23 | 47 | 43 |

NFR-07's count leaves out the lines that the user's own layout produces, the include chain and the excerpt of the user's call site (owner decision 1A; `expect-max-lines-nfr07` in `tools/compile_fail.py`, checked with GCC and Clang), so it is the same in every layout: 11 lines on GCC 13, 19 on GCC 14 and 25 on Clang for the 21-class chain, 17, 35 and 33 for the alternating one, and 17, 37 and 35 with aggregates mixed in.

*What still grows.* The folding needs consecutive notes at one location: a run of requests of one shape, `T` (by value, pointer, `std::unique_ptr` and `std::shared_ptr`) or `T&` (references), into classes of one kind (aggregate or not). A chain whose requests alternate between the shapes puts its notes on two lines in turn, which neither compiler folds, and reaches the backtrace limit about six classes down: GCC 14 then prints about 36 lines plus one per level of include above `<trellis/trellis.hpp>` plus two per extra line of the call that starts the chain, and Clang about 35 plus one per level of include (last rows; with aggregates in the mix as well, up to 48 lines on GCC 14 and 43 on Clang). The operator shapes can't share a line: each needs its own conversion function template, and a common function for the construction would be a second note per class at a second location, which breaks every run. The owner settled it with NFR-07's measurement conditions (decision 1A, 2026-11-01, plan §3.2): the 40 lines don't count the include chain and the excerpt of the user's call site, which leaves such a chain 33 lines on Clang and 35 on GCC 14 in every layout, and the four `explain/alternating_chain_*.cpp` tests check it with `expect-max-lines-nfr07: 40`. With aggregates in the mix it is 35 and 37, which `explain/alternating_aggregates_four_headers_call_over_three_lines.cpp` checks in the worst layout.

*Which root's path.* When several roots fail on one request, the error shows the path of the root whose explain pass the compiler instantiates first: Clang goes depth first, in source order, and GCC instantiates the shorter chain first.

*Depth.* A failing chain of 250 deduced classes on Clang 18 and 290 on GCC 13, the longest the normal pass compiles at the default limits ([resolution-and-storage.md](resolution-and-storage.md)), still gives the one report: the pass nests one function per deduced class, fewer levels than the check.

**Cost.** Only after a failure: one step per class on the failing path, a memoized read (declared) or one probe (deduced) per parameter of each class on it, and seven probes per deduced class for the cycle check and the lookahead: O(depth × arity), as plan §7.8 says. A graph that compiles instantiates none of it; `eager_fails` is false without looking at a binding for an injector that isn't eager, and for an eager one reads the `can_resolve<Key&>` that its constructor's `build_eagerly` asks anyway. Compile times of passing programs with and without T1.6 are in the T1.6 progress entry.

**Tests:** `tests/compile_fail/explain/`: one test per ID, each failing twice with one error, its path and NFR-07's 40 lines; `deep_chain_through_declared.cpp`, `eager_singleton_eight_down.cpp` and the three layout tests `deep_chain_call_over_three_lines.cpp`, `deep_chain_four_headers.cpp` and `deep_chain_four_headers_call_over_three_lines.cpp`, and the four of the alternating chain, `alternating_chain_one_header.cpp`, `alternating_chain_call_over_three_lines.cpp`, `alternating_chain_four_headers.cpp` and `alternating_chain_four_headers_call_over_three_lines.cpp`, which check NFR-07's count, as does `alternating_aggregates_four_headers_call_over_three_lines.cpp` with aggregates mixed in (headers in `explain/layout/`); `e002_incomplete_owner_below.cpp` for an owner of an incomplete class; `cycle_by_reference.cpp`, `cycle_by_value.cpp`, `cycle_eager.cpp` and `cycle_with_rejected_request_back.cpp`, at most six errors and no recursion limit; the three `*_back_to_path.cpp` tests, where a request back to the path that the table rejects is TRL-E005, once; goldens for the ID tests, the two deep chains and the five alternating chains. `tests/unit/explain_test.cpp`: `FailingParameterIsTheFirstThatCantBeServed`, `RootFailingOnItsOwnStopsAtTheRoot`, `DeducedClassIsBuiltWithItsFailingParameter`, `DeclaredClassesAreFollowedWithoutAStepOfTheirOwn`, `LookaheadGivesTheArityOfTheNextClass`, `ClassOnThePathStopsThePass`, `OnlyARequestThatBuildsAClassOnThePathIsACycle`, `RequestThatCanBeServedGivesAHarmlessStop`, `OwnReportsNameTheErrorTypeAndScope`, `OwnerOfAnIncompleteClassIsReportedFirst` and `EagerModeChecksEverySingletonInMakeInjector`. `tests/unit/resolve_forms_test.cpp`, `FailurePassAgrees` (each `explain_arg` operator deduces what `any_arg` deduces, for the 32 deducible forms). Every test in `tests/compile_fail/create/`.

## Choices

- **First failing parameter only** (plan §7.8, ADR-0006 item 5). A root reports one cause, so three forms of one missing binding under one root (`sensor&`, `const sensor&`, `sensor*`) are one error; a second cause appears once the first is fixed. Two roots that fail on different forms of one key are two requests and two errors.
- **The path in a note, not in the condition** (ADR-0006, amended): see `raise` above. The note is `explain_cause`'s (or `owner_reporter`'s), Clang's second below the error and GCC's first "required from" line.
- **TRL-E009** has an internal entry point, `report_override_target<Bindings, Override>()` in `static/validate.hpp`, which `make_injector` calls for each `trellis::override<K>` from T1.7 (its name and signature stay as they are for that); its test drives it directly, as T1.4's tests drove the request check before `create<T>()` existed.

## Open points

- Phase 4: path elision beyond about 8 classes (ADR-0006 item 6), the C++26 message with the path and the parameter (item 4; `TRELLIS_HAS_STATIC_ASSERT_MESSAGE` still requires C++26 mode), and the MSVC results of the mitigation above.
