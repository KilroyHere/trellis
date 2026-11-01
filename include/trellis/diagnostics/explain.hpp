// Trellis diagnostics: the explain pass, which reports why create<T>() can't build its graph, once,
// with the path from the root (plan §5.3 step 5, §7.8, ADR-0006 item 5).
// Copyright (c) 2026 Aryan Patel
// SPDX-License-Identifier: BSL-1.0
//
// Internal (trellis::detail): the static engine's failure path, not user API.
// docs/internals/explain-pass.md explains each technique and lists its tests.
//
// create<R>() is `if constexpr (resolvable) { creation code } else { explain pass }` (ADR-0006
// item 2). The explain pass runs only in the second branch, so a graph that compiles never
// instantiates any of this, and no type of the normal pass holds a path (ADR-0014 item 1). It
// starts at the root request and follows the first failing request down, one class at a time:
//
// - At a class with a declared constructor, ctor<Args...>, it reads the memoized check of each
//   parameter, Ctx::can_resolve<Args>, and continues with the first that fails. These steps are
//   class templates (explain_stop), which finish before anything is reported, so they print no
//   instantiation note: a chain of declared classes costs the error nothing.
// - At a class whose constructor deduction found, the parameters can't be listed (ADR-0006,
//   "Deduced mode limits the explain pass"), but the first failing one can be found: the class is
//   probed with a resolving placeholder at one position and discovery's probes at the others
//   (failing_parameter). The pass then builds the class, in a function that is instantiated and
//   never called, from an explain_arg at that position and inert placeholders at the others.
//   Overload resolution selects the operator that discovery's probe selects, with the request the
//   parameter makes, and instantiates its body, which continues from that request and builds the
//   next deduced class itself: each deduced class costs one instantiation note, and the notes of a
//   chain come from one line of this file, which the compilers print once (see explain_arg).
// - At a request whose own plan fails (no binding, strict mode, an unsafe delete, the request-form
//   table, the copy, discovery), it raises that failure's report (diagnostics/errors.hpp, raise),
//   which names the error type.
// - At a class that is already on the path, a dependency cycle, it stops: the normal pass has
//   reported the cycle (ADR-0014 item 2).
// - A deduced class that takes std::unique_ptr of an incomplete class by value is the exception:
//   the pass reports that parameter, from an unevaluated construction (owner_reporter).
//
// The path, error::path<Consumers...>, grows by one class per step and is a template argument of
// every step, so the step that reports, explain_cause, whose instantiation note is the second
// below the error (Clang) or the first "required from" line (GCC), names the whole path from the
// root and the 1-based index of the failing parameter. The report itself is
// unique to (injector, request), not to the path: raise<Error, Message, Injector>, with Error
// naming the request. So a request that several roots, or several consumers, fail on is one error,
// as plan §7.8 and ADR-0006 promise ("exactly one static_assert per root cause"), and a path in the
// condition would make it one per root (amended by ADR-0006, 2026-11-01, T1.6).
//
// Cost (plan §7.8): one step per class on the failing path, and a read of each parameter's
// memoized check per class (one probe per parameter for a deduced class, and six for the
// lookahead): O(depth x arity), only after a failure.
#ifndef TRELLIS_DIAGNOSTICS_EXPLAIN_HPP
#define TRELLIS_DIAGNOSTICS_EXPLAIN_HPP

#include <trellis/config.hpp>
#include <trellis/diagnostics/errors.hpp>
#include <trellis/discovery/ctor_traits.hpp>
#include <trellis/discovery/deduced.hpp>
#include <trellis/dsl/lifetimes.hpp>
#include <trellis/dsl/module.hpp>
#include <trellis/meta/type_list.hpp>
#include <trellis/static/resolve.hpp>
#include <trellis/static/validate.hpp>

#include <array>
#include <cstddef>
#include <new>  // IWYU pragma: keep
#include <type_traits>
#include <utility>

