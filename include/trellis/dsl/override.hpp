// Trellis binding vocabulary: overrides, and how make_injector applies them (plan §6.10,
// ADR-0010).
// Copyright (c) 2026 Aryan Patel
// SPDX-License-Identifier: BSL-1.0
//
// override<K> starts a binding that replaces the binding of K which make_injector's other
// arguments provide, wherever it is among them, inside modules and named modules included: a test
// swaps one implementation, such as a repository for a mock, and keeps the rest of the production
// wiring (plan §6.10). It has bind<K>'s vocabulary: override_binding<B> wraps the binding B that
// the same chain on bind<K> makes, and each step calls bind's step and wraps the result, so the
// checks, and their messages, are bind's (dsl/bind.hpp). Nothing accumulates (ADR-0010).
//
// make_injector applies the overrides when it flattens its arguments (detail::compose, below):
//
//   - The other arguments' bindings are flattened as make_module flattens them (dsl/module.hpp);
//     an override contributes none.
//   - Each of those bindings whose key an override names is replaced by the override's binding,
//     in place: one lookup per binding in a type_map from the overrides' keys (meta/type_map.hpp).
//     The result keeps the order of the bindings. "Linear" counts instantiations, as in T1.4's
//     flattening: with an override among the arguments, the other arguments take the mixed
//     layout, even one named module, which alone would be copied whole, and the mixed layout
//     names each binding by a deduction against its module's bases, which is slow for a large
//     module (docs/internals/overrides-and-testing.md has the numbers).
//   - Two overrides of one key are TRL-E003, and an override whose key no other binding binds is
//     TRL-E009; make_injector reports them (static/injector.hpp), and the injector then resolves
//     nothing more, as for a key bound twice (static/injector_traits.hpp).
//
// Overrides are arguments of make_injector only: make_module rejects them (dsl/module.hpp), so a
// module holds only bindings.
#ifndef TRELLIS_DSL_OVERRIDE_HPP
#define TRELLIS_DSL_OVERRIDE_HPP

#include <trellis/config.hpp>
#include <trellis/dsl/bind.hpp>
#include <trellis/dsl/lifetimes.hpp>
#include <trellis/dsl/module.hpp>
#include <trellis/dsl/request_forms.hpp>
#include <trellis/meta/type_list.hpp>
#include <trellis/meta/type_map.hpp>

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

namespace trellis::inline v1 {

namespace detail {
struct override_access;
}  // namespace detail

/// @brief An override: a binding that replaces the binding of its key which the other arguments
///        of `trellis::make_injector` provide, made by `trellis::override<K>` and its chain.
///
/// Defined, by the two partial specializations below, for a binding type
/// `trellis::binding<Key, Impl, Lifetime, AllowCopy>`, the one the same chain on
/// `trellis::bind<K>` makes; for any other type it is incomplete.
///
/// @tparam Binding The binding it wraps.
///
/// @par Constraints
/// - `Binding` is a binding type; otherwise the class is incomplete.
///
/// @par Errors
/// None itself. `trellis::make_injector` raises `TRL-E003` for two overrides of one key and
/// `TRL-E009` for an override whose key none of its other arguments binds.
///
/// @par Example
/// @code
/// trellis::override_binding<trellis::binding<IClock, FixedClock, trellis::unique_t>> clock =
///     trellis::override<IClock>.to<FixedClock>();
/// @endcode
template <class Binding>
class override_binding;

/// @brief An override of the key `Key` by objects of type `Impl` that the injector constructs,
///        with the lifetime `Lifetime`: what `trellis::override<K>` and its chain make.
///
/// It wraps the binding that the same chain on `trellis::bind<K>` makes,
/// `binding<Key, Impl, Lifetime, AllowCopy>`, and each function below calls that binding's
/// function and wraps the result, so the checks, and their messages, are `bind<K>`'s. Pass it to
/// `trellis::make_injector`, which replaces the binding of `Key` that its other arguments provide
/// with this one (plan §6.10).
///
/// @tparam Key The key whose binding the override replaces.
/// @tparam Impl The class to construct: `Key`, or a class publicly derived from it; after
///              `.with_ctor<Args...>()`, that class with the declared constructor, as for
///              `trellis::binding`.
/// @tparam Lifetime The lifetime: `trellis::unique_t`, `trellis::singleton_t` or
///                  `trellis::singleton_inline_t`.
/// @tparam AllowCopy Whether a shared object may be requested by value, as a copy.
///
/// @par Constraints
/// - `Key` is an object type without cv qualifiers that is not an array; otherwise compilation
///   fails with `bind<I>`'s static_assert.
///
/// @par Errors
/// None itself. `trellis::make_injector` raises `TRL-E003` for two overrides of one key and
/// `TRL-E009` for an override whose key none of its other arguments binds.
///
/// @par Example
/// @code
/// auto injector = trellis::make_injector(
///     app_module(config), trellis::override<IClock>.to<FixedClock>().in(trellis::singleton));
/// @endcode
template <class Key, class Impl, class Lifetime, bool AllowCopy>
  requires detail::is_constructed_lifetime<Lifetime>
class override_binding<binding<Key, Impl, Lifetime, AllowCopy>> {
  using wrapped = binding<Key, Impl, Lifetime, AllowCopy>;

