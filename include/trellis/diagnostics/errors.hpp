// Trellis diagnostics: the compile-error vocabulary and the one place that reports it (plan §7.8,
// ADR-0006).
// Copyright (c) 2026 Aryan Patel
// SPDX-License-Identifier: BSL-1.0
//
// Trellis reports each mistake it finds at compile time as one static_assert whose condition
// names an error type and whose message starts with the error's ID (ADR-0006 item 3):
//
//   static_assert(Error::reported, "TRL-E0xx: <summary>");   // Error = trellis::error::E0xx<...>
//
// Every error type's `reported` is false, so the assertion fails, and the compiler prints the
// error type with its template arguments: Clang on the first error line, GCC in the note that
// follows it. The message carries the ID, which leads to the page in docs/errors/.
//
// The vocabulary lives in namespace trellis::error, outside inline namespace v1, so that its
// names print short and the same on every compiler: GCC spells inline namespaces out and Clang
// hides them. That is the one documented exception to the project's namespace rule (owner
// decision, ADR-0006). These types are versioned by error ID instead: an ID never changes meaning.
//
// Every one of those static_asserts is in detail::raise<Error, Message, Scope>(), at the end of
// this header, and every check that finds a mistake calls it (T1.6; docs/internals/
// explain-pass.md):
//
// - It is a function, instantiated once per (Error, Message, Scope), so a mistake that several
//   requests, roots or entry points run into is reported once. A static_assert in a class template
//   would not do: Clang marks a class whose static_assert failed as invalid, and every later use
//   is an error of its own.
// - Its template arguments name the error type, so the instantiation note that the compilers print
//   for it names the type too. That is ADR-0006's mitigation for MSVC, which prints a failed
//   static_assert's message but not its condition.
// - Each message has its own branch and its own literal (ADR-0006 item 2), all of them here, so
//   that the catalog's wording lives in one place.
#ifndef TRELLIS_DIAGNOSTICS_ERRORS_HPP
#define TRELLIS_DIAGNOSTICS_ERRORS_HPP

#include <cstddef>

