// Spike S1 prototype: deduced constructor discovery for Trellis (throwaway code).
//
// Two placeholders, with the same conversion-operator shapes:
//   probe_arg<C>       discovery only (unevaluated): finds the constructor arity.
//   any_arg<C, Ctx>    resolution: each operator asks Ctx for the request it deduced.
//
// The operator set (recommended by the spike, see docs/spikes/S1-deduced-discovery.md):
//   template <class T> operator T();          // non-const: value forms (T, T*, unique_ptr<T>, ...)
//   template <class T> operator T&() const;   // const: T& and const T& (T deduced as const U)
// The request a parameter makes is the return type of the operator that overload
// resolution selects: T for the value operator, T& for the reference operator.
#pragma once

#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>

#ifndef S1_MAX_ARITY
#define S1_MAX_ARITY 16
#endif
// Arity search strategy, for the cost measurements: 0 = flat if-constexpr chain
// (recommended), 1 = recursive partial specializations, 2 = fold over every arity.
#ifndef S1_SEARCH
#define S1_SEARCH 0
#endif
// gcc-13 picks operator T&() for an rvalue-reference parameter and then fails
// (fixed in gcc-14). Declaring operator T&&() in the *probe* lets discovery still
// find the right arity, so the failure becomes a resolution error rather than a
// silent switch to a shorter constructor. It is never called.
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 14
#define S1_GCC13_RREF_PROBE 1
#else
#define S1_GCC13_RREF_PROBE 0
#endif

