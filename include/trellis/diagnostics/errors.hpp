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

// ---- T1.4: TRL-E003, TRL-E005, TRL-E012 -------------------------------------------------------

/// @brief Names a request in an error: the type a consumer asked for, spelled as it asked.
///
/// `request<const Config&>` and `request<std::shared_ptr<IClock>>` say which form of the type was
/// requested, which the error type's other arguments don't.
///
/// @tparam Requested The requested type, with its references, pointers and smart pointer.
///
/// @par Constraints
/// None.
///
/// @par Errors
/// None.
///
/// @par Example
/// @code
/// // In a diagnostic, for a constructor parameter `ILogger& log`:
/// //   trellis::error::E005<ILogger, trellis::error::request<ILogger&>, trellis::unique_t>
/// @endcode
template <class Requested>
struct request {};

/// @brief The error type of `TRL-E003`: two bindings for the same key.
///
/// `make_injector` raises it when two of the bindings it collects, from its arguments and the
/// modules among them, bind the same key (plan §5.3, step 1).
///
/// @tparam Key The key bound twice: the bound type, qualifiers included.
/// @tparam First The first binding of the key, in argument order.
/// @tparam Second The second binding of the key.
///
/// @par Constraints
/// None.
///
/// @par Errors
/// - `TRL-E003`: this type is the condition of the static_assert that reports it.
///
/// @par Example
/// @code
/// // trellis::make_injector(trellis::bind<ILogger>.to<ConsoleLogger>(),
/// //                        trellis::bind<ILogger>.to<FileLogger>()) fails with:
/// //   static assertion failed due to requirement
/// //   'trellis::error::E003<ILogger, ..., ...>::reported': TRL-E003: duplicate binding ...
/// @endcode
template <class Key, class First, class Second>
struct E003 {
  /// @brief Always false, so that the static_assert naming it fails and prints this type.
  ///
  /// @par Constraints
  /// None.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// static_assert(!trellis::error::E003<int, char, long>::reported);
  /// @endcode
  static constexpr bool reported = false;
};

/// @brief The error type of `TRL-E005`: a request form that the binding's lifetime doesn't allow.
///
/// Raised when the request-form table of plan §6.3 rejects the form for the lifetime, such as a
/// reference to a unique object, or when the request would drop the constness of a const external
/// object.
///
/// @tparam Key The requested key.
/// @tparam Request `request<R>`, where `R` is the request as the consumer spelled it.
/// @tparam Lifetime The binding's lifetime: `trellis::unique_t`, `trellis::singleton_t`,
///                  `trellis::external_t` and so on.
///
/// @par Constraints
/// None.
///
/// @par Errors
/// - `TRL-E005`: this type is the condition of the static_assert that reports it.
///
/// @par Example
/// @code
/// // A unique Clock requested as Clock& fails with:
/// //   'trellis::error::E005<Clock, trellis::error::request<Clock &>,
/// //                         trellis::unique_t>::reported':
/// //   TRL-E005: request form incompatible with the lifetime: nothing would keep the object
/// //   alive
/// @endcode
template <class Key, class Request, class Lifetime>
struct E005 {
  /// @brief Always false, so that the static_assert naming it fails and prints this type.
  ///
  /// @par Constraints
  /// None.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// static_assert(!trellis::error::E005<int, trellis::error::request<int&>, void>::reported);
  /// @endcode
  static constexpr bool reported = false;
};

/// @brief The error type of `TRL-E012`: an unbound type requested by reference, pointer,
///        `std::shared_ptr` or `std::weak_ptr`.
///
/// An unbound concrete type is implicitly unique, so each request gets a new object that nothing
/// in the injector keeps alive. Only owning requests can take one (ADR-0004).
///
/// @tparam Key The requested key, which has no binding.
/// @tparam Request `request<R>`, where `R` is the request as the consumer spelled it.
///
/// @par Constraints
/// None.
///
/// @par Errors
/// - `TRL-E012`: this type is the condition of the static_assert that reports it.
///
/// @par Example
/// @code
/// // An unbound Clock requested as const Clock& fails with:
/// //   'trellis::error::E012<Clock, trellis::error::request<const Clock &>>::reported':
/// //   TRL-E012: unbound concrete type requested by reference, pointer or shared_ptr: ...
/// @endcode
template <class Key, class Request>
struct E012 {
  /// @brief Always false, so that the static_assert naming it fails and prints this type.
  ///
  /// @par Constraints
  /// None.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// static_assert(!trellis::error::E012<int, trellis::error::request<int&>>::reported);
  /// @endcode
  static constexpr bool reported = false;
};

}  // namespace trellis::error

#endif  // TRELLIS_DIAGNOSTICS_ERRORS_HPP
