// Trellis meta toolkit: trellis::failure and trellis::result<T>, the runtime error model's value
// types (plan §7.1, ADR-0009).
// Copyright (c) 2026 Aryan Patel
// SPDX-License-Identifier: BSL-1.0
//
// A fallible operation, such as the runtime container's try_get<T>(), returns result<T>: a T, or
// a failure saying why there is none. result<T> is std::expected<T, failure> where the standard
// library has it (TRELLIS_HAS_EXPECTED, C++23), and otherwise a fallback with the subset of
// std::expected's interface that code written for both relies on:
//
//   result<int> r = 42;                             a value
//   result<int> e = make_unexpected(failure{"x"});  a failure (std::unexpected is C++23 only)
//   r.has_value(), bool(r)                          which of the two
//   *r, r->member                                   the value (has_value() must be true)
//   r.error()                                       the failure (has_value() must be false)
//   r.value_or(fallback)                            the value, or fallback
//
// result<void> holds no value: a default-constructed one is a success. The fallback leaves out
// value(), whose std::bad_expected_access exists only in C++23, so portable code checks
// has_value() and uses operator*. As with std::expected, T must be an object type or void, not a
// reference. docs/internals/meta-toolkit.md describes the fallback.
//
// Precondition: every translation unit of a program sees the same TRELLIS_HAS_EXPECTED, that is,
// the same language mode and standard library. result<T> is a different type in each mode, and
// make_unexpected has the same signature, and so the same symbol, in both but returns a different
// type: mixing modes is an ODR violation that neither the compiler nor the linker reports.
#ifndef TRELLIS_META_RESULT_HPP
#define TRELLIS_META_RESULT_HPP

#include <trellis/config.hpp>

#include <string_view>
#include <type_traits>
#include <utility>

#if TRELLIS_HAS_EXPECTED
#include <expected>
#else
#include <variant>
#endif

namespace trellis::inline v1 {

/// @brief Why an operation produced no value: the error type of `result<T>`.
///
/// A small value that views a message without copying it. It isn't called `error` because
/// `trellis::error` is the namespace of the compile-error vocabulary (ADR-0006). The runtime
/// error IDs of plan §7.8 and richer reports are later work.
///
/// @par Constraints
/// - The message must outlive every copy of the failure, as a string literal or a name from
///   `type_name<T>()` does; otherwise reading it is undefined behaviour.
///
/// @par Errors
/// None.
///
/// @par Example
/// @code
/// trellis::result<int> parse(std::string_view text) {
///   if (text.empty()) {
///     return trellis::make_unexpected(trellis::failure{"empty input"});
///   }
///   return static_cast<int>(text.size());
/// }
/// @endcode
class failure {
 public:
  /// @brief Makes a failure that views `message`.
  ///
  /// @param message Why there is no value. Not copied.
  ///
  /// @par Constraints
  /// - `message` outlives the failure and its copies.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// constexpr trellis::failure timeout{"timed out"};
  /// @endcode
  constexpr explicit failure(std::string_view message) noexcept : message_(message) {}

  /// @brief The message given at construction.
  ///
  /// @par Constraints
  /// None.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// static_assert(trellis::failure{"timed out"}.message() == "timed out");
  /// @endcode
  [[nodiscard]] constexpr std::string_view message() const noexcept { return message_; }

  /// @brief Whether two failures carry the same message text; `!=` is derived from it.
  ///
  /// @par Constraints
  /// None.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// static_assert(trellis::failure{"a"} == trellis::failure{"a"});
  /// static_assert(trellis::failure{"a"} != trellis::failure{"b"});
  /// @endcode
  [[nodiscard]] friend constexpr bool operator==(const failure& lhs, const failure& rhs) noexcept {
    return lhs.message_ == rhs.message_;
  }

 private:
  std::string_view message_;
};

namespace detail {

#if TRELLIS_HAS_EXPECTED

// trellis::failure, qualified, as everywhere in this namespace: detail has a failure of its own,
// the enum of static/resolve.hpp, which an unqualified name here would find wherever that header
// is included before this one.
template <class T>
using result_type = std::expected<T, trellis::failure>;
using unexpected_type = std::unexpected<trellis::failure>;

#else

// What make_unexpected returns without std::expected: a failure on its way into a result<T>.
class fallback_unexpected {
 public:
  constexpr explicit fallback_unexpected(trellis::failure reason) noexcept : reason_(reason) {}

  [[nodiscard]] constexpr const trellis::failure& error() const noexcept { return reason_; }