namespace s1 {

inline constexpr std::size_t max_ctor_arity = S1_MAX_ARITY;
inline constexpr std::size_t npos = static_cast<std::size_t>(-1);

// not_self<Consumer, T>: a conversion target must not be the consumer itself.
// Why: with one argument, the consumer's copy and move constructors are viable
// through operator T&() (T = const C) and operator T() (T = C). They would make
// every copyable class look like it has a 1-parameter constructor, and make a
// real 1-parameter constructor ambiguous. Excluding the consumer removes them.
template <class Consumer, class T>
concept not_self = !std::same_as<std::remove_cvref_t<T>, Consumer>;

// rep<I, A> is A for every I; it repeats a placeholder N times in a pack expansion.
template <std::size_t, class A>
using rep = A;

// ---- discovery --------------------------------------------------------------

// Why each operator has its cv-qualifier: for a by-value parameter both
// templates are candidates and partial ordering cannot separate T from T&
// (only one is a reference). The implicit object parameter decides instead: the
// placeholder is an rvalue, and binding it to a non-const object parameter beats
// binding it to a const one ([over.ics.rank]/3.2.6), so operator T() wins. For a
// T& or const T& parameter only the reference operator is a candidate
// ([over.match.ref]), so it wins without any tie-break.
template <class Consumer>
struct probe_arg {
  template <class T>
    requires not_self<Consumer, T>
  operator T();
  template <class T>
    requires not_self<Consumer, T>
  operator T&() const;
#if S1_GCC13_RREF_PROBE
  template <class T>
    requires not_self<Consumer, T>
  operator T&&() const;
#endif
};

// inert_arg converts to nothing. A constructor that accepts it at the chosen
// arity is a constructor template or a C variadic constructor: "greedy".
struct inert_arg {};

template <class T, class... A>
concept brace_constructible = requires { T{std::declval<A>()...}; };

// Probe one arity. Aggregates use braces (std::is_aggregate: no user-declared
// constructor in C++20); everything else uses parentheses, so an
// initializer_list constructor can never capture the placeholders.
template <class T, class Seq, template <class> class Arg>
struct probe_seq;
template <class T, std::size_t... Is, template <class> class Arg>
struct probe_seq<T, std::index_sequence<Is...>, Arg> {
  static constexpr bool value = [] {
    if constexpr (std::is_aggregate_v<T>)
      return brace_constructible<T, rep<Is, Arg<T>>...>;
    else
      return std::is_constructible_v<T, rep<Is, Arg<T>>...>;
  }();
};
template <class>
using inert_for = inert_arg;

template <class T, std::size_t N>
concept probe_at = probe_seq<T, std::make_index_sequence<N>, probe_arg>::value;
template <class T, std::size_t N>
concept inert_at = probe_seq<T, std::make_index_sequence<N>, inert_for>::value;
// Conjunctions short-circuit, so arities above the limit are never probed.
template <class T, std::size_t N>
concept probe_ok = (N <= max_ctor_arity) && probe_at<T, N>;

// Largest arity in [0, max_ctor_arity] at which T is constructible from probes.
// Flat chain: discarded if-constexpr branches are not instantiated, so the search
// stops at the first hit (17 - arity probes) and adds one level of depth.
template <class T>
consteval std::size_t search_flat() {
  if constexpr (probe_ok<T, 16>)
    return 16;
  else if constexpr (probe_ok<T, 15>)
    return 15;
  else if constexpr (probe_ok<T, 14>)
    return 14;
  else if constexpr (probe_ok<T, 13>)
    return 13;
  else if constexpr (probe_ok<T, 12>)
    return 12;
  else if constexpr (probe_ok<T, 11>)
    return 11;
  else if constexpr (probe_ok<T, 10>)
    return 10;
  else if constexpr (probe_ok<T, 9>)
    return 9;
  else if constexpr (probe_ok<T, 8>)
    return 8;
  else if constexpr (probe_ok<T, 7>)
    return 7;
  else if constexpr (probe_ok<T, 6>)
    return 6;
  else if constexpr (probe_ok<T, 5>)
    return 5;
  else if constexpr (probe_ok<T, 4>)
    return 4;
  else if constexpr (probe_ok<T, 3>)
    return 3;
  else if constexpr (probe_ok<T, 2>)
    return 2;
  else if constexpr (probe_ok<T, 1>)
    return 1;
  else if constexpr (probe_ok<T, 0>)
    return 0;
  else
    return npos;
}
static_assert(max_ctor_arity <= 16, "the flat search is written out up to 16");

// Recursive alternative (one nested instantiation per arity tried).
template <class T, std::size_t N>
struct search_rec : search_rec<T, N - 1> {};
template <class T>
struct search_rec<T, 0> : std::integral_constant<std::size_t, probe_ok<T, 0> ? 0 : npos> {};
template <class T, std::size_t N>
  requires(N > 0 && probe_ok<T, N>)
struct search_rec<T, N> : std::integral_constant<std::size_t, N> {};

// Fold alternative: probes every arity (no short-circuit), constant depth.
template <class T, std::size_t... Ns>
consteval std::size_t search_fold(std::index_sequence<Ns...>) {
  constexpr bool ok[] = {probe_ok<T, Ns>...};
  for (std::size_t i = sizeof...(Ns); i-- > 0;)
    if (ok[i]) return i;
  return npos;
}

template <class T>
consteval std::size_t search_arity() {
#if S1_SEARCH == 0
  return search_flat<T>();
#elif S1_SEARCH == 1
  return search_rec<T, max_ctor_arity>::value;
#else
  return search_fold<T>(std::make_index_sequence<max_ctor_arity + 1>{});
#endif
}

// Only on the failure path: is there a constructor above the limit? Probes
// max+1 .. 2*max, so E010 can say "raise max_ctor_arity" instead of E002.
template <class T, std::size_t... Ks>
consteval bool viable_above_limit(std::index_sequence<Ks...>) {
  return (probe_at<T, max_ctor_arity + 1 + Ks> || ...);
}

enum class ctor_status {
  ok,                 // arity found, not greedy
  not_class,          // fundamental, pointer, enum...: must be bound (to_value)
  abstract,           // E001: no binding for an abstract type
  not_constructible,  // E002: no accessible, unambiguous constructor in [0, max]
  greedy,             // E004: a constructor template or C variadic accepts anything
  over_limit,         // E010: viable only above max_ctor_arity
};

struct discovery {
  ctor_status status;
  std::size_t arity;
  bool shorter_viable;  // arity - 1 also viable: defaulted parameter or overload
};

template <class T>
consteval discovery discover() {
  if constexpr (!std::is_class_v<T>)
    return {ctor_status::not_class, npos, false};
  else if constexpr (std::is_abstract_v<T>)
    return {ctor_status::abstract, npos, false};
  else {
    constexpr std::size_t n = search_arity<T>();
    if constexpr (n == npos) {
      if constexpr (viable_above_limit<T>(std::make_index_sequence<max_ctor_arity>{}))
        return {ctor_status::over_limit, npos, false};
      else
        return {ctor_status::not_constructible, npos, false};
    } else if constexpr (n > 0 && inert_at<T, n>)
      return {ctor_status::greedy, n, false};
    else if constexpr (n > 0)
      return {ctor_status::ok, n, probe_at<T, n - 1>};
    else
      return {ctor_status::ok, n, false};
  }
}

// ctor_info<T>: memoized per T (one class-template instantiation).
template <class T>
struct ctor_info {
  static constexpr discovery d = discover<T>();
  static constexpr ctor_status status = d.status;
  static constexpr std::size_t arity = d.arity;
  static constexpr bool shorter_viable = d.shorter_viable;
  static constexpr bool braces = std::is_aggregate_v<T>;
};

// ---- resolution -------------------------------------------------------------

// Ctx (an injector) answers, per request type R: can_resolve<R> and resolve<R>().
template <class Ctx, class R>
concept can_resolve = Ctx::template can_resolve<R>;

// any_arg: the same two operator shapes as probe_arg, so it selects the same
// constructor and the same operator per parameter. Each operator comes with a
// deleted twin for requests Ctx cannot satisfy.
//
// Why the deleted twins: if the operators were simply constrained on
// can_resolve, an unresolvable request would not fail; overload resolution would
// quietly pick the other operator. A const X& parameter whose X is unique would
// fall back to operator T() and bind a temporary that dies after the
// constructor (dangling), and a by-value X whose X is a singleton would fall back
// to operator T&() and copy it (sharing hidden). A deleted function still takes
// part in overload resolution, so the selected operator stays the one that
// matches the form, and selecting a deleted one makes the construction
// ill-formed: is_constructible reports false instead of resolving another way.
template <class Consumer, class Ctx>
struct any_arg {
  Ctx& ctx;

