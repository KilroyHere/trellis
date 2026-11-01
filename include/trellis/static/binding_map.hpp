// Trellis static engine: the binding map (plan §5.3 step 1, §7.1, ADR-0010).
// Copyright (c) 2026 Aryan Patel
// SPDX-License-Identifier: BSL-1.0
//
// Internal (trellis::detail): the engine's view of an injector's bindings, not user API.
//
// make_injector flattens its arguments once into a list of bindings (dsl/module.hpp), and the
// injector stores them as the bases stored_binding<J, B_J> of its module. binding_map<list> maps
// each binding's key to that base, as a type_map (meta/type_map.hpp), so finding the binding for
// a key is one overload resolution (plan §8.1 C2) and also names where the binding is stored.
//
// Keys must be distinct in a type_map, and building one with a repeated key is the map's own
// misuse error. So make_injector asks has_duplicate_keys_v first, which doesn't build the map,
// and reports a repeated key as TRL-E003 in its own words (static/injector.hpp). Only then is the
// map used. first_duplicate names the key and its two bindings for that report; it runs only on
// that failure path.
#ifndef TRELLIS_STATIC_BINDING_MAP_HPP
#define TRELLIS_STATIC_BINDING_MAP_HPP

#include <trellis/dsl/bind.hpp>
#include <trellis/dsl/module.hpp>
#include <trellis/meta/type_list.hpp>
#include <trellis/meta/type_map.hpp>

#include <cstddef>
#include <utility>

namespace trellis::inline v1::detail {

// binding_map<type_list<B...>>: the map from each binding's key to its stored_binding<J, B>,
// and whether two bindings share a key. Instantiated once per injector type.
template <class Bindings>
struct binding_map;
template <class... Bindings>
struct binding_map<type_list<Bindings...>> {
  template <std::size_t... J>
  static type_map<entry<typename binding_traits<Bindings>::key, stored_binding<J, Bindings>>...>
      entries(std::index_sequence<J...>);  // only named in decltype

  // The type_map. Naming it, as here and in has_duplicate_keys_v, doesn't build it: only a
  // lookup does, which make_injector allows only when the keys are distinct.
  using type = decltype(binding_map::entries(std::index_sequence_for<Bindings...>{}));
  static constexpr bool has_duplicate_keys = has_duplicate_keys_v<type>;
};

// first_duplicate<type_list<B...>>: the first key bound twice, and its first two bindings. The
// second binding is the first whose key occurs earlier in the list. It costs a scan of the list
// per binding, which is fine on the failure path, the only place it is used.
template <class Bindings>
struct first_duplicate;
template <class... Bindings>
struct first_duplicate<type_list<Bindings...>> {
  using keys = type_list<typename binding_traits<Bindings>::key...>;

  template <std::size_t... J>
  static constexpr std::size_t second_index(std::index_sequence<J...> /*positions*/) noexcept {
    return first_true({(index_of_v<typename binding_traits<Bindings>::key, keys> != J)...});
  }

  static constexpr std::size_t second = second_index(std::index_sequence_for<Bindings...>{});
  using key = at_t<second, keys>;
  static constexpr std::size_t first = index_of_v<key, keys>;
  using first_binding = at_t<first, type_list<Bindings...>>;
  using second_binding = at_t<second, type_list<Bindings...>>;
};

// unbound<Key>: what the map yields for a key that no binding binds.
template <class Key>
struct unbound {};

// binding_for_t<Map, Key>: where the binding of Key is stored, stored_binding<J, B>, or
// unbound<Key>: one overload resolution in the map (lookup_or_t, meta/type_map.hpp). Map is a
// binding_map's type, whose keys are distinct; a lookup builds it.
template <class Map, class Key>
using binding_for_t = lookup_or_t<Key, Map, unbound<Key>>;

}  // namespace trellis::inline v1::detail

#endif  // TRELLIS_STATIC_BINDING_MAP_HPP
