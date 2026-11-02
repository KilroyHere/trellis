> **Draft.** The final report replaces this draft once the last checks are in.

# Phase 1 gate: Core MVP → v0.1

| | |
|---|---|
| Date | 2026-11-01 |
| Commit | `{{COMMIT}}` |
| Exit criteria | plan §12.2, Phase 1 row, as amended by owner decisions 2a and 3a (2026-11-01) |
| Targets | plan §3.2; NFR-07 as amended by decision 1A; NFR-09 as amended by ADR-0014 (provisional) |
| Phase-level review | Failed its early run on four blocking items. The fixes landed in 3c20302 (T1.15) and 0f19f6a. Re-run on the final tree: `{{PHASE_REVIEW}}` |

**Verdict.** {{VERDICT}}

<!-- One paragraph, written once the final runs are in: the result (PASS, PASS with owner-accepted
exceptions, or FAIL); the two Clang exceptions (2a, 3a); the state of Apple Clang and MSVC; and the
owner answers that the tag waits for. -->

The verdicts below rest on the latest recorded evidence. Each `{{…}}` placeholder is a final run on `{{COMMIT}}`. If a final run disagrees with the latest evidence, its verdict changes.

## Tasks

Verify evidence comes from `.verify/<task>.json` and `build/logs/verify/` (local, not committed). Commits in parentheses record the pass.

| Task | Title | Status | Verify evidence | Commits |
|---|---|---|---|---|
| T1.1 | Meta toolkit | PASS | `check-full`: 10 of 10 presets, 1,198 s | 3c25110 (5eb362a) |
| T1.2 | Explicit constructor declaration | PASS | `check-full`: 10 of 10, 546 s | 69d1f08 (961faed) |
| T1.3 | Deduced constructor discovery | PASS | `check-full`: 10 of 10, 810 s | e88d9d9 (0871614) |
| T1.4 | Binding vocabulary and modules | PASS | `check-full`: 10 of 10, 783 s | 94689a7 (5989dc4) |
| T1.5 | Storage, lifetimes and `create<T>` | PASS | `check-full`: 10 of 10, 1,673 s; `ctest -R scenario`: 4 of 4 | de3a8d8 (8126415) |
| T1.6 | Diagnostics in C++20 form | PASS | On 8ce2d6f: `check-full` 10 of 10, 4,023 s; `-L golden` passed on gcc-13 and clang-18 | 5837eb5 (1c61096) |
| T1.7 | Test overrides and testing helpers | PASS | T1.6's `check-full`, reused on the same tree | bffb420 (1c61096) |
| T1.8 | C++20 module and single header | PASS | On 8ce2d6f: `-L module` (clang-18) and `amalgamate.py --check` passed | 320d5ed (1c61096) |
| T1.9 | v0.1 benchmarks against budgets | PASS | On 9ce6866, quiet machine (load 1.49 at the start): `bench-compile` on gcc-13 and on clang-18 `--no-scenario`, 0 budgets missed; `bench-runtime` 2 of 2 presets; `-L property` 5 of 5 | 91ddbdf (d72ed8d) |
| T1.10 | v0.1 release | This gate (owner review) | — | — |
| T1.11 | Chain-depth and arity budgets | `{{PENDING}}` | Being verified. The last record (2026-11-01 16:45) failed: ctest found no `chain_depth` tests | 06c22c2 |
| T1.12 | Documentation scaffold | PASS | `check-fast` passed; the three files exist | 729e43c, 617c3be, bb5ac78 (3b88961) |
| T1.13 | Early MSVC check of discovery and chain depth | PASS | Both `## Addendum: MSVC` checks pass (runs 41235251688, 41239761408) | 51fb862 (09322b7) |
| T1.14 | Meet the v0.1 compile-time and binary-size budgets | `{{PENDING}}` | Being verified | ef5b6e9, 0788aa9 |
| T1.15 | Constructor declared on the binding (`.with_ctor`) | `{{PENDING}}` | Being verified (`check-full` on the final tree) | 3c20302 |

Work outside the task list that the gate depends on:

- Platform fixes for MSVC and Apple Clang: a3fcf90, e17bb73, 602ec1e, 307adfa, 4d74e4c.
- The cl parse fix for `explain.hpp` (C2760): 8ce2d6f.
- The gate fixes after the phase-level review (the NFR-07 measure, the ADR index, 2a and 3a in the plan, the nightly): 0f19f6a.
- Apple Clang's 200-hop aggregate chain and Clang's elision holes: 9ce6866.
- MSVC on the Phase 1 code and decision 4a: branch `fix/msvc-phase1`, 8 commits on 9ce6866 (8d3733b to 056de53). Not on main yet.

### Left before the tag

1. Merge `fix/msvc-phase1`. `{{COMMIT}}` is main after the merge.
2. Pass T1.11, T1.14 and T1.15 on the final tree.
3. For T1.11:
   - re-record `tests/chain_depth/deepest.json` on the four Linux debug presets: it holds a48cca3's numbers, from before 9ce6866 changed `static/` and `discovery/` (CLAUDE.md requires the re-measure);
   - record the MSVC results below in ADR-0014's History, which still says they come with the first platforms run.
4. The final runs:
   - on a quiet machine: `{{FULL_SWEEP}}` and `{{BENCH_RUNTIME}}`;
   - `{{CHECK_FULL}}`, `{{CI_RUN}}` (ci.yml) and `{{PLATFORMS_RUN}}` (macOS and Windows);
   - `{{NIGHTLY_RUN}}`: plan §9.2 says a red nightly blocks releases.
5. The phase-level review on the final tree: `{{PHASE_REVIEW}}`.
6. T1.10's deliverables:
   - `docs/getting-started.md`, which doesn't exist yet;
   - README's status line, which still says "Phase 1 … is in progress";
   - the NFR-12 line count (item 26);
   - a `[0.1.0]` section in CHANGELOG.md, with the known issues;
   - this report as `docs/gates/phase-1.md`.
7. Fix the records:
   - PROGRESS has no entry for T1.11;
   - PROGRESS's gate-fix entry says six alternating-chain tests, but the tree has five (four layouts plus aggregates);
   - CHANGELOG's chain-depth line gives Clang 20's aggregate chain as 246 hops. It is 241 since 9ce6866.