 private:
  trellis::failure reason_;
};

// result<T> without std::expected. A std::variant holds the value (or, for void, an empty
// placeholder) at index 0 and the failure at index 1, so copying, moving, destruction and their
// triviality follow T's, as they do for std::expected. Indices rather than types select the
// alternative, so that result<failure> works too.
//
// Assignment is std::expected's ([expected.object.assign]), not the variant's: putting a value
// over a failure keeps a copy of the failure, and puts it back if constructing the value throws.
// The variant's own assignment would leave it valueless, holding neither, which a result can't
// represent (T1.1 review).
//
// The accessors use std::get rather than std::get_if: GCC refuses get_if's null check on the
// member of a temporary in a constant expression, so *result<int>(42) wasn't constant there. A
// wrong-state access therefore throws std::bad_variant_access (aborts without exceptions), where
// std::expected leaves it undefined. operator-> keeps get_if, which gives the pointer without
// std::addressof and <memory>.
template <class T>
class fallback_result {
  static_assert((std::is_object_v<T> && !std::is_array_v<T>) || std::is_void_v<T>,
                "trellis: result<T> needs an object type or void as T, as std::expected does: not "
                "a reference, a function or an array");

  // T, where T can be stored. void stores an empty placeholder, and so does a T the assertion
  // above rejects, so that the members below stay well-formed and the assertion is the only error.
  using stored = std::conditional_t<std::is_object_v<T> && !std::is_array_v<T>, T, std::monostate>;

 public:
  using value_type = T;
  using error_type = trellis::failure;

  // A value, converted from anything T can be constructed from; implicit when the conversion to T
  // is, as for std::expected.
  template <class U = std::remove_cv_t<stored>>
    requires(!std::is_void_v<T> && std::is_constructible_v<stored, U> &&
             !std::is_same_v<std::remove_cvref_t<U>, fallback_result> &&
             !std::is_same_v<std::remove_cvref_t<U>, fallback_unexpected>)
  constexpr explicit(!std::is_convertible_v<U, stored>) fallback_result(U&& value)
      : state_(std::in_place_index<0>, std::forward<U>(value)) {}

  // result<void>: a success.
  constexpr fallback_result() noexcept
    requires std::is_void_v<T>
      : state_(std::in_place_index<0>) {}

  // A failure, from make_unexpected.
  constexpr fallback_result(fallback_unexpected reason) noexcept
      : state_(std::in_place_index<1>, reason.error()) {}

  // Copying and moving construct a new variant: if that throws, there is no object to repair.
  constexpr fallback_result(const fallback_result&) = default;
  constexpr fallback_result(fallback_result&&) = default;
  ~fallback_result() = default;

  // Assignment, under the conditions std::expected sets; see the class comment.
  constexpr fallback_result& operator=(const fallback_result& other)
    requires std::is_copy_constructible_v<stored> && std::is_copy_assignable_v<stored>
  {
    assign(other);
    return *this;
  }
  // noexcept when T's moves are, as for std::expected. bugprone-exception-escape objects because
  // std::variant::emplace ends with a std::get, which can't throw after the emplace succeeded.
  // NOLINTNEXTLINE(bugprone-exception-escape)
  constexpr fallback_result& operator=(fallback_result&& other) noexcept(
      std::is_nothrow_move_constructible_v<stored> && std::is_nothrow_move_assignable_v<stored>)
    requires std::is_move_constructible_v<stored> && std::is_move_assignable_v<stored>
  {
    assign(std::move(other));
    return *this;
  }

  [[nodiscard]] constexpr bool has_value() const noexcept { return state_.index() == 0; }
  constexpr explicit operator bool() const noexcept { return has_value(); }

  // The value; has_value() must be true.
  [[nodiscard]] constexpr stored& operator*() &
    requires(!std::is_void_v<T>)
  {
    return std::get<0>(state_);
  }
  [[nodiscard]] constexpr const stored& operator*() const&
    requires(!std::is_void_v<T>)
  {
    return std::get<0>(state_);
  }
  [[nodiscard]] constexpr stored&& operator*() &&
    requires(!std::is_void_v<T>)
  {
    return std::get<0>(std::move(state_));
  }
  [[nodiscard]] constexpr const stored&& operator*() const&&
    requires(!std::is_void_v<T>)
  {
    return std::get<0>(std::move(state_));
  }
  [[nodiscard]] constexpr stored* operator->() noexcept
    requires(!std::is_void_v<T>)
  {
    return std::get_if<0>(&state_);
  }
  [[nodiscard]] constexpr const stored* operator->() const noexcept
    requires(!std::is_void_v<T>)
  {
    return std::get_if<0>(&state_);
  }

  // The failure; has_value() must be false.
  [[nodiscard]] constexpr const error_type& error() const& { return std::get<1>(state_); }
  [[nodiscard]] constexpr error_type& error() & { return std::get<1>(state_); }

