# TRL-E009: Override without an existing binding

- **Detected:** at compile time
- **Since:** T1.6 (unreleased; v0.1): the error type and its report, through the internal entry point `trellis::detail::report_override_target<Bindings, Override>()`, which `trellis::make_injector` calls for each `trellis::override<K>` from T1.7. No public entity raises it before T1.7.
- **Raised by:** `trellis::make_injector`, once per `override<K>` whose key no other argument (modules included) binds; the injector then reports nothing more. `override<const K>` gets `bind`'s key error instead. (T1.7)
- **Tests:** T1.7's `tests/compile_fail/override/e009_*.cpp`, through `make_injector`. Before them, [`tests/compile_fail/explain/e009_override_without_binding.cpp`](../../tests/compile_fail/explain/e009_override_without_binding.cpp) drives the entry point twice and gets one error, with goldens for gcc-13, gcc-14, clang-18 and clang-20 in `tests/diagnostics_golden/<compiler>/explain/`; the found case is in [`tests/unit/explain_test.cpp`](../../tests/unit/explain_test.cpp).

## What you see

GCC puts the ID on the error line and the error type in the note after it (gcc-13, normalized as in the goldens; the instantiation context before the error line is left out):

```text
<src>/include/trellis/diagnostics/errors.hpp:L:C: error: static assertion failed: TRL-E009: override without an existing binding: an override replaces the binding of its key, and no other binding or module binds that key; did you mean trellis::bind?
<src>/include/trellis/diagnostics/errors.hpp:L:C: note: 'trellis::error::E009<app::clock, trellis::v1::binding<app::clock, app::fake_clock, trellis::v1::unique_t, false> >::reported' evaluates to false
```

Clang puts both on the error line (clang-18):

```text
<src>/include/trellis/diagnostics/errors.hpp:L:C: error: static assertion failed due to requirement 'trellis::error::E009<app::clock, trellis::binding<app::clock, app::fake_clock, trellis::unique_t, false>>::reported': TRL-E009: override without an existing binding: an override replaces the binding of its key, and no other binding or module binds that key; did you mean trellis::bind?
```

The error type is `E009<Key, Override>`: the key the override names, and the override itself, which shows the implementation it brings.

## Cause

An override replaces the binding of its key that the injector's other arguments, bindings and modules, provide, so that a test can swap one implementation for a fake (plan §6.10). When none of them binds the key, there is nothing to replace. That is usually a misspelt key, a key that names another type than the binding does (the implementation instead of the interface, say), or the module that binds it left out of the injector, so Trellis reports it rather than adding the override as a new binding, which would hide the mistake. `make_injector` reports each such override once and nothing more about the injector.

## Minimal example

With `trellis::override<K>` (T1.7), nothing binds `clock` for the override to replace:

```cpp
#include <trellis/trellis.hpp>

namespace app {
struct logger {};
struct clock {};
struct fixed_clock : clock {};
}  // namespace app

int main() {
  auto injector = trellis::make_injector(trellis::bind<app::logger>,
                                         trellis::override<app::clock>.to<app::fixed_clock>());
  // error: TRL-E009
}
```

Before T1.7, the test drives the check through its entry point:

```cpp
using bindings = trellis::detail::type_list<decltype(trellis::bind<app::logger>)>;
using override_binding = decltype(trellis::bind<app::clock>.to<app::fake_clock>());
trellis::detail::report_override_target<bindings, override_binding>();  // error: TRL-E009
```

## Fix

- **Bind the key instead,** with `trellis::bind<K>`, when the injector has no binding to replace: in a test that builds its own injector, the fake is just a binding.
- **Add the module that binds the key** to the injector, if the override was meant to replace its binding.
- **Name the key the binding names:** the interface it binds, not its implementation. A key is a type without `const` or `volatile`, so `override<const K>` gets `bind`'s key error, not this one.

## Related

- [TRL-E003](TRL-E003.md): two bindings of one key; an override is the way to replace a binding without binding the key twice.
- [TRL-E001](TRL-E001.md): a request names an abstract type that nothing binds.
