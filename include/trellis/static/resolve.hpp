// Trellis static engine: resolution and creation, the code behind injector::create<T>() (plan
// §5.3, §7.3, ADR-0006, ADR-0014).
// Copyright (c) 2026 Aryan Patel
// SPDX-License-Identifier: BSL-1.0
//
// Internal (trellis::detail): the static engine, not user API. docs/internals/
// resolution-and-storage.md explains each technique and lists its tests.
//
// create<R>() is `if constexpr (resolvable) { creation code } else { failure pass }` (ADR-0006
// item 2), from three pieces:
//
//   context<Injector, L>::can_resolve<R>  The check: whether request R, made by a consumer whose
//                                          lifetime is L, can be served. A variable template, so
//                                          the compiler evaluates it once per (injector, consumer
//                                          lifetime, request), which is plan §7.3's memoization
//                                          key: (key, request form, consumer lifetime). Neither a
//                                          path nor a depth is part of it (ADR-0014 item 1).
//   context<Injector, L>::resolve<R>()    The creation code. create<R>() instantiates it only
//                                          after the check passed, so a graph that fails
//                                          instantiates none of it, and nothing cascades.
//   explain<Injector, R>()                The failure pass: follows each failing request to its
//                                          cause and reports the cause once (T1.6 adds the path).
//
// plan<Injector, R> works out, once per (injector, request), what serves R: its binding, or the
// implicit unique binding of an unbound class (ADR-0004), the verdict of the request-form table
// (static/validate.hpp) and, for a class the injector builds, how its constructor is found
// (discovery/). The plan's node is what the check still has to evaluate, which is where the
// recursion through the graph happens: building the class with every constructor parameter
// resolved. Everything else in the plan finishes before the recursion starts, so the recursion
// nests only the node and the constructor's overload resolution per hop of a dependency chain:
// the lean shape of ADR-0014 item 3, which decides the longest chain that compiles (NFR-09).
//
// Deduced classes are checked and built with the resolving placeholder any_arg<Impl, Ctx>
// (discovery/deduced.hpp), whose conversion operators ask Ctx::can_resolve<T> and call
// ctx.resolve<T>(): Ctx is the context of the class being built, so its lifetime is its
// parameters' consumer lifetime. Declared classes resolve their declared parameter types.
#ifndef TRELLIS_STATIC_RESOLVE_HPP
#define TRELLIS_STATIC_RESOLVE_HPP

#include <trellis/config.hpp>
#include <trellis/diagnostics/errors.hpp>
#include <trellis/discovery/ctor_traits.hpp>
#include <trellis/discovery/deduced.hpp>
#include <trellis/dsl/bind.hpp>
#include <trellis/dsl/lifetimes.hpp>
#include <trellis/dsl/module.hpp>
#include <trellis/dsl/options.hpp>
#include <trellis/dsl/request_forms.hpp>
#include <trellis/meta/type_list.hpp>
#include <trellis/static/binding_map.hpp>
#include <trellis/static/injector.hpp>
#include <trellis/static/injector_traits.hpp>
#include <trellis/static/storage.hpp>
#include <trellis/static/validate.hpp>

#include <array>
#include <cstddef>
#include <memory>
#include <new>  // IWYU pragma: keep
#include <type_traits>
#include <utility>

