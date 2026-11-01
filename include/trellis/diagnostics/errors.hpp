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

#include <cstddef>

namespace trellis::error {

/// @brief The error type of TRL-E002, type not constructible, for an explicit constructor
///        declaration that matches no constructor of the type, and for a copy that can't be made.
///
/// Users never name it: it appears in the compiler's output when `std::is_constructible_v<T,
/// Args...>` is false for a declared `trellis::ctor<Args...>`, so that the error shows which type
/// and which declaration disagree (ADR-0006). `create<T>()` also raises it when a by-value request
/// needs a copy of a bound object, or of a new object of a derived class, that `T` can't be
/// constructed from.
///
/// @tparam T The type that can't be constructed as declared, or as the request needs.
/// @tparam Declared The declaration: a `trellis::ctor<Args...>` whose parameters no constructor
///                  of `T` takes; `trellis::detail::no_ctor_declared` where deduction found no
///                  constructor; or `trellis::error::request<R>`, the by-value request that needs
///                  the copy.
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

/// @brief The error type of TRL-E004, ambiguous constructor, for a type whose constructor deduced
///        discovery can't pin down.
///
/// Users never name it: it appears in the compiler's output when a constructor template or a
/// variadic constructor accepts any argument, or when an aggregate's fields can't be counted the
/// same way on every compiler, so that the error shows which type it is (ADR-0006).
///
/// @tparam T The type whose constructor is ambiguous.
///
/// @par Constraints
/// None.
///
/// @par Errors
/// None: it is the type that the TRL-E004 assertion names, not an entity that raises it.
///
/// @par Example
/// @code
/// // What Clang prints for a class whose only constructor is template <class U> greedy(U&&):
/// // error: static assertion failed due to requirement
/// //     'trellis::error::E004<greedy>::reported': TRL-E004: ambiguous constructor: ...
/// @endcode
template <class T>
struct E004 {
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
  /// static_assert(!trellis::error::E004<greedy>::reported);
  /// @endcode
  static constexpr bool reported = false;
};

/// @brief The error type of TRL-E010, constructor arity above `max_ctor_arity`, for a type that
///        deduced discovery finds a constructor for only above the bound.
///
/// Users never name it: it appears in the compiler's output when a type's constructors all take
/// more than `MaxArity` parameters, so that the error shows the type and the bound (ADR-0006).
///
/// @tparam T The type whose constructor takes too many parameters.
/// @tparam MaxArity The bound in effect, the injector's `max_ctor_arity` (16 by default).
///
/// @par Constraints
/// None.
///
/// @par Errors
/// None: it is the type that the TRL-E010 assertion names, not an entity that raises it.
///
/// @par Example
/// @code
/// // What Clang prints for a class whose only constructor takes 17 parameters:
/// // error: static assertion failed due to requirement
/// //     'trellis::error::E010<wide, 16>::reported': TRL-E010: constructor arity above ...
/// @endcode
template <class T, std::size_t MaxArity>
struct E010 {
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
  /// static_assert(!trellis::error::E010<wide, 16>::reported);
  /// @endcode
  static constexpr bool reported = false;
};

// ---- T1.5: TRL-E001, TRL-E013, TRL-E014 -------------------------------------------------------

/// @brief The error type of `TRL-E001`: no binding for an abstract type.
///
/// `create<T>()` raises it when a request names an abstract type that has no binding, or one
/// bound to itself with `bind<I>` alone, so that nothing says which class implements it (plan
/// §7.8). T1.6 adds the path from the root.
///
/// @tparam Key The requested key.
/// @tparam Request `request<R>`, where `R` is the request as the consumer spelled it.
///
/// @par Constraints
/// None.
///
/// @par Errors
/// - `TRL-E001`: this type is the condition of the static_assert that reports it.
///
/// @par Example
/// @code
/// // A constructor parameter std::shared_ptr<IRepository>, with no binding for IRepository:
/// //   'trellis::error::E001<IRepository,
/// //                         trellis::error::request<std::shared_ptr<IRepository>>>::reported':
/// //   TRL-E001: no binding for an abstract type: ...
/// @endcode
template <class Key, class Request>
struct E001 {
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
  /// static_assert(!trellis::error::E001<int, trellis::error::request<int&>>::reported);
  /// @endcode
  static constexpr bool reported = false;
};

/// @brief The error type of `TRL-E013`: an unbound concrete type requested in strict mode.
///
/// `options{.strict = true}` turns off the implicit unique binding of unbound concrete types
/// (ADR-0004), so `create<T>()` raises it when a request names a type without a binding that the
/// injector would otherwise build (plan §7.8, provisional: the ID awaits the owner's
/// confirmation). An abstract type without a binding is `TRL-E001` in every mode, and a concrete
/// one requested by reference, pointer or `std::shared_ptr` is `TRL-E012`.
///
/// @tparam Key The requested key, which has no binding.
/// @tparam Request `request<R>`, where `R` is the request as the consumer spelled it.
///
/// @par Constraints
/// None.
///
/// @par Errors
/// - `TRL-E013`: this type is the condition of the static_assert that reports it.
///
/// @par Example
/// @code
/// // Under options{.strict = true}, an unbound Clock requested by value fails with:
/// //   'trellis::error::E013<Clock, trellis::error::request<Clock>>::reported':
/// //   TRL-E013: unbound concrete type requested in strict mode: ...
/// @endcode
template <class Key, class Request>
struct E013 {
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
  /// static_assert(!trellis::error::E013<int, trellis::error::request<int>>::reported);
  /// @endcode
  static constexpr bool reported = false;
};

/// @brief The error type of `TRL-E014`: an owning request would delete the implementation through
///        a base class whose destructor isn't virtual.
///
/// `create<T>()` raises it when a request `std::unique_ptr<Key>` would hold a new object of a
/// class derived from `Key`, the binding's implementation, while `Key`'s destructor isn't virtual:
/// `std::default_delete<Key>` would then delete the object through `Key`, which is undefined
/// behaviour (plan §7.8, provisional: the ID awaits the owner's confirmation).
///
/// @tparam Key The requested key, the base class.
/// @tparam Request `request<R>`, where `R` is the request as the consumer spelled it.
/// @tparam Impl The binding's implementation, the class that would be deleted through `Key`.
///
/// @par Constraints
/// None.
///
/// @par Errors
/// - `TRL-E014`: this type is the condition of the static_assert that reports it.
///
/// @par Example
/// @code
/// // bind<Shape>.to<Square>(), where Shape's destructor isn't virtual, requested as
/// // std::unique_ptr<Shape>:
/// //   'trellis::error::E014<Shape, trellis::error::request<std::unique_ptr<Shape>>,
/// //                         Square>::reported':
/// //   TRL-E014: owning request would delete the implementation through a base class ...
/// @endcode
template <class Key, class Request, class Impl>
struct E014 {
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
  /// static_assert(!trellis::error::E014<int, trellis::error::request<int>, long>::reported);
  /// @endcode
  static constexpr bool reported = false;
};

}  // namespace trellis::error

#endif  // TRELLIS_DIAGNOSTICS_ERRORS_HPP