namespace trellis::inline v1::detail {

// ---- what a request's own plan reports --------------------------------------------------------

// own_report<Injector, R>: what request R reports when its own plan rejects it (failure_of its
// node is not none): the error type, the message and the scope that raise() takes. Each is the
// report that the check which rejects R would raise itself: request_form_report for the
// request-form table (static/validate.hpp), discovery_report for a class whose constructor can't
// be found (discovery/deduced.hpp), so that create<T>(), T1.4's and discovery's entry points
// raise one report between them. A failure without a report (none, keys_repeat) reports nothing.
template <class Injector, class R, failure Reason = failure_of<typename plan<Injector, R>::node>>
struct own_report {
  using error = void;
  static constexpr error_message message = error_message::none;
  using scope = Injector;
};
template <class Injector, class R>
struct own_report<Injector, R, failure::request_form> : request_form_report<Injector, R> {
  using scope = Injector;
};
template <class Injector, class R>
struct own_report<Injector, R, failure::no_binding> {
  using error = trellis::error::E001<typename plan<Injector, R>::key, trellis::error::request<R>>;
  static constexpr error_message message = error_message::no_binding;
  using scope = Injector;
};
template <class Injector, class R>
struct own_report<Injector, R, failure::strict_unbound> {
  using error = trellis::error::E013<typename plan<Injector, R>::key, trellis::error::request<R>>;
  static constexpr error_message message = error_message::strict_unbound;
  using scope = Injector;
};
template <class Injector, class R>
struct own_report<Injector, R, failure::unsafe_delete> {
  using error = trellis::error::E014<typename plan<Injector, R>::key, trellis::error::request<R>,
                                     typename plan<Injector, R>::impl>;
  static constexpr error_message message = error_message::unsafe_delete;
  using scope = Injector;
};
template <class Injector, class R>
struct own_report<Injector, R, failure::no_implementation> {
  using error = trellis::error::E001<typename plan<Injector, R>::key, trellis::error::request<R>>;
  static constexpr error_message message = error_message::bound_to_itself;
  using scope = Injector;
};
template <class Injector, class R>
struct own_report<Injector, R, failure::copy> {
  using error = trellis::error::E002<typename plan<Injector, R>::key, trellis::error::request<R>>;
  static constexpr error_message message = error_message::copy;
  using scope = Injector;
};
// Discovery's report is unique to the class and the bound, not to the injector (scope void), as
// discovery raises it itself (discover, explicit_ctor_t).
template <class Injector, class R>
struct own_report<Injector, R, failure::discovery>
    : discovery_report<std::remove_cv_t<typename plan<Injector, R>::impl>, no_ctor_declared,
                       plan<Injector, R>::max_arity> {
  using scope = void;
};

// The steps that report, explain_cause and report_owner, pass an own_report's three parts to
// raise() themselves, in their own bodies, and on one line, which GCC 14 quotes in the error:
//   using report = own_report<Injector, R>;
//   detail::raise<typename report::error, report::message, typename report::scope>();
// A function that did it for them would be one more instantiation note in every error.

// last_consumer_t<Path>: the last class of a path, whose constructor makes the request a step
// explains. Only named for a path that isn't empty.
template <class Path>
struct last_consumer;
template <class... Consumers>
struct last_consumer<error::path<Consumers...>> {
  using type = at_t<sizeof...(Consumers) - 1, type_list<Consumers...>>;
};
template <class Path>
using last_consumer_t = typename last_consumer<Path>::type;

// ---- the first failing parameter --------------------------------------------------------------

// resolves_at<Impl, Ctx, K, Indices, Aggregate>: whether the deduced class Impl, built as
// discovery found it, from discovery's probe at every position but K and the resolving placeholder
// any_arg at K, is constructible: whether its parameter K can be served. The probe converts to
// anything, and any_arg is viable for anything too, through its deleted twins, so overload
// resolution selects the same constructor as discovery did and the same operator at every
// position; only the operator at K can be deleted, exactly when Ctx can't serve its request
// (discovery/deduced.hpp). Like node_v, a builtin for a class built with parentheses and a
// requires-expression for an aggregate built with braces (ADR-0003 item 5).
template <class Impl, class Ctx, std::size_t K, class Indices, bool Aggregate>
inline constexpr bool resolves_at = false;
template <class Impl, class Ctx, std::size_t K, std::size_t... Is>
inline constexpr bool resolves_at<Impl, Ctx, K, std::index_sequence<Is...>, false> =
    TRELLIS_IS_CONSTRUCTIBLE(Impl,
                             std::conditional_t<Is == K, any_arg<Impl, Ctx>, probe_arg<Impl>>...);
template <class Impl, class Ctx, std::size_t K, std::size_t... Is>
inline constexpr bool resolves_at<Impl, Ctx, K, std::index_sequence<Is...>, true> = requires {
  Impl{std::declval<std::conditional_t<Is == K, any_arg<Impl, Ctx>, probe_arg<Impl>>>()...};
};

// failing_parameter<Node>: the 1-based index of the first parameter of a failing class node that
// its context can't serve. For a declared constructor, a read of each parameter's memoized check;
// for a deduced one, one probe per parameter (resolves_at). Flat expansions and a scan (plan §8.1
// C3). A class node that fails has a parameter that fails: with every parameter resolvable, the
// constructor that discovery found is viable and still the best (the placeholders' constraints
// only remove candidates, never add one), so the node would hold.
template <class Node>
inline constexpr std::size_t failing_parameter = 0;
template <class Impl, class Ctx, std::size_t... Is, bool Aggregate>
inline constexpr std::size_t
    failing_parameter<deduced_node<Impl, Ctx, std::index_sequence<Is...>, Aggregate>> =
        detail::first_true(
            {!resolves_at<Impl, Ctx, Is, std::index_sequence<Is...>, Aggregate>...}) +
        1;
template <class Impl, class Ctx, bool Aggregate, class... Args>
inline constexpr std::size_t failing_parameter<declared_node<Impl, Ctx, Aggregate, Args...>> =
    detail::first_true({!Ctx::template can_resolve<Args>...}) + 1;

// ---- a by-value owner of an incomplete class --------------------------------------------------

// A deduced class that takes std::unique_ptr<X> by value, with X only declared (owns_incomplete,
// static/injector.hpp), can't be built here even from placeholders: the parameter's destructor
// asserts that X is complete, a second error beside the request's own (discovery's TRL-E002 for
// X; T1.5's review). Such a parameter always fails, so the pass reports it, rather than the first
// failing parameter, and names its request in an unevaluated construction, in which nothing is
// destroyed: the report is raised from the constraint of owner_reporter's operator, which overload
// resolution checks there too (explain_build). Its other failing parameters are reported once X is
// complete.

// owner_probe<Consumer>: discovery's probe, except that its by-value operator refuses an owner of
// an incomplete class. A by-value parameter of that type then has no viable conversion: operator
// T&() yields an lvalue, which the deleted copy constructor can't take. Only declared.
template <class Consumer>
struct owner_probe {
  template <class T>
    requires not_self<Consumer, T> && (!owns_incomplete<T>)
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

// owner_at<Impl, K, Indices, Aggregate>: the deduced class Impl's parameter K owns an incomplete
// class by value: Impl isn't constructible with the owner_probe at K and discovery's probe at the
// other positions. K == sizeof...(Is) puts the owner_probe everywhere: some parameter is one.
template <class Impl, std::size_t K, class Indices, bool Aggregate>
inline constexpr bool owner_at = false;
template <class Impl, std::size_t K, std::size_t... Is>
inline constexpr bool owner_at<Impl, K, std::index_sequence<Is...>, false> =
    !TRELLIS_IS_CONSTRUCTIBLE(
        Impl,
        std::conditional_t<Is == K || K == sizeof...(Is), owner_probe<Impl>, probe_arg<Impl>>...);
template <class Impl, std::size_t K, std::size_t... Is>
inline constexpr bool owner_at<Impl, K, std::index_sequence<Is...>, true> = !requires {
  Impl{std::declval<
      std::conditional_t<Is == K || K == sizeof...(Is), owner_probe<Impl>, probe_arg<Impl>>>()...};
};

// owner_parameter<Node>: the 1-based index of the first parameter of a deduced class node that
// owns an incomplete class by value, or 0 when none does: one probe for the class, and one per
// parameter only when it has such a parameter (Has). Always 0 for a declared constructor, whose
// parameters the pass never builds.
template <class Node>
inline constexpr bool has_owner = false;
template <class Impl, class Ctx, std::size_t... Is, bool Aggregate>
inline constexpr bool has_owner<deduced_node<Impl, Ctx, std::index_sequence<Is...>, Aggregate>> =
    owner_at<Impl, sizeof...(Is), std::index_sequence<Is...>, Aggregate>;

template <class Node, bool Has = has_owner<Node>>
inline constexpr std::size_t owner_parameter = 0;
template <class Impl, class Ctx, std::size_t... Is, bool Aggregate>
inline constexpr std::size_t
    owner_parameter<deduced_node<Impl, Ctx, std::index_sequence<Is...>, Aggregate>, true> =
        detail::first_true({owner_at<Impl, Is, std::index_sequence<Is...>, Aggregate>...}) + 1;

// report_owner<Injector, T>(): raises request T's own report, discovery's TRL-E002 for the
// incomplete class, when T owns one, and is true: owner_reporter's constraint.
template <class Injector, class T>
constexpr bool report_owner() noexcept {
  if constexpr (owns_incomplete<T>) {
    using report = own_report<Injector, T>;
    detail::raise<typename report::error, report::message, typename report::scope>();
  }
  return true;
}

// owner_reporter<Path, Parameter, Injector>: the placeholder at parameter Parameter of the last
// class of Path, which owns an incomplete class by value: its by-value operator's constraint
// raises that request's report. The probe's shapes, so that overload resolution deduces the
// request; only declared, as it is only ever used unevaluated.
template <class Path, std::size_t Parameter, class Injector>
struct owner_reporter {
  using consumer = last_consumer_t<Path>;
  template <class T>
    requires not_self<consumer, T> && (detail::report_owner<Injector, T>())
  operator T();
  template <class T>
    requires not_self<consumer, T>
  operator T&() const;
#if TRELLIS_PROBE_DECLARES_TWINS
  template <class T>
    requires not_self<consumer, T> && twin_tag_request<T>
  operator T() = delete;
  template <class T>
    requires not_self<consumer, T> && twin_tag_request<T>
  operator T&() const = delete;
#endif
};

// ---- the stops --------------------------------------------------------------------------------

// The steps that print an instantiation note in the error. Each names the path from the root to
// where it is, the 1-based index of the parameter it explains in the last class of the path (0 at
// the root), and, last because it is the longest name, the injector. That order puts the path
// first in every note.
template <class Path, std::size_t Parameter, class R, class Injector>
struct explain_cause;
template <class Path, std::size_t Parameter, class Injector, class Lifetime, class Indices,
          bool Aggregate>
struct explain_build;

// explain_nothing::run(): the step for a stop that reports nothing (explain_cycle).
struct explain_nothing {
  static void run() noexcept {}
};

// ---- dependency cycles ------------------------------------------------------------------------

// A dependency cycle is reported by the normal pass, with the compiler's errors (ADR-0014 item 2):
// the check of a request on the cycle re-enters itself, and its memoized value, Ctx::can_resolve,
// can't be read as a constant afterwards; every read is one more error. So the pass stops at a
// class that is already on the path, and at a class whose constructor requests one, before it
// reads its parameters' checks: these questions ask only which class each request's node builds.
// A request counts only when its node is a class node, deduced or declared: only then does the
// normal pass's check recurse into the class and meet the cycle. A request that its own plan
// rejects first (such as std::unique_ptr of a singleton) never recursed, so the compiler reported
// no cycle, and the pass must reach it and report it (T1.6 review).

// on_path<Impl, Path>: whether the class Impl is already on Path. A flat scan (plan §8.1 C3).
template <class Impl, class Path>
inline constexpr bool on_path = false;
template <class Impl, class... Consumers>
inline constexpr bool on_path<Impl, error::path<Consumers...>> =
    detail::first_true({TRELLIS_IS_SAME(Impl, Consumers)...}) < sizeof...(Consumers);

// node_class_t<Node>: the class that a deduced or declared class node builds; void for any other
// node, which builds nothing whose check could recurse: a request its own plan rejects, one a
// binding's object serves, or any request when the injector's keys repeat.
template <class Node>
struct node_class {
  using type = void;
};
template <class Impl, class Ctx, class Indices, bool Aggregate>
struct node_class<deduced_node<Impl, Ctx, Indices, Aggregate>> {
  using type = Impl;
};
template <class Impl, class Ctx, bool Aggregate, class... Args>
struct node_class<declared_node<Impl, Ctx, Aggregate, Args...>> {
  using type = Impl;
};
template <class Node>
using node_class_t = typename node_class<Node>::type;

// request_class_t<Injector, R>: the class whose check request R's check recurses into, or void.
template <class Injector, class R>
using request_class_t = node_class_t<typename plan<Injector, R>::node>;

// cycle_arg<Consumer, Injector, Path>: any_arg's operators, with "the request's node doesn't build
// a class on Path" in place of Ctx::can_resolve, so that the construction is ill-formed exactly
// when a parameter of the constructor discovery found makes such a request. Each operator asks it
// of both forms of its type, T and T&: overload resolution checks the constraints of both
// operators for a parameter by value, so the normal pass's check of the class recursed through
// either, and a request by value whose reference form is on a cycle has met it too (its own
// error, TRL-E005 for a singleton by value, comes once the cycle is gone). Only declared.
template <class Injector, class Path, class R>
concept off_path = !on_path<request_class_t<Injector, R>, Path> &&
                   !on_path<request_class_t<Injector, std::remove_reference_t<R>&>, Path>;

template <class Consumer, class Injector, class Path>
struct cycle_arg {
  template <class T>
    requires not_self<Consumer, T> && off_path<Injector, Path, T>
  operator T();
  template <class T>
    requires not_self<Consumer, T> && (!off_path<Injector, Path, T>)
  operator T() = delete;
  template <class T>
    requires not_self<Consumer, T> && off_path<Injector, Path, T&>
  operator T&() const;
  template <class T>
    requires not_self<Consumer, T> && (!off_path<Injector, Path, T&>)
  operator T&() const = delete;
};

// cycles_back<Impl, Injector, Path, Indices, Aggregate>: whether the deduced class Impl, the last
// class of Path, requests a class on Path: one probe, as node_v builds it.
template <class Impl, class Injector, class Path, class Indices, bool Aggregate>
inline constexpr bool cycles_back = false;
template <class Impl, class Injector, class Path, std::size_t... Is>
inline constexpr bool cycles_back<Impl, Injector, Path, std::index_sequence<Is...>, false> =
    !TRELLIS_IS_CONSTRUCTIBLE(Impl, repeat_t<Is, cycle_arg<Impl, Injector, Path>>...);
template <class Impl, class Injector, class Path, std::size_t... Is>
inline constexpr bool cycles_back<Impl, Injector, Path, std::index_sequence<Is...>, true> =
    !requires { Impl{std::declval<repeat_t<Is, cycle_arg<Impl, Injector, Path>>>()...}; };

// declared_cycles_back<Injector, Path, Args...>: the same for a declared constructor's requests.
template <class Injector, class Path, class... Args>
inline constexpr bool declared_cycles_back =
    detail::first_true({on_path<request_class_t<Injector, Args>, Path>...}) < sizeof...(Args);

// explain_stop<Path, Parameter, R, Ctx>: where the explain pass stops below request R, made in
// context Ctx as parameter Parameter of the last class of Path:
// - at R itself when its own plan rejects it: `reports` is true, and `type` is explain_cause;
// - at R's class when R's node is a deduced class, as only an instantiated construction names its
//   requests: explain_deduced, whose `type` is explain_build, which builds it;
// - through a declared class, at the stop of its first failing parameter, found by reading the
//   memoized checks;
// - at a class that is already on the path, or whose constructor requests one: a dependency
//   cycle (explain_cycle).
// `path` and `parameter` are the stop's: the path grows by the class at each class node. Class
// templates, which finish before anything that could report is instantiated, so a declared class
// prints no note. For a request that can be served, which overload resolution may look at (see
// explain_arg) but the pass never follows, it is a harmless explain_cause (Fails is false), so
// that it is well-formed for every request: a failing node always has a failing parameter.
template <class Path, std::size_t Parameter, class R, class Ctx,
          class Node = typename plan<typename Ctx::injector_type, R>::node,
          bool Fails = !node_v<Node>>
struct explain_stop {
  using path = Path;
  static constexpr std::size_t parameter = Parameter;
  using request = R;
  static constexpr bool reports = true;
  static constexpr bool cycle = false;
  using type = explain_cause<Path, Parameter, R, typename Ctx::injector_type>;
};

// explain_cycle<Path, Parameter, R>: the stop at a request on a dependency cycle (see above). The
// pass reports nothing more and ends: a path that grew around the cycle would make every step a
// new specialization and never end.
template <class Path, std::size_t Parameter, class R>
struct explain_cycle {
  using path = Path;
  static constexpr std::size_t parameter = Parameter;
  using request = R;
  static constexpr bool reports = true;
  static constexpr bool cycle = true;
  using type = explain_nothing;
};

// explain_deduced<Path, R, Injector, Lifetime, Indices, Aggregate>: the stop at a deduced class,
// the last class of Path, built in its own context from as many placeholders as Indices has
// elements, with braces if Aggregate. `parameter` is the first failing parameter, or a parameter
// that owns an incomplete class by value, which the class can't be built with (`owner`, see
// owner_reporter).
template <class Path, class R, class Injector, class Lifetime, class Indices, bool Aggregate>
struct explain_deduced {
  using impl = last_consumer_t<Path>;
  using context_type = context<Injector, Lifetime>;
  using indices = Indices;
  using node = deduced_node<impl, context_type, Indices, Aggregate>;
  using path = Path;
  static constexpr bool owner = owner_parameter<node> != 0;
  static constexpr std::size_t parameter = owner ? owner_parameter<node> : failing_parameter<node>;
  static constexpr bool aggregate = Aggregate;
  using request = R;
  static constexpr bool reports = false;
  static constexpr bool cycle = false;
  using type = explain_build<Path, parameter, Injector, Lifetime, Indices, Aggregate>;
};

// explain_declared<Path, ImplCtx, Aggregate, Args...>: through a declared class, the last class
// of Path, to the stop below its first failing parameter. Its own class template, so that naming
// it reads no check.
template <class Path, class ImplCtx, bool Aggregate, class... Args>
struct explain_declared
    : explain_stop<
          Path,
          failing_parameter<declared_node<last_consumer_t<Path>, ImplCtx, Aggregate, Args...>>,
          at_t<
              failing_parameter<declared_node<last_consumer_t<Path>, ImplCtx, Aggregate, Args...>> -
                  1,
              type_list<Args...>>,
          ImplCtx> {};

template <class... Consumers, std::size_t Parameter, class R, class Ctx, class Impl, class Injector,
          class Lifetime, class Indices, bool Aggregate>
struct explain_stop<error::path<Consumers...>, Parameter, R, Ctx,
                    deduced_node<Impl, context<Injector, Lifetime>, Indices, Aggregate>, true>
    : std::conditional_t<
          on_path<Impl, error::path<Consumers...>> ||
              cycles_back<Impl, Injector, error::path<Consumers..., Impl>, Indices, Aggregate>,
          explain_cycle<error::path<Consumers...>, Parameter, R>,
          explain_deduced<error::path<Consumers..., Impl>, R, Injector, Lifetime, Indices,
                          Aggregate>> {};
template <class... Consumers, std::size_t Parameter, class R, class Ctx, class Impl, class ImplCtx,
          bool Aggregate, class... Args>
struct explain_stop<error::path<Consumers...>, Parameter, R, Ctx,
                    declared_node<Impl, ImplCtx, Aggregate, Args...>, true>
    : std::conditional_t<
          on_path<Impl, error::path<Consumers...>> ||
              declared_cycles_back<typename ImplCtx::injector_type, error::path<Consumers..., Impl>,
                                   Args...>,
          explain_cycle<error::path<Consumers...>, Parameter, R>,
          explain_declared<error::path<Consumers..., Impl>, ImplCtx, Aggregate, Args...>> {};

// explain_cause<Path, Parameter, R, Injector>::run(): reports request R's own failure. Its
// instantiation note, the second below the error, names the path from the root and the
// parameter: create<T>() runs it for a stop it reaches without a deduced class on the way
// (explain_root), and an explain_arg operator for the stop below its request.
template <class Path, std::size_t Parameter, class R, class Injector>
struct explain_cause {
  static void run() {
    using report = own_report<Injector, R>;
    detail::raise<typename report::error, report::message, typename report::scope>();
  }
};

// ---- what the next class needs: the lookahead -------------------------------------------------

// Each step builds the next deduced class itself (explain_arg), so it needs that class's arity as
// a pack before it knows the class: the arity is a property of the request its placeholder will
// be converted to, which only overload resolution finds. The lookahead finds it one class ahead,
// with the probes that find the failing parameter: the class whose placeholder it is, probed with
// discovery's probe everywhere but at the placeholder's position, where query_arg asks one bit of
// the stop's code at a time.

// stop_code<Stop>(): 0 for a stop at the request itself (a report or a cycle), else 1 + the arity
// of the deduced class it builds, at most 33 (max_ctor_arity is at most 32): six bits.
template <class Stop>
constexpr std::size_t stop_code() noexcept {
  if constexpr (Stop::reports) {
    return 0;
  } else {
    return 1 + Stop::indices::size();
  }
}
inline constexpr std::size_t stop_code_bits = 6;

// stop_bit<Path, Parameter, Ctx, Bit>::value<R>: bit Bit of the stop code below request R, made
// as parameter Parameter of the last class of Path in context Ctx.
template <class Path, std::size_t Parameter, class Ctx, std::size_t Bit>
struct stop_bit {
  template <class R>
  static constexpr bool value =
      ((detail::stop_code<explain_stop<Path, Parameter, R, Ctx>>() >> Bit) & 1U) != 0;
};

// query_arg<Consumer, Query>: any_arg's operators, with Query::value<R> in place of
// Ctx::can_resolve<R>: one of each pair is viable for any request, the deleted twin where the
// query is false, so overload resolution selects discovery's constructor and the operator that
// discovery's probe selects, and the construction is ill-formed exactly when the query is false
// for the request that operator deduces. Only declared, as it is only used unevaluated.
template <class Query, class R>
concept query_holds = Query::template value<R>;

template <class Consumer, class Query>
struct query_arg {
  template <class T>
    requires not_self<Consumer, T> && (Query::template value<T>)
  operator T();
  template <class T>
    requires not_self<Consumer, T> && (!query_holds<Query, T>)
  operator T() = delete;
  template <class T>
    requires not_self<Consumer, T> && (Query::template value<T&>)
  operator T&() const;
  template <class T>
    requires not_self<Consumer, T> && (!query_holds<Query, T&>)
  operator T&() const = delete;
};

// query_at<Impl, Indices, Aggregate, K, Query>: whether Query holds for the request that the
// deduced class Impl's parameter K (1-based) makes: Impl built as discovery found it, with
// query_arg at K and discovery's probe elsewhere (as resolves_at, with braces for an aggregate).
template <class Impl, class Indices, bool Aggregate, std::size_t K, class Query>
inline constexpr bool query_at = false;
template <class Impl, std::size_t... Is, std::size_t K, class Query>
inline constexpr bool query_at<Impl, std::index_sequence<Is...>, false, K, Query> =
    TRELLIS_IS_CONSTRUCTIBLE(
        Impl, std::conditional_t<Is + 1 == K, query_arg<Impl, Query>, probe_arg<Impl>>...);
template <class Impl, std::size_t... Is, std::size_t K, class Query>
inline constexpr bool query_at<Impl, std::index_sequence<Is...>, true, K, Query> = requires {
  Impl{std::declval<std::conditional_t<Is + 1 == K, query_arg<Impl, Query>, probe_arg<Impl>>>()...};
};

// next_indices_t<Path, Parameter, Ctx, Indices, Aggregate>: std::make_index_sequence of the arity
// of the deduced class at the stop below parameter Parameter of the last class of Path (a deduced
// class with Indices and Aggregate, in context Ctx), or an empty sequence when the pass stops at
// that request. Six probes of the class, one per bit, flat.
template <class Path, std::size_t Parameter, class Ctx, class Indices, bool Aggregate,
          class Bits = std::make_index_sequence<stop_code_bits>>
struct next_indices;
template <class Path, std::size_t Parameter, class Ctx, class Indices, bool Aggregate,
          std::size_t... Bits>
struct next_indices<Path, Parameter, Ctx, Indices, Aggregate, std::index_sequence<Bits...>> {
  static constexpr std::size_t code = (std::size_t{0} | ... |
                                       (query_at<last_consumer_t<Path>, Indices, Aggregate,
                                                 Parameter, stop_bit<Path, Parameter, Ctx, Bits>>
                                            ? std::size_t{1} << Bits
                                            : std::size_t{0}));
  using type = std::make_index_sequence<code == 0 ? 0 : code - 1>;
};
template <class Path, std::size_t Parameter, class Ctx, class Indices, bool Aggregate>
using next_indices_t = typename next_indices<Path, Parameter, Ctx, Indices, Aggregate>::type;

// ---- the placeholders -------------------------------------------------------------------------

template <class Path, std::size_t Parameter, class Ctx, class Next>
struct explain_arg;

// explain_filler<Consumer>: the placeholder at every other position of a class the pass builds:
// the probe's shapes, so that overload resolution doesn't change, with bodies that do nothing, as
// the construction, never run, instantiates the operator it selects at each position.
template <class Consumer>
struct explain_filler {
  template <class T>
    requires not_self<Consumer, T>
  operator T() {
    detail::unreachable();
  }
  template <class T>
    requires not_self<Consumer, T>
  operator T&() const {
    detail::unreachable();
  }
#if TRELLIS_PROBE_DECLARES_TWINS
  template <class T>
    requires not_self<Consumer, T> && twin_tag_request<T>
  operator T() = delete;
  template <class T>
    requires not_self<Consumer, T> && twin_tag_request<T>
  operator T&() const = delete;
#endif
};

// explain_placeholder_t<I, Path, Parameter, Ctx, Indices, Aggregate>: the placeholder at position
// I (0-based) of the last class of Path, a deduced class with Indices and Aggregate built in
// context Ctx: an explain_arg, with the lookahead's sequence, at the position of the failing
// parameter (I + 1 == Parameter), an explain_filler elsewhere.
template <std::size_t I, class Path, std::size_t Parameter, class Ctx, class Indices,
          bool Aggregate>
using explain_placeholder_t = std::conditional_t<
    I + 1 == Parameter,
    explain_arg<Path, Parameter, Ctx, next_indices_t<Path, Parameter, Ctx, Indices, Aggregate>>,
    explain_filler<last_consumer_t<Path>>>;

// owner_placeholder_t<I, Path, Parameter, Injector>: an owner_reporter at the position of the
// parameter that owns an incomplete class by value, discovery's probe elsewhere.
template <std::size_t I, class Path, std::size_t Parameter, class Injector>
using owner_placeholder_t =
    std::conditional_t<I + 1 == Parameter, owner_reporter<Path, Parameter, Injector>,
                       probe_arg<last_consumer_t<Path>>>;

// explain_arg<Path, Parameter, Ctx, std::index_sequence<Js...>>: the placeholder that explains
// parameter Parameter of the last class of Path, a deduced class whose context is Ctx; Js... is
// the lookahead's sequence, the arity of the deduced class at the stop below its request. It has
// discovery's operator shapes (probe_arg), so overload resolution selects the operator, and
// deduces the request, that discovery's probe and the creation code's any_arg do. The build that
// holds it instantiates the selected operator's body, which asks Ctx::can_resolve for its request,
// as any_arg's constraint does, and continues from the request's stop when it fails: it runs the
// stop's explain_cause, which reports, or builds the stop's deduced class itself, from
// placeholders, with the pack Js... (At the failing parameter the request always fails; the
// check, memoized, is what lets tests/unit/resolve_forms_test.cpp see which request the operator
// deduced.)
//
// So each deduced class on the path costs one instantiation note, the operator's, and the
// operators of a chain are instantiated from the same construction in this class (one per
// operator shape, and braces for an aggregate): for a run of them, Clang and GCC 14 print the
// source line once, and GCC folds the run into one "recursively required from" line, which names
// the outermost operator of the run. That is why a report goes through explain_cause, whose note
// names the stop's path and parameter: the innermost operator's note, which would name them too,
// is folded away on GCC. Building the next class in another function would put the notes'
// locations in two functions in turn, which the compilers print in full, so that a deep chain
// would reach their backtrace limit of ten contexts at three lines each; for the same reason both
// operators write the dispatch out rather than call a common function. On GCC 13 it declares the
// probe's deleted twins too, for the reason config.hpp gives for TRELLIS_PROBE_DECLARES_TWINS.
TRELLIS_PLACEHOLDER_CONVERSIONS_BEGIN

template <class Path, std::size_t Parameter, class Ctx, std::size_t... Js>
struct explain_arg<Path, Parameter, Ctx, std::index_sequence<Js...>> {
  using consumer = last_consumer_t<Path>;
  using injector_type = typename Ctx::injector_type;
  template <class R>
  using stop = explain_stop<Path, Parameter, R, Ctx>;
  // The placeholders of the deduced class at stop S, at position J: an explain_arg or a filler,
  // or, for a class that owns an incomplete class by value, an owner_reporter or a probe.
  template <class S, std::size_t J>
  using next = explain_placeholder_t<J, typename S::path, S::parameter, typename S::context_type,
                                     typename S::indices, S::aggregate>;
  template <class S, std::size_t J>
  using owner = owner_placeholder_t<J, typename S::path, S::parameter, injector_type>;

