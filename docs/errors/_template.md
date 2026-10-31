<!--
The template for an error-catalog page. Copy it to docs/errors/<ID>.md, such as TRL-E001.md,
replace every <...>, delete this comment, and link the page from the index (README.md).
Keep the sections and their order, so every page reads the same way (plan §11).
-->

# TRL-E0xx: <meaning, as the index words it>

- **Detected:** <at compile time | at run time>
- **Since:** <the task that first emits it, and the release, once there is one>
- **Raised by:** <the public entities whose doc comments list this ID, such as `injector::create<T>()`>
- **Tests:** <the compile-fail test, such as `tests/compile_fail/<area>/<name>.cpp`, and its goldens in `tests/diagnostics_golden/`; or the test of a runtime error>

## What you see

<The ID's first error line on GCC and on Clang, normalized as in the goldens, or the runtime error's message. The goldens hold the full output.>

```text
<src>/app.cpp:L:C: error: static assertion failed: TRL-E0xx: <summary>
```

## Cause

<What the mistake is, in one or two sentences, and why Trellis rejects it instead of guessing.>

## Minimal example

```cpp
<The smallest program that raises the error: the compile-fail test, without its directives.>
```

## Fix

<Each way to fix it, the most common first, with the corrected code.>

## Related

- [TRL-E0yy](TRL-E0yy.md): <how it differs from this error.>
