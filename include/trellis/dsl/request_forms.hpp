// Trellis binding vocabulary: request forms and the request-form table (plan §6.3, ADR-0004).
// Copyright (c) 2026 Aryan Patel
// SPDX-License-Identifier: BSL-1.0
//
// Internal (trellis::detail): the data the engines check requests against, not user API.
//
// A request is the type a consumer asks for, such as a constructor parameter `const Config&`. It
// names a key (Config) and a form (a reference). request_traits<R> splits a request into the two,
// and request_form_table says, for each form and each lifetime, whether the injector may serve it
// and, if not, why. The static engine raises TRL-E005 or TRL-E012 from it (static/validate.hpp);
// the runtime engine will check the same table at build() (plan §7.4).
//
// The table is plan §6.3's, cell for cell, as constexpr data, plus one column of its own: unbound
// concrete types, which are implicitly unique and may only be requested in owning forms
// (ADR-0004; their rejections are TRL-E012, not TRL-E005). Phase 2 brings the scoped lifetimes and
// lazy<...> / provider<...>, but their column and row are encoded already, so that the table is
// complete and every cell is tested (tests/unit/request_forms_test.cpp).
#ifndef TRELLIS_DSL_REQUEST_FORMS_HPP
#define TRELLIS_DSL_REQUEST_FORMS_HPP

#include <trellis/config.hpp>
#include <trellis/dsl/lifetimes.hpp>

#include <array>
#include <cstddef>
#include <memory>
#include <type_traits>

