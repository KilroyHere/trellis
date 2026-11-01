// Trellis static engine: resolution and creation, the code behind injector::create<T>() (plan
// §5.3, §7.3, ADR-0006, ADR-0014).
// Copyright (c) 2026 Aryan Patel
// SPDX-License-Identifier: BSL-1.0
//
// Internal (trellis::detail): the static engine, not user API. docs/internals/
// resolution-and-storage.md explains each technique and lists its tests.
//
// create<R>() is `if constexpr (resolvable) { creation code } else { explain pass }` (ADR-0006
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
//   explain_root_t<Injector, R>::run()    The explain pass (diagnostics/explain.hpp): follows the
//                                          first failing request down to its cause and reports it
//                                          once, with the path from the root (T1.6).
//
// plan<Injector, R> works out, once per (injector, request), what serves R: its binding, or the
// implicit unique binding of an unbound class (ADR-0004), the verdict of the request-form table
// (static/validate.hpp) and, for a class the injector builds, how its constructor is found
// (discovery/). The plan's node is what the check still has to evaluate, which is where the
// recursion through the graph happens: building the class with every constructor parameter
// resolved. Everything else in the plan finishes before the recursion starts, so the recursion
// nests only the node and the constructor's overload resolution per hop of a dependency chain:
// the lean shape of ADR-0014 item 3, which decides the longest chain that compiles (NFR-09). The
// node of a deduced class is evaluated by can_resolve<R> itself, so that such a hop instantiates
// one variable template.
//
// Deduced classes are checked and built with the resolving placeholder any_arg<Impl, Ctx>
// (discovery/deduced.hpp), whose conversion operators ask Ctx::can_resolve<T> and call
// ctx.resolve<T>(): Ctx is the context of the class being built, so its lifetime is its
// parameters' consumer lifetime. Declared classes resolve their declared parameter types, from the
// declaration that applies: the binding's .with_ctor<Args...>() (served_binding's binding_ctor),
// else the class's own (discovery/ctor_traits.hpp).
#ifndef TRELLIS_STATIC_RESOLVE_HPP
#define TRELLIS_STATIC_RESOLVE_HPP

#include <trellis/config.hpp>
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
// external object). binding_ctor is the constructor that the binding declares with
// .with_ctor<Args...>(), discovery's BindingCtor (discovery/ctor_traits.hpp): a ctor<Args...>, or
// no_ctor_declared, which leaves the class to its own declaration or to deduction. Both come out
// of the binding's Impl (dsl/bind.hpp, impl_parts). owned says whether the object lives in one of
// the injector's slots.
template <class Found>
struct served_binding;
template <std::size_t J, class Key, class Impl, class Lifetime, bool AllowCopy>
struct served_binding<stored_binding<J, binding<Key, Impl, Lifetime, AllowCopy>>> {
  static constexpr bool bound = true;
  static constexpr std::size_t position = J;
  using binding_type = binding<Key, Impl, Lifetime, AllowCopy>;
  using impl = impl_class_t<Impl>;
  using binding_ctor = impl_ctor_t<Impl>;
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
  using binding_ctor = no_ctor_declared;
  using lifetime = unique_t;  // implicitly unique (ADR-0004)
  static constexpr binding_kind kind = binding_kind::constructed;
  static constexpr bool owned = false;
};

// ---- nodes: what the check evaluates ----------------------------------------------------------

