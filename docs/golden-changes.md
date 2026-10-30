# Golden diagnostics changes

`tools/golden.py --update --reason "..."` adds an entry here whenever it writes golden files under `tests/diagnostics_golden/`. Golden changes need the owner's approval.

## 2026-10-30: clang-18 (18.1.3)

Add the sample golden that shows the golden-diagnostics runner working (task T0.1).

- added: sample/static_assert_message

## 2026-10-30: clang-18 (18.1.3)

Golden normalization now follows spike S3's rules with '/' path separators, as the owner approved on 2026-10-30 (task T0.10). Golden runs compile with -fno-color-diagnostics -fno-caret-diagnostics (GCC: -fdiagnostics-plain-output) in a clean environment with LC_ALL=C. Quoted source, carets, include chains and the 'N errors generated.' line are dropped, paths start with <src>/, and line and column numbers become :L:C.

- updated: sample/static_assert_message