 public:
  /// @brief Makes the override of the key by itself, unique, checking the key as
  ///        `trellis::bind<K>` does: `trellis::override<K>` makes one this way.
  ///
  /// @par Constraints
  /// - `Key` is an object type without cv qualifiers that is not an array; otherwise compilation
  ///   fails with `bind<I>`'s static_assert, once per key.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// constexpr auto clock = trellis::override<IClock>;  // override<K> calls this constructor
  /// @endcode
  constexpr override_binding() noexcept = default;

  /// @brief Overrides the key with the implementation `C`, which the injector constructs.
  ///
  /// @tparam C The implementation: the key itself, or a class publicly derived from it.
  /// @return The override of the key by `C`, with the same lifetime.
  ///
  /// @par Constraints
  /// - As for `bind<I>.to<C>()`: `C` is the key or a class publicly derived from it, without cv
  ///   qualifiers, and no implementation other than the key is named yet; otherwise compilation
  ///   fails with a static_assert naming `bind<I>.to<C>()`.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// auto clock = trellis::override<IClock>.to<FixedClock>();  // unique by default
  /// @endcode
  template <class C>
  [[nodiscard]] constexpr auto to() const noexcept {
    return override_binding::wrap(binding_.template to<C>());
  }

  /// @brief Sets the lifetime: `trellis::unique` (the default), `trellis::singleton` or
  ///        `trellis::singleton_inline`.
  ///
  /// @param lifetime The lifetime value; only its type matters.
  /// @return The override with that lifetime.
  ///
  /// @par Constraints
  /// - As for `bind<I>.in(lifetime)`: the lifetime is one of the three above; otherwise
  ///   compilation fails with a static_assert naming `.in(lifetime)`.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// auto store = trellis::override<IRepository>.to<MemoryRepository>().in(trellis::singleton);
  /// @endcode
  template <class L>
  [[nodiscard]] constexpr auto in(L lifetime) const noexcept {
    return override_binding::wrap(binding_.in(lifetime));
  }

  /// @brief Lets consumers request the shared object by value, as a copy.
  ///
  /// @return The same override, allowing copies.
  ///
  /// @par Constraints
  /// None.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// auto settings = trellis::override<Settings>.in(trellis::singleton).allow_copy();
  /// @endcode
  [[nodiscard]] constexpr auto allow_copy() const noexcept {
    return override_binding::wrap(binding_.allow_copy());
  }

  /// @brief Declares the constructor that the injector calls for the class the override builds:
  ///        the one that takes `Args...`.
  ///
  /// @tparam Args The constructor's parameter types, as the constructor spells them.
  /// @return The override with the declaration.
  ///
  /// @par Constraints
  /// - As for `bind<I>.with_ctor<Args...>()`: the override declares no constructor yet; otherwise
  ///   compilation fails with a static_assert naming `.with_ctor<Args...>()`.
  ///
  /// @par Errors
  /// None itself. A declaration that matches no constructor of the class is `TRL-E002`, raised by
  /// `create<T>()`, as for `bind<I>.with_ctor<Args...>()`.
  ///
  /// @par Example
  /// @code
  /// auto store = trellis::override<IRepository>.to<MemoryRepository>().with_ctor<>();
  /// @endcode
  template <class... Args>
  [[nodiscard]] constexpr auto with_ctor() const noexcept {
    return override_binding::wrap(binding_.template with_ctor<Args...>());
  }

