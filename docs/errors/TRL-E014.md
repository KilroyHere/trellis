# TRL-E014: Owning request would delete the implementation through a base class whose destructor isn't virtual

- **Detected:** at compile time
- **Since:** T1.5 (unreleased; v0.1); with the path from the root since T1.6. The owner confirmed the ID on 2026-11-02, and it is stable from v0.1 ([ADR-0006](../adr/0006-diagnostics-contract.md), History).
- **Raised by:** `injector::create<T>()`, for its request and for every constructor parameter below it
- **Tests:** [`tests/compile_fail/create/e014_unique_ptr_without_virtual_destructor.cpp`](../../tests/compile_fail/create/e014_unique_ptr_without_virtual_destructor.cpp), which makes the failing request at the root and as a constructor parameter, with one error line allowed; the forms that stay allowed, in [`tests/unit/create_test.cpp`](../../tests/unit/create_test.cpp) ("NonVirtualBaseFormsDestroyTheDerivedClass"); with the path and NFR-07's 40 lines, [`tests/compile_fail/explain/e014_parameter_two_roots.cpp`](../../tests/compile_fail/explain/e014_parameter_two_roots.cpp). The `explain/` tests have goldens for gcc-13, gcc-14, clang-18 and clang-20, in `tests/diagnostics_golden/<compiler>/explain/`.

## What you see

GCC puts the ID on the error line and the error type in the note after it (gcc-13, normalized as in the goldens):

```text
<src>/include/trellis/diagnostics/errors.hpp:L:C: error: static assertion failed: TRL-E014: owning request would delete the implementation through a base class whose destructor isn't virtual: std::unique_ptr<I> deletes through I; give I a virtual destructor, or request std::shared_ptr<I>, which deletes the class it was made for
<src>/include/trellis/diagnostics/errors.hpp:L:C: note: 'trellis::error::E014<{anonymous}::base, trellis::error::request<std::unique_ptr<{anonymous}::base> >, {anonymous}::derived>::reported' evaluates to false
```

Clang puts both on the error line:

```text
<src>/include/trellis/diagnostics/errors.hpp:L:C: error: static assertion failed due to requirement 'trellis::error::E014<(anonymous namespace)::base, trellis::error::request<std::unique_ptr<(anonymous namespace)::base, std::default_delete<(anonymous namespace)::base>>>, (anonymous namespace)::derived>::reported': TRL-E014: owning request would delete the implementation through a base class whose destructor isn't virtual: std::unique_ptr<I> deletes through I; give I a virtual destructor, or request std::shared_ptr<I>, which deletes the class it was made for
```

The error type is `E014<Key, request<R>, Impl>`: the requested base class, the request as the consumer spelled it, and the binding's implementation, the class that would be deleted through the base. The path from the root to the request is in the instantiation note of `trellis::detail::explain_cause`, the step that reports: Clang's second note, after the error line, and GCC's first "required from" line, before it (T1.6; [TRL-E001](TRL-E001.md) shows the note).

## Cause

With `trellis::bind<base>.to<derived>()`, a request `std::unique_ptr<base>` gets a new `derived`. When `base`'s destructor isn't virtual, the `std::unique_ptr`'s `std::default_delete<base>` would delete that `derived` through `base`, which is undefined behaviour ([expr.delete]/3): the `derived` part is never destroyed, and the memory may be freed with the wrong size. No compiler warns about it here, as the deletion is in the standard library, so Trellis rejects the request (T1.5's review). Every other form stays allowed and safe: a `std::shared_ptr<base>`'s control block deletes the `derived` it was made for; a singleton's slot destroys its own `derived`; a by-value request copies the new `derived` into a `base`, which slices it but deletes nothing; and `std::unique_ptr<derived>` deletes its own class.

## Minimal example

```cpp
#include <trellis/trellis.hpp>

#include <memory>

struct base {
  base() = default;
  int id = 1;
};
struct derived : base {
  derived() = default;
  int payload = 2;
};

int main() {
  auto injector = trellis::make_injector(trellis::bind<base>.to<derived>());
  auto owned = injector.create<std::unique_ptr<base>>();  // error: TRL-E014
}
```

## Fix

- **Give the base class a virtual destructor,** `virtual ~base() = default;`, if it is meant to be used through pointers to it.
- **Or request `std::shared_ptr<base>`,** whose control block deletes the class it was made for.
- **Or request the implementation itself,** `std::unique_ptr<derived>`, or bind `base` in `trellis::singleton` and request a reference.

## Related

- [TRL-E005](TRL-E005.md): a request form that the binding's lifetime rejects, such as `std::unique_ptr<T>` of a singleton.
- [TRL-E002](TRL-E002.md): a by-value request whose copy the key can't be constructed from.