// Why a request can't be served, as far as its own plan can tell, without looking at the
// dependencies. The explain pass reports each reason with its own message
// (diagnostics/explain.hpp, own_report).
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
// caller checked discoverable_v): the declaration that applies, its binding's (BindingCtor) or its
// own, if there is one, else its deduced arity.
template <class Impl, class Ctx, std::size_t MaxArity, class BindingCtor>
constexpr auto construction_node() noexcept {
  using declared = typename declared_ctor<Impl, BindingCtor>::type;
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
    // for a value binding) takes a copy of it, as const. An incomplete key can't be copied, and
    // std::is_constructible of one is an error of its own, so std::conjunction asks it only of a
    // complete key: the operands of && are all instantiated, whatever the first one's value, and
    // a binding of an incomplete type (.to_ptr(), .to_ref()) would fail every request instead of
    // serving its references and pointers (T1.6 review, T1.7's finding).
    if constexpr (form == request_form::value &&
                  !std::conjunction_v<std::bool_constant<complete_class<key>>,
                                      std::is_constructible<key, const impl&>>) {
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
  } else if constexpr (!discoverable_v<impl, typename served::binding_ctor, Plan::max_arity>) {
    return never_node<failure::discovery>{};
  } else if constexpr (form == request_form::value &&
                       !detail::value_buildable<key, impl, served::owned>()) {
    return never_node<failure::copy>{};
  } else {
    return detail::construction_node<impl, typename Plan::next, Plan::max_arity,
                                     typename served::binding_ctor>();
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

// node_v<Node>: whether a node can be built. Each specialization evaluates its node directly, with
// no class template or function call between one request and the requests of the built class's
// constructor: the deduced node asks the __is_constructible builtin (TRELLIS_IS_CONSTRUCTIBLE),
// whose overload resolution checks the placeholders' constraints, Ctx::can_resolve<T>, the next
// hop's check. The two deduced specializations are discovery/deduced.hpp's
// deduced_constructible_v with the builtin, which saves a level per hop; tests/unit/create_test.cpp
// checks that they agree. can_resolve<R> evaluates a deduced node with the same two expressions,
// in specializations of its own (below context), and any other node through node_v, so a hop of a
// dependency chain of deduced classes costs the variable template can_resolve<R> and the
// constraint check, and a hop through a declared constructor costs can_resolve<R> and node_v
// (ADR-0014 item 3; docs/internals/resolution-and-storage.md has the measurements). The explain
// pass and the tests name a node rather than a request, and use node_v.
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
  // file). Memoized per (Injector, Lifetime, R). Node, which callers never pass, is R's node: the
  // specializations below the class evaluate the node of a deduced class, and node_v any other.
  template <class R, class Node = typename plan<Injector, R>::node>
  static constexpr bool can_resolve = node_v<Node>;

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
          using cell_type = std::remove_reference_t<decltype(cell)>;
          cell.construct(
              injector_access::teardown(owner), injector_access::sync(owner),
              &build<typename p::node, slot_access::checked>::template make_in<cell_type>,
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
        return cell.storage().template share<result>();  // std::shared_ptr or std::weak_ptr
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
        own_context next{owner};
        return built::template share<typename result::element_type>(next);
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

// can_resolve<R> for a class that deduction found the constructor of: node_v's check of its node,
// written out in specializations of can_resolve itself. Through node_v, each hop of a dependency
// chain of deduced classes instantiated two variable templates, can_resolve<R> and node_v<Node>;
// written out, it instantiates one, which saves a level of instantiation depth per hop and, on
// Clang, about a tenth of the frontend stack that a hop takes. The stack is what limits Clang's
// chains of aggregates (-Wstack-exhausted; NFR-09, docs/internals/resolution-and-storage.md).
// Node is a parameter of can_resolve so that these partial specializations can select on it; its
// default is R's node, so callers pass R alone and the check is still memoized per (Injector,
// Lifetime, R). The explain pass and the tests name a node, and keep node_v;
// tests/unit/create_test.cpp checks that the two agree. Defined out of the class: GCC 13 rejects a
// partial specialization of a member variable template inside it.
template <class Injector, class Lifetime, slot_access Access>
template <class R, class Impl, class Ctx, std::size_t... Is>
constexpr bool context<Injector, Lifetime, Access>::can_resolve<
    R, deduced_node<Impl, Ctx, std::index_sequence<Is...>, false>> =
    TRELLIS_IS_CONSTRUCTIBLE(Impl, repeat_t<Is, any_arg<Impl, Ctx>>...);
template <class Injector, class Lifetime, slot_access Access>
template <class R, class Impl, class Ctx, std::size_t... Is>
constexpr bool context<Injector, Lifetime, Access>::can_resolve<
    R, deduced_node<Impl, Ctx, std::index_sequence<Is...>, true>> =
    requires { Impl{std::declval<repeat_t<Is, any_arg<Impl, Ctx>>>()...}; };

// ---- building a class -------------------------------------------------------------------------

// with_access_t<Ctx, Access>: the context Ctx, with the access Access to the singletons.
template <class Ctx, slot_access Access>
using with_access_t = context<typename Ctx::injector_type, typename Ctx::lifetime_type, Access>;

// build<Node, Access>: the class of a construction node, built
//   value(ctx)                    as a prvalue, for a by-value request;
//   fresh(ctx)                    on the heap, for std::unique_ptr;
//   share<Element>(ctx)           owned by a new std::shared_ptr<Element>, for a std::shared_ptr
//                                 request of a unique object (static/storage.hpp,
//                                 make_shared_object);
//   make_in<Cell>(state, injector)  in the storage of the singleton's slot Cell, through the slot's
//                                 make_with: the slot's make_fn (static/storage.hpp).
// Each builds the object exactly as discovery checked it, with braces for an aggregate and
// parentheses otherwise (ADR-0003 item 5), and resolves each parameter in the class's own context:
// the node's, Ctx, with the access Access. A prvalue is returned directly, so the class needn't be
// copyable or movable.
//
// share and make_in hand the storage code a lambda that builds the object where it is told. The
// lambda's body is instantiated with the function that contains it, so the placeholders'
// conversion operators, the next hop, are instantiated one level below that function, which
// resolve<R>() names directly: a hop still nests three function bodies (ADR-0014 item 3), however
// deep the storage code that calls the lambda (std::make_shared, std::allocate_shared) is. The
// storage code can inline the lambda, so a singleton costs one function, make_in, where a separate
// build function called through a pointer cost two (T1.14). GCC inlines it; Clang does where the
// constructor's parameters are few and cheap, as in the research scenario, and otherwise keeps it
// as a function of its own.
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
  template <class Element>
  static std::shared_ptr<Element> share([[maybe_unused]] own& ctx) {
    auto emplace = [&](std::byte* where) {
      ::new (where) Impl(repeat_t<Is, any_arg<Impl, own>>{ctx}...);
    };
    return detail::make_shared_object<Impl, Element>(emplace);
  }
  template <class Cell>
  static teardown_entry make_in(typename Cell::state_type& state, void* injector) {
    [[maybe_unused]] own ctx{*static_cast<typename Ctx::injector_type*>(injector)};
    auto emplace = [&](std::byte* where) {
      ::new (where) Impl(repeat_t<Is, any_arg<Impl, own>>{ctx}...);
    };
    return Cell::make_with(state, emplace);
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
  template <class Element>
  static std::shared_ptr<Element> share([[maybe_unused]] own& ctx) {
    auto emplace = [&](std::byte* where) {
      ::new (where) Impl{repeat_t<Is, any_arg<Impl, own>>{ctx}...};
    };
    return detail::make_shared_object<Impl, Element>(emplace);
  }
  template <class Cell>
  static teardown_entry make_in(typename Cell::state_type& state, void* injector) {
    [[maybe_unused]] own ctx{*static_cast<typename Ctx::injector_type*>(injector)};
    auto emplace = [&](std::byte* where) {
      ::new (where) Impl{repeat_t<Is, any_arg<Impl, own>>{ctx}...};
    };
    return Cell::make_with(state, emplace);
  }
};

template <class Impl, class Ctx, slot_access Access, class... Args>
struct build<declared_node<Impl, Ctx, false, Args...>, Access> {
  using own = with_access_t<Ctx, Access>;
  static Impl value([[maybe_unused]] own& ctx) { return Impl(ctx.template resolve<Args>()...); }
  static Impl* fresh([[maybe_unused]] own& ctx) {
    return new Impl(ctx.template resolve<Args>()...);
  }
  template <class Element>
  static std::shared_ptr<Element> share([[maybe_unused]] own& ctx) {
    auto emplace = [&](std::byte* where) { ::new (where) Impl(ctx.template resolve<Args>()...); };
    return detail::make_shared_object<Impl, Element>(emplace);
  }
  template <class Cell>
  static teardown_entry make_in(typename Cell::state_type& state, void* injector) {
    [[maybe_unused]] own ctx{*static_cast<typename Ctx::injector_type*>(injector)};
    auto emplace = [&](std::byte* where) { ::new (where) Impl(ctx.template resolve<Args>()...); };
    return Cell::make_with(state, emplace);
  }
};

// The declared aggregate's build<declared_node<Impl, Ctx, true, Args...>, Access> is in
// static/declared_aggregate.hpp, included at the end of this file: its declaration may list fewer
// members than the aggregate has, which GCC and Clang warn about, and in one build that header
// has to be a system header (TRELLIS_MODULE_SYSTEM_HEADER, config.hpp).

TRELLIS_PLACEHOLDER_CONVERSIONS_END

// ---- eager mode -------------------------------------------------------------------------------

// build_eagerly(injector): builds every singleton of the injector, for options{.eager = true}
// (plan §7.5, ADR-0012 item 1). Each binding the injector owns is requested by reference, in
// binding order; a request builds its dependencies first, so the singletons are built in
// dependency order, and the teardown stack records the order they were built in. make_injector
// has checked that every singleton can be built (eager_fails, static/injector.hpp), and reported
// the first that can't; the guard below only keeps a failing request's creation code from being
// instantiated.
template <class Injector, class Binding>
bool build_eagerly_one([[maybe_unused]] Injector& owner) {
  if constexpr (owns_object<Binding>) {
    using request = typename binding_traits<Binding>::key&;
    using root = context<Injector, unique_t, slot_access::checked>;
    if constexpr (root::template can_resolve<request>) {
      root ctx{owner};
      static_cast<void>(ctx.template resolve<request>());
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

// The explain pass, which create<T>() and make_injector run when the check fails, needs the plans
// and nodes above; including it last lets either header be included first
// (diagnostics/explain.hpp includes this one).
#include <trellis/diagnostics/explain.hpp>  // IWYU pragma: keep
// The creation code of a declared aggregate, which needs build and declared_node above, in a
// header of its own (static/declared_aggregate.hpp).
#include <trellis/static/declared_aggregate.hpp>  // IWYU pragma: keep

#endif  // TRELLIS_STATIC_RESOLVE_HPP
