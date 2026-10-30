// Spike S2 prototype: the S1 injector stub with interface bindings and three
// strategies for the check pass (throwaway code).
//
//   struct app : s2::map<s2::singleton<ILog, Log>, s2::unique<IRepo, Repo>> {};
//   s2::injector<app> inj;
//   App a = inj.create<App>();
//
// Discovery is spike S1's, unchanged (deduce.hpp: probe_arg and ctor_info<T>,
// memoized per class). Resolution has two passes:
//
//   check     is request R, made by a consumer of lifetime L, resolvable? The
//             recursion runs through the constraints of the conversion operators
//             of check_arg, S1's operator set with deleted twins. S2_STRATEGY
//             picks what the recursion carries, and so what the compiler memoizes:
//               0 memo   nothing: one node per (impl, consumer lifetime) and one
//                        satisfaction per (request, consumer lifetime) (plan 7.3)
//               1 depth  the hop count D: one node per (impl, lifetime, D). At
//                        D > S2_MAX_DEPTH it reports TRL-E007 (option 1 of 7.3)
//               2 path   the classes on the path: one node per (impl, lifetime,
//                        path); a class already on the path reports TRL-E007
//   creation  make_arg: the same two operator shapes without constraints, so
//             overload resolution picks the same operator as the check did, and
//             the creation code is identical under every strategy. It runs only
//             after the check passed, so it needs no checks of its own.
//
// Request forms (plan 6.3 subset, as benchmarks/compile/gen_graph.py generates
// them): T (unique, concrete), T& / const T& / T* / const T* (singleton),
// unique_ptr<T> (unique), shared_ptr<T> (singleton; or unique when the consumer
// is unique: a singleton consumer holding a unique one is the captive rule, E006).
// An unbound concrete class is unique, as in S1.
//
// Instantiation depth: a hop of the check is check_arg's operator deduction ->
// its constraint's satisfaction -> the next node (ctor_ok, a variable template
// partial specialization) -> __is_constructible's overload resolution. No
// constexpr function call stays open across a hop, and every chain the spike
// measured stops at -ftemplate-depth, never at -fconstexpr-depth. The creation
// code nests too: GCC and Clang instantiate a function body where it is first
// used, inside the body that uses it, so construct -> operator T -> resolve is
// 3 levels per hop by value and 4 through instance() (spike S2, section 3.3).
#pragma once

#include <cstddef>
#include <exception>
#include <memory>
#include <type_traits>
#include <utility>

#include "deduce.hpp"  // spike S1

#ifndef S2_STRATEGY
#define S2_STRATEGY 0
#endif
#ifndef S2_MAX_DEPTH
#define S2_MAX_DEPTH 1000000  // hops; the depth strategy's bound
#endif

