// Trellis diagnostics: the compile-error vocabulary (plan §7.8, ADR-0006).
// Copyright (c) 2026 Aryan Patel
// SPDX-License-Identifier: BSL-1.0
//
// Trellis reports each mistake it finds at compile time as one static_assert whose condition
// names an error type and whose message starts with the error's ID (ADR-0006 item 3):
//
//   static_assert(trellis::error::E0xx<...>::reported, "TRL-E0xx: <summary>");
//
// Every error type's `reported` is false, so the assertion fails, and the compiler prints the
// error type with its template arguments: Clang on the first error line, GCC in the note that
// follows it. The message carries the ID, which leads to the page in docs/errors/.
//
// The vocabulary lives in namespace trellis::error, outside inline namespace v1, so that its
// names print short and the same on every compiler: GCC spells inline namespaces out and Clang
// hides them. That is the one documented exception to the project's namespace rule (owner
// decision, ADR-0006). These types are versioned by error ID instead: an ID never changes meaning.
#ifndef TRELLIS_DIAGNOSTICS_ERRORS_HPP
#define TRELLIS_DIAGNOSTICS_ERRORS_HPP

namespace trellis::error {

/// @brief The error type of TRL-E002, type not constructible, for an explicit constructor
///        declaration that matches no constructor of the type.
///
/// Users never name it: it appears in the compiler's output when `std::is_constructible_v<T,
/// Args...>` is false for a declared `trellis::ctor<Args...>`, so that the error shows which type
/// and which declaration disagree (ADR-0006).
///
/// @tparam T The type that can't be constructed as declared.
/// @tparam Declared The declaration: a `trellis::ctor<Args...>` whose parameters no constructor
///                  of `T` takes.
///
/// @par Constraints
/// None.
///
/// @par Errors
/// None: it is the type that the TRL-E002 assertion names, not an entity that raises it.
///
/// @par Example
/// @code
/// // What Clang prints for template <> struct trellis::ctor_traits<service> : trellis::ctor<int>
/// // {}; when service has no constructor that takes an int:
/// // error: static assertion failed due to requirement
/// //     'trellis::error::E002<service, trellis::ctor<int>>::reported': TRL-E002: ...
/// @endcode
template <class T, class Declared>
struct E002 {
  /// @brief Always false, so that the static assertion naming it fails and prints the type.
  ///
  /// @par Constraints
  /// None.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// static_assert(!trellis::error::E002<service, trellis::ctor<int>>::reported);
  /// @endcode
  static constexpr bool reported = false;
};

}  // namespace trellis::error

#endif  // TRELLIS_DIAGNOSTICS_ERRORS_HPP
