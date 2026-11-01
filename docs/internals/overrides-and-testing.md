# Overrides and the testing helpers

How a test swaps one binding of the production wiring for a test double (T1.7): `trellis::override<K>`, the wrapper that gives it `bind<K>`'s vocabulary, and how `make_injector` applies it when it flattens its arguments; then the testing helpers `trellis::testing::wireable` and `trellis::testing::create_all`. Headers: [`include/trellis/dsl/override.hpp`](../../include/trellis/dsl/override.hpp) (the vocabulary and the layout `make_injector` uses), [`include/trellis/static/injector.hpp`](../../include/trellis/static/injector.hpp) and [`injector_traits.hpp`](../../include/trellis/static/injector_traits.hpp) (where the layout is used, and the errors), and [`include/trellis/testing/wiring.hpp`](../../include/trellis/testing/wiring.hpp) (the helpers). Plan [§6.10](../plan.md#610-testing); decisions: [ADR-0010](../adr/0010-module-composition.md) (flattening once, no fluent accumulation) and [ADR-0006](../adr/0006-diagnostics-contract.md) (one error per root cause).

```cpp
TEST(Service, SavesThroughRepository) {
  testing::StrictMock<MockRepository> repo;
  auto injector = di::make_injector(app_module(test_config()),
                                    di::override<IRepository>.to_ref(repo));
  EXPECT_CALL(repo, save(42));
  injector.create<Service>().handle(42);
}
```

## An override wraps the binding that bind<K> makes

**What.** `override<K>` is the variable `override_binding<binding<K, K, unique_t>>{}`. `override_binding<B>` holds a binding `B` and has the chain functions that `B` has, each of which calls `B`'s function and wraps the result:

```cpp
template <class C>
[[nodiscard]] constexpr auto to() const noexcept {
  return override_binding::wrap(binding_.template to<C>());   // override_binding<binding<K, C, L>>
}
```

Two partial specializations mirror `binding`'s own split (dsl/bind.hpp): one for the lifetimes the injector constructs (`unique_t`, `singleton_t`, `singleton_inline_t`), with the whole chain, `.with_ctor<Args...>()` included (T1.15), and one for the bindings that hold their object (`external_t`, `shared_external_t`, `value_t`), with only `.allow_copy()`, and a `.with_ctor<Args...>()` that rejects the step with the wrapped binding's own report, `detail::reject_ctor_declaration<Binding, Args...>` (T1.15; `tests/compile_fail/override/override_with_ctor_on_external.cpp`). They are told apart by a constraint on the lifetime, `detail::is_constructed_lifetime`, and the primary template is left undefined, so `override_binding` of anything but a binding is incomplete.