  /// @brief Overrides the key with an object that the caller owns, by reference: a test double
  ///        that the test keeps, such as a mock.
  ///
  /// The object must outlive the injector's use of it. A consumer can take it by reference or
  /// pointer, or by value as a copy with `.allow_copy()`, but not as `std::shared_ptr`, which
  /// the caller's object doesn't have: use `.to_shared()` for that (plan §6.3).
  ///
  /// @param object The object: an object of the key's type, or of a class publicly derived from
  ///               it.
  /// @return The external override of the key by `object`.
  ///
  /// @par Constraints
  /// - As for `bind<I>.to_ref(x)`: `object` is of the key's type or of a class publicly derived
  ///   from it, not volatile, and the override is still `override<K>` as written; otherwise
  ///   compilation fails with a static_assert naming `bind<I>.to_ref(x)`. An rvalue doesn't
  ///   compile: the binding would dangle.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// testing::StrictMock<MockRepository> repo;
  /// auto injector = trellis::make_injector(app_module(config),
  ///                                        trellis::override<IRepository>.to_ref(repo));
  /// @endcode
  template <class T>
  [[nodiscard]] constexpr auto to_ref(T& object) const noexcept {
    return override_binding::wrap(binding_.to_ref(object));
  }

  // The deleted to_ref(const T&&) below returns an override, not void, as bind<I>.to_ref's
  // deleted twin returns a binding: given void, MSVC also rejects the variable the call
  // initializes (dsl/bind.hpp).
  /// @brief Rejects a temporary: an override by it would dangle.
  ///
  /// @par Constraints
  /// - Never selected for a valid program: `.to_ref()` takes an lvalue.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// // trellis::override<Config>.to_ref(Config{});  // error: deleted function
  /// @endcode
  template <class T>
  [[nodiscard]] override_binding to_ref(const T&& object) const = delete;

  /// @brief Overrides the key with an object that the caller owns, through a pointer.
  ///
  /// The same as `.to_ref(*object)`.
  ///
  /// @param object A pointer to the object, which must not be null.
  /// @return The external override of the key by `*object`.
  ///
  /// @par Constraints
  /// - As for `bind<I>.to_ptr(p)`: `object` points to an object of the key's type or of a class
  ///   publicly derived from it, not volatile, and the override is still `override<K>` as
  ///   written; otherwise compilation fails with a static_assert naming `bind<I>.to_ptr(p)`. A
  ///   null pointer is undefined behaviour once the override is used.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// FakeMetrics metrics;
  /// auto injector = trellis::make_injector(app_module(config),
  ///                                        trellis::override<Metrics>.to_ptr(&metrics));
  /// @endcode
  template <class T>
  [[nodiscard]] constexpr auto to_ptr(T* object) const noexcept {
    return override_binding::wrap(binding_.to_ptr(object));
  }

  /// @brief Overrides the key with a copy of `value`, which the injector owns.
  ///
  /// @param value The value, copied or moved into the override. Its type, without references and
  ///              cv qualifiers, is the key or a class publicly derived from it.
  /// @return The value override.
  ///
  /// @par Constraints
  /// - As for `bind<I>.to_value(v)`: the value's type is the key or a class publicly derived from
  ///   it, the override can store a copy of it, and the override is still `override<K>` as
  ///   written; otherwise compilation fails with a static_assert naming `bind<I>.to_value(v)`.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// auto injector = trellis::make_injector(
  ///     app_module(config), trellis::override<Config>.to_value(Config{"sqlite::memory:"}));
  /// @endcode
  template <class V>
  [[nodiscard]] constexpr auto to_value(V&& value) const
      noexcept(std::is_nothrow_constructible_v<std::remove_cvref_t<V>, V> &&
               std::is_nothrow_move_constructible_v<std::remove_cvref_t<V>>) {
    return override_binding::wrap(binding_.to_value(std::forward<V>(value)));
  }

