# Phase 1 gate: Core MVP → v0.1

| | |
|---|---|
| Date | 2026-11-02 |
| Code | main at 3695945. ba217dc, after it, only adds this report's draft |
| Tag | `v0.1.0` |
| Exit criteria | plan §12.2, Phase 1 row, as amended by owner decisions 2a and 3a (2026-11-01) |
| Targets | plan §3.2; NFR-07 as amended by decision 1A; NFR-09 and NFR-12 as amended by the owner on 2026-11-02 |

**Verdict: PASS, with owner-accepted exceptions.** The final check-full passed 10 of 10 presets on 32995e3, and all 15 Phase 1 tasks pass. Every Phase 1 exit criterion is met, except one at its limit: NFR-01 on GCC, 1.10× by instructions and 1.11× by wall clock, against ≤ 1.1×. Its instruction count is unchanged since T1.14, and the owner chose to tag with it noted. The exceptions are:

- NFR-03 and NFR-06 on Clang 18 (decisions 2a and 3a);
- NFR-09's known limits on MSVC and Apple Clang (2026-11-02).

The full benchmark sweep measured the generated graph above 100 components for the first time, and NFR-04's 1,000-component checkpoint fails on both compilers. That checkpoint is a Phase 3 exit criterion, so it doesn't block v0.1, but it is the first follow-up.

## Tasks

Evidence comes from `.verify/<task>.json`, `build/logs/verify/` and the commits. Commits in parentheses record the pass.

| Task | Title | Status | Evidence | Commits |
|---|---|---|---|---|
| T1.1 | Meta toolkit | PASS | `check-full`: 10 of 10 presets | 3c25110 (5eb362a) |
| T1.2 | Explicit constructor declaration | PASS | `check-full`: 10 of 10 | 69d1f08 (961faed) |
| T1.3 | Deduced constructor discovery | PASS | `check-full`: 10 of 10 | e88d9d9 (0871614) |
| T1.4 | Binding vocabulary and modules | PASS | `check-full`: 10 of 10 | 94689a7 (5989dc4) |
| T1.5 | Storage, lifetimes and `create<T>` | PASS | `check-full`: 10 of 10; `ctest -R scenario`: 4 of 4 | de3a8d8 (8126415) |
| T1.6 | Diagnostics in C++20 form | PASS | On 8ce2d6f: `check-full` 10 of 10; `-L golden` on gcc-13 and clang-18 | 5837eb5 (1c61096) |
| T1.7 | Test overrides and testing helpers | PASS | T1.6's `check-full`, same tree | bffb420 (1c61096) |
| T1.8 | C++20 module and single header | PASS | On 8ce2d6f: `-L module` and `amalgamate.py --check` | 320d5ed (1c61096) |
| T1.9 | v0.1 benchmarks against budgets | PASS | On 9ce6866, quiet machine: 0 budgets missed on gcc-13 or on clang-18 without the scenario; `bench-runtime` 2 of 2 presets; `-L property` 5 of 5 | 91ddbdf (d72ed8d) |
| T1.10 | v0.1 release | PASS: check-full on 32995e3: 10 of 10 presets in 1,979 s | `docs/getting-started.md` and its snippet check; the cleanup pass and the NFR-12 count; this report | ca3f8af, 49522dd, 88c0eda |
| T1.11 | Chain-depth and arity budgets | PASS: check-full on 32995e3: 10 of 10 presets in 1,979 s | `deepest.json` re-recorded on the final `include/`; Apple Clang and MSVC results in ADR-0014's History | 06c22c2, bcfc9cf, 47aa7c6 |
| T1.12 | Documentation scaffold | PASS | `check-fast`; the three files exist | 729e43c, 617c3be, bb5ac78 (3b88961) |
| T1.13 | Early MSVC check | PASS | both `## Addendum: MSVC` checks | 51fb862 (09322b7) |
| T1.14 | Meet the v0.1 compile-time and binary-size budgets | PASS: check-full on 32995e3: 10 of 10 presets in 1,979 s; gcc-13 quick with 9 compiles: NFR-03 +0.20 s, NFR-04 0.97×, NFR-05 0.89×; clang-18 quick passed | Benchmarks as for T1.9. `check-full` on 9ce6866: 9 of 10 presets; timetrace failed only `single_header.main_file`, fixed in e3f04e9 | ef5b6e9, 0788aa9, e3f04e9 |
| T1.15 | Constructor declared on the binding | PASS: check-full on 32995e3: 10 of 10 presets in 1,979 s | — | 3c20302 |

