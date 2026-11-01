# TRL-E005: Requested form incompatible with the lifetime

- **Detected:** at compile time
- **Since:** T1.4 (unreleased; v0.1)
- **Raised by:** the request check of the static engine, `trellis::detail::report_request_form`, which `create<T>()` calls from T1.5 on
- **Tests:** the seven `e005_*.cpp` tests in [`tests/compile_fail/static/`](../../tests/compile_fail/static/), one per reason below; every cell of the table in [`tests/unit/request_forms_test.cpp`](../../tests/unit/request_forms_test.cpp), and again end to end in [`tests/unit/injector_test.cpp`](../../tests/unit/injector_test.cpp). Golden tests come with T1.6.

## What you see

GCC puts the ID and the reason on the error line, and the error type in the note after it:

```text
<src>/include/trellis/static/validate.hpp:L:C: error: static assertion failed: TRL-E005: request form incompatible with the lifetime: nothing would keep the object alive; bind it in trellis::singleton, or request it by value or as std::unique_ptr
<src>/include/trellis/static/validate.hpp:L:C: note: 'trellis::error::E005<{anonymous}::sensor, trellis::error::request<{anonymous}::sensor&>, trellis::v1::unique_t>::reported' evaluates to false
```

Clang puts both on the error line:

```text
<src>/include/trellis/static/validate.hpp:L:C: error: static assertion failed due to requirement 'error::E005<(anonymous namespace)::sensor, trellis::error::request<(anonymous namespace)::sensor &>, trellis::unique_t>::reported': TRL-E005: request form incompatible with the lifetime: nothing would keep the object alive; bind it in trellis::singleton, or request it by value or as std::unique_ptr
```

The error type is `E005<Key, request<R>, Lifetime>`: the requested key, the request as the consumer spelled it, and the lifetime of the key's binding. Until T1.6 adds the path from the root, the consumer appears only in the compiler's instantiation notes.

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

There is no `create<T>()` before T1.5, so the example drives the check through the entry point that `create<T>()` calls:

```cpp
#include <trellis/static/validate.hpp>
#include <trellis/trellis.hpp>

struct sensor {};

int main() {
  auto injector = trellis::make_injector(trellis::bind<sensor>);  // unique
  trellis::detail::report_request_form<decltype(injector), sensor&>();
}
```

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
- [TRL-E003](TRL-E003.md): two bindings of one key.