  /// @brief Overrides the key with an object that the caller shares through a `std::shared_ptr`.
  ///
  /// Consumers can take it in every form a shared object allows, `std::shared_ptr` included.
  ///
  /// @param object The shared pointer, which must not be null. It may point to an object of a
  ///               class publicly derived from the key.
  /// @return The shared external override.
  ///
  /// @par Constraints
  /// - As for `bind<I>.to_shared(sp)`: `object` points to an object of the key's type or of a
  ///   class publicly derived from it, not volatile, and the override is still `override<K>` as
  ///   written; otherwise compilation fails with a static_assert naming `bind<I>.to_shared(sp)`.
  ///   A null pointer is undefined behaviour once the override is used.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// auto repo = std::make_shared<testing::NiceMock<MockRepository>>();
  /// auto injector = trellis::make_injector(app_module(config),
  ///                                        trellis::override<IRepository>.to_shared(repo));
  /// @endcode
  template <class T>
  [[nodiscard]] auto to_shared(std::shared_ptr<T> object) const noexcept {
    return override_binding::wrap(binding_.to_shared(std::move(object)));
  }

 private:
  template <class>
  friend class override_binding;
  friend struct detail::override_access;

  constexpr explicit override_binding(wrapped binding) noexcept(
      std::is_nothrow_move_constructible_v<wrapped>)
      : binding_(std::move(binding)) {}

  // The override of a binding that a step of the chain returned.
  template <class Next>
  static constexpr override_binding<Next> wrap(Next next) noexcept(
      std::is_nothrow_move_constructible_v<Next>) {
    return override_binding<Next>(std::move(next));
  }

  wrapped binding_;
};

/// @brief An override of the key `Key` by an object that the binding holds: the caller's, made
///        by `.to_ref()` or `.to_ptr()`; a shared one, made by `.to_shared()`; or the injector's
///        copy, made by `.to_value()`.
///
/// It wraps the binding that the same chain on `trellis::bind<K>` makes. As that binding, it has
/// only `.allow_copy()` left to chain, and a `.with_ctor<Args...>()` that rejects the step with
/// the binding's static_assert: the injector builds nothing for it.
///
/// @tparam Key The key whose binding the override replaces.
/// @tparam Object The type of the object the binding holds: `Key`, `const Key`, or for a value,
///                the value's type.
/// @tparam Lifetime `trellis::external_t`, `trellis::shared_external_t` or `trellis::value_t`.
/// @tparam AllowCopy Whether the object may be requested by value, as a copy.
///
/// @par Constraints
/// None. Made only by the functions above.
///
/// @par Errors
/// None itself. `trellis::make_injector` raises `TRL-E003` for two overrides of one key and
/// `TRL-E009` for an override whose key none of its other arguments binds.
///
/// @par Example
/// @code
/// FakeClock clock;
/// auto clock_override = trellis::override<IClock>.to_ref(clock);
/// auto injector = trellis::make_injector(app_module(config), clock_override);
/// @endcode
template <class Key, class Object, class Lifetime, bool AllowCopy>
  requires(!detail::is_constructed_lifetime<Lifetime>)
class override_binding<binding<Key, Object, Lifetime, AllowCopy>> {
  using wrapped = binding<Key, Object, Lifetime, AllowCopy>;

 public:
  /// @brief Lets consumers request the object by value, as a copy.
  ///
  /// @return The same override, allowing copies.
  ///
  /// @par Constraints
  /// - For a value override, the value can be copied.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// auto config_override = trellis::override<Config>.to_ref(config).allow_copy();
  /// @endcode
  [[nodiscard]] constexpr auto allow_copy() const& noexcept(
      std::is_nothrow_copy_constructible_v<wrapped>) {
    return override_binding::wrap(binding_.allow_copy());
  }
  /// @brief Lets consumers request the object by value, as a copy; moves the object along.
  ///
  /// @return The same override, allowing copies.
  ///
  /// @par Constraints
  /// None.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// auto clock_override =
  ///     trellis::override<Clock>.to_shared(std::make_shared<FakeClock>()).allow_copy();
  /// @endcode
  [[nodiscard]] constexpr auto allow_copy() && noexcept(
      std::is_nothrow_move_constructible_v<wrapped>) {
    return override_binding::wrap(std::move(binding_).allow_copy());
  }

