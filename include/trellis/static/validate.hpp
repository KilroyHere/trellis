// Trellis static engine: checking a request against the request-form table (plan §5.3 step 3,
// §6.3, ADR-0004).
// Copyright (c) 2026 Aryan Patel
// SPDX-License-Identifier: BSL-1.0
//
// Internal (trellis::detail): the entry points that create<T>() (T1.5) and the explain pass
// (T1.6) call, not user API.
//
//   request_check<Injector, R>          finds the binding that serves request R and its verdict
//                                       from the table (dsl/request_forms.hpp). No errors: it
//                                       is what resolution asks, memoized per (injector,
//                                       request).
//   report_request_form<Injector, R>()  raises the one static_assert for a rejected request:
//                                       TRL-E005 or TRL-E012. For an allowed request it does
//                                       nothing, and so it does for an unbound abstract type,
//                                       which is E001 and the explain pass's (T1.6), and for
//                                       an injector whose keys repeat, which make_injector has
//                                       reported (TRL-E003).
//
// Each rejection reason has its own static_assert, in its own `if constexpr` branch, with its own
// literal starting with the ID (ADR-0006 item 2), so exactly one fires. The consumer's lifetime,
// which the captive-dependency rule needs (E006, Phase 2), joins the key in Phase 2 (plan §7.3).
#ifndef TRELLIS_STATIC_VALIDATE_HPP
#define TRELLIS_STATIC_VALIDATE_HPP

#include <trellis/diagnostics/errors.hpp>
#include <trellis/dsl/bind.hpp>
#include <trellis/dsl/lifetimes.hpp>
#include <trellis/dsl/module.hpp>
#include <trellis/dsl/request_forms.hpp>
#include <trellis/static/binding_map.hpp>
#include <trellis/static/injector.hpp>

#include <cstddef>
#include <type_traits>

