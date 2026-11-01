# Bindings, modules and the request-form table

How the binding vocabulary (T1.4) is put together: the bindings that `bind<I>` makes, how modules and `make_injector` flatten them, the binding map, and the request-form table with its checks. Headers: `include/trellis/dsl/{bind,lifetimes,module,options,request_forms}.hpp`, `include/trellis/static/{binding_map,injector,validate}.hpp` and `include/trellis/diagnostics/errors.hpp`. Decisions: [ADR-0004](../adr/0004-lifetimes-and-request-forms.md) (lifetimes, the table), [ADR-0010](../adr/0010-module-composition.md) (variadic modules, flattening once) and [ADR-0006](../adr/0006-diagnostics-contract.md) (one `static_assert` per root cause).

## One binding type per chain step

**What.** `bind<I>` is a variable, `binding<I, I, unique_t>{}`, and a binding in its own right. Each chain function returns a new specialization of the same class template with one part replaced: `.to<C>()` the implementation, `.in(l)` the lifetime, `.allow_copy()` the copy flag. `.to_ref()`, `.to_ptr()`, `.to_shared()` and `.to_value()` return the partial specializations for `external_t`, `shared_external_t` and `value_t`, which hold their object. `binding<K, I, L, C>` is the whole type; nothing accumulates (ADR-0010; plan §8.1 C4, C5).

```cpp
bind<ILogger>                                    // binding<ILogger, ILogger, unique_t>
    .to<ConsoleLogger>()                         // binding<ILogger, ConsoleLogger, unique_t>
    .in(singleton)                               // binding<ILogger, ConsoleLogger, singleton_t>
```

