// Trellis static engine: the injector, make_injector and create<T>() (plan §5.3, §6.9, §6.13,
// §7.5, ADR-0005, ADR-0010).
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
// The injector owns the objects of its singleton and singleton_inline bindings, in slots
// (static/storage.hpp), and destroys them in reverse order of construction (ADR-0005).
// create<T>() serves a request: the creation code is in static/resolve.hpp, which this header
// includes at its end, because resolution itself needs the injector's class.
#ifndef TRELLIS_STATIC_INJECTOR_HPP
#define TRELLIS_STATIC_INJECTOR_HPP

#include <trellis/diagnostics/errors.hpp>
#include <trellis/discovery/deduced.hpp>
#include <trellis/dsl/lifetimes.hpp>
#include <trellis/dsl/module.hpp>
#include <trellis/dsl/options.hpp>
#include <trellis/dsl/request_forms.hpp>
#include <trellis/meta/type_list.hpp>
#include <trellis/static/binding_map.hpp>
#include <trellis/static/injector_traits.hpp>
#include <trellis/static/storage.hpp>

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace trellis::inline v1 {

namespace detail {

// The constructor key of injector: only make_injector makes one.
struct injector_key {
  explicit injector_key() = default;
};

// Resolution, defined in static/resolve.hpp: the context of a request (whose can_resolve<R> is
// the check and resolve<R>() the creation code), the failure pass, and eager mode's construction
// of every singleton.
template <class Injector, class Lifetime, slot_access Access = slot_access::checked>
struct context;
template <class Injector, class R>
void explain();
template <class Injector, class... Bindings>
void build_eagerly(Injector& owner, type_list<Bindings...> bindings);

// Gives resolution access to an injector's bindings and storage; the injector befriends it.
struct injector_access {
  template <class Injector>
  static auto& bindings(Injector& owner) noexcept {
    return owner.bindings_;
  }
  template <class Injector>
  static auto& slots(Injector& owner) noexcept {
    return owner.slots_;
  }
  template <class Injector>
  static auto& teardown(Injector& owner) noexcept {
    return owner.teardown_;
  }
  template <class Injector>
  static auto& sync(Injector& owner) noexcept {
    return owner.sync_;
  }
};

// The bound that deduced discovery uses for an injector: options::max_ctor_arity, clamped to the
// largest bound discovery supports, so that a larger value is reported once, by make_injector,
// rather than again for every deduced type (discovery/deduced.hpp).
template <options Opts>
inline constexpr std::size_t effective_max_ctor_arity =
    Opts.max_ctor_arity < deduced_arity_limit ? Opts.max_ctor_arity : deduced_arity_limit;

// owns_incomplete<T>: T is std::unique_ptr<X> with an X that is incomplete here. No object of T
// can be destroyed here, as std::default_delete<X> asserts that X is complete, so no call can
// return one and no constructor can take one by value; and no request of one can be served, as
// discovery reports the incomplete X (TRL-E002).
template <class T>
inline constexpr bool owns_incomplete = false;
template <class X>
inline constexpr bool owns_incomplete<std::unique_ptr<X>> = !complete_class<X>;

// returnable<T>: a function can return T: a reference, void, a scalar, or a complete class that
// isn't abstract and can be destroyed here. Not an array or a function type, and not an abstract
// class, which a function can't even be declared to return, or an incomplete one, which a call
// can't return. The class conditions are a concept's conjunction, so std::is_abstract_v is asked
// only about a complete class ([temp.constr.op]).
template <class T>
concept returnable = std::is_reference_v<T> || std::is_void_v<T> || std::is_scalar_v<T> ||
                     ((std::is_class_v<T> || std::is_union_v<T>) && complete_class<T> &&
                      !std::is_abstract_v<T> && !owns_incomplete<T>);

// create_result_t<R>: what create<R>() returns: request_result_t<R>, or void where a function
// can't return that type, such as for create<I>() with an abstract I. No such request can be
// served, and naming its result as the return type would be an error of its own, beside the one
// the failure pass reports (ADR-0006 item 2).
template <class R>
using create_result_t =
    std::conditional_t<returnable<request_result_t<R>>, request_result_t<R>, void>;

}  // namespace detail

/// @brief A static injector: the bindings of a composition root, flattened once, with the
///        options `Opts`. Made by `trellis::make_injector`; `create<T>()` builds objects from it.
///
/// `Parts` are the types of the arguments that `make_injector` received: bindings and modules.
/// Passing one named module keeps the type short, `injector<options{}, app_module>` (ADR-0010):
///
///     struct app_module : decltype(make_app_module(std::declval<const Config&>())) {};
///     auto injector = trellis::make_injector(app_module{make_app_module(config)});
///
/// The injector owns one object per singleton and singleton_inline binding, built on first use
/// (or by `make_injector` with `options{.eager = true}`), and destroys them in reverse order of
/// construction when it is destroyed (ADR-0005). It is neither copyable nor movable, because the
/// objects it creates may refer to it and to its singletons (plan §7.5); `auto injector =
/// make_injector(...)` works through guaranteed copy elision.
///
/// @tparam Opts The options (plan §6.13).
/// @tparam Parts The argument types of `make_injector`, without references and cv qualifiers.
///
/// @par Constraints
/// - Objects that refer to the injector's singletons by reference or pointer don't outlive the
///   injector; holding a singleton by `std::shared_ptr` is the way to outlive it (plan §6.3).
///   Otherwise the behaviour is undefined.
///
/// @par Errors
/// None for the class itself. `trellis::make_injector` reports `TRL-E003`, and `create<T>()`
/// the errors it lists.
///
/// @par Example
/// @code
/// auto injector = trellis::make_injector(storage_module(config), logging_module(),
///                                        trellis::bind<IClock>.to<SystemClock>());
/// App app = injector.create<App>();
/// @endcode
template <options Opts, class... Parts>
class injector {
  using storage = detail::storage_for<Opts.threading, typename detail::flatten<Parts...>::bindings>;

 public:
  /// @brief Takes the bindings out of `parts`: moved from rvalues, copied from lvalues. With
  ///        `options{.eager = true}`, then builds every singleton.
  ///
  /// @param key The constructor key, which only `trellis::make_injector` has.
  /// @param parts The arguments of `make_injector`.
  ///
  /// @par Constraints
  /// - Called by `trellis::make_injector` only, which checks the keys first.
  ///
  /// @par Errors
  /// None. With `options{.eager = true}`, the errors of `create<T>()` for each singleton.
  ///
  /// @par Example
  /// @code
  /// auto injector = trellis::make_injector(trellis::bind<IClock>.to<SystemClock>());
  /// @endcode
  template <class... Args>
  explicit injector([[maybe_unused]] detail::injector_key key, Args&&... parts)
      : bindings_(trellis::make_module(std::forward<Args>(parts)...)) {
    if constexpr (Opts.eager) {
      // Each singleton, in binding order, its dependencies first. If a constructor throws, the
      // exception leaves this constructor, and the members' destructors tear down what was built,
      // in reverse order: make_injector's strong guarantee (plan §7.5, ADR-0005 item 5).
      detail::build_eagerly(*this, typename detail::flatten<Parts...>::bindings{});
    }
  }

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
  /// @brief Destroys the singletons it built, in reverse order of construction, then the
  ///        bindings, and with them the values of `.to_value()` bindings.
  ///
  /// A singleton that a consumer still holds through a `std::shared_ptr` lives on until the last
  /// one is gone (ADR-0005).
  ///
  /// @par Constraints
  /// - No other thread uses the injector meanwhile; otherwise the behaviour is undefined.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// {
  ///   auto injector = trellis::make_injector(trellis::bind<Logger>.in(trellis::singleton));
  ///   Logger& logger = injector.create<Logger&>();
  /// }  // logger is destroyed here, with the injector
  /// @endcode
  ~injector() = default;

  /// @brief Creates or finds the object that request `R` asks for, building its dependencies
  ///        first (plan §5.3).
  ///
  /// `R` is any request form that the binding of its key allows (plan §6.3): `T` (a new object,
  /// or a copy where `.allow_copy()` permits one), `T&` and `const T&`, `T*` and `const T*`,
  /// `std::unique_ptr<T>`, `std::shared_ptr<T>` and `std::weak_ptr<T>`. A type without a binding
  /// is built as a new object each time, unless `options{.strict = true}`. A singleton is built
  /// on its first request, once, even when threads request it concurrently (unless
  /// `threading::none`). Constructor parameters are requests too, served the same way.
  ///
  /// The whole graph is checked at compile time. A graph that can't be built instantiates no
  /// creation code: each mistake is one compile error with a `TRL-` ID (ADR-0006).
  ///
  /// @tparam R The request, such as `App`, `Service&` or `std::shared_ptr<ILogger>`.
  /// @return `R` for a reference request; otherwise an `R` without top-level `const`, as a
  ///         prvalue, so a class that can't be copied or moved can be created by value. `X&&` is
  ///         the request `X` and returns an `X`. (A by-value request of an abstract or incomplete
  ///         class can never be served: its one error is reported, and the declared return type
  ///         is `void`, so that it adds no second error.)
  ///
  /// @par Constraints
  /// - Each request in the graph, `R` and every constructor parameter, is one its binding's
  ///   lifetime allows, has a binding or a class that can be built without one, and names a
  ///   class whose constructor can be discovered; otherwise compilation fails with one of the
  ///   errors below.
  /// - A constructor that throws leaves already built singletons built, and destroys the unique
  ///   objects built for the request (the basic guarantee); a singleton whose constructor threw
  ///   is built again on its next request.
  /// - A constructor doesn't request its own object again, directly or through another
  ///   singleton's constructor, at run time; otherwise the behaviour is undefined until `lazy`
  ///   reports it as R004 (plan §7.8, Phase 2).
  ///
  /// @par Errors
  /// - `TRL-E001`: a request names an abstract type with no binding, or one bound to itself
  ///   with `bind<I>` alone.
  /// - `TRL-E002`: a class's constructor can't be found, or its declaration matches none, or a
  ///   by-value request needs a copy that the key can't be constructed from, or the class is
  ///   only declared, not defined.
  /// - `TRL-E004`: deduction finds an ambiguous constructor.
  /// - `TRL-E005`: a request form that the binding's lifetime rejects.
  /// - `TRL-E010`: a constructor takes more parameters than `max_ctor_arity`.
  /// - `TRL-E012`: an unbound type requested by reference, pointer, `std::shared_ptr` or
  ///   `std::weak_ptr`.
  /// - `TRL-E013`: with `options{.strict = true}`, an unbound type requested by value or as
  ///   `std::unique_ptr` (provisional ID).
  /// - `TRL-E014`: `std::unique_ptr<I>` requested where the binding's implementation derives
  ///   from `I` and `I`'s destructor isn't virtual (provisional ID).
  ///
  /// @par Example
  /// @code
  /// auto injector = trellis::make_injector(
  ///     trellis::bind<ILogger>.to<ConsoleLogger>().in(trellis::singleton));
  /// App app = injector.create<App>();                       // a new App
  /// ILogger& logger = injector.create<ILogger&>();          // the injector's logger
  /// std::shared_ptr<ILogger> shared = injector.create<std::shared_ptr<ILogger>>();
  /// @endcode
  template <class R>
  detail::create_result_t<R> create() {
    // The caller asks as a unique consumer would: nothing it holds is subject to the
    // captive-dependency rule (plan §6.3). An eager injector built every singleton in
    // make_injector, so it reads them without testing their slots (plan §7.6).
    using root =
        detail::context<injector, unique_t,
                        Opts.eager ? detail::slot_access::ready : detail::slot_access::checked>;
    if constexpr (root::template can_resolve<R>) {
      root ctx{*this};
      return ctx.template resolve<R>();
    } else {
      // The graph can't be built: report each cause once, and instantiate no creation code
      // (ADR-0006 item 2). unreachable() stands in for the value; nothing follows the if, which
      // MSVC would report as unreachable code (C4702).
      detail::explain<injector, R>();
      detail::unreachable();
    }
  }

 private:
  friend struct detail::injector_access;

  // The flattened bindings, as one module: the stored_binding<J, B> bases that the binding map's
  // values name. They hold the objects of .to_value() and .to_shared() bindings, which singletons
  // may refer to, so they are destroyed last.
  detail::flat_module_t<Parts...> bindings_;
  // What the slots share for blocking: nothing, except without atomic::wait (ADR-0012 item 3).
  typename storage::sync sync_;
  // One slot per singleton and singleton_inline binding.
  typename storage::slots slots_;
  // Declared after the slots, so that it destroys the objects before their storage goes (ADR-0005
  // item 4, rule 2).
  typename storage::teardown teardown_;
};

/// @brief Builds a static injector from bindings and modules, with the options `Opts`.
///
/// The arguments are flattened once into one binding map keyed by type, qualifiers included
/// (plan §5.3, ADR-0010): a module contributes its bindings, in order. A key bound twice is
/// TRL-E003. Bindings are moved out of rvalue arguments and copied from lvalues. With
/// `options{.eager = true}`, every singleton is built here, in dependency order, and a constructor
/// that throws leaves nothing built (the strong guarantee).
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
/// - `Opts.max_ctor_arity` is at most 32; otherwise compilation fails with a static_assert.
/// - With `options{.eager = true}`, every singleton can be built, as `create<T>()` checks.
///
/// @par Errors
/// - `TRL-E003`: two bindings in `parts`, or in the modules among them, bind the same key.
/// - With `options{.eager = true}`, the errors of `injector::create<T>()` for each singleton.
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
  } else if constexpr (Opts.max_ctor_arity > detail::deduced_arity_limit) {
    // Checked once, here; the engine then uses the clamped bound, so no deduced type reports it
    // again (effective_max_ctor_arity).
    static_assert(Opts.max_ctor_arity <= detail::deduced_arity_limit,
                  "trellis: options::max_ctor_arity is at most 32, the largest arity deduced "
                  "discovery probes; declare longer constructors with trellis::ctor_traits");
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

// Resolution needs the injector's class, which is complete here; including it last lets either
// header be included first (static/resolve.hpp includes this one).
#include <trellis/static/resolve.hpp>  // IWYU pragma: keep

#endif  // TRELLIS_STATIC_INJECTOR_HPP
