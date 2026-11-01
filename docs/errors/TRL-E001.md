# TRL-E001: No binding for an abstract type

- **Detected:** at compile time
- **Since:** T1.5 (unreleased; v0.1), without the path from the root, which T1.6 adds
- **Raised by:** `injector::create<T>()`, for its request and for every constructor parameter below it, and `trellis::make_injector` with `options{.eager = true}`, for each singleton
- **Tests:** [`tests/compile_fail/create/e001_unbound_abstract_parameter.cpp`](../../tests/compile_fail/create/e001_unbound_abstract_parameter.cpp), [`e001_abstract_bound_to_itself.cpp`](../../tests/compile_fail/create/e001_abstract_bound_to_itself.cpp), and by value at the root, where `create<I>()` can't return an abstract `I` (it returns `void` instead, so the error stays the only one), [`e001_root_abstract_by_value.cpp`](../../tests/compile_fail/create/e001_root_abstract_by_value.cpp) and [`e001_root_abstract_bound_to_itself_by_value.cpp`](../../tests/compile_fail/create/e001_root_abstract_bound_to_itself_by_value.cpp); each uses its failing request twice and allows one error line. Golden tests come with T1.6.

## What you see

GCC puts the ID on the error line and the error type in the note after it (gcc-13, normalized as in the goldens):

```text
<src>/include/trellis/static/resolve.hpp:L:C: error: static assertion failed: TRL-E001: no binding for an abstract type: bind it to an implementation with trellis::bind<I>.to<C>()
<src>/include/trellis/static/resolve.hpp:L:C: note: 'trellis::error::E001<{anonymous}::IRepository, trellis::error::request<std::shared_ptr<{anonymous}::IRepository> > >::reported' evaluates to false
```

Clang puts both on the error line:

```text
<src>/include/trellis/static/resolve.hpp:L:C: error: static assertion failed due to requirement 'trellis::error::E001<(anonymous namespace)::IRepository, trellis::error::request<std::shared_ptr<(anonymous namespace)::IRepository>>>::reported': TRL-E001: no binding for an abstract type: bind it to an implementation with trellis::bind<I>.to<C>()
```

The error type is `E001<Key, request<R>>`: the key that nothing implements, and the request as the consumer spelled it. The instantiation notes after the error lead from the request back to the `create<T>()` call; T1.6 adds a path, such as `path<App, Service>`, to the error type itself.

## Cause

A request names an abstract type that no binding implements. Trellis reports it rather than guessing an implementation (plan §7.8). There are two forms of it, each with its own message:

- **An abstract type without a binding,** as above: nothing says which class implements it. An unbound concrete type would be built as a new object (ADR-0004), but an abstract one can't be.
- **An abstract type bound to itself,** with `trellis::bind<I>` alone or `trellis::bind<I>.in(trellis::singleton)`, so that the binding names no implementation. This is TRL-E001 with or without a constructor declaration on the class: `create<T>()` checks for an abstract class before it asks constructor discovery, which would report TRL-E002 for it.

  ```text
  error: static assertion failed due to requirement 'trellis::error::E001<(anonymous namespace)::IStore, trellis::error::request<(anonymous namespace)::IStore &>>::reported': TRL-E001: no binding for an abstract type: bind<I> alone binds it to itself, and nothing implements it; name the implementation with trellis::bind<I>.to<C>()
  ```

Each request that fails is reported once, however many consumers make it. A type that is only declared, not defined, isn't asked whether it is abstract: it goes on to [TRL-E012](TRL-E012.md) or to discovery's [TRL-E002](TRL-E002.md).

## Minimal example

```cpp
#include <trellis/trellis.hpp>

#include <memory>
#include <utility>

struct IRepository {
  virtual ~IRepository() = default;
  virtual void save(int value) = 0;
};
struct Service {
  explicit Service(std::shared_ptr<IRepository> repository) : repository_(std::move(repository)) {}
  std::shared_ptr<IRepository> repository_;
};

int main() {
  auto injector = trellis::make_injector(trellis::bind<Service>.in(trellis::singleton));
  Service& service = injector.create<Service&>();  // error: TRL-E001, nothing implements IRepository
}
```

## Fix

- **Bind the abstract type to an implementation:** `trellis::bind<IRepository>.to<SqlRepository>()`, with the lifetime the consumers need, such as `.in(trellis::singleton)` for a `std::shared_ptr` or reference parameter that should share one object.
- **Or bind it to an object you have:** `trellis::bind<IRepository>.to_ref(repository)`, `.to_ptr(pointer)` or `.to_shared(shared_pointer)`.

## Related

- [TRL-E002](TRL-E002.md): the type has a binding or could be built, but no constructor can be found or called.
- [TRL-E012](TRL-E012.md): a concrete type without a binding, requested by reference, pointer or `std::shared_ptr`.
- [TRL-E013](TRL-E013.md): a concrete type without a binding, requested by value or as `std::unique_ptr` under `options{.strict = true}`.
- [TRL-E005](TRL-E005.md): the type has a binding, but its lifetime doesn't allow the request.
