# Golden diagnostics changes

`tools/golden.py --update --reason "..."` adds an entry here whenever it writes golden files under `tests/diagnostics_golden/`. Golden changes need the owner's approval.

## 2026-10-30: clang-18 (18.1.3)

Add the sample golden that shows the golden-diagnostics runner working (task T0.1).

- added: sample/static_assert_message

## 2026-10-30: clang-18 (18.1.3)

Golden normalization now follows spike S3's rules with '/' path separators, as the owner approved on 2026-10-30 (task T0.10). Golden runs compile with -fno-color-diagnostics -fno-caret-diagnostics (GCC: -fdiagnostics-plain-output) in a clean environment with LC_ALL=C. Quoted source, carets, include chains and the 'N errors generated.' line are dropped, paths start with <src>/, and line and column numbers become :L:C.

- updated: sample/static_assert_message

## 2026-11-01: gcc-13 (13.3.0)

New goldens for T1.6's explain tests (tests/compile_fail/explain/): one per error ID (TRL-E001 to E005, E009, E010, E012, E013, E014), the incomplete owner, and the chains at the compilers' backtrace limit, on gcc-13, gcc-14, clang-18 and clang-20, as T1.6's acceptance asks. No existing golden changes.

- added: explain/deep_chain
- added: explain/e001_two_roots_through_declared
- added: explain/e002_copy_of_singleton
- added: explain/e002_declaration_mismatch_two_roots
- added: explain/e002_incomplete_owner_below
- added: explain/e002_no_constructor_equal_roots
- added: explain/e003_two_injectors
- added: explain/e004_greedy_equal_roots
- added: explain/e005_through_declared_two_roots
- added: explain/e009_override_without_binding
- added: explain/e010_two_roots
- added: explain/e012_three_forms_one_root
- added: explain/e013_strict_two_roots
- added: explain/e014_parameter_two_roots
- added: explain/eager_deep_singleton

## 2026-11-01: gcc-14 (14.2.0)

New goldens for T1.6's explain tests (tests/compile_fail/explain/): one per error ID (TRL-E001 to E005, E009, E010, E012, E013, E014), the incomplete owner, and the chains at the compilers' backtrace limit, on gcc-13, gcc-14, clang-18 and clang-20, as T1.6's acceptance asks. No existing golden changes.

- added: explain/deep_chain
- added: explain/e001_two_roots_through_declared
- added: explain/e002_copy_of_singleton
- added: explain/e002_declaration_mismatch_two_roots
- added: explain/e002_incomplete_owner_below
- added: explain/e002_no_constructor_equal_roots
- added: explain/e003_two_injectors
- added: explain/e004_greedy_equal_roots
- added: explain/e005_through_declared_two_roots
- added: explain/e009_override_without_binding
- added: explain/e010_two_roots
- added: explain/e012_three_forms_one_root
- added: explain/e013_strict_two_roots
- added: explain/e014_parameter_two_roots
- added: explain/eager_deep_singleton

## 2026-11-01: clang-18 (18.1.3)

New goldens for T1.6's explain tests (tests/compile_fail/explain/): one per error ID (TRL-E001 to E005, E009, E010, E012, E013, E014), the incomplete owner, and the chains at the compilers' backtrace limit, on gcc-13, gcc-14, clang-18 and clang-20, as T1.6's acceptance asks. No existing golden changes.

- added: explain/deep_chain
- added: explain/e001_two_roots_through_declared
- added: explain/e002_copy_of_singleton
- added: explain/e002_declaration_mismatch_two_roots
- added: explain/e002_incomplete_owner_below
- added: explain/e002_no_constructor_equal_roots
- added: explain/e003_two_injectors
- added: explain/e004_greedy_equal_roots
- added: explain/e005_through_declared_two_roots
- added: explain/e009_override_without_binding
- added: explain/e010_two_roots
- added: explain/e012_three_forms_one_root
- added: explain/e013_strict_two_roots
- added: explain/e014_parameter_two_roots
- added: explain/eager_deep_singleton

## 2026-11-01: clang-20 (20.1.2)

New goldens for T1.6's explain tests (tests/compile_fail/explain/): one per error ID (TRL-E001 to E005, E009, E010, E012, E013, E014), the incomplete owner, and the chains at the compilers' backtrace limit, on gcc-13, gcc-14, clang-18 and clang-20, as T1.6's acceptance asks. No existing golden changes.

- added: explain/deep_chain
- added: explain/e001_two_roots_through_declared
- added: explain/e002_copy_of_singleton
- added: explain/e002_declaration_mismatch_two_roots
- added: explain/e002_incomplete_owner_below
- added: explain/e002_no_constructor_equal_roots
- added: explain/e003_two_injectors
- added: explain/e004_greedy_equal_roots
- added: explain/e005_through_declared_two_roots
- added: explain/e009_override_without_binding
- added: explain/e010_two_roots
- added: explain/e012_three_forms_one_root
- added: explain/e013_strict_two_roots
- added: explain/e014_parameter_two_roots
- added: explain/eager_deep_singleton

