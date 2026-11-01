# TRL-E004: ambiguous constructor

- **Detected:** at compile time.
- **Since:** T1.3, by deduced constructor discovery. The reflection strategy (Phase 6) will raise it too, for two constructors that tie (plan §7.2).
- **Raised by:** constructor discovery, wherever the static engine asks it for a type that declares no constructor: through `injector.create<T>()` (and `trellis::make_injector` with `options{.eager = true}`) since T1.5, and through `trellis::detail::discovered_ctor_t`, its entry point into discovery.
- **Tests:** in `tests/compile_fail/discovery/`: [`deduced_greedy_template.cpp`](../../tests/compile_fail/discovery/deduced_greedy_template.cpp), [`deduced_variadic_template.cpp`](../../tests/compile_fail/discovery/deduced_variadic_template.cpp), [`deduced_c_variadic.cpp`](../../tests/compile_fail/discovery/deduced_c_variadic.cpp), [`deduced_aggregate_array.cpp`](../../tests/compile_fail/discovery/deduced_aggregate_array.cpp) and [`deduced_aggregate_optional_member.cpp`](../../tests/compile_fail/discovery/deduced_aggregate_optional_member.cpp); through `create<T>()`, [`tests/compile_fail/create/e004_greedy_via_create.cpp`](../../tests/compile_fail/create/e004_greedy_via_create.cpp), and with the path from the root and NFR-07's 40 lines, [`tests/compile_fail/explain/e004_greedy_equal_roots.cpp`](../../tests/compile_fail/explain/e004_greedy_equal_roots.cpp). The `explain/` tests have goldens for gcc-13, gcc-14, clang-18 and clang-20, in `tests/diagnostics_golden/<compiler>/explain/`.

## What you see

GCC puts the ID on the error line, and the type in the note that follows it (gcc-13, normalized as in the goldens):

```text
<src>/include/trellis/diagnostics/errors.hpp:L:C: error: static assertion failed: TRL-E004: ambiguous constructor: a constructor template or variadic constructor accepts any argument, so deduction can't tell what it needs; declare the constructor with trellis::ctor_traits
<src>/include/trellis/diagnostics/errors.hpp:L:C: note: 'trellis::error::E004<app::greedy>::reported' evaluates to false
```

Clang puts all of it on the error line (clang-18):

```text
<src>/include/trellis/diagnostics/errors.hpp:L:C: error: static assertion failed due to requirement 'trellis::error::E004<app::greedy>::reported': TRL-E004: ambiguous constructor: a constructor template or variadic constructor accepts any argument, so deduction can't tell what it needs; declare the constructor with trellis::ctor_traits
```

The template argument of `trellis::error::E004` is the type. Through `create<T>()`, the instantiation note of `trellis::detail::explain_cause`, Clang's second and GCC's first "required from" line, names the path from the root to the class, a `trellis::error::path<...>` (T1.6; [TRL-E001](TRL-E001.md) shows it); discovery's report is unique to the class, so a class that several consumers or roots need is one error. The rest of the message says which of the four causes below it is; for an aggregate it reads `an aggregate with an array member, ...` or `deduction can't initialize every member of this aggregate ...`.

## Cause

The type declares no constructor, so Trellis deduces it ([ADR-0003](../adr/0003-constructor-discovery.md)): it tries placeholder arguments that convert to any parameter type, and the conversion that each parameter selects is its request ([docs/internals/deduction.md](../internals/deduction.md)). Four shapes leave that answer ambiguous, so Trellis stops rather than guess:

- **A constructor template that accepts anything,** such as `template <class U> explicit greedy(U&&)`. It takes the placeholder itself, so nothing is learnt about what the class needs. Detected because the constructor also accepts an argument that converts to nothing (spike S1 §3.5). A template constrained so that the placeholder fails the constraint, such as `template <std::integral U>`, is simply skipped.
- **A variadic constructor,** a template `template <class... Us> C(Us&&...)` or a C ellipsis `C(...)`, for the same reason, at every arity up to the bound.
- **An aggregate with an array member,** such as `struct ports { int numbers[2]; endpoint where; };`. GCC and Clang elide the braces around the array and count each element as a field, MSVC counts the whole array as one (T1.13, spike S1's MSVC addendum), so the requests would differ by compiler. Trellis refuses the aggregate on every compiler instead; tested with GCC and Clang, while MSVC awaits the Windows CI run ([docs/internals/deduction.md](../internals/deduction.md)).
- **An aggregate member that a placeholder can't initialize,** such as a `std::optional`, `std::tuple` or `std::any` member, whose converting constructor template competes with the placeholder (spike S1 §3.2, note c). Deduction would stop at that member, and the members after it would be value-initialized without a word.

## Minimal example

```cpp
#include <trellis/discovery/deduced.hpp>

namespace app {

struct greedy {
  template <class U>
  explicit greedy(U&& /*any*/) {}
};

}  // namespace app

int main() { [[maybe_unused]] trellis::detail::discovered_ctor_t<app::greedy>* unused = nullptr; }
```

Until the injector exists (T1.5), the example reaches discovery through `trellis::detail::discovered_ctor_t`, as the static engine will; `injector.create<app::greedy>()` will reach it the same way.

## Fix

Declare the constructor, so that nothing is deduced. A specialization of `trellis::ctor_traits` leaves the class untouched:

```cpp
template <>
struct trellis::ctor_traits<app::greedy> : trellis::ctor<const app::config&> {};
```

- The declaration lists the parameter types the injector should pass, which Trellis checks with `std::is_constructible` ([TRL-E002](TRL-E002.md) if no constructor takes them). A member alias `using trellis_ctor = trellis::ctor<...>;` in the class, or a binding's `.with_ctor<...>()` (T1.4), declare it too.
- **An aggregate with an array member:** declare one parameter per element, in order. Aggregates are built with braces, so `trellis::ctor<int, int, app::endpoint>` fills `numbers[0]`, `numbers[1]` and `where` on every compiler. Or give the type a constructor, or bind it to a value.
- **An aggregate with a `std::optional`, `std::tuple` or `std::any` member:** declare all its fields, such as `trellis::ctor<std::string, std::optional<int>, int>`; the injector then needs a binding for the `std::optional<int>`. Configuration structs like this one are usually bound as values.
- A constructor template is often a forwarding convenience next to a real constructor. A non-template constructor with more parameters wins over it, so declaring nothing works once the real constructor is the longest.

## Related

- [TRL-E002](TRL-E002.md): deduction found no usable constructor at all, such as one that is private, deleted or one of two overloads with the same number of parameters.
- [TRL-E010](TRL-E010.md): the constructor takes more parameters than `max_ctor_arity`.
