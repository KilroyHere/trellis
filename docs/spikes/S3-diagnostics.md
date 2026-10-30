# Spike S3: diagnostics rendering

Task T0.5. Code, logs and generated tables are in `spikes/S3-diagnostics/`. `bash spikes/S3-diagnostics/run.sh` rebuilds every number here; it writes `spikes/S3-diagnostics/results.md` (all 240 matrix cells and every check) and exits non-zero if a check fails. **The recommendation waits for the owner's sign-off.**

## 1. Question

From plan §12.1: E001 and E005 as C++20 error types and as C++26 messages, on three compilers, and the output size. This decides ADR-06, and the spike is done when the golden outputs are within budget.

Put more sharply: how should Trellis render a wiring error so that each root cause produces exactly one short, readable error that names the error ID, the type, the requested form and the path from the root? And can NFR-07 be met, with the first error line naming the type and the ID and the output staying at or under 40 lines on GCC and Clang?

## 2. Method

- **Compilers** (Ubuntu 24.04 packages): gcc-13 13.3.0, gcc-14 14.2.0, clang-18 18.1.3 and clang-20 20.1.2. gcc-13 uses libstdc++ 13; gcc-14 and both Clangs use libstdc++ 14. One type-name run uses clang-18 with libc++ 18.
- **Modes.**
  - C++20: `-std=c++20`.
  - C++26: `-std=c++26` on gcc-14 and `-std=c++2c` on clang-18 and clang-20.
  - gcc-13 has no C++26 mode, so it ran at `-std=c++2b` to confirm that it lacks P2741.
- **Flags.**
  - The NFR-10 warning set: `-Wall -Wextra -Wpedantic -Wshadow -Wconversion`.
  - `-fsyntax-only`, because every static_assert fires in the front end.
  - No `-Werror`: NFR-07 requires a hard error without it.
  - Otherwise the defaults: source excerpts and carets on, and no colour, because the output is redirected to a file.
- **Environment.** Only `LC_ALL=C`, `PATH` and a dummy `HOME`. Sources are passed by relative path, so the logs come out byte-identical on every run and in every checkout. Each compile runs alone, in a subshell under `ulimit -v 3000000` and `timeout 300`. No compile hit either cap.
- **The mock pipeline** (`src/mock.hpp`) is throwaway code that follows plan §7.3 and §7.8. It has:
  - an inheritance-based binding map;
  - the lifetimes `unique` and `singleton`;
  - seven request forms checked against the §6.3 table;
  - `node<Map, Request, ConsumerLifetime>`, memoized by the compiler, with no path or depth in its key;
  - the `resolvable` concept;
  - the explain pass;
  - a compile-time message builder;
  - real creation code (`make_shared`, `make_unique` and singleton slots).

  Constructor parameters are listed by hand (`using inject = ctor<...>`), because discovery belongs to S1.
- **Cases** (`src/cases.cpp`). The graph is `App -> Service -> Store -> edge`, and three choices of edge give the three errors:
  - E001: `std::shared_ptr<IRepository>`, where `IRepository` is abstract and unbound;
  - E005: `IRepository&`, where `IRepository` is bound with the `unique` lifetime;
  - E012: `Clock&`, where `Clock` is concrete and unbound.

  Each error is triggered two ways: at the root, with `create<edge>()`, and three hops deep, with `create<App>()`.
- **Variants.** There are nine renderings, all listed in the header of `mock.hpp`. In C++20 mode the matrix runs naive, leaf, guard, explain, noret, incomplete, ext and delete. In C++26 mode it runs msg26 and delete26. That makes 240 compiles: 6 cases × 4 compilers × 10 variant/mode pairs.
- **Metrics,** taken from the combined compiler output:
  - lines and bytes;
  - errors, counted as lines containing `error: `, and warnings;
  - the first error line;
  - the "ID+type line": the first line that names both the ID and the failing type;
  - a cascade, recorded when there is more than one error or any warning.
- **Explain-pass cost.** The driver generates layered graphs with width 10 and fan-out 3, in two sizes: N = 100 at depth 10, and N = 1,000 at depth 100.
  - Every node is a singleton requested by reference, and the deepest layer needs `std::shared_ptr<IRepository>`, which is unbound.
  - Each configuration is compiled three times with `-fsyntax-only`; the reported wall time and peak RSS (from `wait4`) are medians.
  - A C++20 build prints the explain pass's hop and read counts at run time; those counts are deterministic.

## 3. Results