  // The report is the wrapped binding's own, detail::reject_ctor_declaration<wrapped, Args...>,
  // so the misuse is one error with bind<I>'s message, also next to the same misuse on the binding.
  /// @brief Rejects a constructor declaration: the override holds an object, so the injector
  ///        builds nothing, as for the binding it wraps.
  ///
  /// @return The same override, unchanged.
  ///
  /// @par Constraints
  /// - Never valid: compilation fails with `bind<I>`'s static_assert naming
  ///   `.with_ctor<Args...>()`, once per binding type and declaration, whichever overload is
  ///   called.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// // trellis::override<IClock>.to_ref(clock).with_ctor<int>();  // error: builds nothing
  /// @endcode
  template <class... Args>
  [[nodiscard]] constexpr const override_binding& with_ctor() const& noexcept {
    detail::reject_ctor_declaration<wrapped, Args...>();
    return *this;
  }
  /// @brief Rejects a constructor declaration, as the overload above does; returns the override
  ///        as an rvalue, so that a move-only value is moved along, not copied.
  ///
  /// @return The same override, unchanged.
  ///
  /// @par Constraints
  /// - Never valid: compilation fails with `bind<I>`'s static_assert naming
  ///   `.with_ctor<Args...>()`, once per binding type and declaration, whichever overload is
  ///   called.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// // trellis::override<Config>.to_value(Config{}).with_ctor<int>();  // error: builds nothing
  /// @endcode
  template <class... Args>
  [[nodiscard]] constexpr override_binding&& with_ctor() && noexcept {
    detail::reject_ctor_declaration<wrapped, Args...>();
    return std::move(*this);
  }

 private:
  template <class>
  friend class override_binding;
  friend struct detail::override_access;

  constexpr explicit override_binding(wrapped binding) noexcept(
      std::is_nothrow_move_constructible_v<wrapped>)
      : binding_(std::move(binding)) {}

  // The override of a binding that a step of the chain returned.
  template <class Next>
  static constexpr override_binding<Next> wrap(Next next) noexcept(
      std::is_nothrow_move_constructible_v<Next>) {
    return override_binding<Next>(std::move(next));
  }