namespace trellis::inline v1::detail {

// ---- what serves a request --------------------------------------------------------------------

// How the binding that serves a request provides its objects: the injector builds them
// (unique, singleton, singleton_inline, and the implicit unique binding of an unbound class), or
// the binding holds one: the caller's (.to_ref(), .to_ptr()), a shared one (.to_shared()) or the
// injector's copy (.to_value()).
enum class binding_kind : unsigned char { constructed, external, shared_external, value };

template <class Lifetime>
inline constexpr binding_kind kind_of = binding_kind::constructed;
template <>
inline constexpr binding_kind kind_of<external_t> = binding_kind::external;
template <>
inline constexpr binding_kind kind_of<shared_external_t> = binding_kind::shared_external;
template <>
inline constexpr binding_kind kind_of<value_t> = binding_kind::value;

// served_binding<Found>: what resolution needs to know about the binding that the binding map
// found for a key (binding_for_t): stored_binding<J, B>, or unbound<Key> for a key without one.
// impl is the class to build, or the type of the object the binding holds (const for a const
// external object). owned says whether the object lives in one of the injector's slots.
template <class Found>
struct served_binding;
template <std::size_t J, class Key, class Impl, class Lifetime, bool AllowCopy>
struct served_binding<stored_binding<J, binding<Key, Impl, Lifetime, AllowCopy>>> {
  static constexpr bool bound = true;
  static constexpr std::size_t position = J;
  using binding_type = binding<Key, Impl, Lifetime, AllowCopy>;
  using impl = Impl;
  using lifetime = Lifetime;
  static constexpr binding_kind kind = kind_of<Lifetime>;
  static constexpr bool owned = owns_object<binding_type>;
};
template <class Key>
struct served_binding<unbound<Key>> {
  static constexpr bool bound = false;
  static constexpr std::size_t position = 0;
  using binding_type = void;
  using impl = Key;
  using lifetime = unique_t;  // implicitly unique (ADR-0004)
  static constexpr binding_kind kind = binding_kind::constructed;
  static constexpr bool owned = false;
};

// ---- nodes: what the check evaluates ----------------------------------------------------------

// Why a request can't be served, as far as its own plan can tell, without looking at the
// dependencies. explain() reports each reason with its own static_assert.
enum class failure : unsigned char {
  none,               // the plan finds nothing wrong; the node decides
  keys_repeat,        // make_injector has reported a key bound twice (TRL-E003)
  request_form,       // the request-form table rejects the request (TRL-E005, TRL-E012)
  no_binding,         // an abstract type without a binding (TRL-E001)
  strict_unbound,     // an owning request of an unbound type, options{.strict = true} (TRL-E013)
  no_implementation,  // an abstract type bound to itself, bind<I> alone (TRL-E001)
  discovery,          // the class's constructor can't be found (TRL-E002, TRL-E004, TRL-E010)
  copy,           // a by-value request needs a copy the key can't be constructed from (TRL-E002)
  unsafe_delete,  // std::unique_ptr<Key> would delete a derived class through Key (TRL-E014)
};

// The node of a request that can't be served, with the reason.
template <failure Reason>
struct never_node {};
// The node of a request that a binding's object serves: nothing left to check.
struct always_node {};
// The node of a class that deduction found the constructor of: Impl built from as many
// any_arg<Impl, Ctx> as Indices has elements, with braces if Aggregate (ADR-0003 item 5). Ctx is
// the class's own context, whose lifetime is the class's binding's.
template <class Impl, class Ctx, class Indices, bool Aggregate>
struct deduced_node {};
// The node of a class with a declared constructor (ctor_traits.hpp): Impl built from the
// requests Args, as request_result_t normalizes them, with braces if Aggregate.
template <class Impl, class Ctx, bool Aggregate, class... Args>
struct declared_node {};

template <class Node>
inline constexpr failure failure_of = failure::none;
template <failure Reason>
inline constexpr failure failure_of<never_node<Reason>> = Reason;

// The node of a declared constructor ctor<Args...>. The requests are normalized as
// request_result_t does (X&& and const X are the request X), which they yield alike, so that
// X&& and X share one memoized check.
template <class Impl, class Ctx, class... Args>
constexpr declared_node<Impl, Ctx, std::is_aggregate_v<Impl>, request_result_t<Args>...>
declared_node_for(ctor<Args...> /*declared*/) noexcept {
  return {};
}

// The node of a class Impl that the injector builds and whose constructor discovery finds (the
// caller checked discoverable_v): its declaration if it has one, else its deduced arity.
template <class Impl, class Ctx, std::size_t MaxArity>
constexpr auto construction_node() noexcept {
  using declared = typename declared_ctor<Impl, no_ctor_declared>::type;
  if constexpr (TRELLIS_IS_SAME(declared, no_ctor_declared)) {
    return deduced_node<Impl, Ctx, std::make_index_sequence<deduction<Impl, MaxArity>::arity>,
                        std::is_aggregate_v<Impl>>{};
  } else {
    return detail::declared_node_for<Impl, Ctx>(declared{});
  }
}

// Whether a by-value request can be served from Impl: a copy of the shared object (Shared), the
// new object itself when it is the key, or a new Impl moved into a Key, which slices it, as any
// copy of a derived object as its base does (as for .to_value(), dsl/bind.hpp).
template <class Key, class Impl, bool Shared>
constexpr bool value_buildable() noexcept {
  if constexpr (Shared) {
    return std::is_constructible_v<Key, const Impl&>;
  } else if constexpr (TRELLIS_IS_SAME(Key, Impl)) {
    return true;
  } else {
    return std::is_constructible_v<Key, Impl>;
  }
}

// Whether a std::unique_ptr<Key> holding a new Impl would delete it through Key: Impl is a class
// derived from Key, and Key's destructor isn't virtual, so std::default_delete<Key> would be
// undefined behaviour ([expr.delete]/3), which no compiler warns about here. A std::shared_ptr<Key>
// is safe: its control block destroys the Impl it was made for. The trait is asked only for a
// derived Impl, so a key that is its own implementation needn't be complete here.
template <class Key, class Impl>
constexpr bool deletes_through_base() noexcept {
  if constexpr (TRELLIS_IS_SAME(Key, Impl)) {
    return false;
  } else {
    return !std::has_virtual_destructor_v<Key>;
  }
}

// select_node<Plan>(): the node of a plan, read through decltype. Each question is asked only
// once the ones before it are settled, in if-constexpr branches, so that a class with a missing
// binding is never probed, and none of this recursion: it finishes before the node is evaluated.
template <class Plan>
constexpr auto select_node() noexcept {
  using check = typename Plan::check;
  using served = typename Plan::served;
  using key = typename Plan::key;
  using impl = typename Plan::impl;
  constexpr request_form form = check::request::form;
  if constexpr (check::verdict == form_verdict::no_binding) {
    return never_node<failure::no_binding>{};
  } else if constexpr (!check::allowed) {
    return never_node<failure::request_form>{};
  } else if constexpr (Plan::opts.strict && !served::bound) {
    // Strict mode turns off the implicit unique binding (ADR-0004), so it rejects exactly the
    // requests that binding would serve: the owning ones. The others are TRL-E012 in any mode.
    return never_node<failure::strict_unbound>{};
  } else if constexpr (served::kind != binding_kind::constructed) {
    // The binding holds the object. A by-value request (allowed with .allow_copy(), and always
    // for a value binding) takes a copy of it, as const.
    if constexpr (form == request_form::value && !std::is_constructible_v<key, const impl&>) {
      return never_node<failure::copy>{};
    } else {
      return always_node{};
    }
  } else if constexpr (form == request_form::unique_ptr &&
                       detail::deletes_through_base<key, impl>()) {
    return never_node<failure::unsafe_delete>{};
  } else if constexpr (!complete_class<impl>) {
    return never_node<failure::discovery>{};  // discovery reports the incomplete class
  } else if constexpr (std::is_abstract_v<impl>) {
    return never_node<failure::no_implementation>{};
  } else if constexpr (!discoverable_v<impl, no_ctor_declared, Plan::max_arity>) {
    return never_node<failure::discovery>{};
  } else if constexpr (form == request_form::value &&
                       !detail::value_buildable<key, impl, served::owned>()) {
    return never_node<failure::copy>{};
  } else {
    return detail::construction_node<impl, typename Plan::next, Plan::max_arity>();
  }
}

// plan<Injector, R>: what serves request R of the injector, worked out once per (injector,
// request). next is the context in which the built class's own parameters are requested: its
// binding's lifetime is their consumer lifetime. When the injector's keys repeat, make_injector
// has reported TRL-E003, nothing resolves, and nothing more is reported.
template <class Injector, class R, bool KeysRepeat = injector_traits<Injector>::has_duplicate_keys>
struct plan {
  using node = never_node<failure::keys_repeat>;
};
template <class Injector, class R>
struct plan<Injector, R, false> {
  static constexpr options opts = injector_traits<Injector>::opts;
  static constexpr std::size_t max_arity = effective_max_ctor_arity<opts>;
  using check = request_check<Injector, R>;
  using key = typename check::key;
  using served = served_binding<typename check::found>;
  using impl = typename served::impl;
  using next = context<Injector, typename served::lifetime>;
  using node = decltype(detail::select_node<plan>());
};

// node_v<Node>: whether a node can be built; the recursion of the check. Each specialization
// evaluates its node directly, with no class template or function call between one request and
// the requests of the built class's constructor: the deduced node asks the __is_constructible
// builtin (TRELLIS_IS_CONSTRUCTIBLE), whose overload resolution checks the placeholders'
// constraints, Ctx::can_resolve<T>, which is the next hop's node_v. So a hop of a dependency chain
// costs the variable template can_resolve<R>, this one, and the constraint check (ADR-0014 item 3;
// docs/internals/resolution-and-storage.md has the measurements). The two deduced specializations
// are discovery/deduced.hpp's deduced_constructible_v with the builtin, which saves a level per
// hop; tests/unit/create_test.cpp checks that they agree.
template <class Node>
inline constexpr bool node_v = false;
template <>
inline constexpr bool node_v<always_node> = true;
template <class Impl, class Ctx, std::size_t... Is>
inline constexpr bool node_v<deduced_node<Impl, Ctx, std::index_sequence<Is...>, false>> =
    TRELLIS_IS_CONSTRUCTIBLE(Impl, repeat_t<Is, any_arg<Impl, Ctx>>...);
template <class Impl, class Ctx, std::size_t... Is>
inline constexpr bool node_v<deduced_node<Impl, Ctx, std::index_sequence<Is...>, true>> =
    requires { Impl{std::declval<repeat_t<Is, any_arg<Impl, Ctx>>>()...}; };
// A declared constructor: every declared request resolves. A flat expansion and a scan rather
// than a fold, as a declaration may list any number of parameters (plan §8.1 C3).
template <class Impl, class Ctx, bool Aggregate, class... Args>
inline constexpr bool node_v<declared_node<Impl, Ctx, Aggregate, Args...>> =
    detail::first_true({!Ctx::template can_resolve<Args>...}) == sizeof...(Args);

// build<Node, Access>: the creation code of a class node, defined below.
template <class Node, slot_access Access>
struct build;

// ---- the context of a request -----------------------------------------------------------------

// context<Injector, Lifetime, Access>: requests made of an injector by a consumer whose lifetime
// is Lifetime: the caller of create<R>(), as unique_t, or the class being built, as its binding's
// lifetime. any_arg holds a reference to one while its class is built. The lifetime is part of
// the type so that can_resolve is memoized per consumer lifetime (plan §7.3), as the
// captive-dependency rule will need (E006, Phase 2); Phase 1 doesn't consult it otherwise.
//
// Access says how the creation code reaches the singletons (static/storage.hpp, slot_access):
// checked, the default, or ready, for an eager injector after make_injector. It changes only
// resolve<R>(): can_resolve<R> evaluates the same node for every Access, and the nodes name
// checked contexts, so the check is memoized once whatever the access.
template <class Injector, class Lifetime, slot_access Access>
struct context {
  using injector_type = Injector;
  using lifetime_type = Lifetime;

