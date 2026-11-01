# ADR-0006: The diagnostics contract

- **Status:** Accepted (owner, 2026-10-30)
- **Plan:** [§4](../plan.md#4-architecture-decisions) ADR-06; §3.2 NFR-07; §7.8; R3, R4
- **Spikes:** [S3 diagnostics rendering](../spikes/S3-diagnostics.md), whose recommendation the owner accepted with NFR-07 reworded. S1 and S2 add findings about deduced mode.
- **Amends:** plan §3.2 (NFR-07) and §7.8 (the example's literal; what each compiler prints; when the C++26 message is used; from T1.5's review, the provisional catalog rows E013 and E014).

## Context

Three of the six frameworks the research measured compiled a missing binding without complaint, and Boost.DI under GCC only warns, then fails at link time (R3). Where errors exist, they are long and name little: for a missing `IRepository` binding, Fruit prints 22–30 lines with 4 errors, and qlibs/di 64–81 lines (S3 §3.3). Trellis wants every statically detectable mistake to be one hard error that names the type, the requested form, the path from the root and a fix (R4), within NFR-07's budget.

Spike S3 built a mock pipeline that follows plan §7.3 and §7.8, and compared nine renderings of E001, E005 and E012, at the root and three hops deep, on gcc-13, gcc-14, clang-18 and clang-20, in C++20 and C++26 modes: 240 compiles (S3 §2).

## Decision

1. **The contract:**
   - exactly one hard error per root cause;
   - the error names the error ID, the type, the requested form, the path from the root and a suggested fix;
   - the IDs (`TRL-E0xx`, `TRL-R0xx`, plan §7.8) are stable and documented in an error catalog;
   - output is golden-tested on every compiler;
   - no deprecation, undefined-function or link-time tricks.
2. **The mechanism, in every mode** (S3 §3.2, §4.1). `create<T>()` is `if constexpr (resolvable<...>) { creation code } else { explain pass; one static_assert; [[noreturn]] call; }`:
   - the creation code of a failing graph is never instantiated, so nothing cascades;
   - the `[[noreturn]]` call stands in for a return value, so it needs no value of `T`, which may be abstract or a reference, and it stops GCC's `-Wreturn-type`;
   - there is one `static_assert` per error kind, each in its own `if constexpr` branch with its own literal, written inline in `create`; a helper function or a macro adds 1–3 lines of "required from" or "in expansion of macro" on GCC;
   - the injector's type stays short, so GCC's instantiation banner stays one line (S3 §3.2; ADR-0010).

   Measured on all four compilers: 1 error, 0 warnings and 7–12 lines, under 1 KB at depth 3 and under 1.9 KB at depth 100 (S3 §3.1, §3.6).
3. **C++20 rendering** (S3 §4.2): `static_assert(error::E00x<T, request<R>, path<...>>::reported, "TRL-E00x: <summary>")`. Clang puts the condition, with the type, the request and the path, on the first error line. GCC puts the literal, which carries the ID, on the error line, and the condition in the attached note. So both parts carry information.
4. **C++26 rendering** (S3 §4.3): a P2741 message of four lines: the ID and the type; the requested form, the consumer and the parameter; the path; the fix. It is built at compile time into a `fixed_string` of exactly the right size, with one pass that counts characters and one that fills them in (S3 §3.5).
   - It is used when `__cpp_static_assert >= 202306L` in C++26 mode, **and also in C++20 and C++23 modes on GCC 14 or later and Clang 20 or later,** where it works as an extension. GCC 13 and Clang 18 reject it there, and Clang 19 wasn't tested, so it stays off for them.
   - `TRELLIS_HAS_STATIC_ASSERT_MESSAGE` in `config.hpp` gates it on compiler versions and `__cplusplus`, not on feature-test macros alone: clang-20 defines `__cpp_deleted_function` at `-std=c++20` and then warns when the feature is used. `#pragma GCC diagnostic ignored "-Wc++26-extensions"` surrounds the one assertion.
   - This puts the type on the first error line for gcc-14 and clang-20 in C++20 mode too.
5. **The explain pass** (S3 §3.5) runs only after a failure. It reads the memoized result of each parameter on the failing path, descends into the first failing one with the path extended and the 1-based parameter index, and classifies the deepest failure into an ID.
   - It makes depth + 1 instantiations and one read per parameter on the path, so it is O(depth × arity): 101 hops and 301 reads at N = 1,000 and depth 100.
   - It costs at most 5% over a build without it, which is within noise, and a failing build is 2.5–5 times cheaper than a passing one, because no creation code is instantiated.
   - It keeps the output's size bounded by the path, at about 8 bytes per hop, instead of by the number of bindings (S3 §3.6).
6. **Long paths** are elided in the middle beyond about 8 hops, in the type and in the message, for example `path<App, A, B, elided<94>, X, Y, Z>` (S3 §4.5). This is Phase 4 work.
7. **`= delete("reason")`** (P2573) is fine for misuse whose reason is fixed text, such as copying an injector, in C++26 builds. It isn't the channel for wiring errors (S3 §4.4).
8. **NFR-07 is reworded by the owner:** "the first error line names the error ID; the same diagnostic names the type". S3 §4 found that the original wording ("the first error line names the type and the error ID") holds on Clang in every mode and on gcc-14 with the message, but not on gcc-13 in C++20 mode. There GCC prints only the literal, which carries the ID, on the error line, and the type follows in the attached note, the last line of a 7-line error. The 40-line budget holds with a wide margin: at most 12 lines with the recommended renderings, for every case, at depth 3 and at depth 100.
9. **Golden tests** use S3's normalization rules (S3 §3.7) plus `/` path separators, as `tools/golden.py` implements them since T0.10, which the owner approved. Line and byte budgets are measured on the default output that users see. Goldens compare the normalized text and are kept per compiler, because spellings differ (S3 §3.4).

### MSVC risk (S3 §3.8: high for the C++20 rendering, untested)

- MSVC prints `error C2338: static_assert failed: '<message>'` without the condition, so the error type, and with it the type and the path, would appear only in its "see reference to ... being compiled" notes.
- Mitigations to try in CI early in Phase 4: on MSVC, put the assertion inside a function or class template whose argument is the error type, so that the first instantiation note names it; or use P2741 once MSVC ships it (no shipped version is known).
- `__FUNCSIG__` names carry `struct ` and `class ` prefixes, so `type_name` needs one prefix-stripping pass on MSVC.
- `if constexpr` and `[[noreturn]]` are portable, so the one-error mechanism itself is low-risk.

## Alternatives

Renderings S3 measured (S3 §3.2, §4.4). Lines are for E001 three hops deep, on gcc-13 / gcc-14 / clang-18 / clang-20.

- **naive** (`static_assert(resolvable<...>)`, with the creation code still instantiated): 11 / 13 / 65 / 65 lines. Clang cascades into libstdc++'s `make_shared` with 3 errors, and nothing names the ID or the type.
- **leaf** (assert at the failing node inside the creation code): the compiler's backtrace is the path, so output grows by 2–6 lines per hop and passes 40 lines at about 5 hops on gcc-14 and Clang.
- **guard** (an `if constexpr` guard without the explain pass): names `resolvable<bindings<...>, App, unique>` instead of the cause, and prints the binding map, so bytes grow with the number of bindings: 63 KB on GCC and 36.9 KB on Clang at N = 1,000.
- **incomplete** (naming a member of an undefined `error::incomplete<Err>`): poor wording ("incomplete type ... used in nested name specifier") and a second error on clang-18.
- **A deleted `create` overload as the main channel:** on Clang the type lands in a note among "candidate template ignored" notes; P2573 reasons must be literals, so they can't name the type; and only Clang 19+ and GCC 15+ support them.
- **The deleted-overload rendering on GCC 13 only,** to keep NFR-07's original wording: a second code path to fix one compiler version that is going out of use.
- **ADR-06 as first drafted** (the message only where `__cpp_static_assert >= 202306L`): gcc-14 would show the type only in a note in C++20 mode.

## Consequences

- **T1.6 implements this** for E001–E005, E009, E010 and E012 in C++20 form. `TRELLIS_HAS_STATIC_ASSERT_MESSAGE` currently requires C++26 mode (T0.1), and T1.6 extends it with the compiler-version gate in item 4. That answers T0.1's open question about messages before C++26. T1.6's acceptance text predates the rewording of NFR-07; under the reworded target, gcc-13 in C++20 mode meets it through the attached note.
- **Errors are reported once per failing `create<>` root,** not once per root cause across roots: two roots that fail on the same missing binding give two errors (S3 §5).
- **Deduced mode limits the explain pass.** The pass needs each class's parameter list, which deduced mode can't enumerate (S2 §5). There it can name the consumer whose construction failed, but not the parameter; the deleted twin names the request in the compiler's note (S1 §3.1). S1 §5 asks that it still report E005 with the parameter index rather than a generic E002; T1.6 works out how.
- **Where the error vocabulary lives (decided by the owner, 2026-10-31):** the compile-error vocabulary (`error::E00x`, `request`, `path`) lives in namespace `trellis::error`, outside `inline namespace v1`. GCC spells inline namespaces out (`trellis::v1::unique`) and Clang hides them, so this keeps names short and the same on both (S3 §3.2, §4.2). It is the one documented exception to the project's namespace rule. These types are versioned by error ID instead: an ID never changes meaning. Everything else stays in `inline namespace v1`, so two major versions can still coexist (plan §7.5).
- **A dependency cycle in deduced mode is an exception to "one error per root cause"** under ADR-0014: by default it ends in the compiler's own 2–6 errors, which the error catalog documents, and `options{.cycle_check = true}` gives exactly one TRL-E007.
- **A C++26 build costs a roughly fixed 0.3–0.4 s more** than a C++20 one at both depths, mostly from the language mode and its headers rather than from rendering the message (S3 §3.5).
- **Diagnostics come earlier:** the owner ordered Phase 4 (diagnostics) before Phase 3 (binding features) (plan §12.2), so the full catalog, the C++26 messages, path elision, golden tests on every compiler and the usability check follow Phase 2.
- **Not prototyped** (S3 §5): E002, E003, E006, E007 and cycles, and more than two lifetimes. GCC 15's SARIF output (`-fdiagnostics-add-output`) may suit golden tests better than text; it wasn't available.

## History

- **2026-10-30, plan draft 1:** drafted as ADR-06 in plan §4, "confirm in S3".
- **2026-10-30, spike S3 (T0.5):** recommended the mechanism and renderings above, the compiler-version gate for P2741 messages and a rewording of NFR-07.
- **2026-10-30, owner:** accepted S3's rendering and reworded NFR-07, and approved the golden normalization, which T0.10 implemented the same day (docs/PROGRESS.md, "Owner decisions").
- **2026-10-31, T0.8:** recorded in this file. Plan §3.2 (NFR-07) and §7.8 amended.
- **2026-10-31, T0.8 review:** keeping the error vocabulary outside `inline namespace v1` moved from the Decision to an open point for T1.6, because it conflicts with the project's namespace rule (CLAUDE.md, plan §7.5). The example in plan §7.8 gained its literal.
- **2026-10-31, owner decision:** the error vocabulary lives in `trellis::error`, outside `inline namespace v1`, as a documented exception to the namespace rule.
- **2026-10-31, T1.1 review:** the runtime error value of `result<T>` (ADR-0009) is `trellis::failure`, because `trellis::error` is the compile-error namespace: a class of that name in `inline namespace v1` makes every use of `trellis::error` ambiguous on GCC, and Clang rejects the namespace outright. Plan §7.1 amended; `tests/unit/error_namespace_test.cpp` guards the name.
- **2026-11-01, T1.5:** `create<T>()` follows item 2: `if constexpr (resolvable) { creation code } else { failure pass }`, so a failing graph instantiates no creation code. The failure pass is minimal, as T1.6 owns the explain pass of item 5; details in [docs/internals/resolution-and-storage.md](../internals/resolution-and-storage.md):
  - It descends along every failing request and reports the cause where it finds it, one `static_assert` per failing request, each in a function body instantiated once per request, so a request that several consumers make is one error. It names a deduced class's failing parameter, which the Consequences above say deduction can't list, by building the class from placeholders with the probe's operator shapes in a function that is instantiated but never called: the operator that overload resolution selects reports its request.
  - The assertion fires inside the descent, so the compiler's instantiation notes trace the path, within the default backtrace limit of 10: 26–31 lines one hop down, up to 41 three hops down (Clang 18). The path in the error type, and NFR-07's 40 lines everywhere, are T1.6's.
  - TRL-E001 is raised for an abstract type without a binding, and for one bound to itself with `bind<I>` alone, with or without a constructor declaration (T1.2's follow-up: discovery's TRL-E002 for an abstract class with a declaration stays for discovery's own entry points).
  - TRL-E002 also covers a by-value request whose copy can't be made, with `E002<Key, request<R>>`.
- **2026-11-01, T1.5 review:** two mistakes that plan §7.8's catalog didn't cover get the next free numbers, as an ID never changes meaning: TRL-E013, an unbound concrete type requested in strict mode (an owning request that the implicit unique binding would otherwise serve), rather than widening TRL-E001, which keeps "no binding for an abstract type"; and TRL-E014, a `std::unique_ptr<I>` request that would delete the binding's implementation through `I`'s non-virtual destructor (plan §6.3, amended by ADR-0004), rather than stretching TRL-E005, which means "incompatible with the lifetime". Both are provisional and await the owner's confirmation (plan §7.8 amended; `[NEEDS CLARIFICATION]` in the progress log). The review also made `create<T>()` raise exactly one error where it raised two: for a by-value request of an abstract class, whose type a function can't return, `create<T>()` returns `void`; for a forward-declared type, which the check no longer asks `std::is_abstract` about; and for a parameter `std::unique_ptr<X>` of an incomplete `X`, whose destructor the failure pass no longer instantiates (docs/internals/resolution-and-storage.md).