  wrapped binding_;
};

// override<K> below is declared auto, as bind<I> is: the type is deduced from the initializer,
// which is then instantiated wherever override<K> is named, decltype included, and its
// constructor checks the key through bind<K>'s binding (dsl/bind.hpp).
/// @brief Starts an override of the key `K`: in a test, replaces the binding of `K` that the
///        other arguments of `trellis::make_injector` provide (plan §6.10).
///
/// It has `trellis::bind<K>`'s vocabulary, with the same checks: chain `.to<C>()`,
/// `.in(lifetime)`, `.allow_copy()` and `.with_ctor<Args...>()`, or `.to_ref(x)`, `.to_ptr(p)`,
/// `.to_value(v)` and `.to_shared(sp)`, then pass it to `trellis::make_injector`. It replaces the
/// binding of `K` wherever it is among the other arguments, inside modules and named modules
/// included, and keeps that binding's place, so the rest of the wiring is unchanged. Each test's
/// injector owns its own singletons, so an override in one test never reaches another.
///
/// @tparam K The key whose binding to replace, as the binding names it: `override<const K>`
///           replaces nothing, as no binding has that key.
///
/// @par Constraints
/// - `K` is an object type without cv qualifiers that is not an array; otherwise compilation
///   fails with `bind<I>`'s static_assert.
/// - The override suits every request made of `K`, as the binding it replaces did: an override
///   with `.to_ref()` serves references and pointers, so a consumer that takes a
///   `std::shared_ptr<K>` needs `.to_shared()` instead, or compilation fails with `TRL-E005` in
///   `create<T>()`.
/// - Only `trellis::make_injector` takes overrides; `trellis::make_module` rejects them with a
///   static_assert.
///
/// @par Errors
/// None itself. `trellis::make_injector` raises `TRL-E003` for two overrides of one key and
/// `TRL-E009` for an override whose key none of its other arguments binds.
///
/// @par Example
/// @code
/// TEST(Service, SavesThroughRepository) {
///   testing::StrictMock<MockRepository> repo;
///   auto injector = trellis::make_injector(app_module(test_config()),
///                                          trellis::override<IRepository>.to_ref(repo));
///   EXPECT_CALL(repo, save(42));
///   injector.create<Service>().handle(42);
/// }
/// @endcode
template <class K>
inline constexpr auto override = override_binding<binding<K, K, unique_t>>{};

namespace detail {

// ---- the arguments of make_injector -----------------------------------------------------------

// override_part<Part>: whether an argument of make_injector is an override (is_override,
// dsl/module.hpp, which make_module checks too).
template <class Part>
concept override_part = is_override<Part>;

// override_traits<Override>::binding: the binding an override holds. Defined for overrides only.
template <class Override>
struct override_traits;
template <class Binding>
struct override_traits<override_binding<Binding>> {
  using binding = Binding;
};

// Reads the binding an override holds, for make_injector: copied from an lvalue or a const
// argument, moved from an rvalue, as make_module takes bindings (dsl/module.hpp).
struct override_access {
  template <class Binding>
  static constexpr const Binding& binding(const override_binding<Binding>& from) noexcept {
    return from.binding_;
  }
  template <class Binding>
  static constexpr Binding&& binding(override_binding<Binding>&& from) noexcept {
    return std::move(from.binding_);
  }
};

// injector_parts<Parts...>: every argument of make_injector is a binding, a module or an
// override. bindings_and_modules (dsl/module.hpp) with overrides; one flat expansion and a scan.
template <class... Parts>
inline constexpr bool injector_parts =
    first_true({!(part_bindings<Parts>::is_part || override_part<Parts>)...}) == sizeof...(Parts);

// injector_part_bindings<Part>::type: the bindings an argument of make_injector gives up: a
// module's, a binding itself, or the binding an override holds.
template <class Part>
struct injector_part_bindings {
  using type = typename part_bindings<Part>::type;
};
template <class Binding>
struct injector_part_bindings<override_binding<Binding>> {
  using type = type_list<Binding>;
};

// injector_parts_can_be_taken<Args...>: bindings_can_be_taken (dsl/module.hpp), with the binding
// that an override holds too: an lvalue or const argument is copied, and a binding that holds a
// move-only value can't be. Args are the deduced types of forwarding references.
template <class... Args>
inline constexpr bool injector_parts_can_be_taken =
    first_true(
        {((std::is_lvalue_reference_v<Args> || std::is_const_v<std::remove_reference_t<Args>>) &&
          !detail::copyable(
              typename injector_part_bindings<std::remove_cvref_t<Args>>::type{}))...}) ==
    sizeof...(Args);

// has_overrides<Parts...>: whether an argument is an override.
template <class... Parts>
inline constexpr bool has_overrides = !no_overrides<Parts...>;

// ---- applying the overrides -------------------------------------------------------------------

// override_source<I, Binding>: the override that argument I of make_injector holds, whose binding
// is Binding: the value of an override's key in the overrides' map.
template <std::size_t I, class Binding>
struct override_source {
  static constexpr std::size_t argument = I;
};
// What the map yields for a key that no override names.
struct not_overridden {};

// override_for<Replaces, Key, Map>::type: the override that replaces the binding of Key,
// override_source<I, B>, or not_overridden. The map is looked up only when Replaces: a map whose
// keys repeat must not be (meta/type_map.hpp), and then make_injector reports TRL-E003.
template <bool Replaces, class Key, class Map>
struct override_for {
  using type = not_overridden;
};
template <class Key, class Map>
struct override_for<true, Key, Map> {
  using type = lookup_or_t<Key, Map, not_overridden>;
};

// replaced<Source, Binding>::type: the binding at a position of the injector's list: the
// override's, or the argument's own binding there.
template <class Source, class Binding>
struct replaced {
  using type = Binding;
};
template <std::size_t I, class Override, class Binding>
struct replaced<override_source<I, Override>, Binding> {
  using type = Override;
};

// Whether the keys of a list of bindings are distinct: the binding map's own check
// (static/binding_map.hpp), on the same slots_of class, so it is instantiated once.
template <class... Bindings>
constexpr bool keys_are_distinct_in(type_list<Bindings...> /*bindings*/) noexcept {
  return detail::keys_are_distinct<typename binding_traits<Bindings>::key...>();
}

// The keys an override can find its binding to replace by: the other bindings' keys, where a key
// that is no key at all, such as the const K of bind<const K>, counts as K without its cv
// qualifiers and reference. bind<I>'s check has reported that key (dsl/bind.hpp), and an override
// of K then adds no TRL-E009 to it (tests/compile_fail/override/override_of_a_bad_key_binding.cpp);
// the binding keeps its place, and serves no request, as without the override. For bindings whose
// keys are all keys, the list is the binding map's own key list, so its slots are instantiated
// once.
template <class... Bindings>
type_list<std::remove_cvref_t<typename binding_traits<Bindings>::key>...> target_keys_of(
    type_list<Bindings...>);  // only named in decltype

// missing_target<Part, Keys>: Part is an override that has no binding to replace: its key is a key,
// and none of Keys. make_injector reports TRL-E009 for each such override. An override whose key is
// no key at all, such as override<const K>, has been reported by bind<I>'s check, and gets nothing
// more. contains_v is one builtin trait per override.
template <class Part, class Keys>
inline constexpr bool missing_target = false;
template <class Binding, class Keys>
inline constexpr bool missing_target<override_binding<Binding>, Keys> =
    is_bindable_key<typename binding_traits<Binding>::key> &&
    !contains_v<typename binding_traits<Binding>::key, Keys>;

// override_layout<Parts...>: the layout of make_injector's arguments when overrides are among
// them, with the interface of flatten's (dsl/module.hpp): bindings, the list the injector holds,
// and make<Module>(args...), which builds it. With an override among the arguments, flatten's
// shape is always the mixed one, so the other arguments' bindings are flat_layout<mixed, ...>'s:
// its positions table names the J-th binding and extracts it, and the overrides contribute none.
// The overrides get a positions table of their own, one slot per override argument, read the same
// way, and a type_map from their keys to where they are. Each binding J then looks its key up once
// in that map: replaced in place by the override that names it, or kept.
template <class... Parts>
struct override_layout {
  // The other arguments' bindings, and the keys an override finds its target by.
  using regular = flat_layout<flat_shape::mixed, Parts...>;
  using regular_bindings = typename regular::bindings;
  using target_keys = decltype(detail::target_keys_of(regular_bindings{}));

