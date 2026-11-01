# TRL-E003: Duplicate binding for a key

- **Detected:** at compile time
- **Since:** T1.4 (unreleased; v0.1)
- **Raised by:** `trellis::make_injector`
- **Tests:** [`tests/compile_fail/static/e003_duplicate_across_modules.cpp`](../../tests/compile_fail/static/e003_duplicate_across_modules.cpp), [`e003_same_binding_twice.cpp`](../../tests/compile_fail/static/e003_same_binding_twice.cpp), and [`e003_then_report_request_form.cpp`](../../tests/compile_fail/static/e003_then_report_request_form.cpp) and [`e003_then_request_check.cpp`](../../tests/compile_fail/static/e003_then_request_check.cpp), which use the injector afterwards and still get this one error; the duplicate checks in [`tests/unit/injector_test.cpp`](../../tests/unit/injector_test.cpp). Golden tests come with T1.6.

## What you see

GCC puts the ID on the error line and the error type, which names the key and both bindings, in the note after it:

```text
<src>/include/trellis/static/injector.hpp:L:C: error: static assertion failed: TRL-E003: duplicate binding for a key: each key may be bound only once per injector
<src>/include/trellis/static/injector.hpp:L:C: note: 'trellis::error::E003<{anonymous}::logger, trellis::v1::binding<{anonymous}::logger, {anonymous}::console_logger, trellis::v1::singleton_t, false>, trellis::v1::binding<{anonymous}::logger, {anonymous}::file_logger, trellis::v1::unique_t, false> >::reported' evaluates to false
```

Clang puts both on the error line:

```text
<src>/include/trellis/static/injector.hpp:L:C: error: static assertion failed due to requirement 'error::E003<(anonymous namespace)::logger, trellis::binding<(anonymous namespace)::logger, (anonymous namespace)::console_logger, trellis::singleton_t, false>, trellis::binding<(anonymous namespace)::logger, (anonymous namespace)::file_logger, trellis::unique_t, false>>::reported': TRL-E003: duplicate binding for a key: each key may be bound only once per injector
```

The error type is `E003<Key, First, Second>`: the key bound twice, then its first two bindings, in the order `make_injector` collected them. Each binding shows the key, the implementation and the lifetime, not the module it came from: modules are values without names of their own, and `make_injector` flattens them before it checks the keys. The instantiation note after the error lists `make_injector`'s argument types, so a named module, a struct derived from one, appears there by its name.

Using the injector afterwards adds no error: the engine checks no request against an injector whose keys repeat.

## Cause

`make_injector` flattens its arguments, bindings and the bindings of every module among them, into one binding map in which each key appears once (plan §5.3, step 1). A key is the bound type, qualifiers included. Two bindings of the same key leave the injector with no way to choose, so Trellis rejects them instead of letting one silently win. The same binding passed twice counts too.

## Minimal example

```cpp
#include <trellis/trellis.hpp>

struct logger {
  virtual ~logger() = default;
  virtual void write() = 0;
};
struct console_logger final : logger {
  void write() override {}
};
struct file_logger final : logger {
  void write() override {}
};

auto console_module() {
  return trellis::make_module(trellis::bind<logger>.to<console_logger>().in(trellis::singleton));
}
auto file_module() { return trellis::make_module(trellis::bind<logger>.to<file_logger>()); }

int main() { auto injector = trellis::make_injector(console_module(), file_module()); }
```

## Fix

- **Remove one of the bindings,** the one the composition root doesn't want. The two bindings in the error type, with their implementations and lifetimes, tell them apart; the instantiation note names `make_injector`'s arguments, which identifies named modules.
- **Use two keys** when both implementations are needed. Qualifiers, which tell two bindings of one interface apart, arrive in Phase 3 (plan §6.5): contextual bindings with `.when_injected_into<C>()`, and annotated keys such as `trellis::annotated<Primary, logger>`.
- **In a test,** replace a binding with `trellis::override<K>` (task T1.7) instead of binding the key again.

## Related

- [TRL-E005](TRL-E005.md): a binding exists, but its lifetime doesn't allow the requested form.
- [TRL-E012](TRL-E012.md): no binding exists, and the request needs one.