Other work the gate depends on:

- the platform fixes: a3fcf90 to 4d74e4c;
- the cl parse fix: 8ce2d6f;
- the gate fixes: 0f19f6a;
- the Apple Clang fix: 9ce6866;
- MSVC and decision 4a: 01b8593;
- cl's known limits: 3695945;
- the owner's decisions recorded in the ADRs and the plan: 8050fa7.

### Left before the tag

1. Done: check-full on 32995e3: 10 of 10 presets in 1,979 s. T1.11, T1.14, T1.15 and T1.10 pass.
2. bench-runtime on a quiet machine: done, see NFR-01.
3. Update README's status line, which still says Phase 1 is in progress.
4. Add a `[0.1.0]` section to CHANGELOG.md, with the large-graph compile time added to its known issues.
5. Replace the draft in `docs/gates/phase-1.md` (ba217dc) with this report.
6. Add PROGRESS entries for 01b8593, 49522dd, ca3f8af, bcfc9cf, 8050fa7 and 3695945, which have none yet.
7. Done: tagged `v0.1.0` and pushed. The nightly run on the final code, 41373551879, passed.

## Reviews

**The phase-level review** (gate step 4) failed its early run on four blocking findings. All are fixed:

1. `.with_ctor` was missing: T1.15 (3c20302).
2. No test measured NFR-07 as 1A amended it: `expect-max-lines-nfr07` and five alternating-chain tests (0f19f6a).
3. The ADR index and "Waiting for owner" left out provisional amendments: 0f19f6a, d6d3b74 and ea4cc84. The owner settled them on 2026-11-02.
4. Decisions 2a and 3a weren't in the plan, and the nightly would have gone red every night: 0f19f6a added the plan markers in §9.2 and §12.2 and gave the clang-18 scenario a nightly step of its own.

**Branch reviews**, from PROGRESS and the commit messages:

| Landing | Reviews and verdict |
|---|---|
| T1.1 (3c25110) | Failed once, on three bugs; the recheck passed |
| T1.2 (69d1f08), T1.3 (e88d9d9), T1.4 (94689a7) | Each failed twice; each third check passed |
| T1.5 (de3a8d8) | Two reviewers, for run time and compile time; each failed once, and both rechecks passed |
| T1.6 (5837eb5) | Failed twice; the third check passed, except the NFR-07 residual, which 1A settled |
| T1.7 (bffb420) | Failed once; the recheck passed |
| T1.8 (320d5ed) | Two stages, both passed; stage 1 with 8 non-blocking fixes |
| T1.9 and T1.14 (91ddbdf, ef5b6e9) | The code passed; the acceptance failed only on the two Clang budgets, later accepted by 2a and 3a |
| T1.11 (06c22c2) | PASS: reviewed before landing (the orchestrator's T1.11 review on 5a5ffcb, whose quiet rounds v0.1.md cites) |
| T1.12 (729e43c, 617c3be) | Passed |
| T1.13 (51fb862) | Failed once, on four documentation gaps; the recheck passed |
| T1.15 (3c20302) | The first review failed the warning fix; the second passed |
| Platform fixes (a3fcf90 to 4d74e4c) | Passed after one doc fix |
| Gate fixes (0f19f6a) | Passed |
| Apple Clang fix (9ce6866) | No code defect found on GCC or Clang |
| MSVC and 4a (01b8593) | FAIL on two items, then fixed. A comment contradicted CI (fixed in 7f1c770). The 42 cl line budgets needed the owner, so they were left out until the owner's decision, then landed in 3695945 |
| Cleanup (49522dd) | Passed; five further findings fixed in 88c0eda |
| Getting started (ca3f8af) | The first review failed on two missing limitations and the tool's fence parsing; both fixed |
| ADRs and plan (8050fa7), cl's known limits (3695945) | 8050fa7: docs only; the orchestrator checked it touches no other file. 3695945: no separate review. Replaying run 41352149959's cl output gave 146 PASS and 6 XFAIL of 152, GCC and Clang verdicts were identical on all 152, and platforms run 41372428494 was green on all three jobs |

## Exit criteria

Plan §12.2, Phase 1: "Scenario warning-free on the pull-request matrix; NFR-01, 03, 06, 07 and 10 met; N = 100 compile ≤ 1.3× hand-written", with NFR-03 (+0.38 s) and NFR-06 (+8,240 B) on Clang accepted for v0.1.

| # | Criterion | Target | Evidence | Verdict |
|---|---|---|---|---|
| 1 | Scenario warning-free on the pull-request matrix | `tests/unit/scenario_test.cpp`, with the project's warnings, builds clean under `-Werror` on ci.yml's six Linux presets, under `/W4 /WX` on MSVC, and on Apple Clang | ci.yml passed on every push to main, 3695945 included (run 41372428543). Platforms run 41372428494 on 3695945 passed: macOS 717 of 717 tests, cl 19.51 547 of 547. Final confirmation: PASS: check-full on 32995e3: 10 of 10 presets in 1,979 s | PASS |
| 2 | NFR-01 | A warm resolve takes ≤ 1.1× hand-wired | **Wall clock:** four quiet runs on 3695945 (load 0.46–0.72), fastest repetition against fastest. gcc-13: 1.11× (46.4 against 41.9 ns). clang-18: 1.05–1.07×. T1.9's undisturbed runs gave 1.07× and 1.01×. **Instructions per resolve** (cachegrind, the same method at every commit): gcc-13 457 against 415, 1.10×. That is the same 457–458 at ef5b6e9 (T1.14), 3c20302, 9ce6866 and 01b8593, so there's no regression. clang-18: 446 against 432, 1.03×. (T1.14's report gave 432 for gcc-13 by another method.) | **PASS on Clang. At the limit on GCC (1.10× by instructions, 1.11× by wall clock): the owner chose to tag with it noted, with a Phase 2 task (2026-11-02)** |
| 3 | NFR-03 | The scenario compiles ≤ +0.3 s slower | gcc-13: +0.09 s (T1.9's verify); +0.21 s in the sweep (2.02 against 1.81 s). clang-18: +0.38 s by interleaved CPU medians (T1.14), accepted by 2a. The sweep's 3-compile median gives +0.42 s wall and +0.39 s CPU (2.98 against 2.56 s) **At the gate, gcc-13:** T1.14's verify read +0.32 s and +0.50 s from 3-compile medians (the Trellis compiles alone ranged from 2.21 to 2.65 s in one run). 9-compile medians gave +0.24 s and +0.23 s, the same +0.24 s T1.14 recorded, so the owner amended T1.14's verify to `--runs 9` (docs/task-amendments.md, 2026-11-02). | PASS with owner-accepted exception (2a) |
| 4 | NFR-06 | The stripped scenario binary is ≤ hand-wired + 8 KB | From the sweep. gcc-13: +4,096 B (22,832 against 18,736). clang-18: +8,240 B (22,984 against 14,744), the figure 3a accepted | PASS with owner-accepted exception (3a) |
| 5 | NFR-07, as amended by 1A | A missing binding is a compile error without `-Werror`. The first error line names the ID, and the same diagnostic names the type. At most 40 lines on GCC and Clang, not counting the include chain and the user's call-site excerpt | 102 of the 152 compile-fail tests expect an error ID on the first error line: E001 7, E002 32, E003 8, E004 8, E005 16, E009 3, E010 4, E012 20, E013 2, E014 2. Under 1A's count, the five alternating-chain tests take 17 lines on gcc-13, 35 to 37 on gcc-14 and 33 to 35 on Clang; a 21-class chain takes 14 to 33. 20 tests have goldens on four compilers. On cl, the error line names the ID and `raise`'s note names the type. 58 tests carry cl alternatives, 42 carry cl budgets and 6 carry `expect-fail-msvc`. All 152 pass on cl 19.51, 6 of them as known limits (run 41372428494) | PASS |
| 6 | NFR-10 | 0 warnings under `-Wall -Wextra -Wpedantic -Wshadow -Wconversion` and `/W4` | The project set adds `-Wsign-conversion -Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual`, with `-Werror`; MSVC builds with `/W4 /permissive- /WX`. CI and platforms are green on 3695945. T1.15 fixed `-Wmissing-field-initializers`, and `tests/warnings/` checks that `-Wdangling-field` still fires. 01b8593 fixed cl 19.44's C4702. Final: PASS: check-full on 32995e3: 10 of 10 presets in 1,979 s | PASS |
| 7 | NFR-04 at N = 100 | ≤ 1.3× hand-wired | T1.9's verify: 1.00× on gcc-13, 0.98× on clang-18. The sweep, fan-in 1-3: 0.94× and 1.01×. Fan-in 10: 1.02× on gcc-13; 1.17× on clang-18, from a single compile | PASS |

Other targets of §3.2. None is a Phase 1 exit criterion.

| Target | Phase (§12.3) | Latest | State |
|---|---|---|---|
| NFR-02: build the injector and graph, ≤ 1.5× | 2 | 1.56× on gcc-13, 1.63× on clang-18 (T1.9's verify) | Over |
| NFR-05: peak compiler memory at N = 100, ≤ 1.5× | not listed | Sweep, fan-in 1-3: 0.89× on gcc-13, 1.11× on clang-18; fan-in 10: 0.90× and 1.05×. At N = 1,000, clang-18 takes 2.10× (fan-in 1-3) and 2.24× (fan-in 10) | Met at N = 100 |
| NFR-09: ≥ 200 hops; ≥ 150 on MSVC before 19.50 | 2 | Linux: 241 to 503 hops. Apple Clang: 213. cl 19.51: `std::unique_ptr` 198, aggregates 15 | Met, except the accepted known limits |
| NFR-11: `-fno-exceptions` and `-fno-rtti` | 1 | `no_exceptions_test.cpp`, `no_exceptions_create_test.cpp` | Met on GCC and Clang; untested on MSVC |
| NFR-12: ≤ 6,000 lines that are neither blank nor comments | 7–8 | 4,406 on 3695945 (9,779 lines in all) | Met |
| NFR-13: GCC ≥ 12, Clang ≥ 16, Apple Clang ≥ 15, MSVC ≥ 19.38 | 1 | Tested: GCC 13 and 14, Clang 18 and 20 (libc++ 18 too), Apple Clang 21, cl 19.51; cl 19.44 runs the chain test only | The minimums are untested |

## Phase 1 features

Test paths are under `tests/`.

| Feature (§12.2) | Requirement | Task | Evidence |
|---|---|---|---|
| Meta toolkit | NFR-12 | T1.1 | `unit/type_list_test.cpp`, `type_map_test.cpp`, `type_name_test.cpp`, `result_test.cpp`; `compile_fail/meta/` (8) |
| Explicit discovery: `ctor_traits`, `trellis_ctor` | FR-06 | T1.2 | `unit/ctor_traits_test.cpp`; `compile_fail/discovery/` |
| `.with_ctor<Args...>()` | FR-06 | T1.15 | `unit/with_ctor_test.cpp`, `with_ctor_engine_test.cpp`, `declared_aggregate_test.cpp`; `warnings/`; `compile_fail/create/e002_with_ctor_*` |
| Deduced discovery | FR-01, FR-06 | T1.3 | `unit/deduced_test.cpp`, `resolve_forms_test.cpp` (S1's 35 forms); `compile_fail/discovery/deduced_*` (17) |
| `bind`, `to`, `in`, `to_ref`, `to_ptr`, `to_value`, `to_shared` | FR-01, FR-02 | T1.4 | `unit/bind_test.cpp`; `compile_fail/dsl/` (23) |
| Lifetimes: unique, singleton, singleton_inline, external, value | FR-02 | T1.4, T1.5 | `unit/create_test.cpp`, `injector_test.cpp` |
| Request forms and the table of §6.3 | FR-03, FR-04 | T1.4, T1.5 | `unit/request_forms_test.cpp` (a test per cell); `compile_fail/static/`, `compile_fail/create/` |
| Storage and teardown | FR-02 | T1.5, T1.14 | `unit/create_test.cpp` (teardown order, retry after a throw, per-injector singletons); `unit/threading_test.cpp`, also under TSan |
| `create<T>()` | FR-01, FR-03 | T1.5 | `unit/scenario_test.cpp`; `examples/hello_world.cpp`; the equivalence oracle on 200 graphs (T1.9) |
| Modules and duplicate detection | FR-04, FR-05 | T1.4 | `unit/module_test.cpp`; `compile_fail/static/e003_*` |
| Overrides and testing helpers | FR-05 | T1.7 | `unit/override_test.cpp`, `testing_test.cpp` (a gMock example); `compile_fail/override/` (13), `compile_fail/testing/` (3) |
| Errors E001–E005, E009, E010, E012 (C++20), plus E013 and E014 | FR-04 | T1.6 | `unit/explain_test.cpp`; `compile_fail/explain/` (33); 20 goldens on four compilers; 11 pages in `docs/errors/` |
| The C++20 module and the single header | ADR-0008 | T1.8 | `module/`, `module.exports`; `single_header/` (a twin of every unit test) |
| Eager mode, `threading::none`, strict mode (brought into Phase 1) | — | T1.5 | `unit/create_test.cpp`, `threading_test.cpp`; `compile_fail/create/e013_strict_unbound.cpp` |
| Chain-depth test, `max_ctor_arity` | NFR-09 | T1.11 | `chain_depth/`; `docs/benchmarks/v0.1.md` |
| Benchmarks and the oracle | NFR-01, NFR-03 to NFR-06 | T1.9, T1.14 | `benchmarks/`; `property/check_sharing.py` |
| Docs: error catalog, internals, changelog, getting started | §11 | T1.12, T1.10 | `docs/errors/`, `docs/internals/`, `CHANGELOG.md`, `docs/getting-started.md` (`tools.doc_snippets`, `example.getting_started`) |

## Platforms

| Platform | Compiler | Final result | Chain test (NFR-09) |
|---|---|---|---|
| Linux, ci.yml | GCC 13 and 14; Clang 18 and 20; Clang 18 with libc++ 18; ASan/UBSan | Passed on every push to main; on 3695945, run 41372428543 | 200 hops, every shape |
| Linux, `check-full` | the six above, plus two release builds, TSan and timetrace | check-full on 32995e3: 10 of 10 presets in 1,979 s. In passed and skipped tests: gcc-13 641 and 18; gcc-14 718 and 18; clang-18 776 and 26; clang-20 781 and 20; clang-18 + libc++ 759 and 22; gcc-13 release 641 and 18; clang-18 release 776 and 26; ASan/UBSan, TSan and timetrace 776 and 26 each. On 9ce6866, 9 of 10 presets passed; the timetrace failure was fixed in e3f04e9 | as above |
| macOS | Apple clang 21.0.0 | 717 of 717 (run 41372428494); 714 of 714 on 47aa7c6 (run 41357148603) | 200 hops; aggregate chains reach 213 (run 41342943535) |
| Windows | cl 19.51 | 547 of 547 (run 41372428494), 6 compile-fail tests among them as known cl limits | 8 of 10 shapes at 200 hops; `unique_ptr` and `aggregate` XFAIL |
| Windows | cl 19.44 | Only the chain test, by design | 9 of 10 shapes at 150 hops; `aggregate` XFAIL |

**cl's known limits**, accepted on 2026-11-02 (3695945). `expect-fail-msvc` reports XFAIL with the reason, and XPASS once cl lifts the limit:

- **CWG 2327** (2 tests): cl can't initialize a by-value parameter of a non-movable class from a conversion function's prvalue.
- **Follow-on errors** (3 tests): cl's error recovery adds C3553, C2516 or C2882 after `bind<I>`'s key check.
- **One report per translation unit** (1 test): cl reports a `static_assert` that fails in several specializations of one function template only once. For a cause that two roots reach, cl prints the second root's path.
- **Chain limits:** `std::unique_ptr` chains stop at 198 hops on cl 19.50 and later (C2999). Aggregate chains stop at 15 hops on every cl (C1054), which plain C++ hits too (`C16 c{}`).

Replaying run 41352149959's cl output against 3695945 gave 146 PASS and 6 XFAIL of 152 compile-fail tests. The final run confirms it.

**NFR-09 on MSVC.** The deepest chain at the default limits, from `--deepest` in run 41349651148, before the cleanup pass. The cleanup changed no behaviour, and the final run's 200- and 150-hop tests show the same pattern.

| Shape | cl 19.51 (200-hop test) | cl 19.44 (150-hop test) |
|---|---|---|
| value, reference, pointer, shared_ptr, weak_ptr, eager | 246 | 161–162 |
| unique_ptr | 198 (C2999, two short) | 162 |
| aggregate | 15 (C1054, cl's own limit; plain C++ hits it too) | 15 |
| declared | 332 | 242 |
| mixed | 288 | 194 |

**Linux.** The deepest chains at the default limits, from `tests/chain_depth/deepest.json`, recorded at 88c0eda. `include/` hasn't changed since.

| Shape | gcc-13 | gcc-14 | clang-18 | clang-20 |
|---|---|---|---|---|
| Request forms: value, `unique_ptr`, reference, pointer, `shared_ptr`, `weak_ptr` | 296–298 | 296–298 | 310–335 | 305–335 |
| Aggregates | 298 | 298 | 335 | 241, stopped by the stack |
| Eager injector | 441 | 441 | 312 | 333 |
| Declared constructors | 442 | 442 | 502 | 503 |
| The mix | 355 | 355 | 401 | 401 |

**Apple Clang 21** reaches 213 hops for aggregates (6% over 200) and 335 by value (run 41342943535; ADR-0014's History).

## Benchmarks

### The full sweep

`scripts/bench-compile.sh` at bcfc9cf. Part 1 compiled each program 3 times and took the median. It started at 23:15 UTC at load 0.70, and some rows overlapped other builders' light work (`full-sweep-part1.log`). Part 2 covered clang-18 with fan-in 10 from N = 100, one compile per program, from 01:16 UTC at load 1.16 (`full-sweep-part2.log`).

Each cell gives hand-wired → Trellis compile time, in seconds, and Trellis's ratio. The harness stops a compile after 900 s.

| N | gcc-13, fan-in 1-3 | gcc-13, fan-in 10 | clang-18, fan-in 1-3 | clang-18, fan-in 10 |
|---|---|---|---|---|
| 10 | 1.65 → 1.84, 1.12× | 2.23 → 2.69, 1.21× | 1.58 → 1.96, 1.24× | 1.70 → 2.29, 1.35× |
| 25 | 3.08 → 3.22, 1.05× | 3.97 → 4.37, 1.10× | 2.82 → 3.69, 1.31× | 3.48 → 4.49, 1.29× |
| 50 | 4.76 → 4.76, 1.00× | 6.81 → 6.71, 0.99× | 5.62 → 5.89, 1.05× | 7.01 → 7.75, 1.11× |
| 100 | 9.84 → 9.27, 0.94× | 14.12 → 14.42, 1.02× | 10.75 → 10.84, 1.01× | 12.71 → 14.82, 1.17× (1 compile) |
| 250 | 27.30 → 38.21, 1.40× | 39.06 → 56.50, 1.45× | 31.48 → 33.06, 1.05× | 34.30 → 44.75, 1.30× (1 compile) |
| 500 | 74.57 → 203.14, 2.72× | 93.80 → 295.78, 3.15× | 70.82 → 83.33, 1.18× | 73.87 → 110.70, 1.50× (1 compile) |
| 1,000 | 223.84 → over 900 (timed out), over 4.0× | 446.28 → over 900 (timed out), over 2.0× | 167.57 → 272.63, 1.63× | 181.97 → 346.82, 1.91× (1 compile) |

Findings:

- **NFR-04's 1,000-component checkpoint fails on both compilers.** The target is ≤ 1.5× (`budgets.json`), and Phase 3's exit criterion is "budgets hold at N = 1,000, fan-in 10". clang-18 takes 1.63× and 1.91×; gcc-13 doesn't finish within 900 s. Part 2's single compiles can't satisfy `run.py`'s median-of-3 rule, so it reported the budgets at N = 100 and 1,000 as missed for that reason, not for their ratios.
- **Growth isn't linear on GCC.** NFR-04 also asks for linear growth to 1,000 components. From N = 250 to 500, Trellis's time grows 5.3× (38.21 to 203.14 s), against 2.7× for hand-wiring.
- **This is not a regression from today's commits.** With e77b6f5's headers, before the Apple Clang fix, gcc-13 at N = 250 takes 38.92 s, against 38.21 s (`pre-apple-bench.log`, one compile).
- **It is the generated graph's first measurement above N = 100.** `docs/benchmarks/v0.1.md` says sizes from 250 to 1,000 were never measured.
- **Memory follows.** Peak memory meets NFR-05 at N = 100. clang-18 takes 1.47× at N = 500 and 2.10× at 1,000 with fan-in 1-3, and 2.24× at 1,000 with fan-in 10.

### Phase 1 budgets in the latest runs

| Target | gcc-13 | clang-18 |
|---|---|---|
| NFR-03: scenario compile | +0.24 s and +0.23 s (9-compile medians); 3-compile medians range from +0.09 to +0.50 s | +0.38 s by interleaved CPU medians (T1.14, accepted by 2a); +0.42 s wall, +0.39 s CPU (sweep) |
| NFR-04: N = 100 | 1.00× (verify); 0.94× (sweep) | 0.98× (verify); 1.01× (sweep) |
| NFR-05: N = 100 memory | 0.89× (verify and sweep) | 1.11× (verify and sweep) |
| NFR-06: stripped scenario | +4,096 B | +8,240 B (accepted by 3a) |
| NFR-01: warm resolve | 1.11× wall clock, 1.10× instructions (457 / 415) | 1.05–1.07× wall clock, 1.03× instructions (446 / 432) |

`bench-runtime.sh` reports ratios but doesn't check NFR-01, so read the resolve ratio by hand. A run whose medians sit more than 5% above its fastest repetitions was disturbed (v0.1.md's rule). T1.14's results before this gate are in `docs/benchmarks/v0.1.md`, "After T1.14".

## Owner decisions (2026-11-02)

The owner answered the gate's questions with the recommended options (PROGRESS, "Owner decisions"). ADRs 0003, 0004, 0006, 0010 and 0014 and the plan record the answers (8050fa7), and the ADR index lists no provisional amendment.

- **Confirmed:**
  - TRL-E013 and TRL-E014 as new IDs;
  - ADR-0006's `detail::raise`, with the path in the instantiation note;
  - ADR-0010's overrides, which go to `make_injector` only;
  - ADR-0003's amendments: T1.2's construction rule, T1.3's amendments, the MSVC workarounds and the Apple Clang refinements;
  - T1.15's four readings, and the module's system header on Clang before 20;
  - `cycle_check` accepted but ignored until Phase 2, and `make_unique_injector` in Phase 2;
  - for misuse errors without an ID, the catalog's sentence narrowed to wiring mistakes, and the misuse numbered in Phase 4;
  - NFR-12 counts lines that are neither blank nor comments;
  - `trellis::module` keeps its name, and the docs show it written qualified.
- **MSVC:** 42 tests get cl budgets (`expect-max-lines-msvc`), and 6 get `expect-fail-msvc`.
- **Known issues for v0.1, each with a Phase 2 task:**
  - NFR-09 on MSVC (`unique_ptr` 198 hops; aggregates 15, by C1054);
  - Apple Clang's 6% aggregate margin;
  - `X&&` aggregate members, which dangle;
  - GCC's own error for an aggregate with a non-movable base.
- **The tag:** v0.1.0, once the final check-full, the full sweep and CI pass, with `docs/gates/phase-1.md` written.

Still open, none of it blocking the tag:

1. **The Phase 2 task list (T2.0).** The draft has 17 tasks and 15 owner points (scratchpad `t2.0-draft.md`). It is to be reviewed after the tag, as T2.0. It still needs tasks for:
   - the `X&&` member;
   - GCC's non-movable base;
   - cl's `std::unique_ptr` depth;
   - Clang's NFR-03 (2a);
   - from this gate, NFR-04 at N = 1,000.
2. **`const std::shared_ptr<T>&` parameters (ADR-0004).** They stay TRL-E012 for v0.1, to be revisited at the API review.
3. **Housekeeping:**
   - review the ADR set;
   - CI on a private repository;
   - delete the eight stale branches;
   - the guardrail changes. The proposed SKILL.md text could also describe `expect-max-lines-nfr07` and the three cl directives;
   - the environment setup script.

## Known issues for v0.1

CHANGELOG.md's "Known issues" (8050fa7) and getting-started's section 8 list:

- an `X&&` aggregate member dangles;
- on GCC, an aggregate whose base can't be moved fails with GCC's own error;
- on MSVC:
  - `unique_ptr` chains reach 198 hops on cl 19.51;
  - aggregate chains stop at 15;
  - only the first Trellis error in a translation unit is reported;
  - the second root's path is shown;
  - CWG 2327;
  - follow-on errors after a rejected key;
  - one error can take 49 to 612 lines;
- Apple Clang's 213-hop aggregate margin;
- `options::cycle_check` is ignored until Phase 2;
- `make_unique_injector` comes in Phase 2.

CHANGELOG's T1.14 line gives the Clang 18 budget exceptions. Add one known issue for the release: large graphs compile slowly. At 1,000 components, gcc-13 doesn't finish within 900 s (hand-wired: 224 s), and clang-18 takes 1.63 to 1.91 times as long as hand-wiring.

## Follow-ups

1. **NFR-04 at large N** (Phase 3's exit criterion; start in Phase 2).
   - Find the superlinear term on GCC: from N = 250, Trellis's compile time grows much faster than hand-wiring's.
   - T1.9 profiled N = 100 only, where code generation was most of the cost.
   - Also bring Clang's memory at N = 1,000 (2.10× and 2.24×) back toward NFR-05.
   - Add N = 250 to 1,000 to the gates that check budgets, and give the T2.0 list a task.
2. **The rest of the T2.0 list's additions:**
   - `X&&` aggregate members become TRL-E004, which amends ADR-0003's operator set;
   - braces in GCC's node and explain pass, with TRL-E002 for a member that can't be initialized from its request;
   - `std::unique_ptr` chains to 200 hops on cl, so that its XFAIL turns into XPASS;
   - Clang's NFR-03 within +0.3 s. A `result<T>` without `<variant>` changes 13 gcc-13 goldens, so it needs the owner's sign-off;
   - NFR-02, at 1.56× to 1.63× against 1.5× (draft task T2.15).
3. **Apple Clang's margin:** dispatch the Apple Clang chain test for every change under `include/trellis/static/` or `discovery/`.
4. **Phase 4, which comes before Phase 3: MSVC's diagnostics.** One report per translation unit, the second root's path, the follow-on errors after the key check, CWG 2327, and cl's line counts (49 to 612 lines).
5. **Coverage gaps:**
   - NFR-13's minimum compilers: GCC 12, Clang 16, Apple Clang 15, cl 19.38 to 19.43;
   - the unit tests on cl 19.44;
   - NFR-05's layouts, for which the harness has no option yet.
6. **Carried from Phase 1:**
   - T1.7's mixed layout costs about 3× at 1,000 bindings (Phase 3);
   - the module isn't installed for `find_package`, and module builds with GCC 15 and MSVC are untested;
   - protected tests with stale comments need the owner's leave to update;
   - 49522dd proposes larger simplifications for a Phase 2 cleanup task.