namespace trellis::error {

/// @brief The error type of TRL-E002, type not constructible: an explicit constructor declaration
///        that matches no constructor of the type, a class for which deduction finds no
///        constructor (or that is incomplete), and a copy that can't be made.
///
/// Users never name it: it appears in the compiler's output when a declared
/// `trellis::ctor<Args...>` can't build `T` the way Trellis builds it (see `trellis::ctor`), so
/// that the error shows which type and which declaration disagree (ADR-0006), and when deduction
/// finds no constructor to call. `create<T>()` also raises it when a by-value request needs a copy
/// of a bound object, or of a new object of a derived class, that `T` can't be constructed from.
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
/// modules among them, bind the same key (plan §5.3, step 1), and when two overrides,
/// `trellis::override<K>`, name the same key (plan §6.10, T1.7).
///
/// @tparam Key The key bound twice: the bound type, qualifiers included.
/// @tparam First The first binding of the key, in argument order, or for two overrides the first
///               override, `trellis::override_binding<...>`.
/// @tparam Second The second binding of the key, or the second override.
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
/// §7.8). The path from the root to the request, a `trellis::error::path<...>`, is in the
/// instantiation note that names `trellis::detail::explain_cause`, Clang's second note and GCC's
/// first "required from" line (T1.6, ADR-0006).
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
/// one requested by reference, pointer, `std::shared_ptr` or `std::weak_ptr` is `TRL-E012`. The
/// path from the root is in the instantiation note that names `trellis::detail::explain_cause`
/// (T1.6).
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
/// behaviour (plan §7.8, provisional: the ID awaits the owner's confirmation). The path from the
/// root is in the instantiation note that names `trellis::detail::explain_cause` (T1.6).
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

// ---- T1.6: the path, TRL-E009 -----------------------------------------------------------------

/// @brief Names the path from the root of a failing `create<T>()` to the failing request: the
///        classes whose constructors lead to it, root first.
///
/// `path<App, Service, Store>` says that `create<App>()`, or `make_injector` for an eager
/// singleton `App`, builds an `App`, whose constructor needs a `Service`, whose constructor needs
/// a `Store`, whose constructor makes the failing request. `path<>` is the root itself. The
/// explain pass builds it after a failure (plan §7.8, ADR-0006 item 5); it appears in the
/// instantiation note of the step that reports, `trellis::detail::explain_cause`, Clang's second
/// note and GCC's first "required from" line, never in the type of a graph that compiles.
///
/// @tparam Consumers The classes, as the injector builds them: a binding's implementation, not its
///                   key.
///
/// @par Constraints
/// None.
///
/// @par Errors
/// None.
///
/// @par Example
/// @code
/// // Clang's second note for a TRL-E012 two classes below create<station>(), from reader's
/// // parameter 1:
/// //   note: in instantiation of member function 'trellis::detail::explain_cause<
/// //       trellis::error::path<station, reader>, 1, sensor &, ...>::run' requested here
/// @endcode
template <class... Consumers>
struct path {};

/// @brief The error type of `TRL-E009`: an override without an existing binding to replace.
///
/// An override (`trellis::override<K>`, T1.7) replaces the binding of its key that the other
/// bindings and modules of the injector provide, so that a test can swap one implementation
/// (plan §6.10). When none of them binds the key, there is nothing to replace, which is usually a
/// misspelt key or a module left out, and `make_injector` reports it rather than adding a binding.
///
/// @tparam Key The override's key.
/// @tparam Override The override, as its binding type.
///
/// @par Constraints
/// None.
///
/// @par Errors
/// - `TRL-E009`: this type is the condition of the static_assert that reports it.
///
/// @par Example
/// @code
/// // An override of IRepository with no binding of IRepository among the other bindings:
/// //   'trellis::error::E009<IRepository, ...>::reported':
/// //   TRL-E009: override without an existing binding: ...
/// @endcode
template <class Key, class Override>
struct E009 {
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
  /// static_assert(!trellis::error::E009<int, long>::reported);
  /// @endcode
  static constexpr bool reported = false;
};

}  // namespace trellis::error

