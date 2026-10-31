// Trellis meta toolkit: type lists and their algorithms (plan §7.1, §8.1 C3).
// Copyright (c) 2026 Aryan Patel
// SPDX-License-Identifier: BSL-1.0
//
// Internal (trellis::detail): building blocks for the engine, not user API. docs/internals/
// meta-toolkit.md explains each technique and lists its tests.
//
// No algorithm here recurses over a pack (plan §8.1 C3), and none folds over one: Clang refuses a
// fold expression over more than 256 operands, fatally, and a list such as a module's bindings can
// be longer (ADR-0010, measured in T1.1). Instead:
//
// - contains_v, index_of_v and at_t's fallback use one class per list, slots_of<Ts...>, that
//   derives from an indexed slot per element, so a query is a builtin trait or one overload
//   resolution;
// - where a pack must be looked at element by element (index_of_v for a repeated type, a map's
//   duplicate check), it is expanded once into a braced list of booleans, flat at any length, and
//   scanned in a constexpr loop (first_true);
// - at_t uses __type_pack_element or C++26 pack indexing where the compiler has them.
//
// No algorithm instantiates the list's element types, and every call is qualified, so that
// argument-dependent lookup doesn't instantiate them either: a list may hold types whose
// instantiation would fail, such as std::optional of an abstract class.
#ifndef TRELLIS_META_TYPE_LIST_HPP
#define TRELLIS_META_TYPE_LIST_HPP

#include <trellis/config.hpp>

#include <cstddef>
#include <initializer_list>
#include <type_traits>
#include <utility>

