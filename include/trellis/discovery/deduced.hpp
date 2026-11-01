// Trellis discovery: deduced constructor discovery, the C++20 default (plan §7.2, ADR-0003).
// Copyright (c) 2026 Aryan Patel
// SPDX-License-Identifier: BSL-1.0
//
// Internal (trellis::detail): the static engine's way into constructor discovery, not user API.
// docs/internals/deduction.md explains each technique and lists its tests.
//
// A type that declares no constructor (ctor_traits.hpp: no binding's .with_ctor<Args...>(), no
// ctor_traits specialization, no member alias) has it deduced, as spike S1 designed and ADR-0003
// accepted, without macros or changes to the type:
//
// 1. Discovery. A context-free placeholder, probe_arg<T>, converts to any parameter type through
//    two conversion operator templates. The arity is the largest N up to the bound (the injector's
//    max_ctor_arity, 16 by default) such that T is constructible from N probes, searched from the
//    top down by a flat if-constexpr chain. Copy and move constructors are excluded, and the limits
//    S1 found are detected (deduce, below). The result is memoized per (T, bound).
// 2. Resolution. The resolving placeholder any_arg<T, Ctx> has the same operator shapes, plus a
//    deleted twin of each, and asks the engine's context Ctx for the request that each parameter
//    makes: the return type of the operator that overload resolution selects. It is used at exactly
//    the discovered arity, so an unresolvable parameter can't select a shorter constructor.
//
// Deduction can't name the parameter types (ADR-0003, plan §5.3: "an arity in deduced mode"), so
// discovered_ctor_t, the one entry point for every mode, gives ctor<probe_arg<T>, ...>: one
// placeholder per parameter, each standing for the request it deduces when the engine replaces it
// with any_arg. A declared ctor<Args...> lists the parameter types instead.
#ifndef TRELLIS_DISCOVERY_DEDUCED_HPP
#define TRELLIS_DISCOVERY_DEDUCED_HPP

#include <trellis/config.hpp>
#include <trellis/diagnostics/errors.hpp>
#include <trellis/discovery/ctor_traits.hpp>

#include <cstddef>
#include <type_traits>
#include <utility>

