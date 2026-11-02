# Task amendments

Owner-approved corrections to docs/tasks.json, recorded by `scripts/tasks.py amend`. Newest last.

## 2026-10-31 18:27 UTC: T0.8.verify

- **Reason:** The original check '! grep -L Status docs/adr/*.md' inverts on GNU grep 3.5 and later (grep -L exits 0 when any file matches), so it failed exactly when every ADR had a Status line. The replacement checks the intent: no ADR lacks a Status line.
- **Approved by:** Aryan Patel (owner), approved in the session on 2026-10-31
- **Old:** `["test $(ls docs/adr/*.md | wc -l) -ge 13", "! grep -L 'Status' docs/adr/*.md"]`
- **New:** `["test $(ls docs/adr/*.md | wc -l) -ge 13", "test -z \"$(grep -L Status docs/adr/*.md)\""]`

## 2026-10-31 18:27 UTC: T1.6.acceptance

- **Reason:** The owner reworded NFR-07 on 2026-10-30 after spike S3: gcc-13 in C++20 mode puts the type on the line after the first error line. Recorded in ADR-0006.
- **Approved by:** Aryan Patel (owner), approved in the session on 2026-10-31
- **Old:** `["Error types exist for E001-E005, E009, E010 and E012. Exactly one static_assert fires per root cause, with no cascading errors.", "A minimal explain pass reports the path from the root.", "Every ID has a compile-fail test, and there are golden tests for gcc-13, gcc-14, clang-18 and clang-20.", "NFR-07 holds: the first error line names the type and the ID, and output stays at or under 40 lines on GCC and Clang."]`
- **New:** `["Error types exist for E001-E005, E009, E010 and E012. Exactly one static_assert fires per root cause, with no cascading errors.", "A minimal explain pass reports the path from the root.", "Every ID has a compile-fail test, and there are golden tests for gcc-13, gcc-14, clang-18 and clang-20.", "NFR-07 holds, as amended by ADR-0006: the first error line names the error ID, the same diagnostic names the type, and output stays at or under 40 lines on GCC and Clang."]`

## 2026-10-31 18:27 UTC: T4.0.depends_on

- **Reason:** The owner ordered the diagnostics phase (plan Phase 4) before the binding-features phase (plan Phase 3) on 2026-10-30.
- **Approved by:** Aryan Patel (owner), approved in the session on 2026-10-31
- **Old:** `["T3.0"]`
- **New:** `["T2.0"]`

## 2026-10-31 18:27 UTC: T3.0.depends_on

- **Reason:** Same owner decision: binding features (plan Phase 3) come after diagnostics (plan Phase 4). When T4.0 expands Phase 4, the last Phase 4 task should become T3.0's dependency, through another amendment.
- **Approved by:** Aryan Patel (owner), approved in the session on 2026-10-31
- **Old:** `["T2.0"]`
- **New:** `["T4.0"]`

## 2026-11-01 16:56 UTC: T1.6.acceptance

- **Reason:** Owner decision 1A: chains that alternate by-value and by-reference requests exceed 40 lines on GCC 14 only through lines the user's own layout produces (the include chain and the call-site excerpt), which ADR-0003's two-operator design can't fold. Measured without those lines, every case fits in 40.
- **Approved by:** owner, in the session on 2026-11-01, answering the four Phase 1 decisions with the recommended options: "Exclude user-layout lines (Recommended)"
- **Old:** `["Error types exist for E001-E005, E009, E010 and E012. Exactly one static_assert fires per root cause, with no cascading errors.", "A minimal explain pass reports the path from the root.", "Every ID has a compile-fail test, and there are golden tests for gcc-13, gcc-14, clang-18 and clang-20.", "NFR-07 holds, as amended by ADR-0006: the first error line names the error ID, the same diagnostic names the type, and output stays at or under 40 lines on GCC and Clang."]`
- **New:** `["Error types exist for E001-E005, E009, E010 and E012. Exactly one static_assert fires per root cause, with no cascading errors.", "A minimal explain pass reports the path from the root.", "Every ID has a compile-fail test, and there are golden tests for gcc-13, gcc-14, clang-18 and clang-20.", "NFR-07 holds, as amended by ADR-0006: the first error line names the error ID, the same diagnostic names the type, and output stays at or under 40 lines on GCC and Clang. The count excludes the lines the user's own layout produces: the include chain (In file included from) and the excerpt of the user's call site (owner decision 1A, 2026-11-01)."]`

## 2026-11-01 16:56 UTC: T1.9.acceptance

