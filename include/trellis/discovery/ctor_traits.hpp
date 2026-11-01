// Trellis discovery: explicit constructor declarations, trellis::ctor<Args...> and
// trellis::ctor_traits<T> (plan §7.2, ADR-0003).
// Copyright (c) 2026 Aryan Patel
// SPDX-License-Identifier: BSL-1.0
//
// Constructor discovery tells the static engine which constructor of an implementation type to
// call, and with what (plan §5.3). The explicit strategy is the first of three (ADR-0003): the
// user lists the constructor's parameter types as a trellis::ctor<Args...>, in one of three
// places, and the first that applies wins:
//
//   1. the binding:         bind<I>.to<C>().with_ctor<Args...>()  (dsl/bind.hpp, T1.15)
//   2. a specialization:    template <> struct trellis::ctor_traits<C> : trellis::ctor<Args...> {};
//   3. a member alias in C: using trellis_ctor = trellis::ctor<Args...>;
//
// Where none applies, the explicit strategy declares nothing (detail::no_ctor_declared), and the
// next strategy decides: reflection in C++26 (Phase 6), otherwise deduction (T1.3).
//
// The order of 2 and 3 takes no code: only the primary template of ctor_traits reads the member
// alias, and a specialization replaces the primary template. The binding comes first because it
// is the most specific: it concerns one binding of one injector, where the other two concern the
// type everywhere. The plan doesn't rank the binding against the other two, so that order is this
// task's reading (docs/internals/constructor-discovery.md).
//
// A declaration can't lie (plan §7.2): detail::explicit_ctor_t checks
// std::is_constructible_v<C, Args...> and reports a declaration that matches no constructor as one
// compile error, the type-not-constructible error of plan §7.8, rendered as ADR-0006 says.
#ifndef TRELLIS_DISCOVERY_CTOR_TRAITS_HPP
#define TRELLIS_DISCOVERY_CTOR_TRAITS_HPP

#include <trellis/diagnostics/errors.hpp>
#include <trellis/meta/type_list.hpp>

#include <type_traits>
#include <utility>