namespace trellis::inline v1::detail {

// error_message: which message a report carries. Each is one static_assert literal in raise(),
// below, and names the ID it starts with; ADR-0006 item 2 gives each kind of failure its own
// literal.
enum class error_message : unsigned char {
  none,  // nothing to report: make_injector has reported TRL-E003 for the injector already
  // TRL-E001, from create<T>()
  no_binding,       // an abstract type without a binding
  bound_to_itself,  // an abstract type bound to itself with bind<I> alone
  // TRL-E002: from discovery (ctor_traits.hpp, deduced.hpp), and from create<T>() for a copy
  declaration,            // a ctor<...> declaration that matches no constructor
  declaration_aggregate,  // a ctor<...> declaration that doesn't brace-initialize an aggregate
  abstract,               // deduction for an abstract class
  not_a_class,            // deduction for a type that isn't a class
  incomplete,             // deduction for a class that is incomplete where it is discovered
  not_constructible,      // deduction found no constructor to call
  copy,                   // a by-value request whose copy the key can't be constructed from
  // TRL-E003, from make_injector
  duplicate_binding,
  duplicate_override,  // two overrides of one key (T1.7)
  // TRL-E004, from deduction
  greedy,              // a constructor template or variadic constructor accepts anything
  array_member,        // an aggregate with an array member
  undeducible_member,  // an aggregate with a member that no placeholder can initialize
  // TRL-E005, from the request-form table (dsl/request_forms.hpp, form_verdict)
  copies_hide_sharing,
  nothing_keeps_it_alive,
  ambiguous_ownership,
  duplicates_ownership,
  not_heap_owned,
  caller_owns_it,
  const_object,
  // TRL-E009, for overrides (T1.7)
  override_without_binding,
  // TRL-E010, from deduction
  above_bound,
  // TRL-E012, from the request-form table
  unbound_shared,     // an unbound concrete type, requested in a sharing form
  smart_pointer_key,  // the same, when the request is a reference or pointer to a smart pointer
  // TRL-E013, from create<T>() (provisional ID)
  strict_unbound,  // an unbound concrete type requested in an owning form, options{.strict = true}
  // TRL-E014, from create<T>() (provisional ID)
  unsafe_delete,  // std::unique_ptr<Key> would delete a derived class through Key
};

// error_args<Error>: the template arguments of an error type, which raise() spells the error type
// out with: first, second and third for the error templates with type parameters only (by their
// arity), first and bound for E010. Only the shapes of the error types above.
template <class Error>
struct error_args;
template <template <class> class E, class A>
struct error_args<E<A>> {
  using first = A;
};
template <template <class, class> class E, class A, class B>
struct error_args<E<A, B>> {
  using first = A;
  using second = B;
};
template <template <class, class, class> class E, class A, class B, class C>
struct error_args<E<A, B, C>> {
  using first = A;
  using second = B;
  using third = C;
};
template <class T, std::size_t MaxArity>
struct error_args<trellis::error::E010<T, MaxArity>> {
  using first = T;
  static constexpr std::size_t bound = MaxArity;
};

// raise<Error, Message, Scope>(): the one static_assert of a mistake, rendered as ADR-0006 item 3
// says. Every check that finds a TRL mistake calls it, with the error type, the message and what
// the report is unique to: Scope is the injector for the mistakes create<T>() finds, so that each
// request is reported once per injector however many roots and consumers make it, and void for
// those that discovery and make_injector find, which are reported once per class or per injector
// type already.
//
// Why a function template with the error type as an argument, rather than an assertion in each
// check (docs/internals/explain-pass.md):
// - A function body is instantiated once per specialization, and a failed static_assert in it,
//   unlike one in a class template, leaves nothing that Clang marks invalid, so a later call is
//   silent: one error per mistake, however many paths lead to it.
// - The compilers print an instantiation note for it that names Error: Clang and GCC in addition
//   to the condition, MSVC instead of it, as MSVC prints only the message (ADR-0006's MSVC
//   mitigation; untested locally, as MSVC runs only in CI).
// - It costs one note more than an assertion written in the check itself (ADR-0006 item 2 measured
//   1 to 3 lines on GCC), which the explain pass more than saves by reporting from a short chain.
//
// Each condition spells its error template out, trellis::error::E0xx<...>, with Error's arguments
// (error_args), rather than naming Error::reported: Clang prints a nested-name-specifier without
// its scope, so `Error::reported` would print as E0xx<...>::reported, while the type spelled out
// prints as trellis::error::E0xx<...>, as GCC prints it too (checked in T1.6 on Clang 18 and 20).
// A Message that names no static_assert reports nothing; error_message::none is that case.
template <class Error, error_message Message, class Scope = void>
constexpr void raise() noexcept {
  using args = error_args<Error>;
  if constexpr (Message == error_message::no_binding) {
    static_assert(trellis::error::E001<typename args::first, typename args::second>::reported,
                  "TRL-E001: no binding for an abstract type: bind it to an implementation with "
                  "trellis::bind<I>.to<C>()");
  } else if constexpr (Message == error_message::bound_to_itself) {
    static_assert(trellis::error::E001<typename args::first, typename args::second>::reported,
                  "TRL-E001: no binding for an abstract type: bind<I> alone binds it to itself, "
                  "and nothing implements it; name the implementation with "
                  "trellis::bind<I>.to<C>()");
  } else if constexpr (Message == error_message::declaration) {
    static_assert(trellis::error::E002<typename args::first, typename args::second>::reported,
                  "TRL-E002: type not constructible: its trellis::ctor<...> declaration matches "
                  "no constructor");
  } else if constexpr (Message == error_message::declaration_aggregate) {
    static_assert(trellis::error::E002<typename args::first, typename args::second>::reported,
                  "TRL-E002: type not constructible: its trellis::ctor<...> declaration doesn't "
                  "brace-initialize the aggregate");
  } else if constexpr (Message == error_message::abstract) {
    static_assert(trellis::error::E002<typename args::first, typename args::second>::reported,
                  "TRL-E002: type not constructible: it is abstract; bind it to an "
                  "implementation");
  } else if constexpr (Message == error_message::not_a_class) {
    static_assert(trellis::error::E002<typename args::first, typename args::second>::reported,
                  "TRL-E002: type not constructible: deduction needs a class type; bind the type "
                  "to a value");
  } else if constexpr (Message == error_message::incomplete) {
    static_assert(trellis::error::E002<typename args::first, typename args::second>::reported,
                  "TRL-E002: type not constructible: it is incomplete here; define the type before "
                  "the point where Trellis discovers its constructor");
  } else if constexpr (Message == error_message::not_constructible) {
    static_assert(trellis::error::E002<typename args::first, typename args::second>::reported,
                  "TRL-E002: type not constructible: deduction found no public, non-deleted, "
                  "unambiguous constructor (same-arity overloads are ambiguous); declare one with "
                  "trellis::ctor_traits");
  } else if constexpr (Message == error_message::copy) {
    static_assert(trellis::error::E002<typename args::first, typename args::second>::reported,
                  "TRL-E002: type not constructible: a by-value request takes a copy of the bound "
                  "object, or a new object of the bound class as the key, and the key can't be "
                  "constructed from it; request a reference or a smart pointer instead");
  } else if constexpr (Message == error_message::duplicate_binding) {
    static_assert(trellis::error::E003<typename args::first, typename args::second,
                                       typename args::third>::reported,
                  "TRL-E003: duplicate binding for a key: each key may be bound only once per "
                  "injector");
  } else if constexpr (Message == error_message::duplicate_override) {
    static_assert(trellis::error::E003<typename args::first, typename args::second,
                                       typename args::third>::reported,
                  "TRL-E003: duplicate binding for a key: two overrides name the same key; each "
                  "key may be overridden only once per injector");
  } else if constexpr (Message == error_message::greedy) {
    static_assert(trellis::error::E004<typename args::first>::reported,
                  "TRL-E004: ambiguous constructor: a constructor template or variadic constructor "
                  "accepts any argument, so deduction can't tell what it needs; declare the "
                  "constructor with trellis::ctor_traits");
  } else if constexpr (Message == error_message::array_member) {
    static_assert(trellis::error::E004<typename args::first>::reported,
                  "TRL-E004: ambiguous constructor: an aggregate with an array member, whose "
                  "elements count as one field or as one each depending on the compiler; declare "
                  "the fields with trellis::ctor_traits");
  } else if constexpr (Message == error_message::undeducible_member) {
    static_assert(trellis::error::E004<typename args::first>::reported,
                  "TRL-E004: ambiguous constructor: deduction can't initialize every member of "
                  "this aggregate (a std::optional, std::tuple or std::any member is ambiguous); "
                  "declare the fields with trellis::ctor_traits");
  } else if constexpr (Message == error_message::copies_hide_sharing) {
    static_assert(trellis::error::E005<typename args::first, typename args::second,
                                       typename args::third>::reported,
                  "TRL-E005: request form incompatible with the lifetime: copies hide sharing; "
                  "request a reference, or opt in with .allow_copy()");
  } else if constexpr (Message == error_message::nothing_keeps_it_alive) {
    static_assert(trellis::error::E005<typename args::first, typename args::second,
                                       typename args::third>::reported,
                  "TRL-E005: request form incompatible with the lifetime: nothing would keep the "
                  "object alive; bind it in trellis::singleton, or request it by value or as "
                  "std::unique_ptr");
  } else if constexpr (Message == error_message::ambiguous_ownership) {
    static_assert(trellis::error::E005<typename args::first, typename args::second,
                                       typename args::third>::reported,
                  "TRL-E005: request form incompatible with the lifetime: ambiguous ownership; "
                  "request std::unique_ptr, or bind it in trellis::singleton");
  } else if constexpr (Message == error_message::duplicates_ownership) {
    static_assert(trellis::error::E005<typename args::first, typename args::second,
                                       typename args::third>::reported,
                  "TRL-E005: request form incompatible with the lifetime: std::unique_ptr would "
                  "duplicate ownership of a shared object; request a reference");
  } else if constexpr (Message == error_message::not_heap_owned) {
    static_assert(
        trellis::error::E005<typename args::first, typename args::second,
                             typename args::third>::reported,
        "TRL-E005: request form incompatible with the lifetime: the object is not "
        "heap-owned, so there is no std::shared_ptr to it; bind it in trellis::singleton");
  } else if constexpr (Message == error_message::caller_owns_it) {
    static_assert(trellis::error::E005<typename args::first, typename args::second,
                                       typename args::third>::reported,
                  "TRL-E005: request form incompatible with the lifetime: the caller owns the "
                  "object, so there is no std::shared_ptr to it; bind it with .to_shared()");
  } else if constexpr (Message == error_message::const_object) {
    static_assert(trellis::error::E005<typename args::first, typename args::second,
                                       typename args::third>::reported,
                  "TRL-E005: request form incompatible with the lifetime: the bound object is "
                  "const; request it through a reference or pointer to const, such as const T& "
                  "or std::shared_ptr<const T>");
  } else if constexpr (Message == error_message::override_without_binding) {
    static_assert(trellis::error::E009<typename args::first, typename args::second>::reported,
                  "TRL-E009: override without an existing binding: an override replaces the "
                  "binding of its key, and no other binding or module binds that key; did you "
                  "mean trellis::bind?");
  } else if constexpr (Message == error_message::above_bound) {
    static_assert(trellis::error::E010<typename args::first, args::bound>::reported,
                  "TRL-E010: constructor arity above max_ctor_arity: raise max_ctor_arity or "
                  "declare the constructor with trellis::ctor_traits");
  } else if constexpr (Message == error_message::unbound_shared) {
    static_assert(trellis::error::E012<typename args::first, typename args::second>::reported,
                  "TRL-E012: unbound concrete type requested by reference, pointer or shared_ptr: "
                  "nothing would own it; bind it with an owning lifetime such as "
                  "trellis::singleton");
  } else if constexpr (Message == error_message::smart_pointer_key) {
    static_assert(trellis::error::E012<typename args::first, typename args::second>::reported,
                  "TRL-E012: unbound concrete type requested by reference, pointer or shared_ptr: "
                  "a reference or pointer to a smart pointer asks for the smart pointer itself, "
                  "which nothing binds; take it by value, such as std::shared_ptr<T>, and the "
                  "injector makes it from T's binding");
  } else if constexpr (Message == error_message::strict_unbound) {
    static_assert(trellis::error::E013<typename args::first, typename args::second>::reported,
                  "TRL-E013: unbound concrete type requested in strict mode: options{.strict = "
                  "true} turns off the implicit unique binding; bind the type, such as "
                  "trellis::bind<T> for a new object per request");
  } else if constexpr (Message == error_message::unsafe_delete) {
    static_assert(trellis::error::E014<typename args::first, typename args::second,
                                       typename args::third>::reported,
                  "TRL-E014: owning request would delete the implementation through a base class "
                  "whose destructor isn't virtual: std::unique_ptr<I> deletes through I; give I a "
                  "virtual destructor, or request std::shared_ptr<I>, which deletes the class it "
                  "was made for");
  }
}

}  // namespace trellis::inline v1::detail

#endif  // TRELLIS_DIAGNOSTICS_ERRORS_HPP
