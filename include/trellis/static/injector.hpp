// Trellis static engine: the injector and make_injector (plan §5.3, §6.9, §6.13, ADR-0010).
// Copyright (c) 2026 Aryan Patel
// SPDX-License-Identifier: BSL-1.0
//
// make_injector<options{...}>(bindings_and_modules...) flattens its arguments once into one list
// of bindings (dsl/module.hpp), checks that no key is bound twice (TRL-E003), and returns an
// injector that holds the bindings. The injector's binding map (static/binding_map.hpp) finds the
// binding for a key in one overload resolution.
//
// The injector's type is injector<Opts, Parts...>, where Parts are make_injector's argument types,
// not the flattened bindings. A named module (struct app : decltype(make_module(...)) {}) thus
// keeps the type short, injector<options{}, app>, in every diagnostic and in the name of
// everything the engine instantiates per injector (ADR-0010's open point; plan §8.1 C5).
//
// T1.4 delivers the injector as a shell: it holds the bindings, and detail::injector_traits gives
// the engine its binding map. Storage, lifetimes at run time and create<T>() come with T1.5.
#ifndef TRELLIS_STATIC_INJECTOR_HPP
#define TRELLIS_STATIC_INJECTOR_HPP

#include <trellis/diagnostics/errors.hpp>
#include <trellis/dsl/module.hpp>
#include <trellis/dsl/options.hpp>
#include <trellis/meta/type_map.hpp>
#include <trellis/static/binding_map.hpp>

#include <type_traits>
#include <utility>

namespace trellis::inline v1 {

template <options Opts, class... Parts>
class injector;

namespace detail {

// The constructor key of injector: only make_injector makes one.
struct injector_key {
  explicit injector_key() = default;
};

// injector_traits<Injector>: what the engine needs to know about an injector type. Its bindings
// are its arguments' bindings, flattened once (the same flatten<...> instantiation as the
// injector's own), and map is their binding map. When a key repeats, make_injector has reported
// TRL-E003, and map is an empty type_map instead: a lookup in a map with a repeated key would be
// the map's own error, a second one for the same mistake. request_check doesn't look up then.
template <class Injector>
struct injector_traits;
template <options Opts, class... Parts>
struct injector_traits<injector<Opts, Parts...>> {
  static constexpr options opts = Opts;
  using bindings = typename flatten<Parts...>::bindings;
  static constexpr bool has_duplicate_keys = binding_map<bindings>::has_duplicate_keys;
  using map =
      std::conditional_t<has_duplicate_keys, type_map<>, typename binding_map<bindings>::type>;
};

}  // namespace detail

/// @brief A static injector: the bindings of a composition root, flattened once, with the
///        options `Opts`. Made by `trellis::make_injector`.
///
/// `Parts` are the types of the arguments that `make_injector` received: bindings and modules.
/// Passing one named module keeps the type short, `injector<options{}, app_module>` (ADR-0010):
///
///     struct app_module : decltype(make_app_module(std::declval<const Config&>())) {};
///     auto injector = trellis::make_injector(app_module{make_app_module(config)});
///
/// An injector is neither copyable nor movable, because the objects it will create may refer to
/// it (plan §7.5); `auto injector = make_injector(...)` works through guaranteed copy elision.
/// Creating objects, `create<T>()`, comes with task T1.5.
///
/// @tparam Opts The options (plan §6.13).
/// @tparam Parts The argument types of `make_injector`, without references and cv qualifiers.
///
/// @par Constraints
/// None. Made by `trellis::make_injector`.
///
/// @par Errors
/// None. `trellis::make_injector` reports `TRL-E003`.
///
/// @par Example
/// @code
/// auto injector = trellis::make_injector(storage_module(config), logging_module(),
///                                        trellis::bind<IClock>.to<SystemClock>());
/// @endcode
template <options Opts, class... Parts>
class injector {
 public:
  /// @brief Takes the bindings out of `parts`: moved from rvalues, copied from lvalues.
  ///
  /// @param key The constructor key, which only `trellis::make_injector` has.
  /// @param parts The arguments of `make_injector`.
  ///
  /// @par Constraints
  /// - Called by `trellis::make_injector` only, which checks the keys first.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// auto injector = trellis::make_injector(trellis::bind<IClock>.to<SystemClock>());
  /// @endcode
  template <class... Args>
  explicit injector([[maybe_unused]] detail::injector_key key, Args&&... parts)
      : bindings_(trellis::make_module(std::forward<Args>(parts)...)) {}