### 3.1 The recommended renderings, all six cases

| Rendering | Compiler | Lines | Bytes | Errors / warnings | First error line names |
|---|---|---|---|---|---|
| C++20 `explain` | gcc-13 | 7 | 711–758 | 1 / 0 | the ID; the type, request and path are in the attached note (line 7) |
| C++20 `explain` | gcc-14 | 9 | 814–869 | 1 / 0 | the ID; type, request and path in the note (line 9) |
| C++20 `explain` | clang-18, clang-20 | 8 | 717–768 | 1 / 0 | the ID, type, request and path |
| C++26 `msg26` | gcc-14 | 12 | 839–902 | 1 / 0 | the ID and type; the request, path and fix follow on lines 2–4 of the message |
| C++26 `msg26` | clang-18, clang-20 | 11 | 731–796 | 1 / 0 | the ID and type; the request, path and fix follow on lines 2–4 |
| C++26 `msg26` | gcc-13 (`c++2b`) | — | — | 2 / 0 | not supported: `expected string-literal before 'message'` |

The output size doesn't depend on depth. Root and three-hops-deep outputs have the same number of lines and differ by under 50 bytes, because the path type grows by one name per hop. On the generated graph at depth 100, the output is still 7–12 lines and at most 1.9 KB (see §3.6).

**Actual output.** C++20, clang-20, E005 three hops deep (8 lines):

```
In file included from spikes/S3-diagnostics/src/cases.cpp:14:
spikes/S3-diagnostics/src/mock.hpp:458:23: error: static assertion failed due to requirement 'E005<IRepository, trellis::error::request<IRepository &>, trellis::unique, trellis::error::path<App, Service, Store>>::reported': TRL-E005: request form incompatible with the lifetime
  458 |         static_assert(err::reported, "TRL-E005: request form incompatible with the lifetime");
      |                       ^~~~~~~~~~~~~
spikes/S3-diagnostics/src/cases.cpp:103:22: note: in instantiation of function template specialization 'trellis::injector<app_module>::create<App>' requested here
  103 |   App app = injector.create<App>();
      |                      ^
1 error generated.
```

C++20, gcc-14, E001 three hops deep (9 lines). GCC prints the literal in the error line and the condition in the note:

```
spikes/S3-diagnostics/src/mock.hpp: In instantiation of 'R trellis::v1::injector<Module>::create() [with R = App; Module = app_module]':
spikes/S3-diagnostics/src/cases.cpp:103:33:   required from here
spikes/S3-diagnostics/src/mock.hpp:456:28: error: static assertion failed: TRL-E001: no binding for an abstract type
spikes/S3-diagnostics/src/mock.hpp:456:28: note: 'trellis::error::E001<IRepository, trellis::error::request<std::shared_ptr<IRepository> >, trellis::error::path<App, Service, Store> >::reported' evaluates to false
```

(The include line and the source-excerpt lines are left out here.)

C++26, gcc-14 `-std=c++26`, E012 three hops deep (12 lines). The excerpt and the trailing note are left out here:

```
spikes/S3-diagnostics/src/mock.hpp:469:21: error: static assertion failed: trellis [TRL-E012]: unbound concrete type 'Clock' requested by reference, pointer or shared_ptr: nobody would own it
  requested as 'Clock&' by 'Store' (parameter 2)
  path: App -> Service -> Store -> Clock
  fix: bind 'Clock' with an owning lifetime such as singleton, or request it by value (docs: errors/E012)
```

GCC and Clang both print the newlines in the message as they are. GCC adds the note `'false' evaluates to false` after it.

### 3.2 Other renderings, with the technique that stops cascades

These figures are for E001 three hops deep, and `results.md` has every case. Lines are given as gcc-13 / gcc-14 / clang-18 / clang-20.