namespace s2 {

using s1::ctor_info;
using s1::ctor_status;
using s1::not_self;
using s1::rep;

enum class life { unique, singleton };

template <class Key, class Impl = Key>
struct singleton {
  using key = Key;
  using impl = Impl;
  static constexpr life lifetime = life::singleton;
};
template <class Key, class Impl = Key>
struct unique {
  using key = Key;
  using impl = Impl;
  static constexpr life lifetime = life::unique;
};

// ---- the binding map ----------------------------------------------------------
// One base class per binding; a lookup is overload resolution against the bases
// (plan 8.1 C2), so it costs O(1) instantiations, not a recursive search. Users
// derive a named struct from map<...> (struct app : map<...> {}), so that the
// names of everything instantiated per node stay short (plan 8.1 C5).
template <class Key, class Binding>
struct entry {};
template <class... Bindings>
struct map : entry<typename Bindings::key, Bindings>... {};

struct no_binding {};
// Derived-to-base beats conversion to const void* ([over.ics.rank]/4.4), so the
// first overload wins whenever the map has a base entry<Key, B>.
template <class Key, class Binding>
Binding find_binding(const entry<Key, Binding>*);
template <class Key>
no_binding find_binding(const void*);

template <class Map, class Key,
          class Found = decltype(find_binding<Key>(static_cast<const Map*>(nullptr)))>
struct binding_of {
  using type = Found;
};
template <class Map, class Key>
struct binding_of<Map, Key, no_binding> {  // unbound: a concrete class is unique
  using type =
      std::conditional_t<std::is_class_v<Key> && !std::is_abstract_v<Key>, unique<Key>, no_binding>;
};

// ---- requests ---------------------------------------------------------------------
enum class form { value, ref, ptr, unique_ptr, shared_ptr, none };

template <class R>
struct request {
  static constexpr form kind = std::is_class_v<R> ? form::value : form::none;
  using key = R;
};
template <class R>
struct request<R&> {
  static constexpr form kind = std::is_class_v<R> ? form::ref : form::none;
  using key = std::remove_cv_t<R>;
};
template <class R>
struct request<R*> {
  static constexpr form kind = std::is_class_v<R> ? form::ptr : form::none;
  using key = std::remove_cv_t<R>;
};
template <class R>
struct request<std::unique_ptr<R>> {
  static constexpr form kind = form::unique_ptr;
  using key = R;
};
template <class R>
struct request<std::shared_ptr<R>> {
  static constexpr form kind = form::shared_ptr;
  using key = R;
};

template <class Map, class R>
using binding_t = typename binding_of<Map, typename request<R>::key>::type;
template <class Impl>
using arity_seq = std::make_index_sequence<ctor_info<Impl>::arity>;

// The node's own check (S3's `self`): a binding exists, the form fits the
// target's lifetime (E005) and the consumer's (E006), and discovery found one
// constructor (E001, E002, E004, E010). The form is checked before ctor_info, so
// requests such as shared_ptr<I>& (the T& shape of a by-value shared_ptr
// parameter) never probe std::shared_ptr's constructors. Evaluated once per
// (request, consumer lifetime); it returns before the recursion starts.
template <class Map, class R, life Consumer>
consteval bool self_check() {
  using q = request<R>;
  if constexpr (q::kind == form::none) {
    return false;
  } else {
    using B = typename binding_of<Map, typename q::key>::type;
    if constexpr (std::is_same_v<B, no_binding>) {
      return false;
    } else {
      constexpr bool fits = [] {
        if constexpr (q::kind == form::ref || q::kind == form::ptr)
          return B::lifetime == life::singleton;
        else if constexpr (q::kind == form::value)
          return B::lifetime == life::unique && std::is_same_v<typename B::impl, typename q::key>;
        else if constexpr (q::kind == form::unique_ptr)
          return B::lifetime == life::unique;
        else
          return B::lifetime == life::singleton || Consumer == life::unique;
      }();
      if constexpr (!fits)
        return false;
      else
        return ctor_info<typename B::impl>::status == ctor_status::ok;
    }
  }
}

// ---- the check pass, one strategy per S2_STRATEGY -----------------------------
// Each strategy defines check_arg<Consumer, Map, L, State> and the concept
// can_take<Map, R, L, State>: "request R from a consumer of lifetime L, in
// recursion state State, is resolvable". The node, ctor_ok, is a variable
// template partial specialization on the arity's index_sequence, so that it can
// expand the placeholders without a function call or a further class template.
//
// Why the constraint-level && and ||: a conjunction or disjunction of atomic
// constraints short-circuits satisfaction, so the deeper node is not even named
// unless the earlier atoms pass. An && inside one atomic expression would
// instantiate both operands.

// Cycle reports (strategies 1 and 2). The strict pass raises TRL-E007 inside the
// check, where the cycle is found, and the node counts as unresolvable (GCC then
// reads value; Clang marks the specialization invalid). Either way the root
// check fails, so on failure create() runs the relaxed pass (State::relaxed),
// which treats the same nodes as resolvable without an error. If it passes, the
// cycle was the only cause and create() adds nothing: one static_assert per
// cause. The relaxed pass runs only on failure, like S3's explain pass.
// depth_limit_reached<Map> is keyed by the injector only, so it fires once
// whichever nodes reach the bound.
template <class Map>
struct depth_limit_reached {
  static_assert(sizeof(Map) == 0,
                "TRL-E007: dependency chain longer than S2_MAX_DEPTH hops: a dependency cycle?");
  static constexpr bool value = false;
};

template <bool Relaxed, class... Classes>
struct path {
  static constexpr bool relaxed = Relaxed;
};
template <class Path, class K>
struct push;
template <bool Relaxed, class... Ks, class K>
struct push<path<Relaxed, Ks...>, K> {
  using type = path<Relaxed, Ks..., K>;
};
template <class K, class Path>
inline constexpr bool on_path = false;
template <class K, bool Relaxed, class... Ks>
inline constexpr bool on_path<K, path<Relaxed, Ks...>> = (__is_same(K, Ks) || ...);

template <class Cycle>
struct cycle_found {
  // Cycle is the path from the root, ending with the repeated class.
  static_assert(sizeof(Cycle) == 0, "TRL-E007: dependency cycle (the path<...> argument lists it)");
  static constexpr bool value = false;
};

#if S2_STRATEGY == 0
struct state0 {};
template <class Consumer, class Map, life L, class State = state0>
struct check_arg;
template <class Map, class Impl, life L, class Seq>
inline constexpr bool ctor_ok = false;
template <class Map, class Impl, life L, std::size_t... Is>
inline constexpr bool ctor_ok<Map, Impl, L, std::index_sequence<Is...>> =
    __is_constructible(Impl, rep<Is, check_arg<Impl, Map, L>>...);

template <class Map, class R, life Consumer, class State = state0>
concept can_take = self_check<Map, R, Consumer>() &&
                   ctor_ok<Map, typename binding_t<Map, R>::impl, binding_t<Map, R>::lifetime,
                           arity_seq<typename binding_t<Map, R>::impl>>;
template <class>
using next_state = state0;
using root_state = state0;
using relaxed_root_state = state0;  // no cycle detection, so nothing to relax

#elif S2_STRATEGY == 1
template <class Consumer, class Map, life L, class State>
struct check_arg;
template <std::size_t D, bool Relaxed>
struct depth {
  static constexpr std::size_t value = D;
  static constexpr bool relaxed = Relaxed;
};
template <class Map, class Impl, life L, class D, class Seq>
inline constexpr bool ctor_ok = false;
template <class Map, class Impl, life L, class D, std::size_t... Is>
inline constexpr bool ctor_ok<Map, Impl, L, D, std::index_sequence<Is...>> =
    __is_constructible(Impl, rep<Is, check_arg<Impl, Map, L, D>>...);

template <class Map, class R, life Consumer, class D>
concept can_take = (D::value <= S2_MAX_DEPTH && self_check<Map, R, Consumer>() &&
                    ctor_ok<Map, typename binding_t<Map, R>::impl, binding_t<Map, R>::lifetime, D,
                            arity_seq<typename binding_t<Map, R>::impl>>) ||
                   (D::value > S2_MAX_DEPTH && (D::relaxed || depth_limit_reached<Map>::value));
template <class D>
using next_state = depth<D::value + 1, D::relaxed>;
using root_state = depth<0, false>;
using relaxed_root_state = depth<0, true>;

#elif S2_STRATEGY == 2
template <class Consumer, class Map, life L, class State>
struct check_arg;
template <class Map, class Impl, life L, class Path, class Seq>
inline constexpr bool ctor_ok = false;
template <class Map, class Impl, life L, class Path, std::size_t... Is>
inline constexpr bool ctor_ok<Map, Impl, L, Path, std::index_sequence<Is...>> =
    __is_constructible(Impl, rep<Is, check_arg<Impl, Map, L, typename push<Path, Impl>::type>>...);

template <class Map, class R, life Consumer, class Path>
concept can_take =
    (self_check<Map, R, Consumer>() && !on_path<typename binding_t<Map, R>::impl, Path> &&
     ctor_ok<Map, typename binding_t<Map, R>::impl, binding_t<Map, R>::lifetime, Path,
             arity_seq<typename binding_t<Map, R>::impl>>) ||
    (self_check<Map, R, Consumer>() && on_path<typename binding_t<Map, R>::impl, Path> &&
     (Path::relaxed ||
      cycle_found<typename push<Path, typename binding_t<Map, R>::impl>::type>::value));
template <class Path>
using next_state = Path;  // ctor_ok pushes the consumer
using root_state = path<false>;
using relaxed_root_state = path<true>;
#else
#error "S2_STRATEGY must be 0 (memo), 1 (depth) or 2 (path)"
#endif

// S1's operator set: non-const operator T() for value forms, const operator T&()
// for T& and const T&, each with a deleted twin (see deduce.hpp for why).
// Declared only: the check is unevaluated.
template <class Consumer, class Map, life L, class State>
struct check_arg {
  template <class T>
    requires not_self<Consumer, T> && can_take<Map, T, L, next_state<State>>
  operator T();
  template <class T>
    requires not_self<Consumer, T> && (!can_take<Map, T, L, next_state<State>>)
  operator T() = delete;
  template <class T>
    requires not_self<Consumer, T> && can_take<Map, T&, L, next_state<State>>
  operator T&() const;
  template <class T>
    requires not_self<Consumer, T> && (!can_take<Map, T&, L, next_state<State>>)
  operator T&() const = delete;
};

// ---- the creation pass ------------------------------------------------------------
template <class Consumer, class Injector>
struct make_arg {
  Injector& inj;
  template <class T>
    requires not_self<Consumer, T>
  operator T() {
    return inj.template resolve<T>();
  }
  template <class T>
    requires not_self<Consumer, T>
  operator T&() const {
    return inj.template resolve<T&>();
  }
};

template <class... Bindings>
map<Bindings...> as_map(const map<Bindings...>*);

template <class Impl>
struct slot {
  std::shared_ptr<Impl> ptr;  // a singleton, created on first use
};
template <class Map>
struct storage_of;
template <class... Bindings>
struct storage_of<map<Bindings...>> : slot<typename Bindings::impl>... {};

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"  // S1 3.3: the operator tie-break itself
#endif

template <class Map>
class injector {
  using storage = storage_of<decltype(as_map(static_cast<const Map*>(nullptr)))>;

