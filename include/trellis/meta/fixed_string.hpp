// Trellis meta toolkit: fixed_string<N>, a string of N characters known at compile time (plan
// §7.1).
// Copyright (c) 2026 Aryan Patel
// SPDX-License-Identifier: BSL-1.0
//
// Two uses: a template argument that is a string, as named qualifiers need
// (trellis::named<"replica">, plan §6.5), and a buffer of exactly the right size for a message
// built at compile time, such as a C++26 static_assert message (plan §7.8, ADR-0006 item 4).
//
// It holds a plain char array and takes string literals, which are arrays, so clang-tidy's
// modernize-avoid-c-arrays doesn't apply to this file. A std::array member would also keep the
// type structural, but would print as a nested std::array{...} wherever a compiler shows a
// fixed_string template argument, as in qualifiers named in diagnostics.
#ifndef TRELLIS_META_FIXED_STRING_HPP
#define TRELLIS_META_FIXED_STRING_HPP

#include <cstddef>
#include <string_view>

// NOLINTBEGIN(modernize-avoid-c-arrays)
namespace trellis::inline v1 {

/// @brief A string of exactly `N` characters, usable as a template argument and in constant
///        expressions.
///
/// `fixed_string{"replica"}` deduces `N = 7`; the terminating `'\0'` is stored but not counted.
/// The characters are a public member, as C++20 requires of a class type used as a template
/// argument, so `named<"replica">` and `named<"replica">` name the same specialization. A
/// default-constructed `fixed_string<N>` holds `N` `'\0'` characters, ready to be filled in, which
/// is how a message of known length is built at compile time. `size()` and `data()` make it a
/// valid C++26 `static_assert` message.
///
/// @tparam N The number of characters, not counting the terminating `'\0'`.
///
/// @par Constraints
/// None.
///
/// @par Errors
/// None.
///
/// @par Example
/// @code
/// template <trellis::fixed_string Name>
/// struct named {};
/// static_assert(!std::is_same_v<named<"primary">, named<"replica">>);
/// @endcode
template <std::size_t N>
struct fixed_string {
  /// @brief The characters, then a `'\0'`, so that `data()` is also a C string.
  ///
  /// Public only because a template argument's members must be; prefer the member functions.
  ///
  /// @par Constraints
  /// - `chars[N]` stays `'\0'`; overwriting it breaks `data()` as a C string.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// static_assert(trellis::fixed_string{"ab"}.chars[1] == 'b');
  /// @endcode
  char chars[N + 1] = {};

  /// @brief Makes a string of `N` `'\0'` characters, to be filled in.
  ///
  /// @par Constraints
  /// None.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// constexpr auto ok = [] {
  ///   trellis::fixed_string<2> text;
  ///   text[0] = 'o';
  ///   text[1] = 'k';
  ///   return text;
  /// }();
  /// static_assert(ok == "ok");
  /// @endcode
  constexpr fixed_string() noexcept = default;

  /// @brief Copies a string literal. Implicit, so that a literal converts where a template
  ///        argument of type `fixed_string` is expected.
  ///
  /// @param text `N` characters followed by a `'\0'`, which is not copied but restored.
  ///
  /// @par Constraints
  /// - `text` is a string literal, or another array of `N` characters and a terminating `'\0'`;
  ///   the last element is ignored either way.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// constexpr trellis::fixed_string replica{"replica"};  // fixed_string<7>
  /// @endcode
  constexpr fixed_string(const char (&text)[N + 1]) noexcept {
    for (std::size_t i = 0; i != N; ++i) {
      chars[i] = text[i];
    }
  }

  /// @brief The number of characters, `N`.
  ///
  /// @par Constraints
  /// None.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// static_assert(trellis::fixed_string{"replica"}.size() == 7);
  /// @endcode
  [[nodiscard]] static constexpr std::size_t size() noexcept { return N; }

  /// @brief The characters, followed by a `'\0'`.
  ///
  /// @par Constraints
  /// None.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// constexpr trellis::fixed_string text{"abc"};
  /// static_assert(text.data()[3] == '\0');
  /// @endcode
  [[nodiscard]] constexpr const char* data() const noexcept { return chars; }

  /// @brief The characters, writable, followed by a `'\0'`.
  ///
  /// @par Constraints
  /// - Writes stay within the first `N` characters.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// trellis::fixed_string text{"abc"};
  /// text.data()[0] = 'x';  // "xbc"
  /// @endcode
  [[nodiscard]] constexpr char* data() noexcept { return chars; }

  /// @brief The characters as a `std::string_view` of size `N`.
  ///
  /// @par Constraints
  /// None.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// static_assert(trellis::fixed_string{"abc"}.view() == "abc");
  /// @endcode
  [[nodiscard]] constexpr std::string_view view() const noexcept { return {chars, size()}; }

