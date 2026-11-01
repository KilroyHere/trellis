# TRL-E005: Requested form incompatible with the lifetime

- **Detected:** at compile time
- **Since:** T1.4 (unreleased; v0.1); with the path from the root since T1.6
- **Raised by:** `injector::create<T>()`, for its request and for every constructor parameter below it, and `trellis::make_injector` with `options{.eager = true}`, for the first singleton it can't build (T1.5); both through the explain pass (T1.6), which raises the request check's report, as `trellis::detail::report_request_form` (T1.4) does
- **Tests:** through `create<T>()`: the four `e005_*.cpp` tests in [`tests/compile_fail/create/`](../../tests/compile_fail/create/), at the root, one hop down in a deduced class and in a declared one, each used twice, and [`tests/compile_fail/explain/e005_through_declared_two_roots.cpp`](../../tests/compile_fail/explain/e005_through_declared_two_roots.cpp), with the path and NFR-07's 40 lines. Through the check's entry point: the seven `e005_*.cpp` tests in [`tests/compile_fail/static/`](../../tests/compile_fail/static/), one per reason below; every cell of the table in [`tests/unit/request_forms_test.cpp`](../../tests/unit/request_forms_test.cpp), and again end to end in [`tests/unit/injector_test.cpp`](../../tests/unit/injector_test.cpp). The `explain/` tests have goldens for gcc-13, gcc-14, clang-18 and clang-20, in `tests/diagnostics_golden/<compiler>/explain/`.

## What you see

GCC puts the ID and the reason on the error line, and the error type in the note after it (gcc-13, normalized as in the goldens):

```text
<src>/include/trellis/diagnostics/errors.hpp:L:C: error: static assertion failed: TRL-E005: request form incompatible with the lifetime: nothing would keep the object alive; bind it in trellis::singleton, or request it by value or as std::unique_ptr
<src>/include/trellis/diagnostics/errors.hpp:L:C: note: 'trellis::error::E005<app::timer, trellis::error::request<app::timer&>, trellis::v1::unique_t>::reported' evaluates to false
```

Clang puts both on the error line:

```text
<src>/include/trellis/diagnostics/errors.hpp:L:C: error: static assertion failed due to requirement 'trellis::error::E005<app::timer, trellis::error::request<app::timer &>, trellis::unique_t>::reported': TRL-E005: request form incompatible with the lifetime: nothing would keep the object alive; bind it in trellis::singleton, or request it by value or as std::unique_ptr
```

The error type is `E005<Key, request<R>, Lifetime>`: the requested key, the request as the consumer spelled it, and the lifetime of the key's binding. The path from the root to the request is in the instantiation note of `trellis::detail::explain_cause`, the step that reports: Clang's second note, after the error line, and GCC's first "required from" line, before it (T1.6): a `trellis::error::path<...>` of the classes whose constructors lead to it, and the 1-based index of the parameter that makes the request. For the test above, `path<dashboard, scheduler>` and parameter 2: `create<dashboard>()` needs a `scheduler&`, whose declared constructor's second parameter is the `timer&`. A request that several roots or consumers fail on is reported once ([TRL-E001](TRL-E001.md) shows the note).

## Cause

Each lifetime allows some request forms and rejects others, as the table in [plan §6.3](../plan.md#63-the-request-form-compatibility-table) says. A rejected form would make ownership wrong: a reference that dangles, a copy that hides sharing, or two owners of one object. Trellis reports it instead of choosing a lifetime from the request, which is what produced dangling references in Boost.DI (ADR-0004). The message names one of these reasons:

| Reason in the message | Requests and lifetimes |
|---|---|
| copies hide sharing; request a reference, or opt in with `.allow_copy()` | `T` of a singleton, singleton_inline, scoped, external or `to_shared` object |
| nothing would keep the object alive | `T&`, `const T&` or `std::weak_ptr<T>` of a unique binding |
| ambiguous ownership; request `std::unique_ptr` | `T*` or `const T*` of a unique binding |
| `std::unique_ptr` would duplicate ownership of a shared object | `std::unique_ptr<T>` of anything but a unique binding |
| the object is not heap-owned | `std::shared_ptr<T>` or `std::weak_ptr<T>` of a singleton_inline or `to_value` binding |
| the caller owns the object | `std::shared_ptr<T>` or `std::weak_ptr<T>` of a `to_ref` or `to_ptr` binding |
| the bound object is const; request a reference or pointer to const | `T&`, `T*`, `std::shared_ptr<T>` or `std::weak_ptr<T>` of a const external object: constness carries through |

`T&&` and `const T&&` parameters are the request `T` (ADR-0003). A parameter of type `const std::shared_ptr<T>&` is a reference to a `std::shared_ptr<T>` object, a different key: see [TRL-E012](TRL-E012.md).

## Minimal example

```cpp
#include <trellis/trellis.hpp>

struct sensor {};

int main() {
  auto injector = trellis::make_injector(trellis::bind<sensor>);  // unique
  sensor& object = injector.create<sensor&>();  // error: TRL-E005, nothing would keep it alive
}
```

A constructor parameter is a request too, reported where `create<T>()` finds it, and each failing request once, however many consumers make it.

## Fix

Change the binding's lifetime, or the request:

- **A reference or pointer to a unique object:** bind it as shared, `trellis::bind<sensor>.in(trellis::singleton)`, or take it by value or as `std::unique_ptr<sensor>`.
- **A shared object by value:** take a reference, `const sensor&`. If a copy is what the consumer wants, opt in on the binding: `trellis::bind<sensor>.in(trellis::singleton).allow_copy()`.
- **`std::unique_ptr` of a shared object:** take `sensor&` or `std::shared_ptr<sensor>` instead.
- **`std::shared_ptr` of an inline singleton or a value:** bind it in `trellis::singleton`, which holds a `std::shared_ptr`.
- **`std::shared_ptr` of an external object:** hand the object over as a `std::shared_ptr`, with `trellis::bind<sensor>.to_shared(pointer)`.
- **Mutable access to a const object:** request `const sensor&`, `const sensor*` or `std::shared_ptr<const sensor>`, or bind a non-const object.

## Related

- [TRL-E012](TRL-E012.md): the same check for a type without a binding, which is implicitly unique.
- [TRL-E014](TRL-E014.md): an allowed `std::unique_ptr<I>` that would delete the implementation through `I`'s non-virtual destructor.
- [TRL-E003](TRL-E003.md): two bindings of one key.