| Variant | What it does | Lines | Errors | Notes |
|---|---|---|---|---|
| naive | `static_assert(resolvable<...>)`; the creation code is still instantiated | 11 / 13 / 65 / 65 | 1 / 1 / 3 / 3 | No ID and no type. Clang cascades into libstdc++'s `make_shared<IRepository>`. GCC printed no second error (it did not go on to instantiate the creation code). GCC's note prints the whole binding list. |
| leaf | the creation code checks each node and asserts at the failing one | 14 / 30 / 29 / 29 | 1 | The compiler's backtrace is the path, so size grows by 2 lines per hop on gcc-13 and 6 on gcc-14 and Clang (root: 8 / 12 / 11 / 11). That exceeds 40 lines at about 5 hops on gcc-14 and Clang. |
| guard | `if constexpr` guard, then `static_assert(resolvable<...>)`; no explain pass | 11 / 13 / 14 / 14 | 1 | Names `resolvable<bindings<...>, App, unique>` instead of the cause. Bytes grow with the binding count (§3.6). |
| **explain** | guard, explain pass, one `static_assert` naming the error type, then a `[[noreturn]]` call | 7 / 9 / 8 / 8 | 1 | Recommended for C++20. |
| noret | like explain, without the `[[noreturn]]` tail | 10 / 12 / 8 / 8 | 1 + 1 warning | GCC adds `-Wreturn-type`: "no return statement in function returning non-void". |
| incomplete | names a member of an undefined `error::incomplete<Err>` | 6 / 8 / 14 / 11 | 1 / 1 / 2 / 1 | The type lands on GCC's first error line too, but the wording is "incomplete type ... used in nested name specifier", and clang-18 reports a second error. |
| ext | the P2741 message in C++20 mode, warning silenced by a pragma | parse error / 12 / parse error / 11 | 1 | Accepted by gcc-14 and clang-20, rejected by gcc-13 and clang-18. The valid program builds warning-free under `-pedantic-errors -Werror` (sanity checks). |
| delete | explain pass in a default template argument of a deleted `create` overload (`= delete;` in C++20) | 8 / 11 / 16 / 20 | 1 (clang-20: + 1 warning) | GCC's first error line names everything, at the user's call site: `use of deleted function 'R ...create() [with R = App; Error = trellis::error::E001<...>]'`. Clang names only `create` and puts the type in a note, among "candidate template ignored" notes. clang-20 defines `__cpp_deleted_function` even at `-std=c++20`, so the gate picked `= delete("…")`, which then warns with `-Wc++26-extensions`. |
| delete26 | the same with `= delete("reason")` (P2573) where available | 8 / 11 / 16 / 16 | 1 | Only clang-20 supports P2573 here; it needs Clang 19 or GCC 15. The others fell back to `= delete;`. The reason must be a literal, so it can't name the type. |

**Stopping cascades.** One construct, used together, gives one error and nothing else on all four compilers:

1. `create<T>()` is `if constexpr (resolvable<...>) { creation code } else { explain pass; one static_assert; [[noreturn]] call; }`. The creation code for a failing graph is never instantiated.
2. The `[[noreturn]]` call stands in for a dummy return. It needs no value of `T`, which may be abstract or a reference, and it stops GCC's `-Wreturn-type`.
3. One static_assert per error kind, each in its own `if constexpr` branch with its own literal, written inline in `create`. A helper function or a macro would add 1–3 lines of "required from" or "in expansion of macro" on GCC; the first prototype used a macro and showed this.
4. The error vocabulary (`error::E00x`, `request`, `path`) lives outside `inline namespace v1`, because GCC spells inline namespaces (`trellis::v1::unique`) and Clang hides them.
5. Short injector types: the injector is parameterized by module names, so GCC's "In instantiation of ... [with R = App; Module = app_module]" banner stays one short line instead of listing every binding.

### 3.3 Existing frameworks, missing `IRepository` binding

The figures come from `research/experiments/results/logs/diagnostics/`. The research program has `App -> Service -> IRepository`, two hops, and was built with gcc 13.3 and clang 18.1 using `-O2 -Wall -Wextra -c`.

| Framework | GCC | Clang | First error names |
|---|---|---|---|
| Boost.DI | compiles with warnings only; fails at **link** time (18 + 3 lines) | 37 lines, 3.1 KB, 1 error | `abstract_type<demo::IRepository>::is_not_bound::error`: the type, no ID, no path |
| Fruit | 22 lines, 9.6 KB, 4 errors | 30 lines, 5.3 KB, 4 errors | a generic text ("No explicit binding was found for T, and note that C is an abstract class"); the type only in `AlwaysFalse<demo::IRepository>` |
| qlibs/di | 64 lines, 30.4 KB, 5 errors | 81 lines, 13.7 KB, 3 errors | "cannot allocate an object of abstract type 'demo::IRepository'"; no path, no ID |
| Kangaru | compiles; at run time "No instance found for the requested abstract service" | same | nothing |
| dingo | compiles; at run time "type not found: std::shared_ptr<demo::IRepository> (required by demo::App -> demo::Service)" | same | the type, form and path (the content model we want, but at run time) |
| Hypodermic | compiles; at run time "Unable to resolve demo.IRepository" plus throw locations (515 bytes) | same | the type |
| **Trellis mock, explain** | **7–9 lines, under 0.9 KB, 1 error** | **8 lines, under 0.8 KB, 1 error** | **the ID; on Clang also the type, request and path** |
| **Trellis mock, msg26** | **12 lines, under 0.95 KB** | **11 lines, under 0.8 KB** | **the ID and type, then request, consumer and parameter, path, fix** |