8. The owner's answers below.

## Exit criteria

Plan §12.2, Phase 1: "Scenario warning-free on the pull-request matrix; NFR-01, 03, 06, 07 and 10 met; N = 100 compile ≤ 1.3× hand-written. On Clang, NFR-03 (+0.38 s) and NFR-06 (+8,240 B) are accepted for v0.1, a Phase 2 task brings NFR-03 within budget, and `budgets.json` stays strict (amended by owner decision, 2026-11-01)."

| # | Criterion | Target | Latest evidence | Final run | Verdict |
|---|---|---|---|---|---|
| 1 | Scenario warning-free on the pull-request matrix | `tests/unit/scenario_test.cpp`, whose warnings are checked as the project's, builds with no warning under `-Werror` on ci.yml's six Linux presets (gcc-13, gcc-14, clang-18, clang-20, clang-18 + libc++, ASan/UBSan), and under `/W4 /WX` on MSVC and with Apple Clang in platforms.yml | ci run 41330765582 on 8ce2d6f: 6 of 6 presets. `check-full` on 8ce2d6f: 10 of 10 presets. cl 19.51 builds every target and passes the scenario tests (run 41352149959, `fix/msvc-phase1`). Apple Clang: 640 of 640 (run 41342943535, before T1.15). After 8ce2d6f, T1.15 found and fixed a `-Wmissing-field-initializers` error in partial aggregate declarations, outside the scenario | `{{CI_RUN}}`, `{{CHECK_FULL}}`, `{{PLATFORMS_RUN}}` | `{{PENDING}}` |
| 2 | NFR-01 | Resolving the scenario root from a warm injector takes ≤ 1.1× hand-wired | T1.9's undisturbed runs: 1.07× on gcc-13, 1.01× on clang-18 (`docs/benchmarks/v0.1.md`). T1.14 runs the same 432 and 447 instructions (hand-wired: 416 and 432). T1.9 verify on 9ce6866: clang-18 1.03× (44.2 against 43.0 ns); gcc-13 0.62× (46.3 against 74.5 ns), because the hand-wired repetitions were disturbed (42.2 to 84.0 ns). The fastest repetitions were 46.1 and 42.2 ns, 1.09× (computed here) | `{{BENCH_RUNTIME}}` | PASS. Small margin on GCC |
| 3 | NFR-03 | The scenario program compiles ≤ +0.3 s slower, C++20 | gcc-13: +0.09 s (2.17 against 2.08 s; T1.9 verify on 9ce6866). clang-18: +0.38 s, the median CPU time of interleaved compiles on a quiet machine (T1.14; the script's median was +0.63 s), accepted by 2a. Not measured since: the amended verify skips the clang-18 scenario | `{{FULL_SWEEP}}` | PASS with owner-accepted exception (Clang, 2a) |
| 4 | NFR-06 | The stripped scenario binary is ≤ hand-wired + 8 KB | gcc-13: +4,096 B (22,832 against 18,736 B; T1.14); on 9ce6866, 22.3 against 18.3 KB. clang-18: +8,240 B (22,984 against 14,744 B; T1.14), 48 B over, the floor of libstdc++'s inline `atomic::wait`, accepted by 3a. With `threading::none`, +0 B on both | `{{FULL_SWEEP}}` | PASS with owner-accepted exception (Clang, 3a) |
| 5 | NFR-07, as amended by 1A | A missing binding is a compile error on GCC, Clang and MSVC without `-Werror`. The first error line names the error ID, and the same diagnostic names the type. Output is ≤ 40 lines on GCC and Clang, not counting the include chain and the excerpt of the user's call site | Every TRL error comes from `detail::raise`. The compile-fail runner compiles without `-Werror` and checks `expect-id` as a whole token on the first error line. Of 152 compile-fail tests, 102 expect an error ID: E001 7, E002 32, E003 8, E004 8, E005 16, E009 3, E010 4, E012 20, E013 2, E014 2. Under 1A's count (`expect-max-lines-nfr07: 40`), the five alternating-chain tests take 17 lines on gcc-13, 35 to 37 on gcc-14 and 33 to 35 on Clang; a 21-class chain takes 14 to 33 lines. 20 tests have goldens on gcc-13, gcc-14, clang-18 and clang-20, and `-L golden` passed on gcc-13 and clang-18 (T1.6 verify). On MSVC, the error line is C2338 with the TRL ID, and `raise`'s note names the error type. Decision 4a's alternatives are in 58 tests | `{{CHECK_FULL}}`, `{{PLATFORMS_RUN}}` | PASS |
| 6 | NFR-10 | 0 warnings under `-Wall -Wextra -Wpedantic -Wshadow -Wconversion` (GCC, Clang) and `/W4` (MSVC) | The project set adds `-Wsign-conversion -Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual`, with `-Werror`; MSVC gets `/W4 /permissive- /WX` (`cmake/TrellisWarnings.cmake`). `check-full`: 10 of 10 presets on 8ce2d6f. Since then: T1.15 fixed `-Wmissing-field-initializers`, and `tests/warnings/` checks that `-Wdangling-field` still fires; `fix/msvc-phase1` fixed cl 19.44's C4702 (5115320); cl 19.51 builds every target (run 41352149959) | `{{CHECK_FULL}}`, `{{CI_RUN}}`, `{{PLATFORMS_RUN}}` | `{{PENDING}}` |
| 7 | NFR-04 at N = 100 | The 100-component graph compiles in ≤ 1.3× hand-wired | T1.9 verify on 9ce6866: gcc-13 1.00× (10.40 against 10.42 s), clang-18 0.98× (10.26 against 10.44 s). T1.14's quiet run: 0.94× and 0.93×. The fan-in 10 graph: at most 1.20× (T1.11) | `{{FULL_SWEEP}}` | PASS |

Other targets of plan §3.2 measured in Phase 1. None is a Phase 1 exit criterion.

| Target | Phase (§12.3) | Latest | State |
|---|---|---|---|
| NFR-02: build the injector and graph, ≤ 1.5× | 2 | 1.56× gcc-13 (128.7 against 82.6 ns), 1.63× clang-18 (131.4 against 80.5 ns), T1.9 verify on 9ce6866 | Over. Draft task T2.15 |
| NFR-05: peak compiler memory at N = 100, ≤ 1.5× | not listed | 0.89× gcc-13 (340.0 against 382.8 MB), 1.11× clang-18 (277.0 against 248.5 MB), T1.9 verify on 9ce6866 | Met. Layouts not measured |
| NFR-09: ≥ 200 hops; ≥ 150 on MSVC before 19.50 (provisional) | 2 | See Platforms | Met on GCC, Clang and Apple Clang. On cl 19.51, `std::unique_ptr` reaches 198 and aggregates 15 |
| NFR-11: `-fno-exceptions` and `-fno-rtti` | 1 | `no_exceptions_test.cpp` and `no_exceptions_create_test.cpp`, built with both flags on GCC and Clang | Met for the static engine on GCC and Clang. Untested on MSVC |
| NFR-12: core headers ≤ 6,000 lines | 7–8 | 9,690 lines in 25 headers (`wc -l` on d72ed8d); 4,367 lines that are neither blank nor `//` comments | The counting method is undecided (item 26) |
| NFR-13: GCC ≥ 12, Clang ≥ 16, Apple Clang ≥ 15, MSVC ≥ 19.38 | 1 | Tested: GCC 13 and 14, Clang 18 and 20 (libc++ 18 too), Apple Clang 21, cl 19.51; cl 19.44 runs only the chain test | Partly covered. The minimums are untested |

## Phase 1 features

The plan's main tasks for Phase 1 (§12.2), with T1.15's `.with_ctor` and the work brought into the phase. Test paths are under `tests/`.

| Feature | Requirement | Task | Evidence |
|---|---|---|---|
| Meta toolkit | NFR-12 | T1.1 | `unit/type_list_test.cpp`, `type_map_test.cpp`, `fixed_string_test.cpp`, `type_name_test.cpp`, `result_test.cpp`, `result_after_injector_test.cpp`; `compile_fail/meta/` (8); `docs/internals/meta-toolkit.md` |
| Explicit discovery: `ctor_traits` and `trellis_ctor` | FR-06 | T1.2 | `unit/ctor_traits_test.cpp`; `compile_fail/discovery/` (specialization and alias cases); `docs/internals/constructor-discovery.md` |
| `.with_ctor<Args...>()` on a binding | FR-06 | T1.15 | `unit/with_ctor_test.cpp`, `with_ctor_engine_test.cpp`, `declared_aggregate_test.cpp`; `warnings/dangling_field_with_ctor.cpp`; `compile_fail/create/e002_with_ctor_*`, `e002_incomplete_with_ctor.cpp`, `eager_e002_with_ctor.cpp`; `compile_fail/explain/e001_below_with_ctor_two_roots.cpp`, `e002_with_ctor_mismatch_two_roots.cpp` |
| Deduced discovery | FR-01, FR-06 | T1.3 | `unit/deduced_test.cpp`; `unit/resolve_forms_test.cpp` (S1's 35 forms); `compile_fail/discovery/deduced_*` (17); `docs/internals/deduction.md` |
| `bind`, `to`, `in`, `to_ref`, `to_ptr`, `to_value`, `to_shared`, `allow_copy` | FR-01, FR-02 | T1.4 | `unit/bind_test.cpp`; `compile_fail/dsl/` (23) |
| Lifetimes: unique, singleton, singleton_inline, external, value | FR-02 | T1.4, T1.5 | `unit/create_test.cpp`, `injector_test.cpp` |
| Request forms and the table of §6.3 | FR-03, FR-04 | T1.4, T1.5 | `unit/request_forms_test.cpp` (a test per cell), `resolve_forms_test.cpp`; `compile_fail/static/e005_*` and `e012_*`; `compile_fail/create/e005_*`, `e012_*`, `e014_unique_ptr_without_virtual_destructor.cpp` |
| Storage and teardown | FR-02 | T1.5, T1.14 | `unit/create_test.cpp`: `TeardownReversesTheOrderOfConstruction`, `AThrowingConstructorLeavesTheSlotRetryable`, `SingletonsArePerInjector`, `UnrequestedSingletonsAreNeverBuilt`; `unit/threading_test.cpp`, also under TSan |
| `create<T>()` | FR-01, FR-03 | T1.5 | `unit/scenario_test.cpp` (4 tests); `examples/hello_world.cpp` (`example.hello_world`, and `tools.readme_example` keeps the README in step); the equivalence oracle `property/check_sharing.py`: 200 random graphs, 400 of 400 wirings (T1.9) |
| Modules and duplicate detection | FR-04, FR-05 | T1.4 | `unit/module_test.cpp`, `injector_test.cpp`; `compile_fail/static/e003_*` (4), `compile_fail/explain/e003_two_injectors.cpp` |
| Overrides | FR-05 | T1.7 | `unit/override_test.cpp`; `compile_fail/override/` (13) |
| Testing helpers: `wireable`, `create_all` | FR-05 | T1.7 | `unit/testing_test.cpp` (a gMock `StrictMock` example, per-injector isolation); `compile_fail/testing/` (3) |
| Errors E001–E005, E009, E010 and E012 in C++20 form, plus E013 and E014; the explain pass | FR-04 | T1.6 | `unit/explain_test.cpp`; `compile_fail/explain/` (33); goldens for 20 tests on gcc-13, gcc-14, clang-18 and clang-20 (`golden_manifest.txt`); 11 pages in `docs/errors/` (E000–E005, E009, E010, E012–E014) |
| The C++20 module | ADR-0008 | T1.8 | `module/` (import, headers, testing, config tests); `module.exports` (`tools/module_exports.py`); `-L module` 44 of 44 on clang-18 and clang-20 (T1.8 review) |
| The single header (a Phase 7 task, delivered early) | ADR-0008 | T1.8 | `single_header/` (a twin of every unit test); `amalgamate.py --check` |
| Eager mode, `threading::none` and strict mode (brought into Phase 1; §12.2 note) | FR-13 in part | T1.5 | `unit/create_test.cpp`, `threading_test.cpp`; `compile_fail/create/e013_strict_unbound.cpp`, `eager_e012.cpp`; `compile_fail/explain/eager_*` |
| Builds without exceptions or RTTI | NFR-11 | T1.5 | `unit/no_exceptions_test.cpp`, `no_exceptions_create_test.cpp` (GCC and Clang) |
| Chain-depth test and `max_ctor_arity` | NFR-09 | T1.11 | `chain_depth/check_chain_depth.py`, `deepest.json`; `docs/benchmarks/v0.1.md` (max_ctor_arity) |
| Benchmarks and the oracle | NFR-01, NFR-03 to NFR-06 | T1.9, T1.14 | `benchmarks/compile/gen_graph.py`, `benchmarks/scenario/trellis.{hpp,cpp}`, `benchmarks/runtime/scenario_bench.cpp`; `docs/benchmarks/v0.1.md` |
| Documentation scaffold | plan §11 | T1.12 | `docs/errors/README.md`, `docs/internals/README.md`, `CHANGELOG.md`; `tools/error_catalog.py` (`tools.error_catalog`) |

## Platforms

| Platform | Compiler | Tests | Chain test (NFR-09) | Run | On the final tree? |
|---|---|---|---|---|---|
| Linux, ci.yml | GCC 13, 14; Clang 18, 20; Clang 18 + libc++ 18; ASan/UBSan | 6 of 6 presets | 200 hops, every shape | 41330765582 on 8ce2d6f | No: `{{CI_RUN}}` |
| Linux, local `check-full` | the six above, plus gcc-13 and clang-18 release, TSan and timetrace | 10 of 10 presets (clang-18: 671 passed, 26 skipped) | as above | T1.6 verify on 8ce2d6f | No: `{{CHECK_FULL}}` |
| macOS | Apple clang 21.0.0 | 640 of 640 | aggregates 213 hops, by value 335 | 41342943535, branch `fix/apple-clang-stack` before T1.15 and 0f19f6a | No: `{{PLATFORMS_RUN}}` |
| Windows | cl 19.51 (toolset 14.51) | 538 of 544 | 8 of 10 shapes at 200 hops | 41352149959, `fix/msvc-phase1` | Not merged: `{{PLATFORMS_RUN}}` |
| Windows | cl 19.44 (toolset 14.44) | not run: platforms.yml only configures and runs the chain test there | 9 of 10 shapes at 150 hops | 41352149959 | Not merged: `{{PLATFORMS_RUN}}` |

### Apple Clang

- Run 41342943535: all 640 tests passed.
- The aggregate chain reaches 213 hops and stops on Clang's stack (`-Wstack-exhausted`). That is 13 hops, or 6%, over NFR-09's 200. The by-value chain reaches 335 and stops at the template depth limit.
- Before 9ce6866 the aggregate chain stopped below 200 (run 41327935786). With only the stack as a limit, it reached 181 hops (runs 41338913950 and 41339845579).
- The run tested the Apple branch before it was rebased onto T1.15 and the gate fixes. The final tree hasn't run on macOS.

### MSVC

- Final run 41352149959, on `fix/msvc-phase1` (9ce6866 plus 8 commits): cl 19.51 builds every target under `/W4 /permissive- /WX` and passes 538 of 544 tests.
- Decision 4a:
  - `expect-regex-msvc` alternatives went into 58 compile-fail tests;
  - `expect-max-lines-msvc` budgets went into 42, each cl's line count plus 10% (37 to 612 lines);
  - GCC and Clang ignore both directives;
  - insert-only scripts added the lines, so no existing line changed.
- Fixed on the branch (5115320):
  - the store layout on cl (`TRELLIS_STORES_ARE_PRIMARY_TEMPLATES`);
  - C4702 on cl 19.44;
  - the alias names that cl printed in diagnostics.
- cl 19.44 runs only the chain test, by design. The unit tests ran on cl 19.51 only.

The six known failures on cl 19.51:

| Test | What cl does | Cause |
|---|---|---|
| `create/e002_copy_not_copyable` | One TRL-E002, but with discovery's "no … constructor" message instead of the copy message | CWG 2327: cl can't pass a non-movable prvalue to a by-value parameter |
| `explain/e002_copy_of_singleton_below` | The same, so the error type and path regexes fail too | CWG 2327 |
| `dsl/bind_bad_key_decltype_value` | cl's own errors follow the key's `static_assert` | cl's follow-on errors after `bind<I>`'s key check (C3553, C2516 and C2882 across the three tests) |
| `dsl/bind_bad_key_named_module` | the same | the same |
| `override/override_bad_key_named_module` | the same, through `override<K>` | the same |
| `override/e009_one_error_per_override` | One TRL-E009 for two overrides with nothing to replace | cl reports one `raise` per translation unit |

### NFR-09 on MSVC

The deepest chain at the default limits, in hops, from `--deepest` in run 41349651148. `include/` is the same as in run 41352149959, where the 200- and 150-hop tests ran.

| Shape | cl 19.51 (200-hop test) | cl 19.44 (150-hop test) |
|---|---|---|
| value, reference, pointer, shared_ptr, weak_ptr, eager | 246 | 161–162 |
| unique_ptr | 198 (C2999, two short) | 162 |
| aggregate | 15 (C1054, cl's own limit; plain C++ hits it too) | 15 |
| declared | 332 | 242 |
| mixed | 288 | 194 |

- In run 41352149959, cl 19.51's 200-hop test fails `unique_ptr` (C2999) and `aggregate` (C1054), and cl 19.44's 150-hop test fails `aggregate` (C1054). The chain step is red on both jobs.
- C1054, "initializers nested too deeply", stops `C16 c{}` at 16 nested aggregates without Trellis (5115320).
- For comparison, T1.13's probe gave 245 hops on cl 19.51 and 161 on cl 19.44 (ADR-0014).
- None of this is in the repository yet. It belongs in ADR-0014's History and `deepest.json` (T1.11).

### Linux deepest chains

In hops, at the default limits. The GCC columns and the plain Clang cells are `deepest.json` at a48cca3 (T1.11). The cells marked † are after 9ce6866 (its commit message; `docs/internals/resolution-and-storage.md`).

| Shape | gcc-13, gcc-14 | clang-18 | clang-20 |
|---|---|---|---|
| by value | 294 | 310 † (the stack) | 305 † (the stack) |
| aggregates | 294 | 335 † (the depth limit) | 241 † (the stack) |
| `unique_ptr`, reference, pointer, `shared_ptr`, `weak_ptr`, eager | 294 | 251, not measured since 9ce6866 | 251, not measured since 9ce6866 |
| declared constructors | 442 | 502 | 503 |
| the mix | 354 | 335 | 335 |

The fix cost clang-20's aggregate chain 5 hops (246 to 241). The margin there is 41 hops.

## Benchmarks

### Final runs

- The full sweep, `scripts/bench-compile.sh` (plan §8.4: N up to 1,000, fan-in 1-3 and 10, gcc-13 and clang-18): `{{FULL_SWEEP}}`
- `scripts/bench-runtime.sh` on a quiet machine: `{{BENCH_RUNTIME}}`

How to read them:

- The sweep checks NFR-04's 1,000-component checkpoint (≤ 1.5×), which has never been measured. Hand-wiring N = 1,000 takes about 172 s per compile on gcc-13 (T0.3), so `docs/benchmarks/v0.1.md` expects the sweep to need more than half an hour of quiet machine.
- The sweep compiles the clang-18 scenario too, whose NFR-03 miss 2a accepts. Compare like with like:
  - the script reports the median wall time of 3 compiles: +0.63 s in T1.14's quiet run;
  - 2a's +0.38 s is the median CPU time of interleaved compiles.

  A Clang scenario above +0.38 s by interleaved compiles, or above +8,240 B, is outside the exceptions as worded. That goes back to the owner.
- `bench-runtime.sh` reports ratios. It doesn't check NFR-01, so read the resolve ratio by hand. If a run's medians sit more than 5% above its fastest repetitions, the run was disturbed (v0.1.md's rule); repeat it.

### Latest: T1.9's verify on 9ce6866

Quiet machine, load 1.49 at the start (`build/logs/verify/T1.9-1.log` to `-3.log`).

| Target | gcc-13 | clang-18 |
|---|---|---|
| NFR-03: scenario compile | +0.09 s (2.17 against 2.08 s) | not run (`--no-scenario`, decision 2a) |
| NFR-04: N = 100 compile | 1.00× (10.40 against 10.42 s) | 0.98× (10.26 against 10.44 s) |
| NFR-05: N = 100 memory | 0.89× (340.0 against 382.8 MB) | 1.11× (277.0 against 248.5 MB) |
| NFR-06: stripped scenario | 22.3 against 18.3 KB | not run |
| NFR-01: resolve | 0.62× (46.3 against 74.5 ns; hand-wired disturbed) | 1.03× (44.2 against 43.0 ns) |
| NFR-02: graph | 1.56× (128.7 against 82.6 ns) | 1.63× (131.4 against 80.5 ns) |

The property oracle passed: 5 of 5 tests on clang-18-release.

### v0.1 results after T1.14

From `docs/benchmarks/v0.1.md`, "After T1.14". T1.14's review ran it on a quiet machine, at load 0.78 to 1.00.

| Target | Budget | gcc-13 | clang-18 | Verdict |
|---|---|---|---|---|
| NFR-04: 100-component graph | ≤ 1.3× hand-wired | 0.94× (10.06 against 10.74 s) | 0.93× (10.46 against 11.24 s) | met |
| NFR-05: peak memory at 100 components | ≤ 1.5× hand-wired | 0.88× (336 against 383 MB) | 1.11× (277 against 249 MB) | met |
| NFR-03: scenario compile | ≤ +0.3 s | +0.24 s (2.30 against 2.06 s) | +0.63 s (3.28 against 2.65 s); +0.38 s by interleaved compiles | met on GCC; missed on Clang, accepted (2a) |
| NFR-06: stripped scenario | ≤ hand-wired + 8 KB | +4,096 B (22,832 against 18,736) | +8,240 B (22,984 against 14,744) | met on GCC; missed on Clang by 48 B, accepted (3a) |
| NFR-01: warm resolve | ≤ 1.1× hand-wired | 432 instructions, as at T1.5 (hand-wired 416) | 447, as at T1.5 (hand-wired 432) | met at T1.5 (1.07× and 1.01×) |

Before T1.14, T1.9 measured de3a8d8 (gcc-13 / clang-18):

- NFR-04 1.68× / 1.78×;
- NFR-05 1.30× / 1.50×;
- NFR-06 +8,192 / +12,352 B;
- NFR-03 +0.21 / +0.31 s, the median of 9 measurements;
- NFR-01 1.07× / 1.01×;
- NFR-02 1.56× / 1.57×.

`max_ctor_arity` keeps its default of 16. At N = 100 it holds NFR-04 and NFR-05 in both graph profiles: at most 1.20× the time (clang-18, fan-in 10) and 1.11× the memory (T1.11).

## Owner decisions needed before the tag

Items 1 to 19 can be confirmed as recommended. Items 20 to 27 need a real choice. Item 28 is the tag. Items 6 and 26 are added by this draft: neither is under "Waiting for owner" yet.

### Confirm (recommended as is)

1. **ADR-0006's T1.6 amendments.** Every TRL `static_assert` is in `detail::raise`, and the path is in the instantiation note, not the condition.
   - (a) Confirm.
   - (b) Put the path back in the condition: one error per root. Two protected tests and E005's argument order change.
   - **Rec: (a).** The open check is answered: cl's note names the error type, as in `E012<app::sensor,…request<R>>` with `R` in a with-block (0fa0127).
2. **ADR-0010's overrides amendment (T1.7).** `override<K>` has `bind<K>`'s vocabulary, is taken by `make_injector` only, and replaces in place. Two overrides of one key are E003; an override with nothing to replace is E009.
   - (a) Confirm.
   - (b) Let modules carry overrides.
   - **Rec: (a).**
3. **ADR-0003, T1.2's construction rule.** An aggregate is checked and built with braces; any other type with `is_constructible` and parentheses. **Rec: confirm.**
4. **ADR-0003, T1.3's amendments.**
   - Abstract and incomplete types are E002.
   - An aggregate member that no placeholder can initialize is E004.
   - gcc-13's probe declares the deleted twins.
   - E010 probes the 16 arities above the bound.
   - `max_ctor_arity` is at most 32.

   **Rec: confirm all five.**
5. **ADR-0003's MSVC workarounds,** `TRELLIS_REPEAT_THROUGH_CLASS` and `TRELLIS_ARRAY_CHECK_WITH_BRACES`.
   - (a) Confirm.
   - (b) Confirm, and build and run the unit tests on cl 19.44 in CI too. Today cl 19.44 runs only the chain test.
   - **Rec: (b).** Not blocking.
6. **ADR-0003's Apple Clang refinements (9ce6866, provisional; only in ADR-0003's History).**
   - The greedy check and the engine's aggregate checks use parentheses. cl keeps braces (`TRELLIS_AGGREGATE_CHECKS_WITH_BRACES`).
   - TRL-E004 then names the inner aggregate that holds a member that accepts anything.

   **Rec: confirm.** It lets Apple Clang reach 213 hops. It also makes Clang report TRL-E004 and TRL-E005 for aggregate members that it used to build anyway.
7. **T1.15's four readings (ADR-0003 History).**
   - `.to<C>()` after `.with_ctor` moves the declaration to `C`.
   - A second `.with_ctor` is an error.
   - E003 and E009 for a declared binding print `detail::declared_impl<…>`.
   - A declaration is checked at use.

   **Rec: confirm all four.**
8. **Declared aggregates in the module on Clang before 20.** The module's build marks `declared_aggregate.hpp` a system header. So with Clang before 20 and with Apple Clang, no warning located there reaches importers, `-Wdangling-field` included.
   - (a) Accept for v0.1.
   - (b) Drop the system header, and let those users see `-Wmissing-field-initializers`.
   - **Rec: (a).**
9. **TRL-E013 and TRL-E014 become stable with v0.1.**
   - (a) Confirm both as new IDs.
   - (b) Fold them into E001 and E005.
   - **Rec: (a),** before the tag.
10. **Misuse errors without a TRL ID.** 31 of the 58 `static_assert`s in `include/` have no ID, 22 of them for public-API misuse.
    - (a) Give the public-API misuse IDs now.
    - (b) Narrow `docs/errors/README.md`'s "every mistake" sentence to wiring mistakes for v0.1, and number misuse in Phase 4's catalog.
    - (c) Leave both.
    - **Rec: (b).**
11. **`options::cycle_check` is accepted and ignored in v0.1.**
    - (a) Keep it as documented.
    - (b) Reject `true` with a `static_assert` until Phase 2.
    - (c) Remove the field until Phase 2.
    - **Rec: (a).**
12. **`make_unique_injector` (plan §7.5)** isn't implemented.
    - (a) Phase 2, in draft task T2.2.
    - (b) Add it to v0.1.
    - (c) Drop it, and document the two forms that work.
    - **Rec: (a),** with the two forms documented now.
13. **`const std::shared_ptr<T>&` parameters** stay TRL-E012 unless `shared_ptr<T>` is bound, to be revisited at the API review. **Rec: confirm.**
14. **`expect-max-lines-msvc` under 4a.** 42 tests carry a cl budget, cl's line count plus 10% (37 to 612 lines), which replaces `expect-max-lines` on cl only. NFR-07's 40 lines are defined for GCC and Clang (plan §3.2), and NFR-07 on every compiler is Phase 4's exit criterion.
    - (a) Confirm it as part of 4a.
    - (b) No line budget on cl until Phase 4.
    - (c) Hold cl to the GCC and Clang budgets, which leaves 42 tests red.
    - **Rec: (a).** It catches regressions in cl's output and loosens nothing on GCC or Clang.
15. **CWG 2327 on cl.** On cl, a constructor that takes a non-movable class by value can't be wired: cl reports TRL-E002, "no constructor" (2 tests). GCC and Clang wire it.
    - (a) Document it as a cl limitation in v0.1, and look for a workaround in Phase 4.
    - (b) Hold the tag for a workaround.
    - **Rec: (a).**
16. **`X&&` aggregate members dangle.** This is undefined behaviour on GCC and Clang, on main since T1.5. The member binds to a temporary that dies at the end of the creation expression. The likely fix, a conversion that yields `const T` for aggregate members, changes ADR-0003's operator set, discovery's probe and the explain pass.
    - (a) Ship it as a known issue, with a Phase 2 task and an ADR-0003 amendment.
    - (b) Fix it before the tag.
    - **Rec: (a).**
17. **GCC's raw error for an aggregate with a non-movable base.** The check passes, then the creation code fails with GCC's "use of deleted function" (CWG 2403). The fix: braces in GCC's node and explain pass, and a TRL-E002 report.
    - (a) A known issue, fixed in Phase 2.
    - (b) Fix it before the tag.
    - **Rec: (a).**
18. **The ADR set.** `docs/adr/README.md` indexes ADR-0001 to ADR-0014 and their provisional amendments. **Rec: review and confirm the index.**
19. **Owner actions, not blocking:**
    - Delete the stale branches `wip/t0.1-scaffold`, `t0.2-ci`, `t0.9-ci-polish`, `t0.10-goldens` and `t1.13-msvc-probe`, now also `fix/platforms` and `fix/apple-clang-stack` (landed as 9ce6866), and `fix/msvc-phase1` once merged. The proxy here refuses branch deletion.
    - The guardrail changes:
      - `protect_files.py` protects committed files only;
      - add `tests/golden_manifest.txt` to the protected paths;
      - the proposed `add-compile-fail-test` SKILL.md text, which should now also describe `expect-max-lines-nfr07`, `expect-regex-msvc` and `expect-max-lines-msvc`.
    - The environment setup script's packages (listed in PROGRESS).
    - CI on a private repository: Linux runs on every push, and macOS and Windows weekly or on demand. A public repository would run the full matrix on every push (see item 24).

### Needs a real choice

20. **NFR-13's MSVC minimum and the 150-hop bound (T1.13, ADR-0014, provisional).** cl 19.44 (VS 2022) reaches 161 to 162 hops, and cl 19.51 reaches 246 (run 41349651148).
    - (a) Keep 19.38 as the minimum, with NFR-09 at ≥ 150 hops on MSVC before 19.50. 19.50 is an assumed boundary: 19.45 to 19.50 aren't on GitHub's image.
    - (b) Raise the MSVC minimum to 19.51.
    - **Rec: (a).** cl 19.44 passes the 150-hop test in every shape except aggregates (item 21), and (b) would drop VS 2022. It is needed before draft task T2.3 (the cycle check), not before the tag.
21. **NFR-09 on MSVC.**
    - On cl 19.51, `std::unique_ptr` chains reach 198 hops (C2999), two short of 200.
    - On both cl versions, aggregate chains reach 15 hops (C1054). That is cl's limit on nested initializers, which `C16 c{}` hits without Trellis (5115320).
    - Every other shape reaches 246 to 332 hops on cl 19.51 and 161 to 242 on cl 19.44.

    Options:
    - (a) Record C1054 as a platform limit on aggregate chains on MSVC, in NFR-09 and ADR-0014, and have the chain test expect it on cl. Accept 198 for v0.1, with a Phase 2 task to bring `unique_ptr` to 200. NFR-09 is a Phase 2 requirement (§12.3).
    - (b) Recover the two hops before the tag.
    - (c) Amend NFR-09 to ≥ 150 hops on every MSVC.

    **Rec: (a).** Until this is decided, the chain step stays red on both MSVC jobs.
22. **The six cl compile-fail failures.**
    - (a) A new directive, `// expect-fail-msvc: <reason>`. It is strict: the test must still fail on cl, and an unexpected pass fails it. Insert-only scripts add it, as under 4a, and platforms can go green.
    - (b) Platforms stays red until Phase 4 fixes or documents them.
    - **Rec: (a).** A red run hides new failures, which is the reasoning behind 2a's nightly step.

    The reasons, by test:
    - `create/e002_copy_not_copyable` and `explain/e002_copy_of_singleton_below`: CWG 2327 (item 15);
    - `dsl/bind_bad_key_decltype_value`, `dsl/bind_bad_key_named_module` and `override/override_bad_key_named_module`: cl adds C3553, C2516 or C2882 after the key's `static_assert`;
    - `override/e009_one_error_per_override`: one `raise` per translation unit (item 23).
23. **cl's one raise per translation unit, and its second-root path.**
    - On cl, two independent causes in one translation unit print as one TRL error: two overrides with nothing to replace give one E009.
    - For a cause that two roots reach, cl prints the path from the second root, whose body it instantiates first. GCC and Clang print the first root's path, and 4a's alternatives encode cl's.

    What this means for ADR-0006 on cl:
    - "one error per root cause" holds there only as "at most one error per compile": a second cause shows only after the first is fixed;
    - the printed path is correct, but it starts at another root.

    Options:
    - (a) Say so in ADR-0006's MSVC section for v0.1, and take it up in Phase 4 (NFR-07 on all compilers).
    - (b) Before the tag, look for a reporting mechanism that gives cl one error per cause.

    **Rec: (a).**
24. **Apple Clang's 213-hop margin.** The aggregate chain stops on the stack at 213 hops, 6% over NFR-09's 200. macOS runs weekly or on demand, so a regression can land unseen for up to a week.
    - (a) Accept it for v0.1. Dispatch the Apple Clang chain test for every Phase 2 change under `include/trellis/static/` or `discovery/`, and record the margin at each gate.
    - (b) Find more stack per hop before the tag. No measured variant beats 213 while it keeps the greedy check: braces then parentheses gave 197 (run 41343816780), and parentheses in the node's requires-expression gave 170.
    - (c) Make the repository public, so that macOS runs on every push.
    - **Rec: (a).**
25. **`trellis::module` and C++20's module directive.** After `using namespace trellis;`, `module m = make_module(...);` at namespace scope is a module directive on Clang 18 and 20.
    - (a) Rename the class before v0.1 fixes the name (about 28 mentions).
    - (b) Keep it, document `trellis::module` or `auto`, and revisit at the API review.
    - **Rec:** (a) if the owner has a name now, otherwise (b). After v0.1, a rename breaks users and needs a migration note.
26. **NFR-12's counting method.** T1.10 must report the count. On d72ed8d, `include/trellis` holds 25 headers:
    - 9,690 lines in all (`wc -l`);
    - 9,478 without `testing/`;
    - 4,367 that are neither blank nor `//` comments.

    The target is ≤ 6,000 lines, and `fix/msvc-phase1` adds 95 net lines. The T2.0 draft raises the same question (its plan issue 6).
    - (a) Count the lines that are neither blank nor comments.
    - (b) Count every line.
    - **Rec: (a).** NFR-12 itself asks for every trick to be commented, and (b) would count those comments against it.
27. **The Phase 2 task list.** The T2.0 draft has 17 tasks, not yet in docs/tasks.json: `/tmp/claude-0/-home-user-trellis/2d27a639-9f98-5872-8a89-4b383c577f29/scratchpad/t2.0-draft.md`, with `t2.0-draft.json` beside it. Its 15 points, each with the draft's default:
    1. Captive rule through owning edges (T2.7): TRL-E006 when a singleton owns a unique object that holds a scoped reference.
    2. `weak_ptr` and the captive rule (T2.7): E006 applies as a rule on top of the table, and the scoped × `weak_ptr` cell stays ✓.
    3. The inner request of `lazy` and `provider` (T2.4, T2.7): `R` is checked as the consumer's own request of `R`.
    4. A lazy's cache under `threading::safe` (T2.4): built once, through the existing slot state.
    5. The shared-scope option (T2.2): `begin_scope<Tag, trellis::threading::safe>(seeds...)`; without it, a scope is single-thread.
    6. Seeds (T2.2): `seed<T&>`, `seed<const T&>` and `seed<T>` only, with no `seed<std::shared_ptr<T>>`. A seed of a key that is already provided is TRL-E003.
    7. `trellis::testing::validate` (T2.3): `testing::validate<Injector, Roots...>()`, with `cycle_check` forced on.
    8. E007's line budget (T2.3): 40 lines, counted as 1A counts them.
    9. The MSVC bound (T2.3): 150 hops before cl 19.50, unless item 20 is answered (b).
    10. Reporting R004 (T2.6): throw `trellis::resolution_error` with exceptions; without them, call the handler, then `std::abort()`. ADR-0005 needs a line.
    11. NFR-06 on Clang (T2.16): measure again, then a lasting exception or an amended NFR-06. Which kilobyte NFR-06 means is decided there too.
    12. Publishing packages (T2.14): in-repository recipes, tested locally and in CI. Publishing at v0.2 is the owner's step.
    13. TRL-E007 for declared constructors (T2.8): keep it, and drop it if Phase 2 runs long.
    14. Runtime targets for scopes, `lazy` and `provider` (T2.15): record the numbers and enforce none.
    15. Eager mode and scopes (T2.1): `make_injector` builds only singletons, and `begin_scope` builds nothing eagerly.

    The draft predates 9ce6866 and the MSVC findings, so it needs tasks or lines for items 16, 17 and 21. Its plan issue 13 matters too: T4.0 depends on T2.0 alone, so Phase 4's expansion becomes ready beside T2.1. Waiting for the v0.2 gate would need an amendment.

    **Rec:** approve with the defaults and those additions.

### The tag

28. **Tag v0.1.0.** The preconditions:
    - every placeholder in this report is filled;
    - every exit criterion is PASS or owner-accepted;
    - T1.11, T1.14 and T1.15 pass;
    - items 1 to 27 are answered or explicitly deferred;
    - CHANGELOG.md has its `[0.1.0]` section and README its new status line.

    **Rec:** approve once the final runs confirm the latest numbers.

## Known issues for v0.1

For CHANGELOG.md and README.md. Adjust them to the owner's answers.

**All compilers**

- An aggregate with an `X&&` member is built with that member bound to a temporary that dies at the end of the creation expression, so the member dangles. Phase 2 makes this TRL-E004.
- `options::cycle_check` is accepted and ignored. A dependency cycle gives the compiler's own errors (ADR-0014). Phase 2 adds the opt-in TRL-E007.
- A `const std::shared_ptr<T>&` parameter is TRL-E012 unless `std::shared_ptr<T>` is bound. Take `std::shared_ptr<T>` by value.
- `make_unique_injector` isn't available. A member initialized from `make_injector(...)`, or `std::unique_ptr<I>(new auto(make_injector(...)))`, works.
- If `trellis::module` keeps its name: after `using namespace trellis;`, write `trellis::module` or `auto`, because Clang reads `module m = ...;` as a module directive.

**GCC**

- An aggregate with a base class that can't be moved passes the check, then fails in the creation code with GCC's "use of deleted function", without a TRL ID.

**Clang**

- With Clang 18, the scenario compiles 0.38 s slower than hand-wired (target 0.3 s). With libstdc++ its stripped binary grows by 8,240 B (target 8 KB); `threading::none` removes that growth. Both are accepted for v0.1.
- In the C++20 module built by Clang before 20 or by Apple Clang, warnings located in `declared_aggregate.hpp`, `-Wdangling-field` included, don't reach importers.

**MSVC (cl 19.51; tested weekly or on demand)**

- A constructor that takes a non-movable class by value can't be wired. cl reports TRL-E002.
- A bad key in `bind<I>` or `override<K>` is followed in some forms by cl's own errors (C3553, C2516, C2882).
- Two independent Trellis errors in one translation unit show as one. For a cause that two roots reach, cl names the path from the second root.
- Diagnostics are longer than on GCC and Clang: a 21-class chain prints 552 lines. NFR-07's 40-line budget covers GCC and Clang until Phase 4.
- Dependency chains of aggregates stop at 15 hops (C1054, a cl limit). `std::unique_ptr` chains stop at 198 hops on cl 19.51. On cl 19.44 (VS 2022), chains of deduced classes stop at 161 to 162 hops.

**Not measured yet**

- NFR-05's binding layouts. NFR-04 at 1,000 components, unless the full sweep measures it.
- NFR-02, building the injector and its graph, takes 1.56 to 1.63 times as long as hand-wiring, against a target of 1.5. It is a Phase 2 target.

## Follow-ups for Phase 2

Additions to the T2.0 draft, and items for later phases.

1. **New task:** `X&&` aggregate members, and a declared `ctor<X&&>` of an aggregate, become TRL-E004 (item 16). It amends ADR-0003's operator set. The likely check misses `int&&` and `const X&&` members (`docs/internals/resolution-and-storage.md`).
2. **New task, or a line in an existing one:** GCC's node and explain pass use braces, and a member whose request resolves but can't initialize it is TRL-E002 (item 17).
3. **NFR-09 on MSVC (item 21):**
   - bring `std::unique_ptr` chains to 200 hops on cl 19.51;
   - record C1054 for aggregate chains;
   - add `msvc-debug` records for cl 19.51 and 19.44 to `deepest.json`, which has none.
4. **Apple Clang's margin (item 24):** a platforms dispatch in the verify of every task that changes `static/` or `discovery/`.
5. **Draft task T2.16, Clang's NFR-03.** `docs/benchmarks/v0.1.md` splits Clang's extra scenario instructions into:
   - the header, 0.279 G;
   - the slot's waiting code, 0.122 G;
   - discovery, checks and creation, 0.313 G.

   A `result<T>` fallback without `<variant>` cuts the header alone by 3%, but changes 13 gcc-13 goldens. That needs the owner's sign-off; the patch is in T1.14's scratch directory.
6. **Draft task T2.15:** NFR-02 is at 1.56× to 1.63× against 1.5× (T1.9's verify).
7. **Measurement gaps:**
   - NFR-05's layouts (one module per 10 components, several translation units; plan §8.4), for which the harness has no option yet;
   - NFR-04 at N = 1,000, if the full sweep can't finish on a quiet machine.
8. **NFR-13's minimums** are untested: GCC 12, Clang 16, Apple Clang 15, and cl 19.38 to 19.43. Plan §9.2 also lists Apple Clang 16 and clang-cl. The unit tests on cl 19.44 belong here too (item 5).
9. **Phase 4, which comes before Phase 3:** MSVC's diagnostics:
   - one error per translation unit, and the second root's path (item 23);
   - the follow-on errors after the key check;
   - a workaround for CWG 2327 (item 15);
   - line counts: 552 lines for a 21-class chain, and 556 for the alternating chain.
10. **Carried from Phase 1:**
    - T1.7's mixed layout costs about 3× at 1,000 bindings (Phase 3's budget);
    - the module isn't installed for `find_package`, and module builds with GCC 15 and MSVC are untested (T1.8);
    - protected tests with stale comments (`static/e005_*`, `static/e012_*` and six `create/` tests) need the owner's leave to update (T1.6).