  // The type of argument I, found as flat_layout finds it, on its indexed bases.
  template <std::size_t I>
  using argument_type = typename std::remove_reference_t<decltype(detail::argument_at<I>(
      std::declval<typename regular::parts_index&>()))>::type;

  // The overrides, in argument order.
  static constexpr std::size_t override_count =
      detail::total({std::size_t{override_part<Parts> ? 1U : 0U}...});
  static constexpr flat_positions<override_count> override_positions =
      detail::locate<override_count>({std::size_t{override_part<Parts> ? 1U : 0U}...});

  template <std::size_t K>
  using override_at = typename override_traits<argument_type<override_positions.part[K]>>::binding;

  template <std::size_t... K>
  static type_list<override_at<K>...> override_list(
      std::index_sequence<K...>);  // only named in decltype
  template <std::size_t... K>
  static type_map<entry<typename binding_traits<override_at<K>>::key,
                        override_source<override_positions.part[K], override_at<K>>>...>
      override_entries(std::index_sequence<K...>);  // only named in decltype

  using override_bindings =
      decltype(override_layout::override_list(std::make_index_sequence<override_count>{}));
  using override_map =
      decltype(override_layout::override_entries(std::make_index_sequence<override_count>{}));

  // Two overrides of one key (TRL-E003), and whether one has no binding to replace (TRL-E009).
  static constexpr bool keys_repeat = has_duplicate_keys_v<override_map>;
  static constexpr bool targets_missing =
      first_true({missing_target<Parts, target_keys>...}) != sizeof...(Parts);
  static constexpr bool apply = !keys_repeat && !targets_missing;

