# TRL-E013: Unbound concrete type requested in strict mode

- **Detected:** at compile time
- **Since:** T1.5 (unreleased; v0.1), without the path from the root, which T1.6 adds. The ID is provisional until the owner confirms it ([ADR-0006](../adr/0006-diagnostics-contract.md), History).
- **Raised by:** `injector::create<T>()`, for its request and for every constructor parameter below it, and `trellis::make_injector` with `options{.eager = true}`, for each singleton, when the injector has `options{.strict = true}`
- **Tests:** [`tests/compile_fail/create/e013_strict_unbound.cpp`](../../tests/compile_fail/create/e013_strict_unbound.cpp), whose failing request two roots make, with one error line allowed; which requests strict mode rejects, in [`tests/unit/create_test.cpp`](../../tests/unit/create_test.cpp) ("strict mode"). Golden tests come with T1.6.

## What you see

GCC puts the ID on the error line and the error type in the note after it (gcc-13, normalized as in the goldens):

```text
<src>/include/trellis/static/resolve.hpp:L:C: error: static assertion failed: TRL-E013: unbound concrete type requested in strict mode: options{.strict = true} turns off the implicit unique binding; bind the type, such as trellis::bind<T> for a new object per request
<src>/include/trellis/static/resolve.hpp:L:C: note: 'trellis::error::E013<{anonymous}::clock_part, trellis::error::request<{anonymous}::clock_part> >::reported' evaluates to false
```

Clang puts both on the error line:

```text
<src>/include/trellis/static/resolve.hpp:L:C: error: static assertion failed due to requirement 'trellis::error::E013<(anonymous namespace)::clock_part, trellis::error::request<(anonymous namespace)::clock_part>>::reported': TRL-E013: unbound concrete type requested in strict mode: options{.strict = true} turns off the implicit unique binding; bind the type, such as trellis::bind<T> for a new object per request
```

The error type is `E013<Key, request<R>>`: the key without a binding, and the request as the consumer spelled it. The instantiation notes after the error lead from the request back to the `create<T>()` call.

## Cause

Without a binding, a concrete type is implicitly unique: an owning request, by value or as `std::unique_ptr`, gets a new object ([ADR-0004](../adr/0004-lifetimes-and-request-forms.md)). `options{.strict = true}` turns that off, so that every type the injector creates is one the composition root names (plan §6.13): a forgotten binding is then an error rather than a new object nobody configured. Strict mode rejects exactly the requests the implicit binding would have served. The others are reported as without it: an abstract type without a binding is [TRL-E001](TRL-E001.md), and a reference, pointer, `std::shared_ptr` or `std::weak_ptr` to an unbound type is [TRL-E012](TRL-E012.md). Each request that fails is reported once, however many consumers make it.

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
