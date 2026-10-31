# TRL-E000: placeholder for the tooling's own tests

- **Detected:** never by the library. Test snippets raise it with a `static_assert` of their own.
- **Since:** T0.1, in tests only. The library never emits it, and the catalog check (`tools/error_catalog.py`) fails if `include/` uses it.
- **Raised by:** no library entity.
- **Tests:** the sample compile-fail test [`tests/compile_fail/sample/static_assert_message.cpp`](../../tests/compile_fail/sample/static_assert_message.cpp) and its clang-18 golden; the fixtures of the runner self-test in `tests/tools/compile_fail_fixtures/`, `tests/tools/stub_compiler.py`, and the sample output in `tests/tools/golden_selftest.py`.

## What you see

The sample test's output, as its clang-18 golden records it:

```text
<src>/tests/compile_fail/sample/static_assert_message.cpp:L:C: error: static assertion failed due to requirement 'always_false<sample::widget>': trellis [TRL-E000]: sample diagnostic: no binding for T
<src>/tests/compile_fail/sample/static_assert_message.cpp:L:C: note: in instantiation of function template specialization 'sample::require_binding<sample::widget>' requested here
```

## Cause

T0.1 built the compile-fail and golden runners before the library reported any error of its own. Their tests need a diagnostic shaped like a Trellis error, with an ID on the first error line and a type name in the output, so test snippets raise one themselves. The placeholder ID keeps those snippets apart from the real IDs, which start at `TRL-E001` (plan §7.8).

## Minimal example

The sample compile-fail test, without its directives:

```cpp
#include <trellis/trellis.hpp>

namespace sample {

template <class T>
inline constexpr bool always_false = false;

template <class T>
void require_binding() {
  static_assert(always_false<T>, "trellis [TRL-E000]: sample diagnostic: no binding for T");
}

struct widget {};

}  // namespace sample

int main() { sample::require_binding<sample::widget>(); }
```

## Fix

There is nothing to fix in user code, because the library never emits this ID. A new fixture for the test tools may use it. A test of a real diagnostic expects the ID that the library emits.

## Related

- [TRL-E001](TRL-E001.md): the first ID of the library's own catalog.