namespace trellis::inline v1::detail {

// A pack of types, as a type. Only its name is ever used, never a value, so any type can be an
// element: incomplete, abstract, void, a reference or another list.
template <class... Ts>
struct type_list {
  static constexpr std::size_t size = sizeof...(Ts);
};

// always_false<Ts...>: false, but dependent on Ts, so that static_assert(always_false<T>, "...")
// fires only when its template is instantiated. (C++20 rejects a plain static_assert(false) in a
// template definition; C++23's P2593 lifts that.)
template <class...>
inline constexpr bool always_false = false;

// first_true(flags): the index of the first true element of flags, or flags.size() if none is.
//
// The callers pass a braced list made by one pack expansion, such as {TRELLIS_IS_SAME(T, Ts)...},
// so "not found" comes out as sizeof...(Ts). An initializer_list takes any length, including an
// empty pack's {}, and keeps this an ordinary function rather than a template instantiated per
// length.
constexpr std::size_t first_true(std::initializer_list<bool> flags) noexcept {
  std::size_t index = 0;
  for (const bool flag : flags) {
    if (flag) {
      break;
    }
    ++index;
  }
  return index;
}

// ---- the indexed set of a list ----------------------------------------------------------------

// type_tag<T>: an empty class that stands for T, as a base class or a pointer target, without
// instantiating T.
template <class T>
struct type_tag {};

// slot<I, T>: T at position I. Its base type_tag<T> is what contains_v and a type_map's duplicate
// check look for; its index and its type member are what at_t's fallback deduces.
template <std::size_t I, class T>
struct slot : type_tag<T> {
  using type = T;
};

// slots_of<Ts...>: one class per list that derives from slot<I, T_I> for each I. A repeated type
// gives two different slots with the same type_tag base, which C++ allows (the base is then
// ambiguous, not duplicated); the algorithms rely on exactly that.
template <class Indices, class... Ts>
struct slots;
template <std::size_t... Is, class... Ts>
struct slots<std::index_sequence<Is...>, Ts...> : slot<Is, Ts>... {};

template <class... Ts>
using slots_of = slots<std::index_sequence_for<Ts...>, Ts...>;

// position<I, T>: index_of_v's overload for T at position I. A non-template, so that finding a
// position deduces nothing: GCC 13 and 14 match T& against a T&& base when deducing (T1.1 review).
// Only named in decltype. It lives in a class of its own, not in slot, so that the lists that
// never ask for a position (most of them, and every map's keys) don't pay for the overloads.
template <std::size_t I, class T>
struct position {
  static std::integral_constant<std::size_t, I> at(const type_tag<T>*);
};

// positions<Ts...>: one class per list that gathers the position overloads of all its elements
// in one scope. A repeated type gives two overloads with the same signature, which makes a call
// ambiguous rather than the class invalid; position_in, below, relies on that.
template <class Indices, class... Ts>
struct positions;
template <std::size_t... Is, class... Ts>
struct positions<std::index_sequence<Is...>, Ts...> : position<Is, Ts>... {
  static constexpr std::size_t size = sizeof...(Ts);
  using position<Is, Ts>::at...;
  // The first position of T, by a flat scan: the answer for a repeated T, whose call is
  // ambiguous, and for a missing one (then size).
  template <class T>
  static constexpr std::size_t scan() noexcept {
    return first_true({TRELLIS_IS_SAME(T, Ts)...});
  }
};

// The primary template of each list operation: List is not a type_list. This one static_assert
// is the whole error; the members let the operation finish without a second one.
template <class List>
struct expects_type_list {
  static_assert(always_false<List>, "trellis: expected a trellis::detail::type_list<...>");
  static constexpr bool value = false;
  static constexpr std::size_t size = 1;
  using type = List;
  template <class T>
  static constexpr std::size_t scan() noexcept {
    return 0;
  }
};

// list_slots<List>: the slots of a type_list (type) and its positions (position_set), through a
// class template keyed on the list, so that the pack is expanded once per list, not again in every
// query (a query that wrote slots_of<Ts...> itself cost clang-18 3.9 s and 713 MB for 1,000
// queries over 1,000 types, T1.1). For anything else both name expects_type_list, whose
// instantiation is the error.
template <class List>
struct list_slots {
  using type = expects_type_list<List>;
  using position_set = expects_type_list<List>;
};
template <class... Ts>
struct list_slots<type_list<Ts...>> {
  using type = slots_of<Ts...>;
  using position_set = positions<std::index_sequence_for<Ts...>, Ts...>;
};

// ---- contains_v -------------------------------------------------------------------------------

// contains_v<T, List>: whether T is one of List's types. The match is exact: const T, T& and T
// are three different types.
//
// is_base_of asks whether type_tag<T> is a base of the list's slots: one builtin trait, with no
// instantiation per element, and true for a repeated type too, as is_base_of ignores ambiguity.
template <class T, class List>
inline constexpr bool contains_v = std::is_base_of_v<type_tag<T>, typename list_slots<List>::type>;

// ---- index_of_v -------------------------------------------------------------------------------

// The position of T's first occurrence in a list, given its Positions; T must occur. When T
// occurs once, exactly one position overload matches, which gives the index in one overload
// resolution. A repeated T makes the call ambiguous and a missing one leaves it without a
// candidate; both fall back to the flat scan, which finds the first position or reports the error.
// (A scan per query cost clang-18 8.4 s and 627 MB for 1,000 queries over 1,000 types; this costs
// 3.0 s and 119 MB. T1.1.)
template <class T, class Positions>
constexpr std::size_t position_in() noexcept {
  if constexpr (requires(const type_tag<T>* tag) { Positions::at(tag); }) {
    return decltype(Positions::at(static_cast<const type_tag<T>*>(nullptr)))::value;
  } else {
    constexpr std::size_t index = Positions::template scan<T>();
    static_assert(index != Positions::size,
                  "trellis: index_of_v<T, List>: T is not one of the types in List");
    return index;
  }
}

// index_of_v<T, List>: the 0-based position of the first T in List (exact match, as contains_v).
// T must be in List; check contains_v first when it might not be.
template <class T, class List>
inline constexpr std::size_t index_of_v = position_in<T, typename list_slots<List>::position_set>();

// ---- at_t -------------------------------------------------------------------------------------

// pack_element_by_*<I, Ts...>: the type at index I of Ts, three ways. pack_element, below, picks
// the cheapest the compiler offers; each is defined wherever it works, so tests can check them
// all.

// Indexed base classes, on every compiler: select_index<I> takes a pointer to a slot<I, T> with I
// fixed, so a call with a pointer to the list's slots deduces T from the only slot whose index is
// I ([temp.deduct.call]/4.3): one overload resolution per lookup, on the class contains_v uses
// too. The call is qualified, so that argument-dependent lookup doesn't instantiate the elements.
template <std::size_t I, class T>
slot<I, T> select_index(const slot<I, T>*);  // only named in decltype

template <std::size_t I, class... Ts>
using pack_element_by_bases =
    typename decltype(detail::select_index<I>(static_cast<const slots_of<Ts...>*>(nullptr)))::type;

#if TRELLIS_HAS_TYPE_PACK_ELEMENT
// GCC 14+ and Clang: a builtin that instantiates nothing.
template <std::size_t I, class... Ts>
using pack_element_by_builtin = __type_pack_element<I, Ts...>;
#endif

#if TRELLIS_HAS_PACK_INDEXING
// C++26 pack indexing (P2662).
template <std::size_t I, class... Ts>
using pack_element_by_indexing = Ts...[I];
#endif

#if TRELLIS_HAS_TYPE_PACK_ELEMENT
template <std::size_t I, class... Ts>
using pack_element = pack_element_by_builtin<I, Ts...>;
#elif TRELLIS_HAS_PACK_INDEXING
template <std::size_t I, class... Ts>
using pack_element = pack_element_by_indexing<I, Ts...>;
#else
template <std::size_t I, class... Ts>
using pack_element = pack_element_by_bases<I, Ts...>;
#endif

template <std::size_t I, class List>
struct at : expects_type_list<List> {};
template <std::size_t I, class... Ts>
struct at<I, type_list<Ts...>> {
  static_assert(I < sizeof...(Ts),
                "trellis: at_t<I, List>: I is not less than the number of types in List");
  // The index is clamped, so that an index past the end is reported by the assertion above
  // alone, not also by the builtin or the deduction. Clamping to 0 keeps the lookup on the same
  // slots class as every other index.
  using type = pack_element<(I < sizeof...(Ts) ? I : 0), Ts...>;
};
// An empty list has no index to clamp to.
template <std::size_t I>
struct at<I, type_list<>> {
  static_assert(always_false<std::integral_constant<std::size_t, I>>,
                "trellis: at_t<I, List>: I is not less than the number of types in List");
  using type = void;
};

// at_t<I, List>: the type at 0-based index I of List. I must be less than List::size.
template <std::size_t I, class List>
using at_t = typename at<I, List>::type;

}  // namespace trellis::inline v1::detail

#endif  // TRELLIS_META_TYPE_LIST_HPP