namespace trellis::inline v1::detail {

// ---- request forms ----------------------------------------------------------------------------

// The rows of the table: how a consumer asks for an object.
enum class request_form : unsigned char {
  value,       // T: a new object or a copy, which the consumer owns
  reference,   // T& and const T&
  pointer,     // T* and const T*, non-owning
  unique_ptr,  // std::unique_ptr<T>
  shared_ptr,  // std::shared_ptr<T>
  weak_ptr,    // std::weak_ptr<T>
  deferred,    // lazy<...> and provider<...> (Phase 2; nothing is classified as deferred yet)
};
inline constexpr unsigned request_form_count = 7;

// A key: the type a binding binds, which a request names. Keys are object types without cv
// qualifiers, so `const Config&` and `Config*` both name the key Config. Arrays aren't keys. (A
// variable rather than a concept, so that a static_assert on it reports just the requirement,
// without Clang's notes on why a concept isn't satisfied.)
template <class T>
inline constexpr bool is_bindable_key =
    std::is_object_v<T> && !std::is_array_v<T> && TRELLIS_IS_SAME(T, std::remove_cv_t<T>);

// What a pointer or smart pointer may point to for its request to be a pointer form: an object
// type, cv-qualified or not, that is not an array. Anything else is a value: a function pointer,
// void*, std::unique_ptr<T[]>.
template <class T>
concept pointee = std::is_object_v<T> && !std::is_array_v<T>;

// request_traits<R>: the form of request R, its key, and whether it asks for mutable access to a
// shared object (T&, T*, std::shared_ptr<T>, std::weak_ptr<T> with T not const), which a const
// external object can't give ("constness carries through", plan §6.3).
//
// The primary template is the value form, for a T and for any type that isn't one of the forms
// below, such as std::function<...> or a std::unique_ptr with its own deleter.
template <class R>
struct request_traits {
  using key = R;
  static constexpr request_form form = request_form::value;
  static constexpr bool mutable_access = false;
};

// Top-level cv qualifiers don't change a request: a parameter `const T` is a T, and `T* const`
// is a T* (as in a function type).
template <class R>
struct request_traits<const R> : request_traits<R> {};
template <class R>
struct request_traits<volatile R> : request_traits<R> {};
template <class R>
struct request_traits<const volatile R> : request_traits<R> {};

// X&& and const X&& are the request X: a fresh object that the consumer owns (amended by
// ADR-0003). So std::unique_ptr<T>&& is a std::unique_ptr<T>, and T&& is a T.
template <class R>
struct request_traits<R&&> : request_traits<std::remove_cv_t<R>> {};

template <class R>
struct request_traits<R&> {
  using key = std::remove_cv_t<R>;
  static constexpr request_form form = request_form::reference;
  static constexpr bool mutable_access = !std::is_const_v<R>;
};

template <class R>
  requires pointee<R>
struct request_traits<R*> {
  using key = std::remove_cv_t<R>;
  static constexpr request_form form = request_form::pointer;
  static constexpr bool mutable_access = !std::is_const_v<R>;
};

// std::unique_ptr<R> is std::unique_ptr<R, std::default_delete<R>>, so only the default deleter
// matches. (Its constness never matters: the unique_ptr row has no allowed shared object.)
template <class R>
  requires pointee<R>
struct request_traits<std::unique_ptr<R>> {
  using key = std::remove_cv_t<R>;
  static constexpr request_form form = request_form::unique_ptr;
  static constexpr bool mutable_access = !std::is_const_v<R>;
};

template <class R>
  requires pointee<R>
struct request_traits<std::shared_ptr<R>> {
  using key = std::remove_cv_t<R>;
  static constexpr request_form form = request_form::shared_ptr;
  static constexpr bool mutable_access = !std::is_const_v<R>;
};

template <class R>
  requires pointee<R>
struct request_traits<std::weak_ptr<R>> {
  using key = std::remove_cv_t<R>;
  static constexpr request_form form = request_form::weak_ptr;
  static constexpr bool mutable_access = !std::is_const_v<R>;
};

// request_result_t<R>: what serving request R yields, which create<R>() returns and the engine's
// resolve<R>() produces (static/resolve.hpp). A reference request yields that reference. Anything
// else yields a prvalue without top-level cv qualifiers: a parameter `const T` takes a T, and X&&
// and const X&& take a new X, the request X (ADR-0003). A prvalue initializes the parameter
// directly, so a by-value parameter of a type that can't be copied or moved still works.
template <class R>
struct request_result {
  using type = std::remove_cv_t<R>;
};
template <class R>
struct request_result<R&> {
  using type = R&;
};
template <class R>
struct request_result<R&&> {
  using type = std::remove_cv_t<R>;
};
template <class R>
using request_result_t = typename request_result<R>::type;

// ---- the table --------------------------------------------------------------------------------

// A cell of the table: allowed, or the reason it isn't. The reasons are plan §6.3's words; the
// diagnostics quote them (static/validate.hpp).
enum class form_verdict : unsigned char {
  allowed,                 // ✓
  allowed_captive,         // ✓, subject to the captive-dependency rule (E006, Phase 2)
  copies_hide_sharing,     // ✗ TRL-E005: copies hide sharing; opt in with .allow_copy()
  nothing_keeps_it_alive,  // ✗ TRL-E005: nothing would keep it alive
  ambiguous_ownership,     // ✗ TRL-E005: ambiguous ownership; use unique_ptr
  duplicates_ownership,    // ✗ TRL-E005: would duplicate ownership
  not_heap_owned,          // ✗ TRL-E005: not heap-owned
  caller_owns_it,          // ✗ TRL-E005: the caller owns the object (§6.3 gives no reason)
  // Not cells: the outcomes of the rules applied on top of a cell (adjusted_verdict, below).
  const_object,    // ✗ TRL-E005: mutable access to a const external object
  unbound_shared,  // ✗ TRL-E012: an unbound concrete type, requested in a sharing form
  no_binding,      // nothing to check: an unbound abstract type (TRL-E001, create<T>())
  keys_repeat,     // nothing to check: make_injector has reported a key bound twice
};

// Whether a form is one of the standard smart pointers.
constexpr bool is_smart_pointer_form(request_form form) noexcept {
  return form == request_form::unique_ptr || form == request_form::shared_ptr ||
         form == request_form::weak_ptr;
}

// Whether a verdict lets the request through.
constexpr bool allows(form_verdict verdict) noexcept {
  return verdict == form_verdict::allowed || verdict == form_verdict::allowed_captive;
}

// The table, indexed [request_form][lifetime_kind]. Cells that plan §6.3 marks "✗" without a
// reason, or "✗ (same)", take the reason of their row where the row has one (unique_ptr: would
// duplicate ownership); for shared_ptr and weak_ptr they take the reason of the matching
// shared_ptr cell, and the weak_ptr of a unique object that of a reference to one. Where §6.3
// gives no reason at all (shared_ptr and weak_ptr of an external object), the reason is
// caller_owns_it. lazy<...> and provider<...> are allowed in every column, as §6.3 says.
constexpr std::array<std::array<form_verdict, lifetime_kind_count>, request_form_count>
make_request_form_table() noexcept {
  constexpr form_verdict yes = form_verdict::allowed;
  constexpr form_verdict captive = form_verdict::allowed_captive;
  constexpr form_verdict copies = form_verdict::copies_hide_sharing;
  constexpr form_verdict alive = form_verdict::nothing_keeps_it_alive;
  constexpr form_verdict owner = form_verdict::ambiguous_ownership;
  constexpr form_verdict twice = form_verdict::duplicates_ownership;
  constexpr form_verdict heap = form_verdict::not_heap_owned;
  constexpr form_verdict caller = form_verdict::caller_owns_it;
  constexpr form_verdict e012 = form_verdict::unbound_shared;
  // clang-format off
  return {{
    // unique  singleton  inline  scoped   external  shared  value  unbound
    {{yes,     copies,    copies, copies,  copies,   copies, yes,   yes }},  // T
    {{alive,   yes,       yes,    captive, yes,      yes,    yes,   e012}},  // T&
    {{owner,   yes,       yes,    captive, yes,      yes,    yes,   e012}},  // T*
    {{yes,     twice,     twice,  twice,   twice,    twice,  twice, yes }},  // unique_ptr
    {{yes,     yes,       heap,   captive, caller,   yes,    heap,  e012}},  // shared_ptr
    {{alive,   yes,       heap,   yes,     caller,   yes,    heap,  e012}},  // weak_ptr
    {{yes,     yes,       yes,    yes,     yes,      yes,    yes,   yes }},  // lazy, provider
  }};
  // clang-format on
}
inline constexpr auto request_form_table = make_request_form_table();

// The cell for a form and a lifetime.
constexpr form_verdict table_verdict(request_form form, lifetime_kind lifetime) noexcept {
  return request_form_table[static_cast<std::size_t>(form)][static_cast<std::size_t>(lifetime)];
}

// The verdict for a request, from its cell and the two rules on top of the table (plan §6.3):
// .allow_copy() lets a shared object be copied by value, and a const object can't be requested for
// mutable access. object_is_const is only ever true for external and to_shared bindings.
constexpr form_verdict adjusted_verdict(form_verdict cell, bool allow_copies, bool object_is_const,
                                        bool mutable_access) noexcept {
  if (cell == form_verdict::copies_hide_sharing && allow_copies) {
    return form_verdict::allowed;
  }
  if (allows(cell) && object_is_const && mutable_access) {
    return form_verdict::const_object;
  }
  return cell;
}

}  // namespace trellis::inline v1::detail

#endif  // TRELLIS_DSL_REQUEST_FORMS_HPP