  /// @brief Deleted: an injector isn't copyable (plan §7.5).
  ///
  /// @par Constraints
  /// None.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// // auto copy = injector;  // error: deleted
  /// @endcode
  injector(const injector&) = delete;
  /// @brief Deleted: an injector isn't movable, because objects it creates may refer to it.
  ///
  /// @par Constraints
  /// None.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// // auto moved = std::move(injector);  // error: deleted
  /// @endcode
  injector(injector&&) = delete;
  /// @brief Deleted: an injector isn't copy-assignable.
  ///
  /// @par Constraints
  /// None.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// // injector = other;  // error: deleted
  /// @endcode
  injector& operator=(const injector&) = delete;
  /// @brief Deleted: an injector isn't move-assignable.
  ///
  /// @par Constraints
  /// None.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// // injector = std::move(other);  // error: deleted
  /// @endcode
  injector& operator=(injector&&) = delete;
  /// @brief Destroys the bindings, and with them the values of `.to_value()` bindings.
  ///
  /// @par Constraints
  /// None.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// {
  ///   auto injector = trellis::make_injector(trellis::bind<Config>.to_value(Config{}));
  /// }  // destroyed here
  /// @endcode
  ~injector() = default;

 private:
  // The flattened bindings, as one module: the stored_binding<J, B> bases that the binding map's
  // values name.
  detail::flat_module_t<Parts...> bindings_;
};

/// @brief Builds a static injector from bindings and modules, with the options `Opts`.
///
/// The arguments are flattened once into one binding map keyed by type, qualifiers included
/// (plan §5.3, ADR-0010): a module contributes its bindings, in order. A key bound twice is
/// TRL-E003. Bindings are moved out of rvalue arguments and copied from lvalues.
///
/// @tparam Opts The options (plan §6.13), such as `trellis::options{.eager = true}`.
/// @param parts Bindings, such as `trellis::bind<IClock>.to<SystemClock>()`, and modules made with
///              `trellis::make_module`, in any order.
/// @return The injector, by value (it is neither copyable nor movable, so bind it to a variable).
///
/// @par Constraints
/// - Each key is bound at most once across `parts`; otherwise compilation fails with TRL-E003.
/// - Every argument is a binding or a module; otherwise compilation fails with a static_assert
///   naming `make_injector`.
/// - The bindings of an lvalue argument can be copied; otherwise compilation fails with a
///   static_assert that says to pass it with `std::move`.
///
/// @par Errors
/// - `TRL-E003`: two bindings in `parts`, or in the modules among them, bind the same key.
///
/// @par Example
/// @code
/// auto injector = trellis::make_injector(storage_module(config),
///                                        trellis::bind<IClock>.to<SystemClock>());
/// auto strict = trellis::make_injector<trellis::options{.strict = true}>(app_module(config));
/// @endcode
template <options Opts = options{}, class... Parts>
[[nodiscard]] injector<Opts, std::remove_cvref_t<Parts>...> make_injector(
    [[maybe_unused]] Parts&&... parts) {
  using result = injector<Opts, std::remove_cvref_t<Parts>...>;
  if constexpr (!detail::bindings_and_modules<std::remove_cvref_t<Parts>...>) {
    // The same check and literal as make_module's: a static_assert needs a literal, and an
    // assertion in a shared helper would add instantiation notes (ADR-0006 item 2).
    static_assert(detail::bindings_and_modules<std::remove_cvref_t<Parts>...>,
                  "trellis: make_module and make_injector take bindings, such as "
                  "trellis::bind<I>.to<C>(), and modules made with trellis::make_module");
    detail::unreachable();
  } else if constexpr (!detail::bindings_can_be_taken<Parts...>) {
    static_assert(
        detail::bindings_can_be_taken<Parts...>,
        "trellis: make_module and make_injector copy the bindings of an lvalue argument, and "
        "one of them can't be copied, such as a .to_value() of a move-only type; "
        "pass that argument with std::move");
    detail::unreachable();
  } else if constexpr (detail::injector_traits<result>::has_duplicate_keys) {
    // One static_assert, in the function the user called, before anything uses the binding map
    // (ADR-0006). The injector type stays complete: its class doesn't build the map.
    using duplicate = detail::first_duplicate<typename detail::injector_traits<result>::bindings>;
    static_assert(error::E003<typename duplicate::key, typename duplicate::first_binding,
                              typename duplicate::second_binding>::reported,
                  "TRL-E003: duplicate binding for a key: each key may be bound only once per "
                  "injector");
    detail::unreachable();
  } else {
    return result(detail::injector_key{}, std::forward<Parts>(parts)...);
  }
}

}  // namespace trellis::inline v1

#endif  // TRELLIS_STATIC_INJECTOR_HPP
