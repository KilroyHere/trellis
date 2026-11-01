// Trellis testing helpers: checking that a graph can be wired, and building every singleton (plan
// §6.10).
// Copyright (c) 2026 Aryan Patel
// SPDX-License-Identifier: BSL-1.0
//
// Tests include this header themselves, after <trellis/trellis.hpp>; the umbrella header doesn't,
// so production code doesn't pay for it, and a program that never includes it never declares the
// namespace trellis::testing beside GoogleTest's ::testing.
//
//   testing::wireable<Injector, Root>  whether make_injector makes the injector and
//                                       injector.create<Root>() compiles: the checks they make
//                                       themselves, never an error
//   testing::create_all(injector)       builds every singleton of the injector, through
//                                       create<Key&>(), so a smoke test fails fast
//
// Overrides, the other half of plan §6.10, are part of the binding vocabulary (dsl/override.hpp).
// testing::unused_bindings<Roots...>() is for the explicit and reflection modes, where the
// parameter types of every constructor can be listed, and arrives with them.
#ifndef TRELLIS_TESTING_WIRING_HPP
#define TRELLIS_TESTING_WIRING_HPP

#include <trellis/discovery/deduced.hpp>
#include <trellis/dsl/bind.hpp>
#include <trellis/dsl/lifetimes.hpp>
#include <trellis/dsl/options.hpp>
#include <trellis/dsl/override.hpp>
#include <trellis/dsl/request_forms.hpp>
#include <trellis/meta/type_list.hpp>
#include <trellis/static/injector.hpp>
#include <trellis/static/injector_traits.hpp>
#include <trellis/static/storage.hpp>

#include <array>
#include <type_traits>