  // The overrides with no binding to replace, which make_injector reports TRL-E009 for, one each
  // (missing_target): a second positions table, read only on that failure path. The filter is
  // here, rather than in a function around each report, so that the error has one instantiation
  // note fewer.
  static constexpr std::size_t missing_count =
      detail::total({std::size_t{missing_target<Parts, target_keys> ? 1U : 0U}...});
  static constexpr flat_positions<missing_count> missing_positions =
      detail::locate<missing_count>({std::size_t{missing_target<Parts, target_keys> ? 1U : 0U}...});

  template <std::size_t K>
  using missing_at = typename override_traits<argument_type<missing_positions.part[K]>>::binding;

  template <std::size_t... K>
  static type_list<missing_at<K>...> missing_list(
      std::index_sequence<K...>);  // only named in decltype

  using missing_bindings =
      decltype(override_layout::missing_list(std::make_index_sequence<missing_count>{}));
  // Whether to replace: only when the overrides apply and the other bindings' keys are distinct.
  // Otherwise make_injector reports the mistake, and the list is the bindings as written, which
  // its TRL-E003 names.
  static constexpr bool replaces = apply && detail::keys_are_distinct_in(regular_bindings{});

  // The binding at position J, what replaces it, and the result, worked out once per J. A class
  // template, not aliases: flat_layout's binding_at<J> is an alias over a deduction against the
  // bases of the argument's store, one per binding of a module, which a compiler repeats for each
  // use of the alias, and a class template specialization is instantiated once.
  template <std::size_t J>
  struct position {
    using own = typename regular::template binding_at<J>;
    using source =
        typename override_for<replaces, typename binding_traits<own>::key, override_map>::type;
    using type = typename replaced<source, own>::type;
  };
  template <std::size_t J>
  using source_at = typename position<J>::source;
  template <std::size_t J>
  using binding_at = typename position<J>::type;

  template <std::size_t... J>
  static type_list<binding_at<J>...> list(std::index_sequence<J...>);  // only named in decltype

  using bindings = decltype(override_layout::list(std::make_index_sequence<regular::size>{}));

  template <class Module, class... Args>
  static constexpr Module make(Args&&... args) {
    argument_refs<std::index_sequence_for<Args...>, Args...> refs{{std::forward<Args>(args)}...};
    return module_access::gather<Module>(override_layout{}, refs);
  }

  // The J-th binding, forwarded out of the argument it comes from: the override's binding, or the
  // argument's own, as flat_layout extracts it. An rvalue argument gives it up by moving.
  template <std::size_t J, class Refs>
  static constexpr decltype(auto) extract(Refs& refs) noexcept {
    using source = source_at<J>;
    if constexpr (TRELLIS_IS_SAME(source, not_overridden)) {
      return regular::template extract<J>(refs);
    } else {
      auto& ref = detail::argument_at<source::argument>(refs);
      using arg = decltype(ref.argument);  // Arg&&, a reference type
      return override_access::binding(static_cast<arg>(ref.argument));
    }
  }
};

// compose<Parts...>: the layout of make_injector's arguments: flatten's, the layout make_module
// uses (dsl/module.hpp), or with an override among them, override_layout's. Naming both in
// conditional_t instantiates neither.
template <class... Parts>
using compose =
    std::conditional_t<has_overrides<Parts...>, override_layout<Parts...>, flatten<Parts...>>;

// The module that holds the injector's bindings, for make_injector's arguments Parts.
template <class... Parts>
using composed_module_t = decltype(detail::module_of(typename compose<Parts...>::bindings{}));

// overrides_apply<Parts...>: whether every override among make_injector's arguments applies: no
// two name one key, and each has a binding to replace. True, without looking at anything, when
// none is an override.
template <bool HasOverrides, class... Parts>
inline constexpr bool overrides_apply_v = true;
template <class... Parts>
inline constexpr bool overrides_apply_v<true, Parts...> = override_layout<Parts...>::apply;

template <class... Parts>
inline constexpr bool overrides_apply = overrides_apply_v<has_overrides<Parts...>, Parts...>;

}  // namespace detail

}  // namespace trellis::inline v1

#endif  // TRELLIS_DSL_OVERRIDE_HPP