### 3.4 Constexpr type names from `__PRETTY_FUNCTION__`

`src/type_name.hpp` slices `__PRETTY_FUNCTION__` (or `__FUNCSIG__` on MSVC) using the prefix and suffix lengths measured once on `raw_name<int>()`, so no offsets are hard-coded. `static_assert`s in `src/type_names.cpp` prove the result is a constant expression on all four compilers and with libc++. Spellings differ by compiler:

| Type | gcc-13, gcc-14 | clang-18, clang-20 (libstdc++) | clang-18 + libc++ |
|---|---|---|---|
| `const IRepository&` | `const IRepository&` | `const IRepository &` | `const IRepository &` |
| `std::unique_ptr<IRepository>` | `std::unique_ptr<IRepository>` | same | same |
| `std::string` | `std::__cxx11::basic_string<char>` | `std::basic_string<char>` | `std::string` |
| anonymous-namespace type | `{anonymous}::Hidden` | `(anonymous namespace)::Hidden` | same |
| lambda | `main()::<lambda()>` | `(lambda at <path>:39:17)` | same |
| `trellis::singleton` (inline `v1`) | `trellis::v1::singleton` | `trellis::singleton` | same |

The same names, built into a C++26-style message, can be printed from a C++20 build (`-DPRINT_EXPLAIN`). The runtime engine (R001) and a `testing::explain<T>()` helper could reuse them.

### 3.5 The explain pass

**Design** (as implemented in `mock.hpp`):

1. The normal pass instantiates `node<Map, R, L>` once per key. It holds `self`, the node's own check (is there a binding, and does the form fit the lifetime?), and `ok`, which is `self` plus a fold over the constructor parameters. Through `std::conjunction`, the parameters are only examined when `self` passes.
2. Only on failure, `explain<Map, Root>` dispatches on `explain_status`, which is one of ok, e001, e005, e012 or dep.
   - For dep, `first_failing` copies the already-memoized `node<Map, P, L>::ok` of each parameter into a `constexpr bool[]` and returns the first false index. The pass then recurses with `path<..., Impl>` and the 1-based parameter index.
   - At a node that fails its own check, the pass yields `error::E00x<T, request<R>, [lifetime,] path<...>>` together with the code, the failing type, the request, the lifetime, the path and the parameter index.
3. The C++26 message is rendered from the same result, in two passes: one counts the characters and the other fills a `fixed_string<N>` of exactly that size.

**Cost.** The pass makes depth + 1 `explain_at` instantiations and one memoized read per parameter on the failing path, so it is O(depth × arity) and costs nothing on success. The deterministic counts:

- N = 100, depth 10: 11 hops and 31 reads;
- N = 1,000, depth 100: 101 hops and 301 reads.

Timings are machine-dependent, median of 3 `-fsyntax-only` runs on the shared 4-vCPU container:

| N | Compiler | Bound (normal pass + creation code) | Unbound, guard only | Unbound, explain | Unbound, msg26 (C++26 mode) |
|---|---|---|---|---|---|
| 100 | gcc-13 | 0.86 s | 0.38 s | 0.36 s | — |
| 100 | gcc-14 | 0.87 s | 0.36 s | 0.37 s | 0.71 s |
| 100 | clang-18 | 1.03 s | 0.39 s | 0.38 s | 0.76 s |
| 100 | clang-20 | 0.99 s | 0.42 s | 0.40 s | 0.78 s |
| 1,000 | gcc-13 | 23.7 s, 1,517 MB | 8.86 s, 460 MB | 8.98 s, 467 MB | — |
| 1,000 | gcc-14 | 23.0 s, 1,635 MB | 9.99 s, 524 MB | 10.5 s, 531 MB | 10.7 s, 572 MB |
| 1,000 | clang-18 | 14.8 s, 750 MB | 2.89 s, 162 MB | 2.93 s, 167 MB | 3.25 s, 193 MB |
| 1,000 | clang-20 | 15.4 s, 755 MB | 3.10 s, 171 MB | 3.17 s, 175 MB | 3.44 s, 202 MB |

