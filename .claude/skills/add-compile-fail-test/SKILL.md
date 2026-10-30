---
name: add-compile-fail-test
description: Add a compile-fail test, a snippet that must not compile, with expectations on the error ID, message and output size. Use for every diagnostic Trellis promises and every documented limit.
---

Compile-fail tests live in `tests/compile_fail/<area>/<name>.cpp`. `tools/compile_fail.py` runs them (CTest label `compile_fail`) with each preset's compiler and flags.

Start the file with these directives:

```cpp
// compile-fail: App needs IRepository, which has no binding
// expect-id: TRL-E001
// expect-regex: no binding for .*IRepository
// expect-max-lines: 40
```

- `compile-fail`: one line saying what misuse the test shows.
- `expect-id`: the stable error ID from plan §7.8, once the diagnostic exists.
- `expect-regex`: one or more. Every regex must match the compiler output. Match the type name and the meaning, not compiler-specific wording, unless that wording is the point.
- `expect-max-lines`: the output budget. The default is 60; NFR-07 sets 40 for wiring errors.

Steps:

1. Write the smallest program that shows the misuse once. Include only public headers.
2. Run `ctest --preset clang-18-debug -L compile_fail -R <name>`, then the same on gcc-13. The test must fail to compile for the expected reason. A failure for some other reason, such as a typo, doesn't count.
3. If the exact wording matters, add a golden test as well, through `tools/golden.py` for new files.

Existing compile-fail tests are protected and must not be edited to make them pass.