  // The value if there is one, otherwise fallback converted to T.
  template <class U>
    requires(!std::is_void_v<T> && std::is_copy_constructible_v<stored> &&
             std::is_convertible_v<U, stored>)
  [[nodiscard]] constexpr std::remove_cv_t<stored> value_or(U&& fallback) const& {
    return has_value() ? **this : static_cast<stored>(std::forward<U>(fallback));
  }
  template <class U>
    requires(!std::is_void_v<T> && std::is_move_constructible_v<stored> &&
             std::is_convertible_v<U, stored>)
  [[nodiscard]] constexpr std::remove_cv_t<stored> value_or(U&& fallback) && {
    return has_value() ? std::move(**this) : static_cast<stored>(std::forward<U>(fallback));
  }

 private:
  // The value of other, a const fallback_result& or a fallback_result&&, to copy or move from.
  // assign checks the state first, so get_if, which doesn't throw, can't return null here.
  template <class Other>
  static constexpr decltype(auto) value_of(Other&& other) noexcept {
    if constexpr (std::is_lvalue_reference_v<Other>) {
      return *std::get_if<0>(&other.state_);
    } else {
      return std::move(*std::get_if<0>(&other.state_));
    }
  }

  template <class Other>
  constexpr void assign(Other&& other) {
    if (!other.has_value()) {
      // Copied first, as other may be *this; constructing a failure can't throw.
      const error_type reason = *std::get_if<1>(&other.state_);
      state_.template emplace<1>(reason);
    } else if (has_value()) {
      *std::get_if<0>(&state_) = value_of(std::forward<Other>(other));
    } else {
      emplace_value(value_of(std::forward<Other>(other)));
    }
  }

  // Puts a value over the failure, and the failure back if constructing the value throws. When it
  // can't throw, or without exceptions, there is nothing to put back.
  template <class V>
  constexpr void emplace_value(V&& value) {
#if TRELLIS_HAS_EXCEPTIONS
    if constexpr (!std::is_nothrow_constructible_v<stored, V>) {
      const error_type kept = *std::get_if<1>(&state_);
      try {
        state_.template emplace<0>(std::forward<V>(value));
      } catch (...) {
        state_.template emplace<1>(kept);
        throw;
      }
      return;
    }
#endif
    state_.template emplace<0>(std::forward<V>(value));
  }

  std::variant<stored, error_type> state_;
};

template <class T>
using result_type = fallback_result<T>;
using unexpected_type = fallback_unexpected;

#endif

}  // namespace detail

/// @brief A `T`, or a `failure` that says why there is none: what a fallible operation returns.
///
/// `std::expected<T, failure>` where the standard library has it (C++23, `TRELLIS_HAS_EXPECTED`),
/// and otherwise a fallback that offers the part of its interface that code written for both
/// uses: construction from a value or from `make_unexpected`, `has_value()`, `operator bool`,
/// `operator*`, `operator->`, `error()` and `value_or()`, but not `value()`. Assignment keeps the
/// old failure if constructing the new value throws, as `std::expected` does.
///
/// @tparam T The value type: an object type or `void`. `result<void>` holds no value; a
///           default-constructed one is a success.
///
/// @par Constraints
/// - `T` is an object type or `void`, not a reference, a function or an array; otherwise
///   compilation fails, with a static_assert in the fallback.
/// - `operator*` and `operator->` need a value, and `error()` a failure; otherwise the behaviour
///   is undefined with `std::expected`, and the fallback throws `std::bad_variant_access`.
/// - Every translation unit of a program sees the same `TRELLIS_HAS_EXPECTED`; mixing language
///   modes is an ODR violation that no tool reports.
///
/// @par Errors
/// None.
///
/// @par Example
/// @code
/// trellis::result<int> digit(char c) {
///   if (c < '0' || c > '9') {
///     return trellis::make_unexpected(trellis::failure{"not a digit"});
///   }
///   return c - '0';
/// }
/// int seven = digit('7').value_or(-1);
/// @endcode
template <class T>
using result = detail::result_type<T>;

/// @brief Wraps a failure for returning or assigning it as the failure of any `result<T>`.
///
/// Returns `std::unexpected<failure>` with `std::expected`, and the fallback's own type
/// otherwise; `std::unexpected` itself exists only from C++23.
///
/// @param reason Why there is no value.
/// @return Something every `result<T>` converts from, holding `reason`.
///
/// @par Constraints
/// - Every translation unit of a program sees the same `TRELLIS_HAS_EXPECTED`: this function has
///   the same signature in both modes but returns a different type.
///
/// @par Errors
/// None.
///
/// @par Example
/// @code
/// trellis::result<void> save(bool disk_full) {
///   if (disk_full) {
///     return trellis::make_unexpected(trellis::failure{"disk full"});
///   }
///   return {};
/// }
/// @endcode
[[nodiscard]] constexpr detail::unexpected_type make_unexpected(failure reason) noexcept {
  return detail::unexpected_type(reason);
}

}  // namespace trellis::inline v1

#endif  // TRELLIS_META_RESULT_HPP