**Why.** A fluent chain over *several* bindings, each step returning a type that holds everything before it, grows quadratically (Fruit's blow-up, R9). A chain over one binding replaces a part and stays the same size. The bindings expose no member types: `detail::binding_traits<B>` reads the parts back from the template arguments, so the public surface is only the chain functions.

**Misuse reports.** The binding's default constructor checks the key, with a `static_assert` in its body. Every binding object is first made by it, so a bad key is reported however the binding is reached: `bind<I>` is an `auto` variable initialized with `binding<I, I, unique_t>{}`, and deducing its type instantiates that initializer even where `bind<I>` appears only inside `decltype` (with the type spelled out, `decltype(bind<I>)` would never look at the initializer); a binding type spelled directly reports when an object of it is made. A constructor is instantiated once per binding type, so the key is reported once. A `static_assert` in the class body would fire again for each binding type of a chain, and Clang drops the members of a class whose assertion failed, so every chain call would add "no member named". Each chain function, `.allow_copy()` included, returns a binding with a bad key unchanged, so no other binding type is constructed, and otherwise checks its own arguments with a `static_assert` and then, in an `if constexpr`, returns either the new binding or `*this`. The error branch thus forms no ill-formed type, and one `static_assert` is the only error (`tests/compile_fail/dsl/bind_bad_key_*.cpp`). The conditions are `constexpr bool` variables rather than concepts (`detail::is_implementation_of`, `is_object_of`, `is_bindable_key`, `is_constructed_lifetime`), because Clang explains a failed concept with several notes (24–30 lines measured), and a failed variable with none (8–12 lines). The value binding's constructor template is constrained against its own type, so that it never outranks the copy constructor for a non-const lvalue.

**Tests:** `tests/unit/bind_test.cpp`; misuse in `tests/compile_fail/dsl/` (`bind_reference_key`, `bind_bad_key_chain`, `bind_bad_key_to_ref`, `bind_bad_key_in_injector`, `bind_bad_key_named_module`, `bind_bad_key_decltype`, `bind_bad_key_decltype_value`, `binding_spelled_directly`, `bind_to_unrelated_class`, `bind_in_not_a_lifetime`, `bind_to_ref_temporary`, `bind_to_value_converting`, `bind_to_value_abstract`, `bind_to_value_move_only_lvalue`).

## Flattening modules once, linearly

**What.** `make_module(parts...)` and `make_injector(parts...)` flatten their arguments into one list of bindings with `detail::flatten<Parts...>`, which picks one of three shapes:

| Shape | The list | The values |
|---|---|---|
| only bindings | the arguments themselves | each argument moved or copied into place |
| one module (or a struct derived from one) | the module's list | the module moved or copied as a whole |
| any other mix | named per binding, through a positions table | extracted per binding |

In the mixed shape, `detail::locate<n>({sizes...})` computes, once and as `constexpr` data, the argument (`part[J]`) and the index within it (`element[J]`) of every binding J. One pack expansion over `J = 0..n-1` then names each binding by two deductions against indexed bases, with the index fixed:

```cpp
// argument_refs<0..k-1, Parts...> derives from argument_ref<I, Part_I>, built once per list
template <std::size_t J>
using part_at = typename std::remove_reference_t<decltype(detail::argument_at<positions.part[J]>(
    std::declval<parts_index&>()))>::type;
// part_bindings<Part>::store derives from stored_binding<E, B_E>, built once per module type
template <std::size_t J>
using binding_at = typename std::remove_reference_t<decltype(detail::stored_at<positions.element[J]>(
    std::declval<typename part_bindings<part_at<J>>::store&>()))>::binding_type;
```

Each deduction is one overload resolution: the index is unique among the bases, so exactly one base matches ([temp.deduct.call]/4.3). The same lookups, on the value-level `argument_refs` of forwarding references, move each binding out of an rvalue argument or copy it from an lvalue (`flat_layout::extract<J>`).

An rvalue argument gives its bindings up by moving them, an lvalue or a const argument by copying them. A binding that holds a move-only value can't be copied, so `make_module` and `make_injector` check that first (`detail::bindings_can_be_taken`) and report it as one error that says to pass the argument with `std::move`, instead of the copy constructor's errors deep in the store.

A module stores its bindings as the bases `stored_binding<J, B_J>` of one struct (`binding_store`), with `[[no_unique_address]]` members, so a module of empty bindings takes one byte on GCC and Clang. The injector holds the flattened bindings as one module. A struct derived from a module counts as a module, because `module_bindings` deduces `module<B...>` from a pointer to the struct.

**Why.**

- **No `std::tuple`:** its recursive implementation nests one class per element, and runs into the default template depth beyond a few hundred bindings.
- **No folds:** Clang refuses a fold over more than 256 operands (ADR-0010), and a module may hold more bindings than that. The scans use a braced list instead (`first_true`, from the meta toolkit).
- **No `at_t` per binding:** `at<I, type_list<Ts...>>` expands the whole list for each index, so naming n bindings that way is quadratic. Measured on 1,000 bindings, `-fsyntax-only`, gcc-13 / clang-18: `make_injector` took 10.2 / 12.2 s with `at_t`, 6 s of it in the type-level flattening, and takes 3.5 / 3.4 s now. One module of 1,000 bindings went from 27.6 / 20.6 s to 3.5 / 3.4 s; ten modules of 100 take 4.8 / 4.6 s. 1,000 one-binding modules are the slow case, at 11.9 / 10.2 s, as each binding's lookup searches 1,000 argument bases. "Linear" counts instantiations: GCC's wall time still grows faster, as the T1.4 review measured 3.5 s at 1,000 bindings and 11.7 s at 2,000 on gcc-13.

**Tests:** `tests/unit/module_test.cpp` (shapes, order, named modules, the positions table, moves and copies, 300 bindings and 300 modules); `tests/unit/injector_test.cpp` (300 bindings passed directly).

## The binding map and duplicate keys

**What.** `detail::binding_map<type_list<B...>>::type` is the meta toolkit's `type_map<entry<key_of<B_J>, stored_binding<J, B_J>>...>`: each key maps to the base of the injector's store that holds its binding, so one lookup gives both the binding type and where it is stored. `binding_for_t<Map, K>` uses `lookup_or_t`, which yields `unbound<K>` for a key without a binding instead of failing.

`make_injector` asks `has_duplicate_keys_v` first, which doesn't build the map, and reports a repeated key as TRL-E003 from its own body, in an `if constexpr` branch that never touches the map; building a `type_map` with a repeated key would be the map's own error instead. `detail::first_duplicate` then names the key and its first two bindings: the second binding is the first whose key's first occurrence (`index_of_v`) lies before it. That costs a scan of the key list per binding, and runs only on the failure path: 1.1 / 2.3 s more than a valid 1,000-binding injector. When keys repeat, `injector_traits::map` is an empty map and `request_check` decides without a lookup (verdict `keys_repeat`, which reports nothing), so using such an injector adds no error to TRL-E003.

**Why.** Keys differing only in `&` and `&&`, nested ones included, are distinct: the meta toolkit's lookups don't deduce against base classes, where GCC 13 and 14 confuse them (T1.1).

**Tests:** `tests/unit/injector_test.cpp` (lookups, duplicates, `&`/`&&` keys, a repeat among 301 bindings); `tests/compile_fail/static/e003_*.cpp`.

## Request forms and the table

**What.** `detail::request_traits<R>` splits a request into a form (the table's row), a key and whether it asks for mutable access. Top-level cv qualifiers are dropped, `X&&` is the request `X` (ADR-0003), and only `T&`, `T*` to object types and the standard smart pointers with the default deleter are forms; anything else, such as a function pointer or a `std::unique_ptr` with its own deleter, is a value. A reference to a smart pointer is a reference to a smart-pointer object, so `const std::shared_ptr<T>&` names the key `std::shared_ptr<T>`.

`detail::request_form_table` is plan §6.3 as `constexpr` data, `[request_form][lifetime_kind]`, with one `form_verdict` per cell: allowed, allowed under the captive rule, or the table's reason for a rejection. An eighth column holds unbound concrete types, implicitly unique, whose sharing forms are TRL-E012 (ADR-0004). `adjusted_verdict` applies the two rules on top of a cell: `.allow_copy()` turns "copies hide sharing" into allowed, and a const external object refuses mutable access.

`detail::request_check<Injector, R>` looks the key up, takes the cell for its binding's lifetime, and adjusts it (for an injector whose keys repeat, a partial specialization answers `keys_repeat` without a lookup): a class template, so each (injector, request) pair is evaluated once (plan §7.3). `detail::report_request_form<Injector, R>()` raises TRL-E005 or TRL-E012 for a rejection, with one `static_assert` per reason in its own `if constexpr` branch, so exactly one fires and its literal carries the reason. It is the entry point that `create<T>()`'s failure branch calls (T1.5); T1.6's explain pass adds the path. A reference or pointer to a smart pointer that nothing binds gets its own E012 message, which says to take the smart pointer by value rather than to bind the smart-pointer type.

**Why one function, not a class, reports.** Clang drops the members of a class whose instantiation failed a `static_assert`, so a later use of such a member is a second error. For the same reason `part_bindings` has no `static_assert`: an argument that is neither a binding nor a module contributes nothing, and `make_module` and `make_injector` report it in their own bodies.

**Tests:** `tests/unit/request_forms_test.cpp` (every cell, the classification, the rules); `tests/unit/injector_test.cpp` (every request form against a binding of each Phase 1 lifetime, end to end); `tests/compile_fail/static/e005_*.cpp` (one per reason) and `e012_*.cpp`.
