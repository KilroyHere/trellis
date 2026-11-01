# TRL-E010: constructor arity above `max_ctor_arity`

- **Detected:** at compile time.
- **Since:** T1.3, by deduced constructor discovery.
- **Raised by:** constructor discovery, wherever the static engine asks it for a type that declares no constructor: through `injector.create<T>()` (and `trellis::make_injector` with `options{.eager = true}`) since T1.5, with the injector's `max_ctor_arity` as the bound, and through `trellis::detail::discovered_ctor_t`, its entry point into discovery, whose last template argument is the bound.
- **Tests:** in `tests/compile_fail/discovery/`: [`deduced_above_bound.cpp`](../../tests/compile_fail/discovery/deduced_above_bound.cpp) (17 parameters, the default bound of 16) and [`deduced_above_lowered_bound.cpp`](../../tests/compile_fail/discovery/deduced_above_lowered_bound.cpp) (5 parameters, a bound of 4); through `create<T>()`, [`tests/compile_fail/create/e010_lowered_bound_via_create.cpp`](../../tests/compile_fail/create/e010_lowered_bound_via_create.cpp) (3 parameters, `options{.max_ctor_arity = 2}`). Goldens come with T1.6.

## What you see

GCC puts the ID on the error line, and the type and the bound in the note that follows it (gcc-13, normalized as in the goldens):

```text
<src>/include/trellis/discovery/deduced.hpp:L:C: error: static assertion failed: TRL-E010: constructor arity above max_ctor_arity: raise max_ctor_arity or declare the constructor with trellis::ctor_traits
<src>/include/trellis/discovery/deduced.hpp:L:C: note: 'trellis::error::E010<app::wide, 16>::reported' evaluates to false
```

Clang puts all of it on the error line (clang-18):

```text
<src>/include/trellis/discovery/deduced.hpp:L:C: error: static assertion failed due to requirement 'trellis::error::E010<app::wide, 16>::reported': TRL-E010: constructor arity above max_ctor_arity: raise max_ctor_arity or declare the constructor with trellis::ctor_traits
```

The first template argument of `trellis::error::E010` is the type, and the second the bound in effect.

## Cause

The type declares no constructor, so Trellis deduces it ([ADR-0003](../adr/0003-constructor-discovery.md)): it looks for the largest number of placeholder arguments, up to the bound, from which the type is constructible. Each arity tried costs compile time, so the bound is the injector's option `max_ctor_arity`, 16 by default (plan §6.13). When no arity up to the bound works, Trellis tries the 16 arities above it, only to tell this case apart from a missing constructor: one of them works here.

A constructor with more parameters than the bound plus 16 gives [TRL-E002](TRL-E002.md) instead, as Trellis doesn't look that far.

## Minimal example

```cpp
#include <trellis/discovery/deduced.hpp>

namespace app {

struct part {};
struct wide {
  wide(part, part, part, part, part, part, part, part, part, part, part, part, part, part, part,
       part, part) {}
};

}  // namespace app

int main() { [[maybe_unused]] trellis::detail::discovered_ctor_t<app::wide>* unused = nullptr; }
```

Until the injector exists (T1.5), the example reaches discovery through `trellis::detail::discovered_ctor_t`, as the static engine will; `injector.create<app::wide>()` will reach it the same way.

## Fix

- **Declare the constructor,** which has no limit and costs nothing measurable to check (spike S1 §3.4):

  ```cpp
  template <>
  struct trellis::ctor_traits<app::wide>
      : trellis::ctor<app::part, app::part, app::part, app::part, app::part, app::part, app::part,
                      app::part, app::part, app::part, app::part, app::part, app::part, app::part,
                      app::part, app::part, app::part> {};
  ```

- **Raise `max_ctor_arity`** for the injector, such as `trellis::options{.max_ctor_arity = 20}` (T1.4, T1.5). Deduction supports a bound of up to 32. Every class it deduces then costs a little more to compile, as deduction tries the arities from the top down (spike S1 §3.4).
- A constructor with this many parameters often wants a parameter object: a struct that groups related settings, bound as one value.

## Related

- [TRL-E002](TRL-E002.md): deduction found no usable constructor at all, within the bound or the 16 arities above it.
- [TRL-E004](TRL-E004.md): a constructor template or variadic constructor, which accepts any number of arguments.