  template <class T>
    requires not_self<consumer, T>
  operator T() {
    using s = stop<T>;
    // A cycle first, in a branch of its own: the normal pass has reported it, and its memoized
    // checks can't be read again (an operand of && is instantiated whatever the other's value).
    // Then nothing to explain for a request that can be served.
    if constexpr (!s::cycle) {
      if constexpr (!Ctx::template can_resolve<T>) {
        if constexpr (s::reports) {
          s::type::run();
        } else if constexpr (s::owner && s::aggregate) {
          // An alias rather than typename s::impl{...}: cl rejects a typename-specifier used as
          // a functional cast inside a requires-expression (C2760, 19.44 and 19.51).
          using impl = typename s::impl;
          static_cast<void>(requires { impl{owner<s, Js>{}...}; });
        } else if constexpr (s::owner) {
          using impl = typename s::impl;
          static_cast<void>(requires { impl(owner<s, Js>{}...); });
        } else {
          using impl = typename s::impl;
          alignas(impl) std::array<std::byte, sizeof(impl)> storage{};
          void* const where = storage.data();
          if constexpr (s::aggregate) {
            ::new (where) impl{next<s, Js>{}...};
          } else {
            ::new (where) impl(next<s, Js>{}...);
          }
        }
      }
    }
    detail::unreachable();
  }
  template <class T>
    requires not_self<consumer, T>
  operator T&() const {
    using s = stop<T&>;
    // A cycle first, in a branch of its own: the normal pass has reported it, and its memoized
    // checks can't be read again (an operand of && is instantiated whatever the other's value).
    // Then nothing to explain for a request that can be served.
    if constexpr (!s::cycle) {
      if constexpr (!Ctx::template can_resolve<T&>) {
        if constexpr (s::reports) {
          s::type::run();
        } else if constexpr (s::owner && s::aggregate) {
          // An alias rather than typename s::impl{...}: cl rejects a typename-specifier used as
          // a functional cast inside a requires-expression (C2760, 19.44 and 19.51).
          using impl = typename s::impl;
          static_cast<void>(requires { impl{owner<s, Js>{}...}; });
        } else if constexpr (s::owner) {
          using impl = typename s::impl;
          static_cast<void>(requires { impl(owner<s, Js>{}...); });
        } else {
          using impl = typename s::impl;
          alignas(impl) std::array<std::byte, sizeof(impl)> storage{};
          void* const where = storage.data();
          if constexpr (s::aggregate) {
            ::new (where) impl{next<s, Js>{}...};
          } else {
            ::new (where) impl(next<s, Js>{}...);
          }
        }
      }
    }
    detail::unreachable();
  }
#if TRELLIS_PROBE_DECLARES_TWINS
  template <class T>
    requires not_self<consumer, T> && twin_tag_request<T>
  operator T() = delete;
  template <class T>
    requires not_self<consumer, T> && twin_tag_request<T>
  operator T&() const = delete;
#endif
};

// explain_build<Path, Parameter, Injector, Lifetime, Indices, Aggregate>::run(): builds the last
// class of Path, the first deduced class below create<T>()'s request, as discovery found it, from
// placeholders, so that the operator selected for its failing parameter is instantiated: with
// parentheses, or braces for an aggregate (ADR-0003 item 5). It is instantiated, never called; the
// object goes into storage of its own size, so the class needn't be movable or destructible. The
// construction fits on one line, which GCC 14 quotes in the error. A class with a parameter that
// owns an incomplete class by value is only built unevaluated, with an owner_reporter at that
// parameter, which reports it (see owner_reporter): nothing is destroyed then. The classes below
// it are built by the explain_args, each in its own operator.
template <class Path, std::size_t Parameter, class Injector, class Lifetime, std::size_t... Is>
struct explain_build<Path, Parameter, Injector, Lifetime, std::index_sequence<Is...>, false> {
  using impl = last_consumer_t<Path>;
  using node = deduced_node<impl, context<Injector, Lifetime>, std::index_sequence<Is...>, false>;
  template <std::size_t I>
  using arg = explain_placeholder_t<I, Path, Parameter, context<Injector, Lifetime>,
                                    std::index_sequence<Is...>, false>;
  template <std::size_t I>
  using owner = owner_placeholder_t<I, Path, Parameter, Injector>;
  static void run() {
    if constexpr (owner_parameter<node> != 0) {
      static_cast<void>(requires { impl(owner<Is>{}...); });
    } else {
      alignas(impl) std::array<std::byte, sizeof(impl)> storage{};
      void* const where = storage.data();
      ::new (where) impl(arg<Is>{}...);
    }
  }
};

template <class Path, std::size_t Parameter, class Injector, class Lifetime, std::size_t... Is>
struct explain_build<Path, Parameter, Injector, Lifetime, std::index_sequence<Is...>, true> {
  using impl = last_consumer_t<Path>;
  using node = deduced_node<impl, context<Injector, Lifetime>, std::index_sequence<Is...>, true>;
  template <std::size_t I>
  using arg = explain_placeholder_t<I, Path, Parameter, context<Injector, Lifetime>,
                                    std::index_sequence<Is...>, true>;
  template <std::size_t I>
  using owner = owner_placeholder_t<I, Path, Parameter, Injector>;
  static void run() {
    if constexpr (owner_parameter<node> != 0) {
      static_cast<void>(requires { impl{owner<Is>{}...}; });
    } else {
      alignas(impl) std::array<std::byte, sizeof(impl)> storage{};
      void* const where = storage.data();
      ::new (where) impl{arg<Is>{}...};
    }
  }
};

TRELLIS_PLACEHOLDER_CONVERSIONS_END

// explain_root<Injector, R>::type: the first step of the explain pass for create<R>(), whose
// request is made by a unique consumer, the caller (static/injector.hpp). The nodes name contexts
// with checked access, so the pass uses one too, whatever the injector's access.
template <class Injector, class R>
struct explain_root {
  using type = typename explain_stop<error::path<>, 0, R, context<Injector, unique_t>>::type;
};

}  // namespace trellis::inline v1::detail

#endif  // TRELLIS_DIAGNOSTICS_EXPLAIN_HPP
