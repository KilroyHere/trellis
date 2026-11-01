// Trellis binding vocabulary: lifetimes (plan §6.2, ADR-0004).
// Copyright (c) 2026 Aryan Patel
// SPDX-License-Identifier: BSL-1.0
//
// A binding's lifetime says who owns the objects it provides and how many there are (plan §6.2).
// unique, singleton and singleton_inline are passed to .in(). The other lifetimes come with the
// binding functions that name an object themselves: external_t with .to_ref() and .to_ptr(),
// shared_external_t with .to_shared() and value_t with .to_value() (dsl/bind.hpp). Scoped
// lifetimes, scoped<Tag>, arrive in Phase 2.
//
// detail::lifetime_kind numbers the lifetimes as the columns of the request-form table
// (dsl/request_forms.hpp), which says which request forms each lifetime allows.
#ifndef TRELLIS_DSL_LIFETIMES_HPP
#define TRELLIS_DSL_LIFETIMES_HPP

#include <trellis/config.hpp>
#include <trellis/meta/type_list.hpp>

namespace trellis::inline v1 {

/// @brief The type of `trellis::unique`: a new object for every request, owned by the consumer.
///
/// @par Constraints
/// None.
///
/// @par Errors
/// None.
///
/// @par Example
/// @code
/// auto clock = trellis::bind<IClock>.to<SystemClock>().in(trellis::unique);
/// @endcode
struct unique_t {};

/// @brief The default lifetime: each request gets a new object, which the consumer owns.
///
/// A binding is unique until `.in()` says otherwise, and an unbound concrete type is implicitly
/// unique (ADR-0004). A unique object can be requested by value, as `std::unique_ptr<T>` or as
/// `std::shared_ptr<T>`, but not by reference or raw pointer (plan §6.3).
///
/// @par Constraints
/// None.
///
/// @par Errors
/// None.
///
/// @par Example
/// @code
/// auto injector =
///     trellis::make_injector(trellis::bind<IClock>.to<SystemClock>().in(trellis::unique));
/// @endcode
inline constexpr unique_t unique{};

/// @brief The type of `trellis::singleton`: one object per injector, held in a `std::shared_ptr`.
///
/// @par Constraints
/// None.
///
/// @par Errors
/// None.
///
/// @par Example
/// @code
/// auto logger = trellis::bind<ILogger>.to<ConsoleLogger>().in(trellis::singleton);
/// @endcode
struct singleton_t {};

/// @brief One object per injector, which the injector owns through a `std::shared_ptr`.
///
/// A singleton can be requested as `T&`, `T*`, `std::shared_ptr<T>` or `std::weak_ptr<T>`, but not
/// by value (copies would hide the sharing; `.allow_copy()` opts in) or as `std::unique_ptr<T>`
/// (plan §6.3).
///
/// @par Constraints
/// None.
///
/// @par Errors
/// None.
///
/// @par Example
/// @code
/// auto injector = trellis::make_injector(
///     trellis::bind<ILogger>.to<ConsoleLogger>().in(trellis::singleton));
/// @endcode
inline constexpr singleton_t singleton{};

/// @brief The type of `trellis::singleton_inline`: one object per injector, stored inline.
///
/// @par Constraints
/// None.
///
/// @par Errors
/// None.
///
/// @par Example
/// @code
/// auto bus = trellis::bind<Bus>.in(trellis::singleton_inline);
/// @endcode
struct singleton_inline_t {};

/// @brief One object per injector, stored inside the injector without a heap allocation.
///
/// It allows the same requests as `singleton`, except `std::shared_ptr<T>` and
/// `std::weak_ptr<T>`, because no `std::shared_ptr` owns the object (plan §6.3).
///
/// @par Constraints
/// None.
///
/// @par Errors
/// None.
///
/// @par Example
/// @code
/// auto injector = trellis::make_injector(trellis::bind<Bus>.in(trellis::singleton_inline));
/// @endcode
inline constexpr singleton_inline_t singleton_inline{};

/// @brief The lifetime of bindings made with `.to_ref()` and `.to_ptr()`: the caller's object.
///
/// The caller owns the object and keeps it alive for as long as the injector uses it. The
/// injector never copies or destroys it. There is no variable to pass to `.in()`: the binding
/// functions set this lifetime themselves.
///
/// @par Constraints
/// None.
///
/// @par Errors
/// None.
///
/// @par Example
/// @code
/// const Config config = load_config();
/// auto binding = trellis::bind<Config>.to_ref(config);  // lifetime: trellis::external_t
/// @endcode
struct external_t {};

/// @brief The lifetime of bindings made with `.to_shared()`: an object the caller shares.
///
/// The binding holds a `std::shared_ptr` to the caller's object, so the object lives at least as
/// long as the injector, and can be requested as `std::shared_ptr<T>` (plan §6.3).
///
/// @par Constraints
/// None.
///
/// @par Errors
/// None.
///
/// @par Example
/// @code
/// auto binding = trellis::bind<IClock>.to_shared(std::make_shared<SystemClock>());
/// @endcode
struct shared_external_t {};

/// @brief The lifetime of bindings made with `.to_value()`: a copy that the injector owns.
///
/// @par Constraints
/// None.
///
/// @par Errors
/// None.
///
/// @par Example
/// @code
/// auto binding = trellis::bind<Config>.to_value(Config{"postgres://db"});
/// @endcode
struct value_t {};

namespace detail {

// The lifetimes as the columns of the request-form table (dsl/request_forms.hpp), in the order of
// plan §6.3, plus `unbound`: an unbound concrete type, implicitly unique (ADR-0004), whose column
// comes from the TRL-E012 rule rather than from the table. `scoped` has no lifetime type until
// Phase 2, but its column is encoded already.
enum class lifetime_kind : unsigned char {
  unique,
  singleton,
  singleton_inline,
  scoped,
  external,
  shared_external,
  value,
  unbound,
};
inline constexpr unsigned lifetime_kind_count = 8;

// The column of a lifetime type. The primary template is the misuse: Lifetime is not one of the
// lifetime types above.
template <class Lifetime>
struct lifetime_traits {
  static_assert(always_false<Lifetime>,
                "trellis: expected a lifetime type such as trellis::unique_t");
  static constexpr lifetime_kind kind = lifetime_kind::unique;
};
template <>
struct lifetime_traits<unique_t> {
  static constexpr lifetime_kind kind = lifetime_kind::unique;
};
template <>
struct lifetime_traits<singleton_t> {
  static constexpr lifetime_kind kind = lifetime_kind::singleton;
};
template <>
struct lifetime_traits<singleton_inline_t> {
  static constexpr lifetime_kind kind = lifetime_kind::singleton_inline;
};
template <>
struct lifetime_traits<external_t> {
  static constexpr lifetime_kind kind = lifetime_kind::external;
};
template <>
struct lifetime_traits<shared_external_t> {
  static constexpr lifetime_kind kind = lifetime_kind::shared_external;
};
template <>
struct lifetime_traits<value_t> {
  static constexpr lifetime_kind kind = lifetime_kind::value;
};

// What .in() accepts: the lifetimes whose objects the injector constructs from a class. The other
// lifetimes name an object instead, so they come from .to_ref(), .to_ptr(), .to_shared() and
// .to_value(). A variable rather than a concept, so that the static_assert on it reports just the
// requirement.
template <class Lifetime>
inline constexpr bool is_constructed_lifetime =
    TRELLIS_IS_SAME(Lifetime, unique_t) || TRELLIS_IS_SAME(Lifetime, singleton_t) ||
    TRELLIS_IS_SAME(Lifetime, singleton_inline_t);

}  // namespace detail

}  // namespace trellis::inline v1

#endif  // TRELLIS_DSL_LIFETIMES_HPP
