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