 public:
  injector() = default;
  injector(const injector&) = delete;  // make_arg and the slots refer to this object
  injector& operator=(const injector&) = delete;

  template <class R>
  static constexpr bool resolvable = can_take<Map, R, life::unique, root_state>;
  template <class R>
  static constexpr bool resolvable_but_for_cycles =
      can_take<Map, R, life::unique, relaxed_root_state>;

  // The root request, made by the caller as a unique consumer. One static_assert
  // per root cause; the depth and path strategies report cycles inside the check.
  template <class R>
  R create() {
    if constexpr (resolvable<R>) {
      return resolve<R>();
    } else if constexpr (resolvable_but_for_cycles<R>) {
      std::terminate();  // TRL-E007 is already reported, by the check
    } else {
      static_assert(resolvable<R>, "TRL-E002: the request is not resolvable");
      std::terminate();
    }
  }

  template <class R>
  R resolve() {
    using q = request<R>;
    using B = binding_t<Map, R>;
    using Impl = typename B::impl;
    if constexpr (B::lifetime == life::singleton) {
      const std::shared_ptr<Impl>& p = instance<Impl>();
      if constexpr (q::kind == form::shared_ptr)
        return p;
      else if constexpr (q::kind == form::ptr)
        return p.get();
      else
        return *p;
    } else if constexpr (q::kind == form::unique_ptr || q::kind == form::shared_ptr) {
      return R(construct_new<Impl>(arity_seq<Impl>{}));
    } else {
      return construct<Impl>(arity_seq<Impl>{});
    }
  }

 private:
  template <class Impl>
  const std::shared_ptr<Impl>& instance() {
    std::shared_ptr<Impl>& p = static_cast<slot<Impl>&>(storage_).ptr;
    if (!p) p.reset(construct_new<Impl>(arity_seq<Impl>{}));
    return p;
  }
  template <class Impl, std::size_t... Is>
  Impl* construct_new(std::index_sequence<Is...>) {
    return new Impl(rep<Is, make_arg<Impl, injector>>{*this}...);
  }
  template <class Impl, std::size_t... Is>
  Impl construct(std::index_sequence<Is...>) {
    return Impl(rep<Is, make_arg<Impl, injector>>{*this}...);
  }

  storage storage_;
};

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

}  // namespace s2