- **The explain pass** costs at most 5% over the guard-only build, at both sizes. That is within the noise of a shared machine.
- **A failing build** is 2.5–5× cheaper than a passing one, because no creation code is instantiated.
- **msg26** is compiled in C++26 mode and costs a roughly fixed 0.3–0.4 s more than explain at both depths. Because the cost doesn't grow from depth 10 to depth 100, most of it comes from the language mode and its headers rather than from rendering the message. This spike didn't separate the two.

### 3.6 Output size on the generated graphs

Lines and bytes; GCC means gcc-13 / gcc-14, and Clang means clang-18 and clang-20, which give identical output.

| N (depth) | explain | msg26 | guard (no explain pass) |
|---|---|---|---|
| 100 (10), GCC | 7 / 9 lines, 809 / 920 B | gcc-14: 12 lines, 958 B | 11 / 13 lines, 7.1 / 7.2 KB, with one 6.2 KB line |
| 100 (10), Clang | 8 lines, 821 B | 11 lines, 850 B | 14 lines, 4.5 KB |
| 1,000 (100), GCC | 7 / 9 lines, 1,532 / 1,643 B | gcc-14: 12 lines, 1,862 B | 11 / 13 lines, 63 KB, with one 62 KB line |
| 1,000 (100), Clang | 8 lines, 1,544 B | 11 lines, 1,754 B | 14 lines, 36.9 KB |

The guard variant prints the binding map: GCC in its "evaluated to false" note and Clang in its "because ... evaluated to false" note. The explain pass is what keeps bytes bounded by the path instead of by the number of bindings. The path is the only part that still grows, at about 8 bytes per hop.

### 3.7 Normalization rules for golden tests

Validated by `run.sh` on every compiler. Each variant below must normalize to exactly the same text as the default run:

| Compiler | Variants checked |
|---|---|
| GCC | `-fdiagnostics-color=always`, `-fdiagnostics-urls=always`, `-fdiagnostics-plain-output`, `LC_ALL=C.UTF-8`, an absolute source path |
| Clang | `-fdiagnostics-color=always`, `-fno-caret-diagnostics`, an absolute path |

Clang 18 and 20 reject `-fdiagnostics-plain-output` as an unknown argument.

**Environment and flags for golden runs:**

- Use `LC_ALL=C`; GCC prints ‘curly’ quotes in UTF-8 locales.
- Use a clean environment: no `GCC_COLORS`, `GCC_URLS`, `TERM_URLS`, `COLUMNS` or `CPATH`.
- GCC: `-fdiagnostics-plain-output`, which needs GCC 11 or later. It turns off carets, line numbers, colour and URLs.
- Clang: `-fno-color-diagnostics -fno-caret-diagnostics`.
- Measure budgets on the default-flag output, which is what users see. Compare the golden text on the normalized output.

**Text rules,** applied in order (`normalize()` in `tools/measure.py`):

1. Strip ANSI SGR sequences (`ESC[…m`, `ESC[K`) and OSC-8 hyperlinks.
2. Map ‘ and ’ to `'`.
3. Drop:
   - source-excerpt and caret/fix-it lines (`^\s*\d*\s*\|`);
   - Clang's `N errors generated.` banner;
   - GCC's `compilation terminated.`;
   - include chains (`In file included from …` and continuation lines `                 from …`).
4. Rewrite paths: everything up to the source directory becomes `<src>/`, and the standard-library include directory becomes `<std>/`. This also covers paths inside lambda type names.
5. Rewrite `:line:col` (or `:line`) after a normalized path to `:L:C`.
6. Strip trailing whitespace.

Then apply the checks of plan §7.8: the compile fails; the first error line matches a regular expression that contains the ID; the output is within the line and byte budgets. Keep golden files per compiler, because spellings differ (§3.4). GCC 14 also adds excerpt lines to "required from" notes that GCC 13 doesn't print, so line counts move between compiler versions.

### 3.8 MSVC risk (not testable here)

**High for the C++20 rendering.**

- MSVC prints `error C2338: static_assert failed: '<message>'` without the condition. The error type, and with it the type and path, would therefore appear only through the "see reference to ... being compiled" notes.
- Mitigation, to try in CI early in Phase 4:
  - on MSVC, put the assertion inside a function or class template whose argument is the error type, so the first instantiation note names it;
  - or rely on P2741 once MSVC ships it. I don't know of a shipped version.