- **Reason:** Owner decisions 2a and 3a accept the two Clang misses for v0.1.
- **Approved by:** owner, in the session on 2026-11-01, answering the four Phase 1 decisions with the recommended options: "Accept for v0.1 (Recommended)" and "Accept the 48 bytes (Recommended)"
- **Old:** `["There is a trellis variant in gen_graph.py and in the runtime benchmark.", "The equivalence oracle passes on 200 random graphs (Trellis versus hand-wired checksums).", "NFR-01, NFR-03, NFR-06, and a 100-component compile within 1.3x of hand-wiring, are measured on gcc-13 and clang-18 and recorded in docs/benchmarks/v0.1.md.", "If a budget is missed, stop and report it. Never change budgets.json to make a result pass."]`
- **New:** `["There is a trellis variant in gen_graph.py and in the runtime benchmark.", "The equivalence oracle passes on 200 random graphs (Trellis versus hand-wired checksums).", "NFR-01, NFR-03, NFR-06, and a 100-component compile within 1.3x of hand-wiring, are measured on gcc-13 and clang-18 and recorded in docs/benchmarks/v0.1.md.", "If a budget is missed, stop and report it. Never change budgets.json to make a result pass.", "Owner decisions 2a and 3a, 2026-11-01: on clang-18, NFR-03 (+0.38 s against +0.3 s) and NFR-06 (+8,240 B against +8 KB) are accepted for v0.1 and recorded as exceptions in docs/benchmarks/v0.1.md; a Phase 2 task brings them within budget. Every other budget holds on both compilers."]`

## 2026-11-01 16:56 UTC: T1.9.verify

- **Reason:** Owner decision 2a accepts clang-18's NFR-03 miss for v0.1, so the Clang run skips only the scenario program (NFR-03); budgets.json stays strict so the nightly job keeps flagging it until Phase 2 fixes it.
- **Approved by:** owner, in the session on 2026-11-01, answering the four Phase 1 decisions with the recommended options: "Accept for v0.1 (Recommended)"
- **Old:** `["scripts/bench-compile.sh --quick", "scripts/bench-runtime.sh", "ctest --preset clang-18-release -L property"]`
- **New:** `["scripts/bench-compile.sh --quick --compilers gcc-13", "scripts/bench-compile.sh --quick --compilers clang-18 --no-scenario", "scripts/bench-runtime.sh", "ctest --preset clang-18-release -L property"]`

## 2026-11-01 16:56 UTC: T1.14.acceptance

- **Reason:** Owner decisions 2a and 3a accept clang-18's NFR-03 and NFR-06 misses for v0.1; a Phase 2 task brings them within budget.
- **Approved by:** owner, in the session on 2026-11-01, answering the four Phase 1 decisions with the recommended options: "Accept for v0.1 (Recommended)" and "Accept the 48 bytes (Recommended)"
- **Old:** `["With T1.9's Trellis variants, scripts/bench-compile.sh --quick reports no missed budget on a quiet machine (1-minute load below 1.5), and the numbers are recorded in docs/benchmarks/v0.1.md.", "NFR-06 holds on gcc-13 and clang-18 for the scenario with the default threading option, measured as T1.9 measured it.", "Behaviour is unchanged: every existing test passes as it is, including the threading, scenario, sanitizer and compile-fail tests, and budgets.json is unchanged."]`
- **New:** `["With T1.9's Trellis variants, scripts/bench-compile.sh --quick reports no missed budget on a quiet machine (1-minute load below 1.5), except the owner-accepted v0.1 exception for NFR-03 on clang-18, and the numbers are recorded in docs/benchmarks/v0.1.md.", "NFR-06 holds on gcc-13 for the scenario with the default threading option, measured as T1.9 measured it; clang-18's +8,240 B (48 B over, the floor of libstdc++'s inline atomic::wait) is an owner-accepted v0.1 exception (decision 3a, 2026-11-01).", "Behaviour is unchanged: every existing test passes as it is, including the threading, scenario, sanitizer and compile-fail tests, and budgets.json is unchanged."]`

## 2026-11-01 16:56 UTC: T1.14.verify

- **Reason:** Owner decision 2a accepts clang-18's NFR-03 miss for v0.1, so the Clang run skips only the scenario program; budgets.json stays strict.
- **Approved by:** owner, in the session on 2026-11-01, answering the four Phase 1 decisions with the recommended options: "Accept for v0.1 (Recommended)"
- **Old:** `["scripts/check-full.sh", "scripts/bench-compile.sh --quick"]`
- **New:** `["scripts/check-full.sh", "scripts/bench-compile.sh --quick --compilers gcc-13", "scripts/bench-compile.sh --quick --compilers clang-18 --no-scenario"]`

## 2026-11-02 03:25 UTC: T1.14.verify

- **Reason:** The gcc-13 quick benchmark's 3-compile medians swing by more than NFR-03's margin on this shared VM. They read +0.32 s and +0.50 s for the scenario, with Trellis compiles alone ranging from 2.21 to 2.65 s in one run, while 9-compile medians gave +0.24 s and +0.23 s twice, as T1.14 recorded. The budget is unchanged; --runs 9 only takes more compiles for its median.
- **Approved by:** owner, in the session on 2026-11-02, answering 'Amend to 9 compiles (Recommended)' to the question on settling T1.14's verify
- **Old:** `["scripts/check-full.sh", "scripts/bench-compile.sh --quick --compilers gcc-13", "scripts/bench-compile.sh --quick --compilers clang-18 --no-scenario"]`
- **New:** `["scripts/check-full.sh", "scripts/bench-compile.sh --quick --compilers gcc-13 --runs 9", "scripts/bench-compile.sh --quick --compilers clang-18 --no-scenario"]`