namespace trellis::inline v1 {

/// @brief Declares the constructor that Trellis calls for a class, as the list of its parameter
///        types.
///
/// A `ctor` is a declaration, never an object. Declare one for a class in one of three places,
/// the first that applies winning: the class's binding, with `.with_ctor<Args...>()`; a
/// specialization of `ctor_traits`; or a member alias `trellis_ctor` (see `ctor_traits`). The
/// injector then calls the constructor that takes `Args...`, with one argument per parameter, in
/// order.
///
/// @tparam Args The constructor's parameter types, as the constructor spells them, such as
///              `const config&` or `std::unique_ptr<clock>`. Each one is a request, served as the
///              request-form table says (plan §6.3); a parameter of type `X&&` or `const X&&` is
///              the request `X`, as in every discovery mode (ADR-0003).
///
/// @par Constraints
/// - Where it is declared for a class `T`, Trellis can build a `T` from `Args...` the way it will
///   build one: an aggregate with braces, `T{args...}`, so no argument may narrow; any other class
///   with parentheses, `T(args...)`, as `std::is_constructible_v<T, Args...>` checks, so the
///   constructor exists and is public, and an `initializer_list` constructor never takes the
///   arguments over. Otherwise compilation fails with TRL-E002 where Trellis uses the declaration.
///
/// @par Errors
/// - `TRL-E002`: no constructor of the class takes `Args...`, or for an aggregate, `Args...`
///   don't brace-initialize it.
///
/// @par Example
/// @code
/// class service {
///  public:
///   service(const config& settings, std::unique_ptr<clock> time);
/// };
///
/// template <>
/// struct trellis::ctor_traits<service>
///     : trellis::ctor<const config&, std::unique_ptr<clock>> {};
/// @endcode
template <class... Args>
struct ctor {
  /// @brief The parameter types, in order, as a type list for the static engine to walk.
  ///
  /// @par Constraints
  /// None.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// static_assert(trellis::ctor<const config&, std::unique_ptr<clock>>::args::size == 2);
  /// @endcode
  using args = detail::type_list<Args...>;
};

namespace detail {

// The explicit strategy's "nothing declared": what the primary ctor_traits<T> derives from when T
// has no member alias, and what declared_ctor_t yields when no declaration applies at all. The
// next discovery strategy takes over from it (deduction, T1.3).
struct no_ctor_declared {};

// is_ctor_v<D>: whether D is a ctor<Args...>.
template <class D>
inline constexpr bool is_ctor_v = false;
template <class... Args>
inline constexpr bool is_ctor_v<ctor<Args...>> = true;

// member_ctor<T>::type: what T declares with a member alias trellis_ctor, or no_ctor_declared if
// T has none. The partial specialization applies when T has a member type of that name that is
// accessible here: a requires-expression checks access, so a private alias stays invisible, as it
// is to everyone outside T (documented with ctor_traits). Name lookup also finds an alias that T
// inherits, and nothing in C++20 tells it from T's own (documented with ctor_traits too). An
// alias that names something other than a ctor<...> meets the static_assert, which is the one
// error: type is then no_ctor_declared, so nothing fails after it.
//
// Clang marks a class template specialization whose static_assert failed as invalid, and every
// later lookup of its member type is an error of its own. That is harmless here only because one
// thing reads member_ctor<T>::type: the primary ctor_traits<T>, instantiated once per T, however
// often T is used (tests/compile_fail/discovery/member_alias_not_a_ctor_used_twice.cpp). Any new
// reader needs the function form that checked_ctor uses below.
template <class T>
struct member_ctor {
  using type = no_ctor_declared;
};
template <class T>
  requires requires { typename T::trellis_ctor; }
struct member_ctor<T> {
  static_assert(is_ctor_v<typename T::trellis_ctor>,
                "trellis: a member alias trellis_ctor must name trellis::ctor<Args...>, the "
                "constructor's parameter types");
  using type = std::conditional_t<is_ctor_v<typename T::trellis_ctor>, typename T::trellis_ctor,
                                  no_ctor_declared>;
};

}  // namespace detail

/// @brief The explicit constructor declaration of a class: specialize it to choose the
///        constructor that Trellis calls, without changing the class.
///
/// `ctor_traits<T>` declares a constructor by deriving from a `ctor<Args...>`. A specialization,
/// full or partial, derives from the `ctor` it declares. The primary template, which applies
/// where there is no specialization, derives from the member alias `T::trellis_ctor` if `T` has
/// one; otherwise it declares nothing, and the next discovery strategy decides (plan §7.2).
///
/// The first declaration that applies wins: a binding's `.with_ctor<Args...>()`, which concerns
/// one binding of one injector; then a specialization, which replaces the primary template and so
/// the member alias that it reads; then the member alias.
///
/// @tparam T The class whose constructor is declared, without cv-qualifiers.
///
/// @par Constraints
/// - A specialization is declared at global scope, as in the example, or in namespace `trellis`.
///   It derives publicly from one `ctor<...>`, so declare it as a `struct`: declared as a `class`,
///   its base is private, which the compiler reports with an error of its own. A specialization
///   that derives from no `ctor`, or from several, declares nothing and hides `T::trellis_ctor`,
///   and so does one that is declared but never defined.
/// - Every translation unit that uses `T` with Trellis sees the specialization before that use,
///   so declare it next to `T` or in a header that the composition root includes. Otherwise the
///   behaviour is undefined, and compilers needn't diagnose it ([temp.expl.spec]).
/// - `T::trellis_ctor`, where present, is public and names a `ctor<Args...>`. Trellis can't see a
///   private alias, so `T` then declares nothing; an alias that names another type fails to
///   compile with a static assertion.
/// - A derived class inherits its base's `trellis_ctor`, like any member, and C++20 can't tell an
///   inherited alias from the class's own. That suits a class that inherits the constructors as
///   well (`using Base::Base;`). Give any other derived class a declaration of its own, an alias
///   or a specialization: otherwise it fails with TRL-E002 if the base's declaration matches none
///   of its constructors, and silently gets the base's choice if one matches. Two bases with
///   aliases make the name ambiguous, so the class declares nothing. Specializations aren't
///   inherited.
/// - Trellis can build a `T` from the declared `Args...`, as `ctor` describes; otherwise
///   compilation fails with TRL-E002 where Trellis uses the declaration.
///
/// @par Errors
/// - `TRL-E002`: the declared `ctor<Args...>` matches no constructor of `T`, or doesn't
///   brace-initialize `T` if it is an aggregate.
///
/// @par Example
/// @code
/// // Outside the class, which then needn't know Trellis (ADR-0007):
/// template <>
/// struct trellis::ctor_traits<service> : trellis::ctor<const config&, logger&> {};
///
/// // Inside the class, as an opt-in member alias:
/// class repository {
///  public:
///   using trellis_ctor = trellis::ctor<const config&>;
///   explicit repository(const config& settings);
/// };
/// @endcode
template <class T>
struct ctor_traits : detail::member_ctor<T>::type {};

namespace detail {

// ctor_base_of(traits): the ctor<Args...> that *traits derives from, or no_ctor_declared if it
// derives from none. Only named in decltype. Given a pointer to a class with a base ctor<Args...>,
// the first overload deduces Args from that base ([temp.deduct.call]/4.3), and its derived-to-base
// conversion beats the second overload's conversion to const void* ([over.ics.rank]/4.4). Without
// such a base the deduction fails and only the second overload is viable; with two such bases it
// fails too, as ambiguous, which is why a specialization derives from one ctor at most. One
// overload resolution, whatever the arity.
//
// GCC 13 and 14 match T& against a T&& base when a template argument is given and compared with
// the bases, as T1.1 found in its type_map lookups. Here no argument is given: all of Args comes
// from the one base, which every compiler reads exactly. The tests of parameter types that differ
// only in & and && (tests/unit/ctor_traits_test.cpp) guard that.
template <class... Args>
ctor<Args...> ctor_base_of(const ctor<Args...>*);
no_ctor_declared ctor_base_of(const void*);

// declared_ctor<T, BindingCtor>::type: the explicit declaration that applies to T, unchecked.
// BindingCtor is what T's binding declares with .with_ctor<Args...>(): a ctor<Args...>, which
// wins without instantiating ctor_traits<T>, or no_ctor_declared, and then ctor_traits<T> decides.
// Any other BindingCtor meets the undefined primary template: internal misuse, by the binding DSL.
template <class T, class BindingCtor = no_ctor_declared>
struct declared_ctor;
template <class T, class... Args>
struct declared_ctor<T, ctor<Args...>> {
  using type = ctor<Args...>;
};
template <class T>
struct declared_ctor<T, no_ctor_declared> {
  // Qualified, so that argument-dependent lookup neither searches the namespaces of T and its
  // template arguments (instantiating class templates there to look for friends) nor finds a
  // user's function of the same name.
  using type = decltype(detail::ctor_base_of(static_cast<const ctor_traits<T>*>(nullptr)));
};

// declared_ctor_t<T, BindingCtor>: declared_ctor's type for T without its cv-qualifiers, so that
// const X finds the declaration written for X, specialization included. BindingCtor, here and in
// explicit_ctor_t below, is where a binding plugs in: the static engine passes the ctor<Args...>
// of the binding's .with_ctor<Args...>() (dsl/bind.hpp, impl_ctor_t; static/resolve.hpp,
// served_binding), and a binding without one leaves the default, no_ctor_declared.
template <class T, class BindingCtor = no_ctor_declared>
using declared_ctor_t = typename declared_ctor<std::remove_cv_t<T>, BindingCtor>::type;

// builds_from<T, Args...>(): whether a T can be built from Args... the way the static engine will
// build it, which is the rule T1.5's creation code follows. An aggregate is built with braces,
// T{args...}, as deduced discovery builds one (ADR-0003), so a narrowing argument is rejected and
// brace elision works. Any other type is built with parentheses, T(args...), which
// std::is_constructible checks, so an initializer_list constructor never takes the arguments over:
// std::vector<int>(n, value) holds n copies, where braces would make a list or fail to narrow. The
// requires-expression turns an ill-formed T{args...}, narrowing included, into false.
template <class T, class... Args>
constexpr bool builds_from() noexcept {
  if constexpr (std::is_aggregate_v<T>) {
    return requires { T{std::declval<Args>()...}; };
  } else {
    return std::is_constructible_v<T, Args...>;
  }
}

// ctor_matches_v<T, Declared>: whether the declaration Declared matches how a T will be built: for
// a ctor<Args...>, builds_from<T, Args...>(), with T's cv-qualifiers ignored; no_ctor_declared has
// nothing to check. A plain constant, with no assertion, for the resolver's normal pass, which
// only asks (ADR-0006 item 2).
template <class T, class Declared>
inline constexpr bool ctor_matches_v = true;
template <class T, class... Args>
inline constexpr bool ctor_matches_v<T, ctor<Args...>> =
    detail::builds_from<std::remove_cv_t<T>, Args...>();

// declaration_message<T>: the TRL-E002 message for a declaration that doesn't match how a T will
// be built. Aggregates get a message of their own, as ADR-0006 gives each kind of failure its own
// literal. The explain pass reads it too (diagnostics/explain.hpp), to raise the same report.
template <class T>
inline constexpr error_message declaration_message =
    std::is_aggregate_v<T> ? error_message::declaration_aggregate : error_message::declaration;

// checked_ctor<T, Declared>(): a Declared, once the declaration is known to match how a T will be
// built. When it doesn't, raise() reports TRL-E002, rendered as ADR-0006 item 3 says: the
// condition names the error type, which carries T and the declaration, and the message starts with
// the ID (diagnostics/errors.hpp).
//
// It is a function with a deduced return type, read through decltype, and not a class template:
// Clang marks a class template specialization whose static_assert failed as invalid, and then
// every later use of its member type was an error of its own ("no type named 'type'"), one per
// consumer of the declaration. A deduced return type survives the failed assertion, and later
// uses reuse it, so the error comes once however often the declaration is used
// (tests/compile_fail/discovery/*_used_twice.cpp). Deducing the return type is also what makes
// the compiler instantiate the body, and with it the check, when explicit_ctor_t names it.
template <class T, class Declared>
constexpr auto checked_ctor() noexcept {
  if constexpr (!ctor_matches_v<T, Declared>) {
    detail::raise<trellis::error::E002<T, Declared>, declaration_message<T>>();
  }
  return Declared{};
}

// explicit_ctor_t<T, BindingCtor>: the explicit strategy's result for T, checked: the declaration
// that applies, or no_ctor_declared, which leaves T to the next strategy. T's cv-qualifiers are
// ignored, as in declared_ctor_t. This is the static engine's way into explicit discovery. It is
// memoized per (T without cv-qualifiers, declaration), so each declaration is checked, and
// reported, once.
//
// Two choices keep its errors short. It names declared_ctor directly rather than through
// declared_ctor_t, because each alias between them adds a note to every error raised inside
// ctor_traits<T> (3 lines on Clang). And the third parameter, which callers never pass, names T
// without cv-qualifiers once, so that the definition fits on one line: GCC 14 quotes every source
// line of it in each error's backtrace (4 more lines when it took three).
template <class T, class BindingCtor = no_ctor_declared, class Class = std::remove_cv_t<T>>
using explicit_ctor_t =
    decltype(detail::checked_ctor<Class, typename declared_ctor<Class, BindingCtor>::type>());

}  // namespace detail
}  // namespace trellis::inline v1

#endif  // TRELLIS_DISCOVERY_CTOR_TRAITS_HPP