## 2026-11-01: gcc-13 (13.3.0)

T1.6 review round: the explain pass builds each deduced class in the operator of the request that leads to it, one instantiation note per class from one line of explain.hpp (Clang prints its source once, GCC folds a run into one 'recursively required from' line), and reports through explain_cause, whose note names the path; the operators lost their StopPath and StopParameter arguments and explain_arg gained the lookahead's sequence. The error lines and the error types are unchanged. Goldens written by T1.6 itself, not yet landed.

- updated: explain/deep_chain
- updated: explain/e001_two_roots_through_declared
- updated: explain/e002_copy_of_singleton
- updated: explain/e002_declaration_mismatch_two_roots
- updated: explain/e002_incomplete_owner_below
- updated: explain/e002_no_constructor_equal_roots
- updated: explain/e004_greedy_equal_roots
- updated: explain/e005_through_declared_two_roots
- updated: explain/e010_two_roots
- updated: explain/e012_three_forms_one_root
- updated: explain/e013_strict_two_roots
- updated: explain/e014_parameter_two_roots
- updated: explain/eager_deep_singleton

## 2026-11-01: gcc-14 (14.2.0)

T1.6 review round: the explain pass builds each deduced class in the operator of the request that leads to it, one instantiation note per class from one line of explain.hpp (Clang prints its source once, GCC folds a run into one 'recursively required from' line), and reports through explain_cause, whose note names the path; the operators lost their StopPath and StopParameter arguments and explain_arg gained the lookahead's sequence. The error lines and the error types are unchanged. Goldens written by T1.6 itself, not yet landed.

- updated: explain/deep_chain
- updated: explain/e001_two_roots_through_declared
- updated: explain/e002_copy_of_singleton
- updated: explain/e002_declaration_mismatch_two_roots
- updated: explain/e002_incomplete_owner_below
- updated: explain/e002_no_constructor_equal_roots
- updated: explain/e004_greedy_equal_roots
- updated: explain/e005_through_declared_two_roots
- updated: explain/e010_two_roots
- updated: explain/e012_three_forms_one_root
- updated: explain/e013_strict_two_roots
- updated: explain/e014_parameter_two_roots
- updated: explain/eager_deep_singleton

## 2026-11-01: clang-18 (18.1.3)

T1.6 review round: the explain pass builds each deduced class in the operator of the request that leads to it, one instantiation note per class from one line of explain.hpp (Clang prints its source once, GCC folds a run into one 'recursively required from' line), and reports through explain_cause, whose note names the path; the operators lost their StopPath and StopParameter arguments and explain_arg gained the lookahead's sequence. The error lines and the error types are unchanged. Goldens written by T1.6 itself, not yet landed.

- updated: explain/deep_chain
- updated: explain/e001_two_roots_through_declared
- updated: explain/e002_copy_of_singleton
- updated: explain/e002_declaration_mismatch_two_roots
- updated: explain/e002_incomplete_owner_below
- updated: explain/e002_no_constructor_equal_roots
- updated: explain/e004_greedy_equal_roots
- updated: explain/e005_through_declared_two_roots
- updated: explain/e010_two_roots
- updated: explain/e012_three_forms_one_root
- updated: explain/e013_strict_two_roots
- updated: explain/e014_parameter_two_roots
- updated: explain/eager_deep_singleton

## 2026-11-01: clang-20 (20.1.2)

T1.6 review round: the explain pass builds each deduced class in the operator of the request that leads to it, one instantiation note per class from one line of explain.hpp (Clang prints its source once, GCC folds a run into one 'recursively required from' line), and reports through explain_cause, whose note names the path; the operators lost their StopPath and StopParameter arguments and explain_arg gained the lookahead's sequence. The error lines and the error types are unchanged. Goldens written by T1.6 itself, not yet landed.

- updated: explain/deep_chain
- updated: explain/e001_two_roots_through_declared
- updated: explain/e002_copy_of_singleton
- updated: explain/e002_declaration_mismatch_two_roots
- updated: explain/e002_incomplete_owner_below
- updated: explain/e002_no_constructor_equal_roots
- updated: explain/e004_greedy_equal_roots
- updated: explain/e005_through_declared_two_roots
- updated: explain/e010_two_roots
- updated: explain/e012_three_forms_one_root
- updated: explain/e013_strict_two_roots
- updated: explain/e014_parameter_two_roots
- updated: explain/eager_deep_singleton

## 2026-11-01: gcc-13 (13.3.0)

T1.6 review round: four of T1.6's own explain tests are renamed, as their comments described the first explain pass (deep_chain -> deep_chain_through_declared, eager_deep_singleton -> eager_singleton_eight_down, e001_two_roots_through_declared -> e001_abstract_below_declared, e002_copy_of_singleton -> e002_copy_of_singleton_below); same code and expectations. The goldens of the old names, written by T1.6 and not landed, are deleted with them.