namespace trellis::inline v1::detail {

// served_by<Found>: what the table needs to know about the binding that binding_for_t found, or
// about the implicit unique binding of an unbound type.
template <class Found>
struct served_by;
template <std::size_t J, class Binding>
struct served_by<stored_binding<J, Binding>> {
  using lifetime = typename binding_traits<Binding>::lifetime;
  static constexpr lifetime_kind kind = lifetime_traits<lifetime>::kind;
  static constexpr bool allow_copies = binding_traits<Binding>::allow_copies;
  // Only external and to_shared bindings can hold a const object (dsl/bind.hpp).
  static constexpr bool object_is_const = std::is_const_v<typename binding_traits<Binding>::impl>;
  static constexpr bool abstract_unbound = false;
};
template <class Key>
struct served_by<unbound<Key>> {
  using lifetime = unique_t;  // implicitly unique (ADR-0004)
  static constexpr lifetime_kind kind = lifetime_kind::unbound;
  static constexpr bool allow_copies = false;
  static constexpr bool object_is_const = false;
  // An abstract type can't be implicitly unique. std::is_abstract_v needs a complete class, as
  // constructing an unbound Key would anyway; an incomplete one is the trait's own error.
  static constexpr bool abstract_unbound = std::is_abstract_v<Key>;
};

// request_check<Injector, R>: the verdict of the table on request R made of the injector. A class
// template, so the compiler memoizes it per (injector, request). For an injector whose keys
// repeat, the partial specialization below decides without looking anything up.
template <class Injector, class R,
          bool KeysAreDistinct = !injector_traits<Injector>::has_duplicate_keys>
struct request_check {
  using request = request_traits<R>;
  using key = typename request::key;
  using found = binding_for_t<typename injector_traits<Injector>::map, key>;
  using served = served_by<found>;
  static constexpr form_verdict verdict =
      served::abstract_unbound
          ? form_verdict::no_binding
          : adjusted_verdict(table_verdict(request::form, served::kind), served::allow_copies,
                             served::object_is_const, request::mutable_access);
  static constexpr bool allowed = allows(verdict);
};

// What a request against an injector whose keys repeat is served by: nothing. make_injector has
// reported TRL-E003, so the verdict, keys_repeat, reports nothing more.
struct served_by_nothing {
  using lifetime = void;
};
template <class Injector, class R>
struct request_check<Injector, R, false> {
  using request = request_traits<R>;
  using key = typename request::key;
  using served = served_by_nothing;
  static constexpr form_verdict verdict = form_verdict::keys_repeat;
  static constexpr bool allowed = false;
};

// Raises TRL-E005 or TRL-E012 when the table rejects request R of the injector, and nothing
// otherwise: not for an allowed request, an unbound abstract type (E001, T1.6) or an injector
// whose keys repeat (TRL-E003, reported already). The narrow entry point of these errors until
// create<T>() exists (T1.5), and what its failure branch calls; T1.6 adds the path from the root.
template <class Injector, class R>
constexpr void report_request_form() noexcept {
  using check = request_check<Injector, R>;
  using key = typename check::key;
  using request = error::request<R>;
  using lifetime = typename check::served::lifetime;
  constexpr form_verdict verdict = check::verdict;
  if constexpr (verdict == form_verdict::copies_hide_sharing) {
    static_assert(error::E005<key, request, lifetime>::reported,
                  "TRL-E005: request form incompatible with the lifetime: copies hide sharing; "
                  "request a reference, or opt in with .allow_copy()");
  } else if constexpr (verdict == form_verdict::nothing_keeps_it_alive) {
    static_assert(error::E005<key, request, lifetime>::reported,
                  "TRL-E005: request form incompatible with the lifetime: nothing would keep the "
                  "object alive; bind it in trellis::singleton, or request it by value or as "
                  "std::unique_ptr");
  } else if constexpr (verdict == form_verdict::ambiguous_ownership) {
    static_assert(error::E005<key, request, lifetime>::reported,
                  "TRL-E005: request form incompatible with the lifetime: ambiguous ownership; "
                  "request std::unique_ptr, or bind it in trellis::singleton");
  } else if constexpr (verdict == form_verdict::duplicates_ownership) {
    static_assert(error::E005<key, request, lifetime>::reported,
                  "TRL-E005: request form incompatible with the lifetime: std::unique_ptr would "
                  "duplicate ownership of a shared object; request a reference");
  } else if constexpr (verdict == form_verdict::not_heap_owned) {
    static_assert(
        error::E005<key, request, lifetime>::reported,
        "TRL-E005: request form incompatible with the lifetime: the object is not "
        "heap-owned, so there is no std::shared_ptr to it; bind it in trellis::singleton");
  } else if constexpr (verdict == form_verdict::caller_owns_it) {
    static_assert(error::E005<key, request, lifetime>::reported,
                  "TRL-E005: request form incompatible with the lifetime: the caller owns the "
                  "object, so there is no std::shared_ptr to it; bind it with .to_shared()");
  } else if constexpr (verdict == form_verdict::const_object) {
    static_assert(error::E005<key, request, lifetime>::reported,
                  "TRL-E005: request form incompatible with the lifetime: the bound object is "
                  "const; request it through a reference or pointer to const, such as const T& "
                  "or std::shared_ptr<const T>");
  } else if constexpr (verdict == form_verdict::unbound_shared &&
                       is_smart_pointer_form(request_traits<key>::form)) {
    // A reference or pointer to a smart pointer, such as const std::shared_ptr<T>&, names the
    // smart pointer type as its key. Binding that key in a lifetime would give an empty
    // pointer; taking the smart pointer by value lets the injector make it from T's binding.
    static_assert(error::E012<key, request>::reported,
                  "TRL-E012: unbound concrete type requested by reference, pointer or shared_ptr: "
                  "a reference or pointer to a smart pointer asks for the smart pointer itself, "
                  "which nothing binds; take it by value, such as std::shared_ptr<T>, and the "
                  "injector makes it from T's binding");
  } else if constexpr (verdict == form_verdict::unbound_shared) {
    static_assert(error::E012<key, request>::reported,
                  "TRL-E012: unbound concrete type requested by reference, pointer or shared_ptr: "
                  "nothing would own it; bind it with an owning lifetime such as "
                  "trellis::singleton");
  }
}

}  // namespace trellis::inline v1::detail

#endif  // TRELLIS_STATIC_VALIDATE_HPP