  Injector& owner;

  // The check: whether request R can be served, its dependencies included (see the top of this
  // file). Memoized per (Injector, Lifetime, R).
  template <class R>
  static constexpr bool can_resolve = node_v<typename plan<Injector, R>::node>;

  // The creation code: the object request R asks for, as request_result_t says (a reference, or
  // a prvalue). Only instantiated after can_resolve<R> passed. Every function it calls that leads
  // to the next hop is a build<Node, Access> member, called or named right here, so that a hop of
  // the creation code nests three function bodies: this one, the build function, and the
  // placeholder's conversion operator (ADR-0014 item 3).
  template <class R>
  request_result_t<R> resolve() {
    using p = plan<Injector, R>;
    using served = typename p::served;
    using result = request_result_t<R>;
    constexpr request_form form = p::check::request::form;
    if constexpr (served::kind == binding_kind::constructed && served::owned) {
      // A singleton: built on the first request, in its slot (static/storage.hpp). The slot gets
      // a function pointer, so the slot's code doesn't instantiate the creation code. An eager
      // injector has built it already, so it is read with no test at all.
      auto& cell = injector_access::slots(owner)
                       .template at<served::position, typename served::binding_type>();
      if constexpr (Access == slot_access::checked) {
        if (!cell.ready()) [[unlikely]] {
          cell.construct(injector_access::teardown(owner), injector_access::sync(owner),
                         &build<typename p::node, slot_access::checked>::place,
                         static_cast<void*>(std::addressof(owner)));
        }
      }
      if constexpr (form == request_form::reference) {
        return cell.object();
      } else if constexpr (form == request_form::pointer) {
        return std::addressof(cell.object());
      } else if constexpr (form == request_form::value) {
        return result(std::as_const(cell.object()));  // a copy, which .allow_copy() permits
      } else {
        return result(cell.storage().pointer());  // std::shared_ptr or std::weak_ptr
      }
    } else if constexpr (served::kind == binding_kind::constructed) {
      // A new object, which the consumer owns: unique, or unbound. Its parameters are requested
      // in its own context, with this context's access to the singletons.
      using built = build<typename p::node, Access>;
      using own_context = context<Injector, typename served::lifetime, Access>;
      if constexpr (form == request_form::unique_ptr) {
        own_context next{owner};
        return result(built::fresh(next));
      } else if constexpr (form == request_form::shared_ptr) {
        // One allocation for the object and its control block, as std::make_shared would make.
        return std::allocate_shared<typename p::impl>(construct_with<typename p::impl>(
            &built::place, static_cast<void*>(std::addressof(owner))));
      } else if constexpr (TRELLIS_IS_SAME(typename p::impl, result)) {
        own_context next{owner};
        return built::value(next);
      } else {
        own_context next{owner};
        return result(built::value(next));
      }
    } else {
      // An object the binding holds (dsl/bind.hpp).
      auto& stored = module_access::binding<served::position>(injector_access::bindings(owner));
      if constexpr (served::kind == binding_kind::external) {
        auto* object = binding_access::object(stored);
        if constexpr (form == request_form::reference) {
          return *object;
        } else if constexpr (form == request_form::pointer) {
          return object;
        } else {
          return result(std::as_const(*object));
        }
      } else if constexpr (served::kind == binding_kind::shared_external) {
        const auto& shared = binding_access::object(stored);
        if constexpr (form == request_form::reference) {
          return *shared;
        } else if constexpr (form == request_form::pointer) {
          return shared.get();
        } else if constexpr (form == request_form::value) {
          return result(std::as_const(*shared));
        } else {
          return result(shared);  // std::shared_ptr or std::weak_ptr
        }
      } else {
        auto& value = binding_access::object(stored);
        if constexpr (form == request_form::reference) {
          return value;
        } else if constexpr (form == request_form::pointer) {
          return std::addressof(value);
        } else {
          return result(std::as_const(value));
        }
      }
    }
  }
};

// ---- building a class -------------------------------------------------------------------------

// with_access_t<Ctx, Access>: the context Ctx, with the access Access to the singletons.
template <class Ctx, slot_access Access>
using with_access_t = context<typename Ctx::injector_type, typename Ctx::lifetime_type, Access>;

// build<Node, Access>::value(ctx), fresh(ctx) and place(injector, where): the class of a
// construction node as a prvalue (for a by-value request), on the heap (for std::unique_ptr), or
// in given storage (a slot, or allocate_shared's control block, through a build_fn). Each builds
// the object exactly as discovery checked it, with braces for an aggregate and parentheses
// otherwise (ADR-0003 item 5), and resolves each parameter in the class's own context: the node's,
// Ctx, with the access Access. A prvalue is returned directly, so the class needn't be copyable or
// movable.
//
// The order in which the parameters are created is the compiler's order of evaluation of
// function arguments, which C++ leaves unspecified for parentheses (GCC goes right to left, Clang
// left to right), and left to right for braces. Teardown follows whatever order the singletons
// were built in.
TRELLIS_PLACEHOLDER_CONVERSIONS_BEGIN

template <class Impl, class Ctx, slot_access Access, std::size_t... Is>
struct build<deduced_node<Impl, Ctx, std::index_sequence<Is...>, false>, Access> {
  using own = with_access_t<Ctx, Access>;
  static Impl value([[maybe_unused]] own& ctx) {
    return Impl(repeat_t<Is, any_arg<Impl, own>>{ctx}...);
  }
  static Impl* fresh([[maybe_unused]] own& ctx) {
    return new Impl(repeat_t<Is, any_arg<Impl, own>>{ctx}...);
  }
  static void place(void* injector, std::byte* where) {
    [[maybe_unused]] own ctx{*static_cast<typename Ctx::injector_type*>(injector)};
    ::new (where) Impl(repeat_t<Is, any_arg<Impl, own>>{ctx}...);
  }
};

template <class Impl, class Ctx, slot_access Access, std::size_t... Is>
struct build<deduced_node<Impl, Ctx, std::index_sequence<Is...>, true>, Access> {
  using own = with_access_t<Ctx, Access>;
  static Impl value([[maybe_unused]] own& ctx) {
    return Impl{repeat_t<Is, any_arg<Impl, own>>{ctx}...};
  }
  static Impl* fresh([[maybe_unused]] own& ctx) {
    return new Impl{repeat_t<Is, any_arg<Impl, own>>{ctx}...};
  }
  static void place(void* injector, std::byte* where) {
    [[maybe_unused]] own ctx{*static_cast<typename Ctx::injector_type*>(injector)};
    ::new (where) Impl{repeat_t<Is, any_arg<Impl, own>>{ctx}...};
  }
};

template <class Impl, class Ctx, slot_access Access, class... Args>
struct build<declared_node<Impl, Ctx, false, Args...>, Access> {
  using own = with_access_t<Ctx, Access>;
  static Impl value([[maybe_unused]] own& ctx) { return Impl(ctx.template resolve<Args>()...); }
  static Impl* fresh([[maybe_unused]] own& ctx) {
    return new Impl(ctx.template resolve<Args>()...);
  }
  static void place(void* injector, std::byte* where) {
    [[maybe_unused]] own ctx{*static_cast<typename Ctx::injector_type*>(injector)};
    ::new (where) Impl(ctx.template resolve<Args>()...);
  }
};

template <class Impl, class Ctx, slot_access Access, class... Args>
struct build<declared_node<Impl, Ctx, true, Args...>, Access> {
  using own = with_access_t<Ctx, Access>;
  static Impl value([[maybe_unused]] own& ctx) { return Impl{ctx.template resolve<Args>()...}; }
  static Impl* fresh([[maybe_unused]] own& ctx) {
    return new Impl{ctx.template resolve<Args>()...};
  }
  static void place(void* injector, std::byte* where) {
    [[maybe_unused]] own ctx{*static_cast<typename Ctx::injector_type*>(injector)};
    ::new (where) Impl{ctx.template resolve<Args>()...};
  }
};

TRELLIS_PLACEHOLDER_CONVERSIONS_END

// ---- the failure pass -------------------------------------------------------------------------

// explain_parameter<Ctx, R>(): reports why request R, made in context Ctx, fails, if it does.
template <class Ctx, class R>
void explain_parameter() {
  if constexpr (!Ctx::template can_resolve<R>) {
    detail::explain<typename Ctx::injector_type, R>();
  }
}

// explain_arg<Consumer, Ctx>: the placeholder of the failure pass. It has the shapes of the
// discovery probe, so overload resolution selects, for each parameter of the deduced constructor,
// the operator that any_arg selects, and deduces the same request (discovery/deduced.hpp). The
// selected operator's body is instantiated, because the failure pass builds the class in a function
// that is instantiated (never called), and it reports the request if it fails. That names a
// deduced parameter's request without enumerating the parameters, which deduction can't
// (ADR-0006, "Deduced mode limits the explain pass"). The bodies call explain themselves rather
// than through explain_parameter, as each function between two explains is one more line of
// instantiation notes in the error. On GCC 13 it declares the probe's deleted twins too, for the
// reason config.hpp gives for TRELLIS_PROBE_DECLARES_TWINS.
template <class Consumer, class Ctx>
struct explain_arg {
  template <class T>
    requires not_self<Consumer, T>
  operator T() {
    if constexpr (!Ctx::template can_resolve<T>) {
      detail::explain<typename Ctx::injector_type, T>();
    }
    detail::unreachable();
  }
  template <class T>
    requires not_self<Consumer, T>
  operator T&() const {
    if constexpr (!Ctx::template can_resolve<T&>) {
      detail::explain<typename Ctx::injector_type, T&>();
    }
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

// owner_converts<Report, Ctx, T>(): whether owner_arg's by-value operator converts to T: to
// anything but an owner of an incomplete class (owns_incomplete, static/injector.hpp). With
// Report it converts to that too, after naming the failure pass of the request T: the odr-use
// instantiates explain<Injector, T>, which reports it (discovery's TRL-E002 for the incomplete
// class), once, as it is the same specialization a by-value parameter's explain_arg would call.
template <bool Report, class Ctx, class T>
constexpr bool owner_converts() noexcept {
  if constexpr (!owns_incomplete<T>) {
    return true;
  } else if constexpr (Report) {
    static_cast<void>(&detail::explain<typename Ctx::injector_type, T>);
    return true;
  } else {
    return false;
  }
}

// owner_arg<Consumer, Ctx, Report>: the placeholder of the failure pass for a class with a
// parameter that owns an incomplete class, std::unique_ptr<X> by value with X only declared.
// Building that class from explain_arg would instantiate the parameter's destructor, whose
// assertion that X is complete is a second error after the request's own (T1.5's review). So
// explain_node first asks, in an unevaluated construction from owner_arg<..., false>, whose
// by-value operator refuses such a parameter, whether the class has one; if it has, it reports
// through owner_arg<..., true>'s constraint instead of building, unevaluated too, so that no
// destructor is instantiated. Its other failing parameters are reported once X is complete. The
// operators have the probe's shapes and twins, so that overload resolution deduces the requests
// explain_arg would (discovery/deduced.hpp); they are only declared.
template <class Consumer, class Ctx, bool Report>
struct owner_arg {
  template <class T>
    requires not_self<Consumer, T> && (detail::owner_converts<Report, Ctx, T>())
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

// explain_node<Node>::run(): reports the failing parameters of a class whose own plan is fine.
// Each failing parameter is a cause of its own, and explain<Injector, R> is instantiated once per
// request, so a request that fails on several paths is reported once.
template <class Node>
struct explain_node;

TRELLIS_PLACEHOLDER_CONVERSIONS_BEGIN

template <class Impl, class Ctx, std::size_t... Is>
struct explain_node<deduced_node<Impl, Ctx, std::index_sequence<Is...>, false>> {
  static void run() {
    if constexpr (requires { Impl(repeat_t<Is, owner_arg<Impl, Ctx, false>>{}...); }) {
      alignas(Impl) std::array<std::byte, sizeof(Impl)> storage{};
      ::new (static_cast<void*>(storage.data())) Impl(repeat_t<Is, explain_arg<Impl, Ctx>>{}...);
    } else {
      static_cast<void>(requires { Impl(repeat_t<Is, owner_arg<Impl, Ctx, true>>{}...); });
    }
  }
};

template <class Impl, class Ctx, std::size_t... Is>
struct explain_node<deduced_node<Impl, Ctx, std::index_sequence<Is...>, true>> {
  static void run() {
    if constexpr (requires { Impl{repeat_t<Is, owner_arg<Impl, Ctx, false>>{}...}; }) {
      alignas(Impl) std::array<std::byte, sizeof(Impl)> storage{};
      ::new (static_cast<void*>(storage.data())) Impl{repeat_t<Is, explain_arg<Impl, Ctx>>{}...};
    } else {
      static_cast<void>(requires { Impl{repeat_t<Is, owner_arg<Impl, Ctx, true>>{}...}; });
    }
  }
};

TRELLIS_PLACEHOLDER_CONVERSIONS_END

template <class Impl, class Ctx, bool Aggregate, class... Args>
struct explain_node<declared_node<Impl, Ctx, Aggregate, Args...>> {
  static void run() {
    [[maybe_unused]] const std::array<bool, sizeof...(Args) + 1> explored{
        true, (detail::explain_parameter<Ctx, Args>(), true)...};
  }
};

// explain<Injector, R>(): reports why request R of the injector can't be served, each cause with
// its own static_assert in its own if-constexpr branch, whose literal starts with the ID (ADR-0006
// items 2 and 3). It is a function, instantiated once per (injector, request), so a cause that
// several requests lead to is reported once; and its assertions are in its body, which Clang,
// unlike a class whose static_assert failed, doesn't mark invalid, so later uses add no error.
// It must report something whenever can_resolve<R> is false, as create<R>() then relies on it to
// stop the compilation: the first branches cover R's own plan, and otherwise the node failed, so
// one of the class's requests fails, which explain_node finds.
template <class Injector, class R>
void explain() {
  using p = plan<Injector, R>;
  using node = typename p::node;
  constexpr failure reason = failure_of<node>;
  if constexpr (reason == failure::keys_repeat) {
    // make_injector has reported TRL-E003: nothing more to say.
  } else if constexpr (reason == failure::request_form) {
    detail::report_request_form<Injector, R>();
  } else if constexpr (reason == failure::no_binding) {
    static_assert(trellis::error::E001<typename p::key, trellis::error::request<R>>::reported,
                  "TRL-E001: no binding for an abstract type: bind it to an implementation with "
                  "trellis::bind<I>.to<C>()");
  } else if constexpr (reason == failure::strict_unbound) {
    static_assert(trellis::error::E013<typename p::key, trellis::error::request<R>>::reported,
                  "TRL-E013: unbound concrete type requested in strict mode: options{.strict = "
                  "true} turns off the implicit unique binding; bind the type, such as "
                  "trellis::bind<T> for a new object per request");
  } else if constexpr (reason == failure::no_implementation) {
    static_assert(trellis::error::E001<typename p::key, trellis::error::request<R>>::reported,
                  "TRL-E001: no binding for an abstract type: bind<I> alone binds it to itself, "
                  "and nothing implements it; name the implementation with "
                  "trellis::bind<I>.to<C>()");
  } else if constexpr (reason == failure::discovery) {
    // Discovery's own assertion: TRL-E002, TRL-E004 or TRL-E010, once per class and bound.
    static_cast<void>(sizeof(discovered_ctor_t<typename p::impl, no_ctor_declared, p::max_arity>));
  } else if constexpr (reason == failure::copy) {
    static_assert(trellis::error::E002<typename p::key, trellis::error::request<R>>::reported,
                  "TRL-E002: type not constructible: a by-value request takes a copy of the bound "
                  "object, or a new object of the bound class as the key, and the key can't be "
                  "constructed from it; request a reference or a smart pointer instead");
  } else if constexpr (reason == failure::unsafe_delete) {
    static_assert(trellis::error::E014<typename p::key, trellis::error::request<R>,
                                       typename p::impl>::reported,
                  "TRL-E014: owning request would delete the implementation through a base class "
                  "whose destructor isn't virtual: std::unique_ptr<I> deletes through I; give I a "
                  "virtual destructor, or request std::shared_ptr<I>, which deletes the class it "
                  "was made for");
  } else {
    explain_node<node>::run();
  }
}

// ---- eager mode -------------------------------------------------------------------------------

// build_eagerly(injector): builds every singleton of the injector, for options{.eager = true}
// (plan §7.5, ADR-0012 item 1). Each binding the injector owns is requested by reference, in
// binding order; a request builds its dependencies first, so the singletons are built in
// dependency order, and the teardown stack records the order they were built in. A singleton that
// can't be built is reported as create<Key&>() would report it.
template <class Injector, class Binding>
bool build_eagerly_one([[maybe_unused]] Injector& owner) {
  if constexpr (owns_object<Binding>) {
    using request = typename binding_traits<Binding>::key&;
    using root = context<Injector, unique_t, slot_access::checked>;
    if constexpr (root::template can_resolve<request>) {
      root ctx{owner};
      static_cast<void>(ctx.template resolve<request>());
    } else {
      detail::explain<Injector, request>();
    }
  }
  return true;
}

// One flat expansion over the bindings, left to right in a braced list, never a fold, as an
// injector may hold more than 256 bindings (plan §8.1 C3). The injector passes its flattened
// bindings.
template <class Injector, class... Bindings>
void build_eagerly(Injector& owner, type_list<Bindings...> /*bindings*/) {
  [[maybe_unused]] const std::array<bool, sizeof...(Bindings) + 1> built{
      true, detail::build_eagerly_one<Injector, Bindings>(owner)...};
}

}  // namespace trellis::inline v1::detail

#endif  // TRELLIS_STATIC_RESOLVE_HPP