- `__FUNCSIG__` names carry `struct ` and `class ` prefixes, and `__cdecl` lies outside the probed slice, so `type_name` needs one prefix-stripping pass on MSVC.
- `if constexpr` and `[[noreturn]]` are portable, so the "one error" mechanism itself is low-risk.

## 4. Recommendation

**Confirm ADR-06,** with these refinements.

1. **The mechanism, in all modes:** the `if constexpr` guard, the explain pass and one inline static_assert per error kind, followed by a `[[noreturn]]` call (§3.2). This is the cascade-stopping technique. Measured on all four compilers: 1 error, 0 warnings and 7–12 lines, under 1 KB at depth 3 and under 1.9 KB at depth 100.
2. **C++20 rendering:** `static_assert(error::E00x<T, request<R>, path<...>>::reported, "TRL-E00x: <summary>")`. The literal is what GCC shows in the error line, and the condition is what Clang shows, so both carry information. Keep the error vocabulary outside `inline namespace v1`.
3. **C++26 rendering:** a P2741 message in four lines: the ID and type; the requested form, the consumer and the parameter; the path; the fix.
   - Use it when `__cpp_static_assert >= 202306L`.
   - Also use it in C++20 and C++23 modes on GCC 14 or later and Clang 20 or later, where it works as an extension (Clang 19 untested). Gate it behind `TRELLIS_HAS_STATIC_ASSERT_MESSAGE` in `config.hpp`, with `#pragma GCC diagnostic ignored "-Wc++26-extensions"` around the one assertion.
   - Gate on compiler versions or `__cplusplus`, not on feature-test macros alone: clang-20 defines `__cpp_deleted_function` at `-std=c++20` and then warns when the feature is used.
   - This change to ADR-06 ("where `__cpp_static_assert >= 202306L`") puts the type on the first error line for gcc-14 and clang-20 in C++20 mode too.
4. **Reject:**
   - naive (it cascades on Clang and names nothing);
   - leaf (it grows by 2–6 lines per hop);
   - guard (it prints the binding list);
   - the incomplete-type trick (poor wording, and a second error on clang-18);
   - a deleted overload as the main channel (on Clang the type lands in a note among candidate notes; P2573 reasons are literals; only clang-19+ and gcc-15+ support it).

   `= delete("reason")` is still good for misuse whose reason is fixed text, such as copying an injector, in C++26 builds.
5. **Long paths:** elide the middle of paths longer than about 8 hops, both in the type and in the message, for example `path<App, A, B, elided<94>, X, Y, Z>`. Add this to the Phase 4 work.

**NFR-07 verdict: feasible, with one wording change.**

- The 40-line budget is met with a wide margin on GCC and Clang: at most 12 lines with the recommended renderings, for every case, at depth 3 and at depth 100. The research baselines are Fruit at 22–30 lines and qlibs/di at 64–81.
- "The first error line names the type and the ID" holds:
  - on Clang in every mode;
  - on gcc-14 in C++26, and in C++20 with the extension;
  - not on gcc-13 in C++20, where GCC prints only the literal (which carries the ID) and the type follows in the attached note, the last line of a 7-line error.

Proposed wording: "the first error line names the error ID, and the same diagnostic names the type". The alternative is to use the deleted-overload rendering on GCC 13 only. That adds a second code path to fix one compiler version that is going out of use, so I don't recommend it.

**Cost of the alternatives:**

- Keeping ADR-06 exactly as written means gcc-14 shows the type only in a note in C++20 mode.
- The leaf variant breaks the line budget at about 5 hops.
- Dropping the explain pass breaks the byte budget at about 100 bindings.

## 5. Limits and open questions

- This is a mock. Deduced constructor discovery (S1) may fail inside conversion operators, where the normal pass can't see it. The explain pass needs each class's parameter list, which S1 and S2 must provide. Cycles (S2) and E002, E003, E006 and E007 weren't prototyped. There are only two lifetimes.
- Errors are reported once per failing `create<>` root, not once per root cause across roots. Two roots that fail on the same missing binding give two errors.
- MSVC is untested (§3.8). Clang 19 and GCC 15 weren't available, and neither was GCC 15's `-fdiagnostics-add-output`/SARIF, which may suit golden tests better than text.
- Timings come from a shared machine with other agents compiling, so read them as ratios.
- Separate from S3: at N = 1,000, the mock's normal pass takes about 10 s on GCC against about 3 s on Clang. That points at the overload-based binding lookup with 1,000 `find` overloads. S2 should measure the real map.