**Why.** The override's vocabulary is `bind<K>`'s by definition, so the wrapper reuses it rather than copying it: every check, and every message, is `bind<K>`'s, one `static_assert` per mistake, and a step that fails returns the binding unchanged, as `bind<K>`'s does, so nothing cascades (`tests/compile_fail/override/override_to_unrelated_class.cpp`, `override_to_ref_temporary.cpp`). The key is checked once, by the wrapped binding's default constructor, and `override<K>` is an `auto` variable for the same reason `bind<K>` is (`override_bad_key.cpp`). The messages name `bind<I>`; that is the price of not duplicating some 300 lines of chain functions and their doc comments. The deleted `to_ref(const T&&)` returns the wrapper, not `void`, for the reason `bind<I>`'s returns a binding (MSVC's C3313). Nothing accumulates: each step replaces one part of one binding (ADR-0010).

**Tests:** `tests/unit/override_test.cpp`, the static assertions on the types each step makes, and one test per form.

## Applying overrides: flatten, then replace in place

**What.** `make_injector` uses `detail::compose<Parts...>` where `make_module` uses `flatten<Parts...>` (dsl/module.hpp): `flatten` itself when no argument is an override, and otherwise `override_layout<Parts...>`, which has the same interface, `bindings` and `make<Module>(args...)`:

1. **The other arguments' bindings.** With an override among the arguments, `flatten`'s shape is always the mixed one, so they are `flat_layout<mixed, Parts...>`'s, and its positions table names and extracts each of them. An override contributes no binding there (`part_bindings`, the primary template).
2. **The overrides.** A second positions table, with one slot per override argument, read through the same indexed bases (`argument_refs`), lists them in argument order, and a `type_map` maps each override's key to `override_source<I, B>`: argument `I`, whose binding is `B`.
3. **Replacing.** Binding `J` looks its key up in that map, once: `override_source<I, B>` replaces it with `B`, and `not_overridden` keeps it. The list keeps its order and length, and `extract<J>` takes binding `J` from override argument `I` (moved from an rvalue, copied from an lvalue, through `override_access`) or from where `flat_layout` finds it. The binding, its source and the result are worked out once per `J`, in the class template `position<J>`: `flat_layout`'s `binding_at<J>` is an alias over a deduction against the bases of the argument's store, which a compiler repeats for each use of the alias, and used twice per binding it cost a third more (clang-18, 1,000 bindings: 15.6 s, then 11.6 s).

```cpp
template <std::size_t J>   // override_source<I, B>, or not_overridden
using source_at =
    typename override_for<replaces,
                          typename binding_traits<typename regular::template binding_at<J>>::key,
                          override_map>::type;
template <std::size_t J>   // B, or the argument's own binding J
using binding_at =
    typename replaced<source_at<J>, typename regular::template binding_at<J>>::type;
```

The injector, its storage, eager mode and `injector_traits` all read `compose<Parts...>`'s list, so everything downstream sees one binding per key, the override's in the place of the binding it replaced: eager mode builds it there, and teardown follows.

**Why.** In place keeps the order the production wiring had, which eager mode follows, and makes the replacement one lookup per binding, with no recursion and no fold over a pack (ADR-0010, plan §8.1 C2, C3). The map is a `type_map`, one overload resolution per lookup on a class built once.

**What it costs.** "Linear" counts instantiations, as in T1.4's flattening (bindings-and-modules.md): the wall time isn't linear. With an override among the arguments, the other arguments always take T1.4's mixed layout, even a single named module, which alone `flatten` copies whole (the one-module shape); the mixed layout names each of its bindings by a deduction against the module store's bases, which compilers don't do in constant time, and emits an extraction function per binding. So an override costs what any second argument beside a large module costs. Measured on a named module of 1,000 bindings (T1.7's review and its recheck, a 4-vCPU machine under load, so GCC's numbers vary):

| `-fsyntax-only` | the module alone | plus one binding (mixed layout) | plus one override instead |
|---|---|---|---|
| clang-18 | 3.9–4.3 s | 10.9–11.7 s | 11.6 s (14.7–15.6 s before `position<J>`) |
| gcc-13 | 3.8–4.5 s | 18.2–23.2 s | 20.2 s (16.0–33.5 s before `position<J>`) |

At `-O0 -c` on clang-18 the same three take 4.9 s, 21.6 s and 22.5 s, and the object grows from 0.6 MB to 31.5 and 32.1 MB, the extraction functions. At 100 bindings an override adds 0.1–0.3 s (1.2 s against 0.9 s alone, on both compilers). A cheaper path for the common case, one module and some overrides, would take the module's list as it is and reach each of its bindings through its known base class, `stored_binding<J, B_J>`, rather than a deduction; that changes T1.4's extraction, so it is a follow-up, not part of T1.7. Filtering the replaced bindings out and appending the overrides would have needed a third positions table and changed the order. Overrides are arguments of `make_injector` only: `make_module` rejects them with a message of its own (`detail::no_overrides`, dsl/module.hpp), so a module stays a set of bindings and the replacement happens once, in the injector. The injector's type names its arguments, the override among them, as for modules: `injector<options{}, app_module, override_binding<...>>`.

**Tests:** `tests/unit/override_test.cpp`: the lists `injector_traits` sees for a module, a named module and bindings passed directly; 300 bindings with one overridden; replacing in modules, named modules and several modules; every form; lifetimes that change with the override; overrides copied from lvalues and moved from rvalues (a `std::shared_ptr`'s use count); a move-only value; eager mode's build order; the production injector unchanged.

## When overrides don't apply: one error, then nothing

**What.** `override_layout` answers, without building anything that could fail:

- `keys_repeat`, two overrides of one key: `has_duplicate_keys_v` on the overrides' map, which doesn't build it. `make_injector` reports TRL-E003, with the error type `E003<Key, override_binding<B1>, override_binding<B2>>` and a literal of its own ("two overrides name the same key", `error_message::duplicate_override` in `detail::raise`). As `raise` reports once per error type, two injectors with the same two overrides of one key are one error, as two with the same key bound twice are (T1.6).
- `apply`: no key repeats, and no override misses its target (`missing_target`): every override's key is one of the other bindings' keys (`contains_v`, one builtin trait per override, on the `slots_of` class that the binding map's duplicate check builds too). Otherwise `make_injector` reports TRL-E009 for each override in `missing_bindings`, the overrides with nothing to replace, through T1.6's entry point `report_override_target<Bindings, Override>()`: each is a root cause of its own (`e009_one_error_per_override.cpp`). `missing_bindings` is a second positions table, read only on that failure path.
- An override whose key is no key at all, such as `override<const K>`, has been reported by `bind<I>`'s check: it misses no target, so its one error stays its only one (`override_bad_key.cpp`, also named only in `decltype`: `override_bad_key_decltype.cpp`, `override_bad_key_named_module.cpp`).
- A binding whose key is no key at all, such as the `const K` of `bind<const K>`, counts as a binding of `K` for an override's target (`target_keys_of` strips cv qualifiers and references), so `override<K>` beside it adds no TRL-E009 to `bind<I>`'s error (`override_of_a_bad_key_binding.cpp`). It replaces nothing: replacing goes by the exact key, and no request reaches a binding of `const K`, with or without the override. For bindings whose keys are all keys, the stripped list is the binding map's own key list, the same type, so the target search costs no second set of slots.
- `replaces`: the overrides apply and the other bindings' keys are distinct. Only then is the map looked up; a map whose keys repeat must not be (meta/type_map.hpp). Otherwise the list is the bindings as written, which is what the TRL-E003 for a key bound twice names (`e003_bound_twice_and_overridden.cpp`): an override replaces a binding, it doesn't settle two.

`make_injector` checks the overrides before the duplicate keys, and `injector_traits::has_duplicate_keys` is true for overrides that don't apply as well as for a key bound twice: it means "`make_injector` has reported the binding set". The request check and the plan then resolve nothing and report nothing (T1.4's and T1.5's `keys_repeat` path), so `create<T>()` on such an injector adds no error (`e009_override_without_binding.cpp`, `e003_two_overrides_of_one_key.cpp`). Each report is instantiated once per argument list, so two identical `make_injector` calls give one error.

**Why.** "Two overrides of one key" is the simplest reading of the duplicate rule: a key has one binding per injector, and two overrides leave nothing to choose between them, as two bindings do; it is TRL-E003, with the overrides in the error type. Reporting an override with nothing to replace, rather than adding it as a binding, catches a misspelt key or a module left out (plan §7.8, TRL-E009). The reports are functions, as everywhere in the engine: Clang marks a class whose `static_assert` failed as invalid, and every later use would add an error. The E009 expansion is in `report_overrides` itself, and the filter is a type list rather than a function around each report, so that the error carries as few instantiation notes as can be. Both errors go through T1.6's `detail::raise` (diagnostics/errors.hpp), as every TRL error does: TRL-E009 through `report_override_target`, and the override TRL-E003 with its own message, `error_message::duplicate_override`. Measured on GCC 13 and 14 and Clang 18 and 20: one TRL-E009 takes 11 to 18 lines, two overrides of one key 10 to 20, and two TRL-E009s in one call 18 to 46 (NFR-07's 40 lines per error).

**Tests:** `tests/compile_fail/override/`, each used twice or followed by `create<T>()`; the switch itself in `tests/unit/override_test.cpp` (`overrides_apply`, `has_duplicate_keys`, the unreplaced list).

## wireable: make_injector's and create's own checks

**What.** `trellis::testing::wireable<Injector, Root>` is a concept over `Injector` without cv qualifiers and references: `detail::makeable<Injector>`, then `context<Injector, unique_t>::can_resolve<Root>`. `makeable` asks, in order, the questions `make_injector` and the bindings answer with an assertion, here as plain `constexpr` values: the type is an injector; every argument is a binding, a module or an override (`injector_parts`); every binding they hold, an override's included, has a key that is a key; `max_ctor_arity` is at most 32; the set of bindings is sound (`injector_traits::has_duplicate_keys` is false: no key bound twice, overrides that apply); and with `options{.eager = true}`, every singleton can be built (`eager_fails`, `make_injector`'s own check, which reads `can_resolve<Key&>` for each).

**Why.** `can_resolve<R>` is the check `create<R>()` makes before anything else (T1.5): `create<R>()` instantiates its creation code when it holds, and the failure pass, which always reports something, when it doesn't (resolution-and-storage.md). It can't report an error itself: the check finishes every question before it recurses and asks discovery through `discoverable_v`, never through an assertion. `create<R>()` uses a context with `slot_access::ready` for an eager injector, which changes only the creation code, never `can_resolve`. But `decltype(make_injector(...))` names an injector's type without instantiating `make_injector`'s body, so `can_resolve` alone was true for injectors that `make_injector` rejects, such as `decltype(make_injector(42))` (T1.7's review); `makeable` closes that. A concept's conjunction checks each atomic constraint only once the ones before it hold, so `accepted<Injector>`, which needs an injector type, is never asked about anything else, and nothing is looked into past the first question that fails. The one thing a type can't tell is how the arguments were passed: an lvalue whose bindings can't be copied is the call's mistake (`injector_parts_can_be_taken`), and `wireable` doesn't see it.

**Tests:** `tests/unit/testing_test.cpp`: `static_assert`s both ways, one per error of `create<T>()` (TRL-E001, E002, E004, E005, E010, E012, E013, E014), a failure deep in the graph, a const external object, incomplete types, keys repeating, an override with nothing to replace, overrides that change the answer, eager injectors, references on the injector type, and injectors `make_injector` rejects for their arguments (an `int`, `max_ctor_arity = 40`, a `const` key on a binding and on an override, an eager singleton that can't be built); the true cases built with `create<T>()`. `tests/compile_fail/testing/wireable_false_create_e005.cpp` and `wireable_false_create_e012.cpp` assert `!wireable` and then call `create<T>()`, which gives exactly its one error; `tests/compile_fail/override/override_bad_key_named_module.cpp` asserts `!wireable` for an injector type with an override of a `const` key.

## create_all: create<Key&>() for each singleton

**What.** `trellis::testing::create_all(injector)` requests each singleton and singleton_inline binding by reference, `injector.create<Key&>()`, in binding order, in one flat braced list (never a fold, plan §8.1 C3).

**Why.** Going through `create<Key&>()` gives each singleton what a request gives it: dependencies first, the teardown order recorded, a constructor's exception straight to the caller (the singletons built before it stay built, and the failed one is built again on its next request), and at compile time `create`'s own check and error, once per request however often `create_all` is called (`create_all_unbuildable_singleton.cpp`). Eager mode's loop, `build_eagerly`, is the injector constructor's, whose checking belongs to `make_injector` (and moves there entirely with T1.6); `create<Key&>()` is the public request, with its check and its error in one place. Unique bindings build nothing until requested, and an eager injector has built everything already. A binding whose key is no key at all, which `bind<I>`'s check has reported, is skipped.

**Tests:** `tests/unit/testing_test.cpp`, `CreateAll.*`: the build order with a dependency, unique bindings left alone, a throwing constructor and the retry, overrides built in their place, eager injectors.

## Isolation

Each injector owns its singletons (ADR-0005), so two tests that make injectors from the same module share nothing, overrides included: `tests/unit/testing_test.cpp`, `Isolation.*`.

## Not here

`trellis::testing::unused_bindings<Roots...>()` (plan §6.10) lists the bindings no root reaches, which needs the parameter types of every constructor: it comes with the explicit and reflection modes. Deduced constructors can't list theirs (ADR-0006).