  /// @brief An iterator to the first character.
  ///
  /// @par Constraints
  /// None.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// for (const char letter : trellis::fixed_string{"abc"}) {
  ///   std::putchar(letter);
  /// }
  /// @endcode
  [[nodiscard]] constexpr const char* begin() const noexcept { return chars; }

  /// @brief An iterator past the last character, at the terminating `'\0'`.
  ///
  /// @par Constraints
  /// None.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// constexpr trellis::fixed_string text{"abc"};
  /// static_assert(text.end() - text.begin() == 3);
  /// @endcode
  [[nodiscard]] constexpr const char* end() const noexcept { return chars + N; }

  /// @brief The character at index `i`, writable.
  ///
  /// @param i An index less than `N`.
  ///
  /// @par Constraints
  /// - `i < N`; otherwise the behaviour is undefined, and not a constant expression.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// trellis::fixed_string text{"abc"};
  /// text[1] = 'x';  // "axc"
  /// @endcode
  [[nodiscard]] constexpr char& operator[](std::size_t i) noexcept { return chars[i]; }

  /// @brief The character at index `i`.
  ///
  /// @param i An index less than `N`.
  ///
  /// @par Constraints
  /// - `i < N`; otherwise the behaviour is undefined, and not a constant expression.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// static_assert(trellis::fixed_string{"abc"}[1] == 'b');
  /// @endcode
  [[nodiscard]] constexpr const char& operator[](std::size_t i) const noexcept { return chars[i]; }
};

// Deduces N from a string literal, whose array includes the '\0' that N doesn't count.
template <std::size_t N>
fixed_string(const char (&)[N]) -> fixed_string<N - 1>;

/// @brief Whether two fixed strings hold the same characters, whatever their sizes; `!=` is
///        derived from it.
///
/// @par Constraints
/// None.
///
/// @par Errors
/// None.
///
/// @par Example
/// @code
/// static_assert(trellis::fixed_string{"abc"} == trellis::fixed_string{"abc"});
/// static_assert(trellis::fixed_string{"abc"} != trellis::fixed_string{"ab"});
/// @endcode
template <std::size_t N, std::size_t M>
[[nodiscard]] constexpr bool operator==(const fixed_string<N>& lhs,
                                        const fixed_string<M>& rhs) noexcept {
  return lhs.view() == rhs.view();
}

/// @brief Whether a fixed string holds the characters of a string literal or a
///        `std::string_view`, on either side; `!=` is derived from it.
///
/// @par Constraints
/// None.
///
/// @par Errors
/// None.
///
/// @par Example
/// @code
/// static_assert(trellis::fixed_string{"abc"} == "abc");
/// static_assert("abd" != trellis::fixed_string{"abc"});
/// @endcode
template <std::size_t N>
[[nodiscard]] constexpr bool operator==(const fixed_string<N>& lhs, std::string_view rhs) noexcept {
  return lhs.view() == rhs;
}

/// @brief Joins two fixed strings into one of size `N + M`.
///
/// @par Constraints
/// None.
///
/// @par Errors
/// None.
///
/// @par Example
/// @code
/// static_assert(trellis::fixed_string{"key:"} + trellis::fixed_string{"value"} == "key:value");
/// @endcode
template <std::size_t N, std::size_t M>
[[nodiscard]] constexpr fixed_string<N + M> operator+(const fixed_string<N>& lhs,
                                                      const fixed_string<M>& rhs) noexcept {
  fixed_string<N + M> joined;
  for (std::size_t i = 0; i != N; ++i) {
    joined.chars[i] = lhs.chars[i];
  }
  for (std::size_t i = 0; i != M; ++i) {
    joined.chars[N + i] = rhs.chars[i];
  }
  return joined;
}

/// @brief Appends a string literal to a fixed string.
///
/// @par Constraints
/// - `rhs` is a string literal, or another array whose last element is the `'\0'`.
///
/// @par Errors
/// None.
///
/// @par Example
/// @code
/// static_assert(trellis::fixed_string{"key:"} + "value" == "key:value");
/// @endcode
template <std::size_t N, std::size_t M>
[[nodiscard]] constexpr fixed_string<N + M - 1> operator+(const fixed_string<N>& lhs,
                                                          const char (&rhs)[M]) noexcept {
  return lhs + fixed_string<M - 1>(rhs);
}

/// @brief Prepends a string literal to a fixed string.
///
/// @par Constraints
/// - `lhs` is a string literal, or another array whose last element is the `'\0'`.
///
/// @par Errors
/// None.
///
/// @par Example
/// @code
/// static_assert("key:" + trellis::fixed_string{"value"} == "key:value");
/// @endcode
template <std::size_t N, std::size_t M>
[[nodiscard]] constexpr fixed_string<N - 1 + M> operator+(const char (&lhs)[N],
                                                          const fixed_string<M>& rhs) noexcept {
  return fixed_string<N - 1>(lhs) + rhs;
}

}  // namespace trellis::inline v1
// NOLINTEND(modernize-avoid-c-arrays)

#endif  // TRELLIS_META_FIXED_STRING_HPP
