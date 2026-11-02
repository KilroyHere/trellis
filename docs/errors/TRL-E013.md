# TRL-E013: Unbound concrete type requested in strict mode

- **Detected:** at compile time
- **Since:** T1.5 (unreleased; v0.1); with the path from the root since T1.6. The owner confirmed the ID on 2026-11-02, and it is stable from v0.1 ([ADR-0006](../adr/0006-diagnostics-contract.md), History).
- **Raised by:** `injector::create<T>()`, for its request and for every constructor parameter below it, and `trellis::make_injector` with `options{.eager = true}`, for the first singleton it can't build, when the injector has `options{.strict = true}`
- **Tests:** [`tests/compile_fail/create/e013_strict_unbound.cpp`](../../tests/compile_fail/create/e013_strict_unbound.cpp), whose failing request two roots make, with one error line allowed; which requests strict mode rejects, in [`tests/unit/create_test.cpp`](../../tests/unit/create_test.cpp) ("strict mode"); with the path and NFR-07's 40 lines, [`tests/compile_fail/explain/e013_strict_two_roots.cpp`](../../tests/compile_fail/explain/e013_strict_two_roots.cpp). The `explain/` tests have goldens for gcc-13, gcc-14, clang-18 and clang-20, in `tests/diagnostics_golden/<compiler>/explain/`.

## What you see

GCC puts the ID on the error line and the error type in the note after it (gcc-13, normalized as in the goldens):

```text
<src>/include/trellis/diagnostics/errors.hpp:L:C: error: static assertion failed: TRL-E013: unbound concrete type requested in strict mode: options{.strict = true} turns off the implicit unique binding; bind the type, such as trellis::bind<T> for a new object per request
<src>/include/trellis/diagnostics/errors.hpp:L:C: note: 'trellis::error::E013<{anonymous}::clock_part, trellis::error::request<{anonymous}::clock_part> >::reported' evaluates to false
```

Clang puts both on the error line:

```text
<src>/include/trellis/diagnostics/errors.hpp:L:C: error: static assertion failed due to requirement 'trellis::error::E013<(anonymous namespace)::clock_part, trellis::error::request<(anonymous namespace)::clock_part>>::reported': TRL-E013: unbound concrete type requested in strict mode: options{.strict = true} turns off the implicit unique binding; bind the type, such as trellis::bind<T> for a new object per request
```

The error type is `E013<Key, request<R>>`: the key without a binding, and the request as the consumer spelled it. The path from the root to the request is in the instantiation note of `trellis::detail::explain_cause`, the step that reports: Clang's second note, after the error line, and GCC's first "required from" line, before it (T1.6): a `trellis::error::path<...>` of the classes whose constructors lead to it, with the 1-based index of the parameter that makes the request ([TRL-E001](TRL-E001.md) shows the note).

## Cause

Without a binding, a concrete type is implicitly unique: an owning request, by value or as `std::unique_ptr`, gets a new object ([ADR-0004](../adr/0004-lifetimes-and-request-forms.md)). `options{.strict = true}` turns that off, so that every type the injector creates is one the composition root names (plan §6.13): a forgotten binding is then an error rather than a new object nobody configured. Strict mode rejects exactly the requests the implicit binding would have served. The others are reported as without it: an abstract type without a binding is [TRL-E001](TRL-E001.md), and a reference, pointer, `std::shared_ptr` or `std::weak_ptr` to an unbound type is [TRL-E012](TRL-E012.md). Each request that fails is reported once, however many consumers and roots make it.

## Minimal example

```cpp
#include <trellis/trellis.hpp>

struct clock_part {
  clock_part() = default;
  int ticks = 0;
};
struct timer {
  explicit timer(clock_part source) : ticks(source.ticks) {}
  int ticks;
};

int main() {
  auto injector = trellis::make_injector<trellis::options{.strict = true}>(trellis::bind<timer>);
  timer t = injector.create<timer>();  // error: TRL-E013, clock_part has no binding
}
```

## Fix

- **Bind the type,** with the lifetime its consumers need: `trellis::bind<clock_part>` for a new object per request, or `.in(trellis::singleton)` to share one.
- **Or bind it to a value or an object you have:** `trellis::bind<clock_part>.to_value(clock_part{})`, `.to_ref(clock)` or `.to_shared(pointer)`.
- **Or drop `.strict = true`,** if the implicit unique binding of concrete types is what you want.

## Related

- [TRL-E001](TRL-E001.md): an abstract type without a binding, in any mode.
- [TRL-E012](TRL-E012.md): an unbound concrete type requested by reference, pointer or `std::shared_ptr`, in any mode.