namespace trellis::inline v1::detail {

// The default bound on a deduced constructor's arity: options::max_ctor_arity's default (plan
// §6.13, ADR-0003), measured by spike S1 at 1.5 to 3.2 ms per class, and by T1.11 against the
// compile-time budgets, which 100-class graphs meet with it (docs/benchmarks/v0.1.md).
inline constexpr std::size_t default_max_ctor_arity = 16;

// The largest bound the arity search supports. A constructor with more parameters can always be
// declared explicitly, which has no limit.
inline constexpr std::size_t deduced_arity_limit = 32;

// ---- the placeholders -------------------------------------------------------------------------

// not_self<Consumer, T>: the conversion target T is not the class under construction. With one
// argument, Consumer's copy and move constructors would be viable through operator T&() (T = const
// Consumer) and operator T() (T = Consumer): every copyable class would seem to have a constructor
// with one parameter, and a real one would become ambiguous. Every operator below carries this
// constraint, which removes them (S1 §3.5). Both sides lose their cv-qualifiers, so that a const
// or volatile Consumer excludes them too. TRELLIS_IS_SAME is the __is_same builtin where there is
// one, which instantiates nothing per pair of types.
template <class Consumer, class T>
concept not_self = !TRELLIS_IS_SAME(std::remove_cvref_t<T>, std::remove_cv_t<Consumer>);

#if TRELLIS_PROBE_DECLARES_TWINS
// The GCC 13 workaround (config.hpp) declares deleted twins in the probe, constrained so that no
// constructor parameter selects them. The constraint names twin_tag, an internal class that no
// constructor takes, rather than being plainly false: a template whose constraints no type
// satisfies has no valid specialization, which makes a program ill-formed ([temp.res.general]/6).
struct twin_tag {};
template <class T>
concept twin_tag_request = TRELLIS_IS_SAME(std::remove_cvref_t<T>, twin_tag);
#endif

// probe_arg<Consumer>: discovery's placeholder. The operators are only declared: discovery asks
// whether a construction is well-formed and never evaluates one.
//
// Why each operator has its cv-qualifier (S1 §3.1, ADR-0003 item 1): for a by-value parameter
// (X, X*, std::unique_ptr<X>, std::function<...>, ...) both templates are candidates and partial
// ordering can't separate T from T&. The implicit object parameter decides: the placeholder is an
// rvalue, and binding it to a non-const object parameter beats binding it to a const one
// ([over.ics.rank]/3.2.6), so operator T() wins. For X& and const X&, only an lvalue-yielding
// conversion function is a candidate ([dcl.init.ref]/5.1.2, [over.match.ref]), so operator T&()
// wins, with T deduced as const X for const X&.
template <class Consumer>
struct probe_arg {
  template <class T>
    requires not_self<Consumer, T>
  operator T();
  template <class T>
    requires not_self<Consumer, T>
  operator T&() const;
#if TRELLIS_PROBE_DECLARES_TWINS
  template <class T>
    requires not_self<Consumer, T> && twin_tag_request<T>
  operator T() = delete;
  template <class T>
    requires not_self<Consumer, T> && twin_tag_request<T>
  operator T&() const = delete;
#endif
};

// inert_arg converts to nothing. A constructor that accepts it at the discovered arity is a
// constructor template or a C variadic constructor, which would accept any argument: "greedy",
// TRL-E004 (S1 §3.5).
struct inert_arg {};

// any_arg<Consumer, Ctx>: resolution's placeholder, S1's operator set exactly (ADR-0003 item 1).
// It has probe_arg's two shapes, so it selects the same constructor and the same operator for each
// parameter; the selected operator asks ctx for its request, T or T&. The calls are member calls,
// so argument-dependent lookup never searches the namespaces of Ctx or of the request.
//
// Ctx is the engine's context. Its member variable template Ctx::can_resolve<R> says whether it can
// serve the request R, which the engine memoizes per (key, request form, consumer lifetime) (plan
// §7.3, ADR-0014): Ctx stands for an injector and the lifetime of the object under construction,
// and R is the key and the form.
//
// The two real operators name Ctx::can_resolve<R> directly: through the concept can_resolve, Clang
// counts one more level of instantiation depth per hop of a dependency chain (NFR-09,
// docs/internals/deduction.md). The two deleted twins must use the concept. Where
// Ctx::can_resolve<R> doesn't exist for some R (a constrained or enable_if'd variable template),
// the atomic constraint !Ctx::can_resolve<R> fails to substitute and so is not satisfied, which
// would leave neither the real operator nor its twin viable and select the other operator: a
// temporary bound to a const X&. The concept-id can_resolve<Ctx, R> is then false instead, so
// !can_resolve<Ctx, R> holds and the twin is selected (tests/unit/deduced_test.cpp, sfinae_ctx).
//
// Why the deleted twins: constrained on can_resolve alone, an unresolvable request wouldn't fail,
// as overload resolution would pick the other operator. A const X& of a unique X would fall back
// to operator T() and bind a temporary that dies when the constructor returns, and a singleton
// requested by value would fall back to operator T&() and be copied. A deleted function still takes
// part in overload resolution, so the operator that matches the form stays selected, and the
// construction becomes ill-formed instead (S1 §3.1; stub_test.cpp shows both hazards).
template <class Ctx, class R>
concept can_resolve = Ctx::template can_resolve<R>;

// other_than<T, Consumer>: not_self<Consumer, T> with the type first, so that the real operators
// can state it as a type-constraint, other_than<Consumer> T. A template's associated constraints
// are a list, a type-constraint and then the requires-clause, which Clang checks in order, stopping
// at the first not satisfied, as it does a conjunction; but a conjunction in one requires-clause
// nests one more evaluation per operand, and the real operators' check of Ctx::can_resolve<R> is on
// every hop of a dependency chain: 0.6 KB of Clang 20's stack per hop, about 1 KB of Apple Clang's
// (docs/internals/deduction.md).
template <class T, class Consumer>
concept other_than = not_self<Consumer, T>;

template <class Consumer, class Ctx>
struct any_arg {
  Ctx& ctx;

  template <other_than<Consumer> T>
    requires(Ctx::template can_resolve<T>)
  operator T() {
    return ctx.template resolve<T>();
  }
  template <class T>
    requires not_self<Consumer, T> && (!can_resolve<Ctx, T>)
  operator T() = delete;

