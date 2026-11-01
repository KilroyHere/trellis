// Trellis static engine: what the engine knows about an injector type (plan §5.3, ADR-0010).
// Copyright (c) 2026 Aryan Patel
// SPDX-License-Identifier: BSL-1.0
//
// Internal (trellis::detail): declares the injector class template and defines
// detail::injector_traits, so that the request check (static/validate.hpp) and resolution
// (static/resolve.hpp) can name an injector's options, bindings and binding map without the
// injector's definition (static/injector.hpp). The injector's create<T>() needs resolution, which
// needs the request check, so the check must not need the injector's header: this one breaks the
// cycle.
#ifndef TRELLIS_STATIC_INJECTOR_TRAITS_HPP
#define TRELLIS_STATIC_INJECTOR_TRAITS_HPP

#include <trellis/dsl/module.hpp>
#include <trellis/dsl/options.hpp>
#include <trellis/meta/type_map.hpp>
#include <trellis/static/binding_map.hpp>

#include <type_traits>

namespace trellis::inline v1 {

template <options Opts, class... Parts>
class injector;

namespace detail {

// injector_traits<Injector>: what the engine needs to know about an injector type. Its bindings
// are its arguments' bindings, flattened once (the same flatten<...> instantiation as the
// injector's own), and map is their binding map. When a key repeats, make_injector has reported
// TRL-E003, and map is an empty type_map instead: a lookup in a map with a repeated key would be
// the map's own error, a second one for the same mistake. request_check doesn't look up then.
template <class Injector>
struct injector_traits;
template <options Opts, class... Parts>
struct injector_traits<injector<Opts, Parts...>> {
  static constexpr options opts = Opts;
  using bindings = typename flatten<Parts...>::bindings;
  static constexpr bool has_duplicate_keys = binding_map<bindings>::has_duplicate_keys;
  using map =
      std::conditional_t<has_duplicate_keys, type_map<>, typename binding_map<bindings>::type>;
};

}  // namespace detail
}  // namespace trellis::inline v1

#endif  // TRELLIS_STATIC_INJECTOR_TRAITS_HPP
