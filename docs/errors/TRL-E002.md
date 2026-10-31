# TRL-E002: type not constructible: no viable constructor for the available bindings

- **Detected:** at compile time.
- **Since:** T1.2, for an explicit constructor declaration that matches no constructor of its type. The general case, a type with no constructor that the available bindings can satisfy, comes with T1.6.
- **Raised by:** `trellis::ctor<Args...>` and `trellis::ctor_traits<T>`, wherever the static engine uses the declaration: today through `trellis::detail::explicit_ctor_t`, its entry point into explicit discovery, and from T1.5 on through `injector.create<T>()`.
- **Tests:** in `tests/compile_fail/discovery/`, [`specialization_wrong_types.cpp`](../../tests/compile_fail/discovery/specialization_wrong_types.cpp), [`specialization_wrong_arity.cpp`](../../tests/compile_fail/discovery/specialization_wrong_arity.cpp), [`member_alias_wrong_types.cpp`](../../tests/compile_fail/discovery/member_alias_wrong_types.cpp), [`member_alias_wrong_arity.cpp`](../../tests/compile_fail/discovery/member_alias_wrong_arity.cpp), [`binding_ctor_wrong_types.cpp`](../../tests/compile_fail/discovery/binding_ctor_wrong_types.cpp) and [`aggregate_narrowing.cpp`](../../tests/compile_fail/discovery/aggregate_narrowing.cpp); and, for a declaration that two consumers use, still one error, [`specialization_used_twice.cpp`](../../tests/compile_fail/discovery/specialization_used_twice.cpp), [`member_alias_used_twice.cpp`](../../tests/compile_fail/discovery/member_alias_used_twice.cpp) and [`binding_ctor_used_twice.cpp`](../../tests/compile_fail/discovery/binding_ctor_used_twice.cpp). Goldens come with T1.6.

## What you see

GCC puts the ID on the error line, and the type and the declaration in the note that follows it (gcc-13, normalized as in the goldens):

```text
<src>/include/trellis/discovery/ctor_traits.hpp:L:C: error: static assertion failed: TRL-E002: type not constructible: its trellis::ctor<...> declaration matches no constructor
<src>/include/trellis/discovery/ctor_traits.hpp:L:C: note: 'trellis::error::E002<app::service, trellis::v1::ctor<int> >::reported' evaluates to false
```

Clang puts all of it on the error line (clang-18):

```text
<src>/include/trellis/discovery/ctor_traits.hpp:L:C: error: static assertion failed due to requirement 'trellis::error::E002<app::service, trellis::ctor<int>>::reported': TRL-E002: type not constructible: its trellis::ctor<...> declaration matches no constructor
```

The first template argument of `trellis::error::E002` is the type, and the second is the declaration that matches none of its constructors. The lines before and after are the compiler's usual instantiation notes, which lead to the code that used the declaration. For an aggregate, the message ends instead with "its trellis::ctor<...> declaration doesn't brace-initialize the aggregate".

## Cause

An explicit constructor declaration lists parameter types that no public constructor of the type takes: a type is wrong, or there are too many or too few. Trellis checks a declaration before it uses it, so a declaration can't lie ([plan §7.2](../plan.md#72-constructor-discovery-confirm-in-s1-and-s5)); otherwise the injector would try to call a constructor that doesn't exist. The check follows how the injector will build the object: an aggregate with braces, `T{args...}`, so an argument that narrows, such as a `double` for an `int` member, is rejected; any other class with parentheses, `T(args...)`, which is `std::is_constructible_v<T, Args...>`.

A type's declaration comes from one of three places, and the first that applies is the one checked: the binding's `.with_ctor<Args...>()`, then a specialization of `trellis::ctor_traits<T>`, then a member alias `trellis_ctor` in the class ([ADR-0003](../adr/0003-constructor-discovery.md)). A class inherits its base class's member alias like any member, so a derived class that declares nothing of its own gets its base's declaration, and this error if that declaration matches none of the derived class's constructors.

## Minimal example

```cpp
#include <trellis/discovery/ctor_traits.hpp>

namespace app {

struct config {};
struct service {
  explicit service(const config& /*settings*/) {}
};

}  // namespace app

template <>
struct trellis::ctor_traits<app::service> : trellis::ctor<int> {};

int main() { [[maybe_unused]] trellis::detail::explicit_ctor_t<app::service>* unused = nullptr; }
```

Until the injector exists (T1.5), the example reaches the check through `trellis::detail::explicit_ctor_t`, as the static engine will; `injector.create<app::service>()` will reach it the same way.

## Fix

List the parameter types of a public constructor of the type, in the constructor's order:

```cpp
template <>
struct trellis::ctor_traits<app::service> : trellis::ctor<const app::config&> {};
```

- Find the declaration that applies: a binding's `.with_ctor<...>()` wins over a `ctor_traits` specialization, which wins over a member alias `trellis_ctor`. The second template argument of `E002` shows which `ctor<...>` it is.
- Spell each type as the constructor does. Another spelling that the constructor accepts, such as `config` for a `const config&` parameter, passes the check, but asks Trellis for a different request form ([plan §6.3](../plan.md#63-the-request-form-compatibility-table)).
- If the constructor you mean is private or protected, make it public: Trellis can only call public constructors.
- For an aggregate, list its members' types in order, or fewer of them, and make sure that none narrows: `trellis::ctor<int, double>` for `struct endpoint { int port; double timeout; };`, not `trellis::ctor<double, double>`.
- If the class inherits the declaration from a base class (`E002` names the derived class with the base's `ctor<...>`), give the derived class a declaration of its own: an alias `using trellis_ctor = trellis::ctor<...>;` in it, or a `ctor_traits` specialization for it. A derived class that inherits the base's constructors too, with `using Base::Base;`, can keep the base's declaration.
- To use another constructor for one binding only, leave the type's declaration alone and declare the constructor on that binding with `.with_ctor<Args...>()` (T1.4).

## Related

- [TRL-E004](TRL-E004.md): an ambiguous constructor in deduced mode. An explicit declaration is the fix there, and this check then applies to it.
- [TRL-E010](TRL-E010.md): a constructor with more parameters than `max_ctor_arity` in deduced mode. An explicit declaration is a fix there too.