  template <other_than<Consumer> T>
    requires(Ctx::template can_resolve<T&>)
  operator T&() const {
    return ctx.template resolve<T&>();
  }
  template <class T>
    requires not_self<Consumer, T> && (!can_resolve<Ctx, T&>)
  operator T&() const = delete;
};

// ---- constructibility at one arity ------------------------------------------------------------

// repeat_t<I, A> is A for every I: it repeats a placeholder N times in a pack expansion over
// std::make_index_sequence<N>, which the compilers build with a builtin, so no arity recurses. On
// MSVC it goes through the class template repeat (TRELLIS_REPEAT_THROUGH_CLASS, config.hpp).
#if TRELLIS_REPEAT_THROUGH_CLASS
template <std::size_t, class A>
struct repeat {
  using type = A;
};
template <std::size_t I, class A>
using repeat_t = typename repeat<I, A>::type;
#else
template <std::size_t, class A>
using repeat_t = A;
#endif

// constructible_from_n<T, Arg, std::index_sequence<Is...>>: whether a T can be built from as many
// Arg as Is by the one construction rule, by which explicit declarations are checked too
// (ctor_traits.hpp, builds_from; ADR-0003 item 5): an aggregate with braces, so that each
// placeholder initializes one field, and anything else with parentheses, as is_constructible
// checks, so that an initializer_list constructor never captures the placeholders. A variable
// template, memoized per (T, Arg, arity).
template <class T, class Arg, class Indices>
inline constexpr bool constructible_from_n = false;
template <class T, class Arg, std::size_t... Is>
inline constexpr bool constructible_from_n<T, Arg, std::index_sequence<Is...>> =
    detail::builds_from<T, repeat_t<Is, Arg>...>();

// probes_at<T, N, MaxArity>: T is constructible from N probes, and N is within the bound. A
// concept, because a conjunction of constraints stops at the first one not satisfied: arities above
// the bound are never probed. A plain && in an if-constexpr condition would instantiate both sides.
template <class T, std::size_t N, std::size_t MaxArity>
concept probes_at =
    (N <= MaxArity) && constructible_from_n<T, probe_arg<T>, std::make_index_sequence<N>>;

// accepts_inert<T, Indices>: whether a T can be built from as many inert_arg as Indices has: with
// parentheses, also for an aggregate, except on MSVC (TRELLIS_AGGREGATE_CHECKS_WITH_BRACES,
// config.hpp). Parentheses never elide braces, so an aggregate accepts the inert arguments only if
// each of its deduced members accepts one directly, as a member whose type accepts anything does.
// With braces, an inert argument that converts to
// no member of aggregate class type was tried on that member's first member, and so on down, so
// an aggregate holding an aggregate that holds such a member was itself TRL-E004: deduction takes
// the member as one request, whose own deduction reports the member that accepts anything. The
// elision also nested one level of Clang's stack per aggregate below, which stopped Apple Clang's
// chain of aggregates before 200 hops (docs/internals/deduction.md).
template <class T, class Indices>
inline constexpr bool accepts_inert = false;
template <class T, std::size_t... Is>
inline constexpr bool accepts_inert<T, std::index_sequence<Is...>> =
#if TRELLIS_AGGREGATE_CHECKS_WITH_BRACES
    detail::builds_from<T, repeat_t<Is, inert_arg>...>();
#else
    TRELLIS_IS_CONSTRUCTIBLE(T, repeat_t<Is, inert_arg>...);
#endif

// greedy_at<T, N>: a constructor of T with N > 0 parameters, or N > 0 members of an aggregate T,
// accept N inert arguments.
template <class T, std::size_t N>
concept greedy_at = (N > 0) && accepts_inert<T, std::make_index_sequence<N>>;

// ---- the arity search -------------------------------------------------------------------------

inline constexpr std::size_t no_arity = static_cast<std::size_t>(-1);

// The flat, top-down search of S1 §3.4 (ADR-0003 item 5): the first arity whose probe succeeds
// wins, and the discarded branches of an if-constexpr chain are never instantiated, so a class with
// k parameters costs 17 - k probes and one instantiation level, where a recursive search would nest
// up to 17 (NFR-09). It is written out because a fold over the arities would probe all 17 (6-41%
// slower in S1), and recursion would nest. Two chains: arities 16 down to 0, and, for a bound above
// 16 only, 32 down to 17 and then the first. No branch is followed by code, which MSVC would report
// as unreachable (C4702, T1.13).
template <class T, std::size_t MaxArity>
consteval std::size_t search_up_to_16() {
  if constexpr (probes_at<T, 16, MaxArity>) {
    return 16;
  } else if constexpr (probes_at<T, 15, MaxArity>) {
    return 15;
  } else if constexpr (probes_at<T, 14, MaxArity>) {
    return 14;
  } else if constexpr (probes_at<T, 13, MaxArity>) {
    return 13;
  } else if constexpr (probes_at<T, 12, MaxArity>) {
    return 12;
  } else if constexpr (probes_at<T, 11, MaxArity>) {
    return 11;
  } else if constexpr (probes_at<T, 10, MaxArity>) {
    return 10;
  } else if constexpr (probes_at<T, 9, MaxArity>) {
    return 9;
  } else if constexpr (probes_at<T, 8, MaxArity>) {
    return 8;
  } else if constexpr (probes_at<T, 7, MaxArity>) {
    return 7;
  } else if constexpr (probes_at<T, 6, MaxArity>) {
    return 6;
  } else if constexpr (probes_at<T, 5, MaxArity>) {
    return 5;
  } else if constexpr (probes_at<T, 4, MaxArity>) {
    return 4;
  } else if constexpr (probes_at<T, 3, MaxArity>) {
    return 3;
  } else if constexpr (probes_at<T, 2, MaxArity>) {
    return 2;
  } else if constexpr (probes_at<T, 1, MaxArity>) {
    return 1;
  } else if constexpr (probes_at<T, 0, MaxArity>) {
    return 0;
  } else {
    return no_arity;
  }
}

template <class T, std::size_t MaxArity>
consteval std::size_t search_down_from_32() {
  if constexpr (probes_at<T, 32, MaxArity>) {
    return 32;
  } else if constexpr (probes_at<T, 31, MaxArity>) {
    return 31;
  } else if constexpr (probes_at<T, 30, MaxArity>) {
    return 30;
  } else if constexpr (probes_at<T, 29, MaxArity>) {
    return 29;
  } else if constexpr (probes_at<T, 28, MaxArity>) {
    return 28;
  } else if constexpr (probes_at<T, 27, MaxArity>) {
    return 27;
  } else if constexpr (probes_at<T, 26, MaxArity>) {
    return 26;
  } else if constexpr (probes_at<T, 25, MaxArity>) {
    return 25;
  } else if constexpr (probes_at<T, 24, MaxArity>) {
    return 24;
  } else if constexpr (probes_at<T, 23, MaxArity>) {
    return 23;
  } else if constexpr (probes_at<T, 22, MaxArity>) {
    return 22;
  } else if constexpr (probes_at<T, 21, MaxArity>) {
    return 21;
  } else if constexpr (probes_at<T, 20, MaxArity>) {
    return 20;
  } else if constexpr (probes_at<T, 19, MaxArity>) {
    return 19;
  } else if constexpr (probes_at<T, 18, MaxArity>) {
    return 18;
  } else if constexpr (probes_at<T, 17, MaxArity>) {
    return 17;
  } else {
    return detail::search_up_to_16<T, MaxArity>();
  }
}

template <class T, std::size_t MaxArity>
consteval std::size_t search_arity() {
  if constexpr (MaxArity > 16) {
    return detail::search_down_from_32<T, MaxArity>();
  } else {
    return detail::search_up_to_16<T, MaxArity>();
  }
}

// probes_above<T, MaxArity>: T is constructible from some number of probes in MaxArity + 1 to
// MaxArity + 16, so that a constructor just above the bound is TRL-E010 ("raise max_ctor_arity")
// rather than TRL-E002. Only reached on the failure path. A fold, of 16 operands at most.
template <class T, std::size_t MaxArity, std::size_t... Ks>
consteval bool probes_above(std::index_sequence<Ks...> /*offsets*/) {
  return (constructible_from_n<T, probe_arg<T>, std::make_index_sequence<MaxArity + 1 + Ks>> ||
          ...);
}

// ---- aggregates -------------------------------------------------------------------------------

// An aggregate is probed with braces, one placeholder per field (ADR-0003 item 5). Two things can
// make that count wrong, and deduce reports both, so that an aggregate deduces the same fields on
// every compiler or not at all.

// Array members. GCC and Clang elide the braces around an array member and give each element its
// own placeholder; MSVC initializes the whole array from one, through operator T&() with T = int[2]
// (S1's MSVC addendum, T1.13). no_array_arg is the probe without any conversion to an array
// reference: in parentheses, which never elide braces, it can't initialize an array member on any
// compiler. So an aggregate with an array member is not constructible from as many no_array_arg as
// it has deduced fields, whichever way the compiler counted them, while any other aggregate is,
// as parentheses then initialize the same fields as braces.
//
// On MSVC (TRELLIS_ARRAY_CHECK_WITH_BRACES, config.hpp) parentheses fail for members of aggregate
// class type too, so the check uses braces there, at MSVC's count, where each array is one field,
// and no_array_arg declares its conversion to an array reference deleted: MSVC selects it for an
// array member, which makes the initialization ill-formed, and never for anything else, where
// no_array_arg converts as the probe does.
template <class Consumer>
struct no_array_arg {
  template <class T>
    requires not_self<Consumer, T>
  operator T();
  template <class T>
    requires not_self<Consumer, T> && (!std::is_array_v<T>)
  operator T&() const;
#if TRELLIS_ARRAY_CHECK_WITH_BRACES
  template <class T>
    requires std::is_array_v<T>
  operator T&() const = delete;
#endif
#if TRELLIS_PROBE_DECLARES_TWINS
  template <class T>
    requires not_self<Consumer, T> && twin_tag_request<T>
  operator T() = delete;
  template <class T>
    requires not_self<Consumer, T> && twin_tag_request<T>
  operator T&() const = delete;
#endif
};

template <class T, class Indices>
inline constexpr bool arrays_free = false;
template <class T, std::size_t... Is>
inline constexpr bool arrays_free<T, std::index_sequence<Is...>> =
#if TRELLIS_ARRAY_CHECK_WITH_BRACES
    requires { T{std::declval<repeat_t<Is, no_array_arg<T>>>()...}; };
#else
    std::is_constructible_v<T, repeat_t<Is, no_array_arg<T>>...>;
#endif

// Members the probe can't initialize. Such a member (std::optional, std::tuple, std::any: S1
// §3.2 note c) ends the deduced field list, and the members after it would be value-initialized
// without a word. If T{probes..., {}} is well-formed, a field follows the deduced ones: the empty
// braces initialize it as it would be if left out, which T{probes...} already did.
template <class T, class Indices>
inline constexpr bool field_follows = false;
template <class T, std::size_t... Is>
inline constexpr bool field_follows<T, std::index_sequence<Is...>> =
    requires { T{std::declval<repeat_t<Is, probe_arg<T>>>()..., {}}; };

// The two checks for an aggregate with N deduced fields, as concepts, so that a type that isn't an
// aggregate stops at the first constraint and pays for neither.
template <class T, std::size_t N>
concept has_array_member = std::is_aggregate_v<T> && !arrays_free<T, std::make_index_sequence<N>>;
template <class T, std::size_t N>
concept has_undeducible_member =
    std::is_aggregate_v<T> && field_follows<T, std::make_index_sequence<N>>;

// ---- deduction --------------------------------------------------------------------------------

// complete_class<T>: T is a complete type here. Every probe and every type trait deduce uses
// (is_abstract, is_aggregate, is_constructible) needs a complete class, and an incomplete one gives
// dozens of errors in the standard library. Checked first, so that it is one TRL-E002 instead. Like
// any memoized answer about completeness, it holds from the first discovery of T on, which is why
// the message asks for the definition where T is discovered.
template <class T>
concept complete_class = requires { sizeof(T); };

// What deduction found for a type.
enum class deduction_status : unsigned char {
  deduced,             // the arity is known: one request per parameter
  not_a_class,         // fundamental, pointer, enumeration, union...: bind it instead
  incomplete,          // declared but not defined where discovery first looked at it
  abstract,            // never constructible: bind it to an implementation
  not_constructible,   // no public, non-deleted, unambiguous constructor within the bound
  greedy,              // a constructor template or variadic constructor accepts anything
  array_member,        // an aggregate with an array member
  undeducible_member,  // an aggregate with a member the probe can't initialize
  above_bound,         // a constructor exists only above the bound
};

struct deduction_result {
  deduction_status status;
  std::size_t arity;  // the discovered arity; 0 unless status is deduced, greedy or a member case
};

// deduce<T, MaxArity>(): S1's discovery (S1 §3.5, ADR-0003 item 5), each limit checked only where
// the step before it leaves the question open, so that a well-behaved class pays for the search
// and one inert probe, and an aggregate for two more checks.
template <class T, std::size_t MaxArity>
consteval deduction_result deduce() {
  static_assert(MaxArity <= deduced_arity_limit,
                "trellis: deduced discovery supports a max_ctor_arity of at most 32; declare "
                "longer constructors with trellis::ctor_traits");
  if constexpr (!std::is_class_v<T>) {
    return {deduction_status::not_a_class, 0};
  } else if constexpr (!complete_class<T>) {
    return {deduction_status::incomplete, 0};
  } else if constexpr (std::is_abstract_v<T>) {
    return {deduction_status::abstract, 0};
  } else if constexpr (constexpr std::size_t arity = detail::search_arity<T, MaxArity>();
                       arity == no_arity) {
    if constexpr (detail::probes_above<T, MaxArity>(std::make_index_sequence<16>{})) {
      return {deduction_status::above_bound, 0};
    } else {
      return {deduction_status::not_constructible, 0};
    }
  } else if constexpr (greedy_at<T, arity>) {
    return {deduction_status::greedy, arity};
  } else if constexpr (has_array_member<T, arity>) {
    return {deduction_status::array_member, arity};
  } else if constexpr (has_undeducible_member<T, arity>) {
    return {deduction_status::undeducible_member, arity};
  } else {
    return {deduction_status::deduced, arity};
  }
}

// deduction<T, MaxArity>: deduce's result for T, memoized per (T, MaxArity) like any class template
// specialization, so each type is probed once per bound. Unchecked: the engine's normal pass reads
// it without asserting (ADR-0006 item 2). T's cv-qualifiers are stripped, so const X and X share
// one deduce and one set of probes.
template <class T, std::size_t MaxArity = default_max_ctor_arity>
struct deduction {
  static constexpr deduction_result result = detail::deduce<std::remove_cv_t<T>, MaxArity>();
  static constexpr deduction_status status = result.status;
  static constexpr std::size_t arity = result.arity;
};

// ---- the discovery entry point ----------------------------------------------------------------

// deduced_ctor<T>(std::make_index_sequence<N>{}): ctor<probe_arg<T>, ...>, N times. Only named
// in discover's return statement.
template <class T, std::size_t... Is>
constexpr ctor<repeat_t<Is, probe_arg<T>>...> deduced_ctor(std::index_sequence<Is...> /*arity*/) {
  return {};
}

// deduction_message(status): the message for a deduction that found no constructor: TRL-E004 for
// an ambiguous constructor, TRL-E010 above the bound, TRL-E002 otherwise; none when it found one.
constexpr error_message deduction_message(deduction_status status) noexcept {
  switch (status) {
    case deduction_status::greedy:
      return error_message::greedy;
    case deduction_status::array_member:
      return error_message::array_member;
    case deduction_status::undeducible_member:
      return error_message::undeducible_member;
    case deduction_status::above_bound:
      return error_message::above_bound;
    case deduction_status::abstract:
      return error_message::abstract;
    case deduction_status::not_a_class:
      return error_message::not_a_class;
    case deduction_status::incomplete:
      return error_message::incomplete;
    case deduction_status::not_constructible:
      return error_message::not_constructible;
    default:
      return error_message::none;
  }
}

// declaration_report<T, Declared>: TRL-E002 for a declaration that doesn't match how a T will be
// built, naming it. A T that is incomplete here can't be checked, nor asked whether it is an
// aggregate: its report is the one deduction gives an incomplete class, so that one class is one
// report however its constructor is declared, by a binding, a ctor_traits specialization or not
// at all (T1.15).
template <class T, class Declared, bool Complete = complete_class<T>>
struct declaration_report {
  using error = trellis::error::E002<T, Declared>;
  static constexpr error_message message = declaration_message<T>;
};
template <class T, class Declared>
struct declaration_report<T, Declared, false> {
  using error = trellis::error::E002<T, no_ctor_declared>;
  static constexpr error_message message = error_message::incomplete;
};

// discovery_report<T, BindingCtor, MaxArity>: what discovery reports for T when it finds no
// constructor to call: the error type and the message. For a declaration, declaration_report; for
// a deduction, the message of its status, with TRL-E004's, TRL-E010's or TRL-E002's error type.
// discover, below, raises it, and so does the explain pass for a class whose constructor can't be
// found (diagnostics/explain.hpp): the same arguments, so one report however the class is
// reached. T has no cv-qualifiers.
template <class T, class BindingCtor, std::size_t MaxArity,
          class Declared = typename declared_ctor<T, BindingCtor>::type>
struct discovery_report : declaration_report<T, Declared> {};
template <class T, class BindingCtor, std::size_t MaxArity>
struct discovery_report<T, BindingCtor, MaxArity, no_ctor_declared> {
  static constexpr deduction_status status = deduction<T, MaxArity>::status;
  static constexpr error_message message = detail::deduction_message(status);
  using error = std::conditional_t<
      message == error_message::greedy || message == error_message::array_member ||
          message == error_message::undeducible_member,
      trellis::error::E004<T>,
      std::conditional_t<message == error_message::above_bound, trellis::error::E010<T, MaxArity>,
                         trellis::error::E002<T, no_ctor_declared>>>;
};

// discover<T, BindingCtor, MaxArity>(): the constructor declaration that discovery gives T,
// checked, with one report per root cause. A declaration (a binding's, a ctor_traits
// specialization or a member alias) wins, checked by explicit_ctor_t (TRL-E002); otherwise
// deduction decides, and a deduction that finds no constructor raises its report
// (discovery_report, above; raise, diagnostics/errors.hpp). Its return type is the result, so the
// entry point reads it through decltype, and the auto return type makes that instantiate the
// body, which holds the check. A failed assertion in a function body, unlike one in a class
// template, leaves nothing invalid behind: Clang reports a later use of a class whose static_assert
// failed again, as a second error. Memoized per (T, BindingCtor, MaxArity), so a type is reported
// once however often it is used.
template <class T, class BindingCtor, std::size_t MaxArity>
constexpr auto discover() {
  if constexpr (!TRELLIS_IS_SAME(typename declared_ctor<T, BindingCtor>::type, no_ctor_declared)) {
    return explicit_ctor_t<T, BindingCtor>{};
  } else if constexpr (deduction<T, MaxArity>::status == deduction_status::deduced) {
    return detail::deduced_ctor<T>(std::make_index_sequence<deduction<T, MaxArity>::arity>{});
  } else {
    using report = discovery_report<T, BindingCtor, MaxArity>;
    detail::raise<typename report::error, report::message>();
    return no_ctor_declared{};
  }
}

// discovered_ctor_t<T, BindingCtor, MaxArity>: the one entry point into constructor discovery, in
// every mode (plan §5.3 step 3.3): the declaration that applies, ctor<Args...> with the declared
// parameter types, or else the deduced ctor<probe_arg<T>, ...> with one placeholder per parameter.
// Checked: each mistake is one static_assert, TRL-E002 (a declaration that lies, or no constructor
// to deduce), TRL-E004 (an ambiguous constructor) or TRL-E010 (above the bound); after it the type
// is no_ctor_declared, so nothing else fails. MaxArity is the injector's max_ctor_arity, which
// create<T>() passes (T1.5). BindingCtor is for a binding's .with_ctor<Args...>(), as for
// explicit_ctor_t: create<T>() passes the declaration of the class's binding, or
// no_ctor_declared (static/resolve.hpp, served_binding; T1.15). cv-qualifiers are stripped.
template <class T, class BindingCtor = no_ctor_declared,
          std::size_t MaxArity = default_max_ctor_arity>
using discovered_ctor_t = decltype(detail::discover<std::remove_cv_t<T>, BindingCtor, MaxArity>());

// discoverable_v<T, BindingCtor, MaxArity>: whether discovered_ctor_t<T, BindingCtor, MaxArity>
// gives a constructor rather than an error. A plain constant, with no assertion, for the resolver's
// normal pass, which only asks (ADR-0006 item 2).
template <class T, class BindingCtor, std::size_t MaxArity>
consteval bool discoverable() {
  using declared = typename declared_ctor<T, BindingCtor>::type;
  if constexpr (!TRELLIS_IS_SAME(declared, no_ctor_declared)) {
    return ctor_matches_v<T, declared>;
  } else {
    return deduction<T, MaxArity>::status == deduction_status::deduced;
  }
}
template <class T, class BindingCtor = no_ctor_declared,
          std::size_t MaxArity = default_max_ctor_arity>
inline constexpr bool discoverable_v =
    detail::discoverable<std::remove_cv_t<T>, BindingCtor, MaxArity>();

// ---- building a deduced type ------------------------------------------------------------------

// deduced_constructible_v<T, Arity, Ctx>: whether T, at its deduced arity, is constructible with
// every parameter resolved by Ctx: the deleted twins make it false when a request can't be served.
// The engine's resolvable check for a deduced type (T1.5), so it sits on the resolution path once
// per hop, and each template between it and the check costs a level of instantiation depth per hop
// of a dependency chain (NFR-09, docs/internals/deduction.md). So it is itself the specialization
// that expands the arity and makes the check, through parameters that callers never pass: Class is
// T without cv-qualifiers, Indices the arity as a sequence, and Aggregate selects the check. For a
// class built with parentheses, the check is builds_from's construction rule (ctor_traits.hpp),
// written out rather than called: a call would be one more level per hop. An aggregate differs by
// design: builds_from uses braces, and this check parentheses, except on MSVC
// (TRELLIS_AGGREGATE_CHECKS_WITH_BRACES, config.hpp), as braces would elide into a member whose
// request fails, where the deleted twin is selected, and Clang would then build that member from
// its own members' requests. tests/unit/deduced_test.cpp checks that the two agree wherever no such
// member fails. Once every member's request resolves, the braces of construct_deduced initialize
// each member directly.
template <class T, std::size_t Arity, class Ctx, class Class = std::remove_cv_t<T>,
          class Indices = std::make_index_sequence<Arity>,
          bool Aggregate = std::is_aggregate_v<Class>>
inline constexpr bool deduced_constructible_v = false;
template <class T, std::size_t Arity, class Ctx, class Class, std::size_t... Is>
inline constexpr bool
    deduced_constructible_v<T, Arity, Ctx, Class, std::index_sequence<Is...>, true> =
#if TRELLIS_AGGREGATE_CHECKS_WITH_BRACES
        requires { Class{std::declval<repeat_t<Is, any_arg<Class, Ctx>>>()...}; };
#else
        std::is_constructible_v<Class, repeat_t<Is, any_arg<Class, Ctx>>...>;
#endif
template <class T, std::size_t Arity, class Ctx, class Class, std::size_t... Is>
inline constexpr bool
    deduced_constructible_v<T, Arity, Ctx, Class, std::index_sequence<Is...>, false> =
        std::is_constructible_v<Class, repeat_t<Is, any_arg<Class, Ctx>>...>;

TRELLIS_PLACEHOLDER_CONVERSIONS_BEGIN

// construct_deduced<T, Arity>(ctx): a T, without its cv-qualifiers, built from Arity resolving
// placeholders as discovery probed it: with braces for an aggregate, otherwise with parentheses. A
// prvalue, so T needn't be movable, and the engine can initialize storage from it directly. T must
// satisfy deduced_constructible_v.
template <class T, class Ctx, std::size_t... Is>
T construct_deduced_from(Ctx& ctx, std::index_sequence<Is...> /*arity*/) {
  if constexpr (std::is_aggregate_v<T>) {
    return T{repeat_t<Is, any_arg<T, Ctx>>{ctx}...};
  } else {
    return T(repeat_t<Is, any_arg<T, Ctx>>{ctx}...);
  }
}
template <class T, std::size_t Arity, class Ctx>
std::remove_cv_t<T> construct_deduced(Ctx& ctx) {
  return detail::construct_deduced_from<std::remove_cv_t<T>>(ctx,
                                                             std::make_index_sequence<Arity>{});
}

TRELLIS_PLACEHOLDER_CONVERSIONS_END

}  // namespace trellis::inline v1::detail

#endif  // TRELLIS_DISCOVERY_DEDUCED_HPP
