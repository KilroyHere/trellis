# TRL-E012: Unbound concrete type requested by reference, pointer or shared_ptr

- **Detected:** at compile time
- **Since:** T1.4 (unreleased; v0.1)
- **Raised by:** `injector::create<T>()`, for its request and for every constructor parameter below it, and `trellis::make_injector` with `options{.eager = true}`, for each singleton (T1.5); both through the request check of the static engine, `trellis::detail::report_request_form` (T1.4)
- **Tests:** through `create<T>()`: [`tests/compile_fail/create/e012_hello_world_without_service.cpp`](../../tests/compile_fail/create/e012_hello_world_without_service.cpp) (plan §6.1's hello world without its `bind<Service>` line), [`e012_parameter_used_twice.cpp`](../../tests/compile_fail/create/e012_parameter_used_twice.cpp), [`e012_three_hops_down.cpp`](../../tests/compile_fail/create/e012_three_hops_down.cpp), [`eager_e012.cpp`](../../tests/compile_fail/create/eager_e012.cpp) and [`e012_incomplete_by_pointer.cpp`](../../tests/compile_fail/create/e012_incomplete_by_pointer.cpp) (a type that is only declared, taken by pointer). Through the check's entry point: [`tests/compile_fail/static/e012_unbound_by_reference.cpp`](../../tests/compile_fail/static/e012_unbound_by_reference.cpp), [`e012_unbound_shared_ptr.cpp`](../../tests/compile_fail/static/e012_unbound_shared_ptr.cpp) and [`e012_reference_to_shared_ptr.cpp`](../../tests/compile_fail/static/e012_reference_to_shared_ptr.cpp); the unbound column of the table in [`tests/unit/request_forms_test.cpp`](../../tests/unit/request_forms_test.cpp) and [`tests/unit/injector_test.cpp`](../../tests/unit/injector_test.cpp). Golden tests come with T1.6.

## What you see

GCC puts the ID on the error line and the error type in the note after it:

```text
<src>/include/trellis/static/validate.hpp:L:C: error: static assertion failed: TRL-E012: unbound concrete type requested by reference, pointer or shared_ptr: nothing would own it; bind it with an owning lifetime such as trellis::singleton
<src>/include/trellis/static/validate.hpp:L:C: note: 'trellis::error::E012<{anonymous}::sensor, trellis::error::request<const {anonymous}::sensor&> >::reported' evaluates to false
```

Clang puts both on the error line:

```text
<src>/include/trellis/static/validate.hpp:L:C: error: static assertion failed due to requirement 'error::E012<(anonymous namespace)::sensor, trellis::error::request<const (anonymous namespace)::sensor &>>::reported': TRL-E012: unbound concrete type requested by reference, pointer or shared_ptr: nothing would own it; bind it with an owning lifetime such as trellis::singleton
```

The error type is `E012<Key, request<R>>`: the key without a binding, and the request as the consumer spelled it.

## Cause

A concrete type without a binding is implicitly unique: the injector builds a new one for each request, and the consumer owns it (ADR-0004). Only owning requests can take such an object, by value or as `std::unique_ptr<T>`. Requested as `T&`, `const T&`, `T*`, `const T*`, `std::shared_ptr<T>` or `std::weak_ptr<T>`, nothing in the injector would own it, so the reference would dangle or the sharing would be an illusion. Trellis reports that rather than building an object nobody keeps, which is what a forgotten registration silently did in Hypodermic (R11).

`std::shared_ptr<T>` is rejected here although a bound unique object may be requested so: an explicit binding says the consumer may own the object, an unbound type is only ever built for owning requests.

A parameter of type `const std::shared_ptr<T>&` is a reference to a `std::shared_ptr<T>` object, so its key is `std::shared_ptr<T>`, which is usually unbound: this error, with a message of its own for a reference or pointer to a `std::shared_ptr`, `std::unique_ptr` or `std::weak_ptr`:

```text
<src>/include/trellis/static/validate.hpp:L:C: error: static assertion failed: TRL-E012: unbound concrete type requested by reference, pointer or shared_ptr: a reference or pointer to a smart pointer asks for the smart pointer itself, which nothing binds; take it by value, such as std::shared_ptr<T>, and the injector makes it from T's binding
```

Take the `std::shared_ptr<T>` by value: the injector then makes it from `T`'s binding, as the request-form table allows. Don't bind `std::shared_ptr<T>` itself in a lifetime such as `trellis::singleton`: that object would be an empty pointer. (ADR-0004 leaves open whether such parameters should be allowed; T1.4 keeps rejecting them.)

An unbound abstract type is a different error, [TRL-E001](TRL-E001.md) (no binding for an abstract type).

`create<T>()` reports the request where it finds it, one or more constructors below the request it was given, and each failing request once, however many consumers make it.

## Minimal example

```cpp
#include <trellis/trellis.hpp>

struct sensor {};
struct reader {
  explicit reader(const sensor& input) : input_(&input) {}
  const sensor* input_;
};

int main() {
  auto injector = trellis::make_injector();  // nothing binds sensor
  reader built = injector.create<reader>();  // error: TRL-E012 for const sensor&
}
```

Plan §6.1's hello world gives this error when its `bind<Service>` line is deleted: `App` takes `Service&`, and nothing would own the Service.

## Fix

- **Bind the type with an owning lifetime,** so that the injector keeps the object: `trellis::bind<sensor>.in(trellis::singleton)`, or an external object with `trellis::bind<sensor>.to_ref(the_sensor)`.
- **Or request it in an owning form:** by value, `sensor`, or as `std::unique_ptr<sensor>`.

## Related

- [TRL-E005](TRL-E005.md): the same check for a type with a binding, against its lifetime.
- [TRL-E003](TRL-E003.md): two bindings of one key.