- added: explain/deep_chain_through_declared
- added: explain/eager_singleton_eight_down
- added: explain/e001_abstract_below_declared
- added: explain/e002_copy_of_singleton_below

## 2026-11-01: gcc-14 (14.2.0)

T1.6 review round: four of T1.6's own explain tests are renamed, as their comments described the first explain pass (deep_chain -> deep_chain_through_declared, eager_deep_singleton -> eager_singleton_eight_down, e001_two_roots_through_declared -> e001_abstract_below_declared, e002_copy_of_singleton -> e002_copy_of_singleton_below); same code and expectations. The goldens of the old names, written by T1.6 and not landed, are deleted with them.

- added: explain/deep_chain_through_declared
- added: explain/eager_singleton_eight_down
- added: explain/e001_abstract_below_declared
- added: explain/e002_copy_of_singleton_below

## 2026-11-01: clang-18 (18.1.3)

T1.6 review round: four of T1.6's own explain tests are renamed, as their comments described the first explain pass (deep_chain -> deep_chain_through_declared, eager_deep_singleton -> eager_singleton_eight_down, e001_two_roots_through_declared -> e001_abstract_below_declared, e002_copy_of_singleton -> e002_copy_of_singleton_below); same code and expectations. The goldens of the old names, written by T1.6 and not landed, are deleted with them.

- added: explain/deep_chain_through_declared
- added: explain/eager_singleton_eight_down
- added: explain/e001_abstract_below_declared
- added: explain/e002_copy_of_singleton_below

## 2026-11-01: clang-20 (20.1.2)

T1.6 review round: four of T1.6's own explain tests are renamed, as their comments described the first explain pass (deep_chain -> deep_chain_through_declared, eager_deep_singleton -> eager_singleton_eight_down, e001_two_roots_through_declared -> e001_abstract_below_declared, e002_copy_of_singleton -> e002_copy_of_singleton_below); same code and expectations. The goldens of the old names, written by T1.6 and not landed, are deleted with them.

- added: explain/deep_chain_through_declared
- added: explain/eager_singleton_eight_down
- added: explain/e001_abstract_below_declared
- added: explain/e002_copy_of_singleton_below

## 2026-11-01: gcc-13 (13.3.0)

Phase 1 gate: the chain that alternates by-value and by-reference requests, behind owner decision 1A (NFR-07's count), in the four layouts of the deep-chain tests

- added: explain/alternating_chain_one_header
- added: explain/alternating_chain_call_over_three_lines
- added: explain/alternating_chain_four_headers
- added: explain/alternating_chain_four_headers_call_over_three_lines

## 2026-11-01: gcc-14 (14.2.0)

Phase 1 gate: the chain that alternates by-value and by-reference requests, behind owner decision 1A (NFR-07's count), in the four layouts of the deep-chain tests

- added: explain/alternating_chain_one_header
- added: explain/alternating_chain_call_over_three_lines
- added: explain/alternating_chain_four_headers
- added: explain/alternating_chain_four_headers_call_over_three_lines

## 2026-11-01: clang-18 (18.1.3)

Phase 1 gate: the chain that alternates by-value and by-reference requests, behind owner decision 1A (NFR-07's count), in the four layouts of the deep-chain tests

- added: explain/alternating_chain_one_header
- added: explain/alternating_chain_call_over_three_lines
- added: explain/alternating_chain_four_headers
- added: explain/alternating_chain_four_headers_call_over_three_lines

## 2026-11-01: clang-20 (20.1.2)

Phase 1 gate: the chain that alternates by-value and by-reference requests, behind owner decision 1A (NFR-07's count), in the four layouts of the deep-chain tests

- added: explain/alternating_chain_one_header
- added: explain/alternating_chain_call_over_three_lines
- added: explain/alternating_chain_four_headers
- added: explain/alternating_chain_four_headers_call_over_three_lines

## 2026-11-01: gcc-13 (13.3.0)

Phase 1 gate review: the alternating chain with aggregates mixed in, the largest output measured (37 lines by NFR-07's count on GCC 14), in the worst layout

- added: explain/alternating_aggregates_four_headers_call_over_three_lines

## 2026-11-01: gcc-14 (14.2.0)

Phase 1 gate review: the alternating chain with aggregates mixed in, the largest output measured (37 lines by NFR-07's count on GCC 14), in the worst layout

- added: explain/alternating_aggregates_four_headers_call_over_three_lines

## 2026-11-01: clang-18 (18.1.3)

Phase 1 gate review: the alternating chain with aggregates mixed in, the largest output measured (37 lines by NFR-07's count on GCC 14), in the worst layout

- added: explain/alternating_aggregates_four_headers_call_over_three_lines

## 2026-11-01: clang-20 (20.1.2)

Phase 1 gate review: the alternating chain with aggregates mixed in, the largest output measured (37 lines by NFR-07's count on GCC 14), in the worst layout

- added: explain/alternating_aggregates_four_headers_call_over_three_lines
