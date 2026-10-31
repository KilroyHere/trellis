// Trellis meta toolkit: a compile-time map from types to types (plan §7.1, §8.1 C2).
// Copyright (c) 2026 Aryan Patel
// SPDX-License-Identifier: BSL-1.0
//
// Internal (trellis::detail): building blocks for the engine, not user API. docs/internals/
// meta-toolkit.md explains each technique and lists its tests.
//
// type_map<entry<K, V>...> is a single struct that inherits from each of its entries. Each entry
// contributes one overload of a static lookup function, taking a pointer to type_tag<K>, and the
// map gathers them with a using-declaration, so looking up a key is one overload resolution among
// non-template functions: no search, no deduction, and no class instantiated beyond the map (plan
// §8.1 C2, ADR-0010). A struct derived from a map is a map for lookup_t too, so that a long entry
// list can hide behind a short name in everything instantiated from it (struct app :
// type_map<...> {}; ADR-0010).
//
// Keys must be distinct. has_duplicate_keys_v answers without building the map, so the engine can
// report a repeated key in its own words (the duplicate-binding error of plan §7.8, T1.4) before
// it builds one; building a map with a repeated key anyway is a compile error.
//
// Nothing here instantiates a key or a value type, and no call is subject to argument-dependent
// lookup, which would instantiate the classes in a map's entries.
#ifndef TRELLIS_META_TYPE_MAP_HPP
#define TRELLIS_META_TYPE_MAP_HPP

#include <trellis/meta/type_list.hpp>

#include <type_traits>

namespace trellis::inline v1::detail {

// One entry of a type_map: the key K and the value V, both types of any kind.
template <class K, class V>
struct entry {
  using key = K;
  using value = V;

  // The overload this entry adds to its map's lookup (lookup_t, below): a non-template function,
  // so that finding it deduces nothing. Deduction against base classes is what GCC 13 and 14 get
  // wrong for keys that differ only in T& and T&&, nested ones included (T1.1 review). Only named
  // in decltype.
  static std::type_identity<V> lookup(const type_tag<K>*);
};

// type_map<entry<K, V>...>: defined only for a list of entries.
template <class... Entries>
struct type_map;

// ---- has_duplicate_keys_v ---------------------------------------------------------------------

// Whether no key occurs twice in Ks, with one class per map and no deduction: slots_of<Ks...>
// (type_list.hpp) derives from type_tag<K> once for each occurrence of K. A pointer to it converts
// to a pointer to type_tag<K> only when that base is unambiguous, that is, when K occurs exactly
// once; each K of Ks occurs at least once. The flags are scanned flat (first_true).
template <class... Ks>
constexpr bool keys_are_distinct() noexcept {
  using keys = slots_of<Ks...>;
  return first_true({!std::is_convertible_v<const keys*, const type_tag<Ks>*>...}) == sizeof...(Ks);
}

// The primary template: Map is not a type_map. This one static_assert is the whole error.
template <class Map>
struct expects_type_map {
  static_assert(always_false<Map>, "trellis: expected a trellis::detail::type_map<entry<K, V>...>");
  static constexpr bool value = false;
};

// has_duplicate_keys_v<Map>: whether two entries of the type_map Map have the same key. Asking
// doesn't instantiate the map, so it is safe for a map that would not compile. Map must be a
// type_map specialization itself, not a struct derived from one.
template <class Map>
inline constexpr bool has_duplicate_keys_v = expects_type_map<Map>::value;
template <class... Ks, class... Vs>
inline constexpr bool has_duplicate_keys_v<type_map<entry<Ks, Vs>...>> =
    !keys_are_distinct<Ks...>();

// ---- the map ----------------------------------------------------------------------------------

// The result of a lookup that found no entry: its static_assert is lookup_t's error, which names
// the key in the instantiation context.
template <class K>
struct missing_key {
  static_assert(always_false<K>, "trellis: lookup_t<K, Map>: K is not a key of Map");
  using type = missing_key;
};

// The result of a lookup in a map that has already reported its repeated key.
struct lookup_in_invalid_map {
  using type = lookup_in_invalid_map;
};

// The map's bases: its entries, with their lookup overloads gathered in one scope, and a template
// overload that a key without an entry falls back to. For a key with an entry, both that entry's
// overload and the template match exactly, and overload resolution prefers the non-template
// ([over.match.best]); the other entries' overloads aren't viable, as type_tag<K> converts to no
// other type_tag.
template <bool KeysAreDistinct, class... Entries>
struct map_bases : Entries... {
  using Entries::lookup...;
  template <class K>
  static missing_key<K> lookup(const type_tag<K>*);
};

// What a map with a repeated key derives from instead of its entries. Deriving from the entries
// would fail on its own for two identical ones (the same base class twice), so the map's one
// error is the static_assert below, and its lookup adds no other on GCC. Clang drops the members
// of a class whose instantiation failed, so there a later lookup in the map reports its own error
// too: check has_duplicate_keys_v first, as the engine does.
template <class... Entries>
struct map_bases<false, Entries...> {
  static_assert(always_false<Entries...>,
                "trellis: type_map: a key appears in more than one entry; check "
                "has_duplicate_keys_v before building the map");
  template <class K>
  static lookup_in_invalid_map lookup(const type_tag<K>*);
};

template <class... Ks, class... Vs>
struct type_map<entry<Ks, Vs>...>
    : map_bases<!has_duplicate_keys_v<type_map<entry<Ks, Vs>...>>, entry<Ks, Vs>...> {};

// ---- lookup_t ---------------------------------------------------------------------------------

// lookup_t<K, Map>: the value type of the entry with key K in the type_map Map, or in a struct
// derived from one. K must be a key of Map. Map::lookup is a qualified call, so argument-
// dependent lookup plays no part.
template <class K, class Map>
using lookup_t = typename decltype(Map::lookup(static_cast<const type_tag<K>*>(nullptr)))::type;

// ---- lookup_or_t and contains_key_v -----------------------------------------------------------

// The same lookup, read without asserting: an entry's overload returns std::type_identity<V>, and
// a key without an entry gets missing_key<K>, which is only named here, never instantiated, so
// its static_assert stays silent.
template <class Lookup, class Fallback>
struct found_or {
  using type = Fallback;
};
template <class V, class Fallback>
struct found_or<std::type_identity<V>, Fallback> {
  using type = V;
};

template <class Lookup>
inline constexpr bool found_v = false;
template <class V>
inline constexpr bool found_v<std::type_identity<V>> = true;

// lookup_or_t<K, Map, Fallback>: the value type of the entry with key K in Map, or Fallback when
// Map has no such entry; for "the binding of this key, or none" (plan §5.3).
template <class K, class Map, class Fallback>
using lookup_or_t =
    typename found_or<decltype(Map::lookup(static_cast<const type_tag<K>*>(nullptr))),
                      Fallback>::type;

// contains_key_v<K, Map>: whether Map has an entry with key K.
template <class K, class Map>
inline constexpr bool contains_key_v =
    found_v<decltype(Map::lookup(static_cast<const type_tag<K>*>(nullptr)))>;

}  // namespace trellis::inline v1::detail

#endif  // TRELLIS_META_TYPE_MAP_HPP