  template <class T>
    requires not_self<Consumer, T> && can_resolve<Ctx, T>
  operator T() {
    return ctx.template resolve<T>();
  }
  template <class T>
    requires not_self<Consumer, T> && (!can_resolve<Ctx, T>)
  operator T() = delete;

  template <class T>
    requires not_self<Consumer, T> && can_resolve<Ctx, T&>
  operator T&() const {
    return ctx.template resolve<T&>();
  }
  template <class T>
    requires not_self<Consumer, T> && (!can_resolve<Ctx, T&>)
  operator T&() const = delete;
};

// Is T constructible with every parameter resolved by Ctx, at its discovered arity?
template <class T, class Ctx, class Seq>
struct resolvable_seq;
template <class T, class Ctx, std::size_t... Is>
struct resolvable_seq<T, Ctx, std::index_sequence<Is...>> {
  static constexpr bool value = [] {
    if constexpr (std::is_aggregate_v<T>)
      return brace_constructible<T, rep<Is, any_arg<T, Ctx>>...>;
    else
      return std::is_constructible_v<T, rep<Is, any_arg<T, Ctx>>...>;
  }();
};
template <class T, class Ctx>
consteval bool constructible_from() {
  if constexpr (ctor_info<T>::status != ctor_status::ok)
    return false;
  else
    return resolvable_seq<T, Ctx, std::make_index_sequence<ctor_info<T>::arity>>::value;
}

// GCC's -Wconversion reports every by-value conversion below ("choosing
// operator T() over operator T&() const ... because conversion sequence for the
// argument is better"): the tie-break between the two operators is exactly the
// object-parameter rule this design relies on. The diagnostic is about the
// placeholder, not user code, so it is silenced here and only here (Trellis
// would put this pair of pragmas behind macros in config.hpp).
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif
// Build a T, every parameter resolved by ctx. A prvalue, so non-movable types work.
template <class T, class Ctx, std::size_t... Is>
T construct_seq(Ctx& ctx, std::index_sequence<Is...>) {
  if constexpr (std::is_aggregate_v<T>)
    return T{rep<Is, any_arg<T, Ctx>>{ctx}...};
  else
    return T(rep<Is, any_arg<T, Ctx>>{ctx}...);
}
template <class T, class Ctx>
T construct(Ctx& ctx) {
  return construct_seq<T>(ctx, std::make_index_sequence<ctor_info<T>::arity>{});
}
template <class T, class Ctx, std::size_t... Is>
T* construct_new_seq(Ctx& ctx, std::index_sequence<Is...>) {
  if constexpr (std::is_aggregate_v<T>)
    return new T{rep<Is, any_arg<T, Ctx>>{ctx}...};
  else
    return new T(rep<Is, any_arg<T, Ctx>>{ctx}...);
}
template <class T, class Ctx>
T* construct_new(Ctx& ctx) {
  return construct_new_seq<T>(ctx, std::make_index_sequence<ctor_info<T>::arity>{});
}

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

}  // namespace s1