namespace trellis::inline v1 {

namespace detail {

// is_injector<T>: whether T is an injector type, injector<Opts, Parts...>.
template <class T>
inline constexpr bool is_injector = false;
template <options Opts, class... Parts>
inline constexpr bool is_injector<injector<Opts, Parts...>> = true;

// Whether every binding of a list has a key that is a key (is_bindable_key, dsl/request_forms.hpp).
template <class... Bindings>
constexpr bool keys_are_keys(type_list<Bindings...> /*bindings*/) noexcept {
  return first_true({!is_bindable_key<typename binding_traits<Bindings>::key>...}) ==
         sizeof...(Bindings);
}

// accepted<Injector>: the checks on an injector type's arguments that make_injector and the
// bindings make, each answered without an assertion: every argument is a binding, a module or an
// override (injector_parts, dsl/override.hpp); the key of every binding they hold, an override's
// included, is a key, which the binding's constructor checks (dsl/bind.hpp);
// options::max_ctor_arity is at most 32. Each member is instantiated only where makeable, below,
// asks it.
template <class Injector>
struct accepted;
template <options Opts, class... Parts>
struct accepted<injector<Opts, Parts...>> {
  static constexpr bool parts = injector_parts<Parts...>;
  static constexpr bool keys =
      first_true({!detail::keys_are_keys(typename injector_part_bindings<Parts>::type{})...}) ==
      sizeof...(Parts);
  static constexpr bool arity = Opts.max_ctor_arity <= deduced_arity_limit;
};

// singletons_build<Injector>: with options{.eager = true}, whether every singleton of the injector
// can be built, which make_injector needs, as it builds them; true, without looking, for an
// injector that isn't eager. It is make_injector's own check, eager_fails (static/injector.hpp),
// which reads the memoized can_resolve<Key&> of create<Key&>() for each singleton.
template <class Injector>
inline constexpr bool singletons_build =
    !eager_fails<injector_traits<Injector>::opts.eager, Injector>;

// makeable<Injector>: whether make_injector makes an injector of this type, from arguments of its
// argument types, passed as rvalues: the checks of accepted, then a sound set of bindings (no key
// bound twice, overrides that apply: injector_traits's has_duplicate_keys) and, for an eager
// injector, singletons that can be built. A concept's conjunction asks each question only once
// the ones before it hold, so a type that isn't an injector, or an argument that is no binding,
// is never looked into further, and none of them reports an error. How the arguments were passed
// isn't in the type: an lvalue whose bindings can't be copied is the call's mistake.
template <class Injector>
concept makeable = is_injector<Injector> && accepted<Injector>::parts && accepted<Injector>::keys &&
                   accepted<Injector>::arity && (!injector_traits<Injector>::has_duplicate_keys) &&
                   singletons_build<Injector>;

// create_singleton<Binding>(target): requests the object of a binding that the injector owns,
// singleton or singleton_inline, by reference, which builds it and its dependencies; nothing for
// the others. A binding whose key is no key at all has been reported by bind<I>'s check
// (dsl/bind.hpp), and gets nothing more.
template <class Binding, class Injector>
bool create_singleton([[maybe_unused]] Injector& target) {
  using key = typename binding_traits<Binding>::key;
  if constexpr (owns_object<Binding> && is_bindable_key<key>) {
    static_cast<void>(target.template create<key&>());
  }
  return true;
}

// One flat expansion over the bindings, left to right in a braced list, never a fold, as an
// injector may hold more than 256 bindings (plan §8.1 C3).
template <class Injector, class... Bindings>
void create_singletons(Injector& target, type_list<Bindings...> /*bindings*/) {
  [[maybe_unused]] const std::array<bool, sizeof...(Bindings) + 1> created{
      true, detail::create_singleton<Bindings>(target)...};
}

}  // namespace detail

namespace testing {

/// @brief Whether a composition root wires: true exactly when `make_injector` makes an injector of
///        type `Injector` and `injector.create<Root>()` compiles for it.
///
/// It asks the checks that `make_injector` and `create<Root>()` make themselves (plan §5.3), and
/// never reports an error: when it is false, `make_injector` or `create<Root>()` is the place to
/// see why, with the error's `TRL-` ID. Cv qualifiers and references on `Injector` are ignored,
/// so `decltype` of an injector variable or of a function returning one works. It is false:
/// - for a type that isn't an injector;
/// - for an injector that `make_injector` rejects: an argument that is neither a binding, a module
///   nor an override; a binding whose key is no key, such as `bind<const K>`; an
///   `options::max_ctor_arity` above 32; a key bound twice or two overrides of one key
///   (`TRL-E003`); an override with no binding to replace (`TRL-E009`); with
///   `options{.eager = true}`, a singleton that can't be built;
/// - where `create<Root>()` fails to compile, with any of its errors.
///
/// It reads types only, so it can't see how `make_injector`'s arguments are passed: an lvalue
/// argument whose bindings can't be copied, such as a `.to_value()` of a move-only type, is the
/// call's mistake, which `make_injector` reports.
///
/// @tparam Injector The injector type, such as `decltype(make_app_injector(config))`.
/// @tparam Root The request, in any form `create<T>()` takes, such as `App` or `ILogger&`.
///
/// @par Constraints
/// None.
///
/// @par Errors
/// None: it is false where `make_injector` or `create<Root>()` would fail to compile.
///
/// @par Example
/// @code
/// static_assert(trellis::testing::wireable<
///     decltype(make_app_injector(std::declval<const Config&>())), App>);
/// @endcode
template <class Injector, class Root>
concept wireable =
    detail::makeable<std::remove_cvref_t<Injector>> &&
    detail::context<std::remove_cvref_t<Injector>, unique_t>::template can_resolve<Root>;

/// @brief Builds every singleton of an injector, each singleton and singleton_inline binding in
///        binding order with its dependencies first: a smoke test that fails fast.
///
/// Each singleton is requested by reference, as `injector.create<Key&>()` requests it, so a
/// singleton that can't be built is that call's compile error, and a constructor that throws
/// stops here with its exception, leaving the singletons built before it built. Unique bindings
/// build nothing until they are requested. With `options{.eager = true}` every singleton is built
/// already, and this builds nothing.
///
/// @param target The injector.
///
/// @par Constraints
/// - Every singleton of the injector can be built; otherwise compilation fails with the error
///   `target.create<Key&>()` gives for the first one that can't, in binding order.
/// - No other thread destroys the injector meanwhile.
///
/// @par Errors
/// The errors of `injector::create<T>()`, for a singleton or a request in its graph:
/// - `TRL-E001`: an abstract type with no binding, or bound to itself with `bind<I>` alone.
/// - `TRL-E002`: a class whose constructor can't be found, or a copy that can't be made.
/// - `TRL-E004`: deduction finds an ambiguous constructor.
/// - `TRL-E005`: a request form that the binding's lifetime rejects.
/// - `TRL-E010`: a constructor takes more parameters than `max_ctor_arity`.
/// - `TRL-E012`: an unbound type requested by reference, pointer, `std::shared_ptr` or
///   `std::weak_ptr`.
/// - `TRL-E013`: with `options{.strict = true}`, an unbound type requested by value or as
///   `std::unique_ptr`.
/// - `TRL-E014`: `std::unique_ptr<I>` that would delete the implementation through `I`.
///
/// @par Example
/// @code
/// TEST(Wiring, EverySingletonBuilds) {
///   auto injector = make_app_injector(test_config());
///   trellis::testing::create_all(injector);  // throws what a constructor throws
/// }
/// @endcode
template <options Opts, class... Parts>
void create_all(injector<Opts, Parts...>& target) {
  detail::create_singletons(target,
                            typename detail::injector_traits<injector<Opts, Parts...>>::bindings{});
}

}  // namespace testing

}  // namespace trellis::inline v1

#endif  // TRELLIS_TESTING_WIRING_HPP
