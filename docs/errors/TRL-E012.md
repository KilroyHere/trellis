# TRL-E012: Unbound concrete type requested by reference, pointer or shared_ptr

- **Detected:** at compile time
- **Since:** T1.4 (unreleased; v0.1); with the path from the root since T1.6
- **Raised by:** `injector::create<T>()`, for its request and for every constructor parameter below it, and `trellis::make_injector` with `options{.eager = true}`, for the first singleton it can't build (T1.5); both through the explain pass (T1.6), which raises the request check's report, as `trellis::detail::report_request_form` (T1.4) does
- **Tests:** through `create<T>()`: [`tests/compile_fail/create/e012_hello_world_without_service.cpp`](../../tests/compile_fail/create/e012_hello_world_without_service.cpp) (plan §6.1's hello world without its `bind<Service>` line), [`e012_parameter_used_twice.cpp`](../../tests/compile_fail/create/e012_parameter_used_twice.cpp), [`e012_three_hops_down.cpp`](../../tests/compile_fail/create/e012_three_hops_down.cpp), [`eager_e012.cpp`](../../tests/compile_fail/create/eager_e012.cpp) and [`e012_incomplete_by_pointer.cpp`](../../tests/compile_fail/create/e012_incomplete_by_pointer.cpp) (a type that is only declared, taken by pointer); with the path and NFR-07's 40 lines, [`tests/compile_fail/explain/e012_three_forms_one_root.cpp`](../../tests/compile_fail/explain/e012_three_forms_one_root.cpp) (one error for three forms of one missing binding), [`deep_chain_through_declared.cpp`](../../tests/compile_fail/explain/deep_chain_through_declared.cpp) and [`eager_singleton_eight_down.cpp`](../../tests/compile_fail/explain/eager_singleton_eight_down.cpp), and in three layouts of the user's code, [`deep_chain_call_over_three_lines.cpp`](../../tests/compile_fail/explain/deep_chain_call_over_three_lines.cpp), [`deep_chain_four_headers.cpp`](../../tests/compile_fail/explain/deep_chain_four_headers.cpp) and [`deep_chain_four_headers_call_over_three_lines.cpp`](../../tests/compile_fail/explain/deep_chain_four_headers_call_over_three_lines.cpp), and in four layouts for a chain whose requests alternate between by value and by reference, with NFR-07's count: [`alternating_chain_one_header.cpp`](../../tests/compile_fail/explain/alternating_chain_one_header.cpp), [`alternating_chain_call_over_three_lines.cpp`](../../tests/compile_fail/explain/alternating_chain_call_over_three_lines.cpp), [`alternating_chain_four_headers.cpp`](../../tests/compile_fail/explain/alternating_chain_four_headers.cpp) and [`alternating_chain_four_headers_call_over_three_lines.cpp`](../../tests/compile_fail/explain/alternating_chain_four_headers_call_over_three_lines.cpp), and with aggregates mixed in, [`alternating_aggregates_four_headers_call_over_three_lines.cpp`](../../tests/compile_fail/explain/alternating_aggregates_four_headers_call_over_three_lines.cpp). Through the check's entry point: [`tests/compile_fail/static/e012_unbound_by_reference.cpp`](../../tests/compile_fail/static/e012_unbound_by_reference.cpp), [`e012_unbound_shared_ptr.cpp`](../../tests/compile_fail/static/e012_unbound_shared_ptr.cpp) and [`e012_reference_to_shared_ptr.cpp`](../../tests/compile_fail/static/e012_reference_to_shared_ptr.cpp); the unbound column of the table in [`tests/unit/request_forms_test.cpp`](../../tests/unit/request_forms_test.cpp) and [`tests/unit/injector_test.cpp`](../../tests/unit/injector_test.cpp). The `explain/` tests have goldens for gcc-13, gcc-14, clang-18 and clang-20, in `tests/diagnostics_golden/<compiler>/explain/`.

## What you see

GCC puts the ID on the error line and the error type in the note after it (gcc-13, normalized as in the goldens):

```text
<src>/include/trellis/diagnostics/errors.hpp:L:C: error: static assertion failed: TRL-E012: unbound concrete type requested by reference, pointer or shared_ptr: nothing would own it; bind it with an owning lifetime such as trellis::singleton
<src>/include/trellis/diagnostics/errors.hpp:L:C: note: 'trellis::error::E012<app::sensor, trellis::error::request<app::sensor&> >::reported' evaluates to false
```

Clang puts both on the error line:

```text
<src>/include/trellis/diagnostics/errors.hpp:L:C: error: static assertion failed due to requirement 'trellis::error::E012<app::sensor, trellis::error::request<app::sensor &>>::reported': TRL-E012: unbound concrete type requested by reference, pointer or shared_ptr: nothing would own it; bind it with an owning lifetime such as trellis::singleton
```

The error type is `E012<Key, request<R>>`: the key without a binding, and the request as the consumer spelled it. The path from the root to the request is in the instantiation note of `trellis::detail::explain_cause`, the step that reports: Clang's second note, after the error line, and GCC's first "required from" line, before it (T1.6). Clang 18:

```text
<src>/include/trellis/diagnostics/explain.hpp:L:C: note: in instantiation of member function 'trellis::detail::explain_cause<trellis::error::path<app::station, app::reader>, 1, app::sensor &, trellis::injector<options{false, false, 0, 16, false}>>::run' requested here
```

GCC 13, before the error line: `required from 'static void trellis::v1::detail::explain_cause<Path, Parameter, R, Injector>::run() [with Path = trellis::error::path<app::station, app::reader>; long unsigned int Parameter = 1; R = app::sensor&; ...]'`. So `create<station>()` builds a `reader`, whose parameter 1 is the `sensor&`. When one missing binding is requested in several forms, `sensor&`, `const sensor&` and `sensor*`, the explain pass follows only the first failing parameter of each class, so one root reports it once.

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
