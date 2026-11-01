// Trellis binding vocabulary: modules, and how bindings are collected (plan §6.9, ADR-0010).
// Copyright (c) 2026 Aryan Patel
// SPDX-License-Identifier: BSL-1.0
//
// A module is a set of bindings, as a value: make_module(bindings_or_modules...) flattens its
// arguments into one module<B1, ..., Bn>, so a module never holds another module, and
// make_injector flattens its own arguments the same way (static/injector.hpp). make_injector also
// takes overrides, which dsl/override.hpp applies to the flattened list; make_module rejects them,
// so a module holds only bindings. Flattening is linear in the number of bindings, without
// recursing or folding over a pack (plan §8.1 C3, C4; ADR-0010). flatten<Parts...> picks one of
// three shapes:
//
//   - Only bindings: the arguments are the list, in order, and each is moved or copied into place.
//   - One module, possibly a struct derived from one: its list, and its bindings are moved or
//     copied as one.
//   - Any other mix: a constexpr table of positions gives, for the J-th binding of the result,
//     the argument it comes from and its index there. One expansion over J = 0..n-1 then names
//     each binding and moves or copies it out of its argument.
//
// The mixed shape finds an argument, and a binding inside a module, by deduction against indexed
// base classes, argument_ref<I, Arg> and stored_binding<J, B>, with the index fixed: one overload
// resolution per binding, on classes built once per argument list and once per module type. A
// lookup with at_t (meta/type_list.hpp) would expand the whole list for every binding, which
// made flattening quadratic: 6 s of a 10 s compile for 1,000 bindings on gcc-13 (T1.4).
//
// Bindings are stored as the bases stored_binding<J, B_J> of one struct, not in a std::tuple,
// whose recursive implementation nests one class per element and runs into the compilers' depth
// limits beyond a few hundred bindings. Clang also refuses a fold over more than 256 operands,
// which a module's bindings can exceed (ADR-0010).
#ifndef TRELLIS_DSL_MODULE_HPP
#define TRELLIS_DSL_MODULE_HPP

#include <trellis/config.hpp>
#include <trellis/dsl/bind.hpp>
#include <trellis/meta/type_list.hpp>

#include <array>
#include <cstddef>
#include <cstdlib>
#include <initializer_list>
#include <type_traits>
#include <utility>

namespace trellis::inline v1 {

template <class... Bindings>
class module;

// An override, trellis::override<K> (dsl/override.hpp): an argument of make_injector, which
// make_module rejects with a message of its own.
template <class Binding>
class override_binding;

namespace detail {

// Stands in for a value after a failed static_assert, so that a branch that reports an error
// needs no value of the type it would return (ADR-0006 item 2). It never runs: the translation
// unit doesn't compile.
[[noreturn]] inline void unreachable() noexcept { std::abort(); }

// is_override<Part>: whether an argument is an override, which make_injector takes and
// make_module doesn't; no_overrides<Parts...>: whether none is. One flat expansion and a scan, not
// a fold, so any number of arguments works (meta/type_list.hpp).
template <class Part>
inline constexpr bool is_override = false;
template <class Binding>
inline constexpr bool is_override<override_binding<Binding>> = true;

template <class... Parts>
inline constexpr bool no_overrides = first_true({is_override<Parts>...}) == sizeof...(Parts);

// ---- storage ----------------------------------------------------------------------------------

// The binding at position J of a module or an injector. Most bindings are empty, and
// TRELLIS_NO_UNIQUE_ADDRESS, [[no_unique_address]] or MSVC's spelling of it (config.hpp), lets
// those take no space; binding_store's TRELLIS_EMPTY_BASES does the rest on MSVC (T1.5).
template <std::size_t J, class Binding>
struct stored_binding {
  using binding_type = Binding;
  TRELLIS_NO_UNIQUE_ADDRESS Binding binding;
};

// The base stored_binding<J, B> of a store, found by deduction from J alone: J is unique among
// the bases, so exactly one base matches.
template <std::size_t J, class Binding>
constexpr stored_binding<J, Binding>& stored_at(stored_binding<J, Binding>& stored) noexcept {
  return stored;
}
template <std::size_t J, class Binding>
constexpr const stored_binding<J, Binding>& stored_at(
    const stored_binding<J, Binding>& stored) noexcept {
  return stored;
}

// The constructor tags of binding_store and module: from the bindings themselves, in order, or
// gathered from a mixed argument list through a flatten<...>.
struct from_bindings_t {
  explicit from_bindings_t() = default;
};
struct from_layout_t {
  explicit from_layout_t() = default;
};

// All the bindings of a module, as the bases of one struct (see the top of this file).
#if TRELLIS_STORES_ARE_PRIMARY_TEMPLATES
// On MSVC, binding_store names a primary template over the stored bindings, stored_bindings: cl
// applies TRELLIS_EMPTY_BASES in full only to a primary template, and gave the partial
// specialization below a byte for each empty binding after the second (config.hpp,
// TRELLIS_STORES_ARE_PRIMARY_TEMPLATES). Every use passes std::index_sequence_for, which
// binding_store_of checks, so the stored bindings come in index order, and the J-th base is
// binding J.
template <class... Stored>
struct TRELLIS_EMPTY_BASES stored_bindings : Stored... {
  stored_bindings() = default;
  template <class... Args>
  constexpr explicit stored_bindings(from_bindings_t /*tag*/, Args&&... bindings)
      : Stored{std::forward<Args>(bindings)}... {}
  template <class Layout, class Refs>
  constexpr stored_bindings(from_layout_t tag, Layout layout, Refs& refs)
      : stored_bindings(tag, layout, refs, std::index_sequence_for<Stored...>{}) {}
  // Binding J is Layout::extract<J>(refs): moved or copied out of the arguments refs refers to.
  template <class Layout, class Refs, std::size_t... J>
  constexpr stored_bindings(from_layout_t /*tag*/, Layout /*layout*/, Refs& refs,
                            std::index_sequence<J...> /*positions*/)
      : Stored{Layout::template extract<J>(refs)}... {}
};
template <class Indices, class... Bindings>
struct binding_store_of;
template <std::size_t... J, class... Bindings>
struct binding_store_of<std::index_sequence<J...>, Bindings...> {
  // stored_bindings' from_layout_t constructor gives its N-th base Layout::extract<N>, which is
  // binding J only when J is N: the indices must be 0, 1, ... in order.
  static_assert(TRELLIS_IS_SAME(std::index_sequence<J...>, std::index_sequence_for<Bindings...>),
                "trellis: binding_store<Indices, Bindings...>: Indices must be "
                "std::index_sequence_for<Bindings...>");
  using type = stored_bindings<stored_binding<J, Bindings>...>;
};
template <class Indices, class... Bindings>
using binding_store = typename binding_store_of<Indices, Bindings...>::type;
#else
template <class Indices, class... Bindings>
struct binding_store;
template <std::size_t... J, class... Bindings>
struct TRELLIS_EMPTY_BASES binding_store<std::index_sequence<J...>, Bindings...>
    : stored_binding<J, Bindings>... {
  binding_store() = default;
  template <class... Args>
  constexpr explicit binding_store(from_bindings_t /*tag*/, Args&&... bindings)
      : stored_binding<J, Bindings>{std::forward<Args>(bindings)}... {}
  // Binding J is Layout::extract<J>(refs): moved or copied out of the arguments refs refers to.
  template <class Layout, class Refs>
  constexpr binding_store(from_layout_t /*tag*/, Layout /*layout*/, Refs& refs)
      : stored_binding<J, Bindings>{Layout::template extract<J>(refs)}... {}
};
#endif

// ---- the arguments of make_module and make_injector -------------------------------------------

// The bindings of a module, and their store: deduced from a module<B...>, or from a struct
// derived from one, so that a named module (struct app : decltype(make_module(...)) {}) is a
// module too.
template <class... Bindings>
type_list<Bindings...> module_bindings(const module<Bindings...>*);  // only named in decltype
template <class... Bindings>
binding_store<std::index_sequence_for<Bindings...>, Bindings...> module_store(
    const module<Bindings...>*);  // only named in decltype

template <class Part>
concept module_part = requires(const Part* part) { detail::module_bindings(part); };

template <class Part>
concept binding_part = binding_traits<Part>::is_binding;

// part_bindings<Part>: what an argument contributes: a module's bindings, or the binding itself,
// as a type_list (type) and as indexed bases (store, for the mixed shape's lookups). Anything
// else contributes nothing, and is_part is false; make_module and make_injector report it, in
// their own bodies. (A static_assert here would make Clang drop the members of the failed class
// and report a second error where they are used.)
template <class Part>
struct part_bindings {
  static constexpr bool is_part = false;
  using type = type_list<>;
  using store = binding_store<std::index_sequence<>>;
};
template <binding_part Part>
struct part_bindings<Part> {
  static constexpr bool is_part = true;
  using type = type_list<Part>;
  using store = binding_store<std::index_sequence<0>, Part>;
};
template <module_part Part>
struct part_bindings<Part> {
  static constexpr bool is_part = true;
  using type = decltype(detail::module_bindings(static_cast<const Part*>(nullptr)));
  using store = decltype(detail::module_store(static_cast<const Part*>(nullptr)));
};

// Whether every argument is a binding or a module: one flat expansion and a scan, not a fold, so
// any number of arguments works (meta/type_list.hpp).
template <class... Parts>
inline constexpr bool bindings_and_modules =
    first_true({!part_bindings<Parts>::is_part...}) == sizeof...(Parts);

// Whether a list of bindings can be copied, which an lvalue argument's bindings must be.
template <class... Bindings>
constexpr bool copyable(type_list<Bindings...> /*bindings*/) noexcept {
  return first_true({!std::is_copy_constructible_v<Bindings>...}) == sizeof...(Bindings);
}

// Whether every argument can give up its bindings: an rvalue moves them out, but an lvalue, or
// a const rvalue, copies them, and a binding that holds a move-only value can't be copied. Args
// are the deduced types of forwarding references. One flat expansion, as above.
template <class... Args>
inline constexpr bool bindings_can_be_taken =
    first_true(
        {((std::is_lvalue_reference_v<Args> || std::is_const_v<std::remove_reference_t<Args>>) &&
          !detail::copyable(typename part_bindings<std::remove_cvref_t<Args>>::type{}))...}) ==
    sizeof...(Args);

// The I-th argument. As a value, Arg is the deduced type of a forwarding reference, so Arg&& is an
// lvalue reference for an lvalue and an rvalue reference for an rvalue. As a type only, never
// constructed, Arg is the argument's type, which type names.
template <std::size_t I, class Arg>
struct argument_ref {
  using type = Arg;
  Arg&& argument;
};
template <class Indices, class... Args>
struct argument_refs;
template <std::size_t... I, class... Args>
struct argument_refs<std::index_sequence<I...>, Args...> : argument_ref<I, Args>... {};

// The base argument_ref<I, Arg> of an argument_refs, found by deduction from I alone.
template <std::size_t I, class Arg>
constexpr argument_ref<I, Arg>& argument_at(argument_ref<I, Arg>& ref) noexcept {
  return ref;
}

// Gives make_module, make_injector and the injector access to a module's bindings.
struct module_access {
  // A module<B...> from its bindings, in order.
  template <class Module, class... Args>
  static constexpr Module from_bindings(Args&&... bindings) {
    return Module(from_bindings_t{}, std::forward<Args>(bindings)...);
  }
  // A module<B...> gathered from mixed arguments through their flatten<...>.
  template <class Module, class Layout, class Refs>
  static constexpr Module gather(Layout layout, Refs& refs) {
    return Module(from_layout_t{}, layout, refs);
  }
  // The J-th binding of a module, or of a struct derived from one, as an lvalue or an rvalue.
  template <std::size_t J, class... Bindings>
  static constexpr auto& binding(module<Bindings...>& from) noexcept {
    return detail::stored_at<J>(from.store_).binding;
  }
  template <std::size_t J, class... Bindings>
  static constexpr const auto& binding(const module<Bindings...>& from) noexcept {
    return detail::stored_at<J>(from.store_).binding;
  }
  template <std::size_t J, class... Bindings>
  static constexpr auto&& binding(module<Bindings...>&& from) noexcept {
    return std::move(detail::stored_at<J>(from.store_).binding);
  }
};

// ---- flattening -------------------------------------------------------------------------------

constexpr std::size_t total(std::initializer_list<std::size_t> sizes) noexcept {
  std::size_t sum = 0;
  for (const std::size_t size : sizes) {
    sum += size;
  }
  return sum;
}

// Where each binding of a flattened list comes from: argument part[J], at index element[J] there.
template <std::size_t Count>
struct flat_positions {
  std::array<std::size_t, Count> part{};
  std::array<std::size_t, Count> element{};
};

// The positions for arguments that contribute sizes[0], sizes[1], ... bindings, in order.
template <std::size_t Count>
constexpr flat_positions<Count> locate(std::initializer_list<std::size_t> sizes) noexcept {
  flat_positions<Count> positions{};
  std::size_t flat = 0;
  std::size_t part = 0;
  for (const std::size_t size : sizes) {
    for (std::size_t element = 0; element != size; ++element) {
      positions.part[flat] = part;
      positions.element[flat] = element;
      ++flat;
    }
    ++part;
  }
  return positions;
}

// The three shapes of an argument list (see the top of this file).
enum class flat_shape : unsigned char { bindings, one_module, mixed };

// An if-constexpr chain with nothing after it: with plain ifs, cl 19.44 reports the code after the
// first one as unreachable (C4702) wherever its condition is constant, as for make_injector's
// bindings (chain-depth test, platforms run 41330765617).
template <class... Parts>
constexpr flat_shape shape_of() noexcept {
  if constexpr (first_true({!binding_part<Parts>...}) == sizeof...(Parts)) {
    return flat_shape::bindings;  // also for no arguments at all
  } else if constexpr (sizeof...(Parts) == 1 && first_true({!module_part<Parts>...}) == 1) {
    return flat_shape::one_module;
  } else {
    return flat_shape::mixed;
  }
}

// flat_layout<Shape, Parts...>: the bindings of the arguments Parts (types without references or
// cv qualifiers), in argument order (bindings), and how to build the module that holds them
// (make). The primary template is the mixed shape.
template <flat_shape Shape, class... Parts>
struct flat_layout {
  static constexpr std::size_t size = detail::total({part_bindings<Parts>::type::size...});
  static constexpr flat_positions<size> positions =
      detail::locate<size>({part_bindings<Parts>::type::size...});

  // The arguments' types as indexed bases, built once, for part_at's lookups.
  using parts_index = argument_refs<std::index_sequence_for<Parts...>, Parts...>;

  // The argument that the J-th binding comes from, and the binding itself.
  template <std::size_t J>
  using part_at = typename std::remove_reference_t<decltype(detail::argument_at<positions.part[J]>(
      std::declval<parts_index&>()))>::type;
  template <std::size_t J>
  using binding_at =
      typename std::remove_reference_t<decltype(detail::stored_at<positions.element[J]>(
          std::declval<typename part_bindings<part_at<J>>::store&>()))>::binding_type;

  template <std::size_t... J>
  static type_list<binding_at<J>...> list(std::index_sequence<J...>);  // only named in decltype

  using bindings = decltype(flat_layout::list(std::make_index_sequence<size>{}));

  template <class Module, class... Args>
  static constexpr Module make(Args&&... args) {
    argument_refs<std::index_sequence_for<Args...>, Args...> refs{{std::forward<Args>(args)}...};
    return module_access::gather<Module>(flat_layout{}, refs);
  }

  // The J-th binding, forwarded out of the argument it comes from: an rvalue reference into an
  // rvalue argument, an lvalue reference into an lvalue. Each binding is extracted once.
  template <std::size_t J, class Refs>
  static constexpr decltype(auto) extract(Refs& refs) noexcept {
    auto& ref = detail::argument_at<positions.part[J]>(refs);
    using arg = decltype(ref.argument);  // Arg&&, a reference type
    if constexpr (module_part<part_at<J>>) {
      return module_access::binding<positions.element[J]>(static_cast<arg>(ref.argument));
    } else {
      return static_cast<arg>(ref.argument);
    }
  }
};

// Only bindings: they are the list, in order.
template <class... Parts>
struct flat_layout<flat_shape::bindings, Parts...> {
  using bindings = type_list<Parts...>;

  template <class Module, class... Args>
  static constexpr Module make(Args&&... args) {
    return module_access::from_bindings<Module>(std::forward<Args>(args)...);
  }
};

// One module: its list, and the module itself is copied or moved (its module<B...> base, for a
// struct derived from one).
template <class Part>
struct flat_layout<flat_shape::one_module, Part> {
  using bindings = typename part_bindings<Part>::type;

  template <class Module, class Arg>
  static constexpr Module make(Arg&& arg) {
    return Module(std::forward<Arg>(arg));
  }
};

// flatten<Parts...>: the layout of an argument list. Instantiated once per list of argument
// types, and shared by make_module and make_injector.
template <class... Parts>
using flatten = flat_layout<detail::shape_of<Parts...>(), Parts...>;

template <class... Bindings>
module<Bindings...> module_of(type_list<Bindings...>);  // only named in decltype

// The module that make_module returns for the arguments Parts.
template <class... Parts>
using flat_module_t = decltype(detail::module_of(typename flatten<Parts...>::bindings{}));

}  // namespace detail

/// @brief A set of bindings, as a value: what `trellis::make_module` returns.
///
/// A module holds its bindings, including the objects that external, shared and value bindings
/// refer to, and is copied or moved like any value. Pass it to `trellis::make_injector`, or to
/// another `trellis::make_module`, which takes its bindings out: modules don't nest.
///
/// A struct derived from a module is a module too. It gives the bindings a short name, which then
/// appears in the injector's type, in place of every binding (ADR-0010):
///
///     struct app_module : decltype(make_app_module(std::declval<const Config&>())) {};
///     auto injector = trellis::make_injector(app_module{make_app_module(config)});
///
/// @tparam Bindings The bindings, in the order the arguments of `make_module` gave them.
///
/// @par Constraints
/// None. Made by `trellis::make_module`. A module default-constructs only when all its bindings
/// do, which external, shared and value bindings don't.
///
/// @par Errors
/// None. A key bound twice is reported where the module is used: `TRL-E003`, raised by
/// `trellis::make_injector`.
///
/// @par Example
/// @code
/// inline auto logging_module() {
///   return trellis::make_module(
///       trellis::bind<ILogger>.to<ConsoleLogger>().in(trellis::singleton));
/// }
/// auto injector = trellis::make_injector(logging_module());
/// @endcode
template <class... Bindings>
class module {
 public:
  /// @brief Default-constructs every binding: available when all bindings are empty ones, made
  ///        with `.to<C>()` and `.in()`.
  ///
  /// @par Constraints
  /// - Every binding is default constructible; otherwise the constructor is deleted.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// struct app_module
  ///     : decltype(trellis::make_module(trellis::bind<IClock>.to<SystemClock>())) {};
  /// auto injector = trellis::make_injector(app_module{});
  /// @endcode
  module() = default;

 private:
  friend struct detail::module_access;

  template <class... Args>
  constexpr explicit module(detail::from_bindings_t tag, Args&&... bindings)
      : store_(tag, std::forward<Args>(bindings)...) {}
  template <class Layout, class Refs>
  constexpr module(detail::from_layout_t tag, Layout layout, Refs& refs)
      : store_(tag, layout, refs) {}

  detail::binding_store<std::index_sequence_for<Bindings...>, Bindings...> store_;
};

/// @brief Collects bindings and modules into one module (plan §6.9).
///
/// The arguments are flattened once into a single `module<B1, ..., Bn>`, in argument order: a
/// module argument contributes its bindings, so modules never nest (ADR-0010). Bindings are
/// moved out of rvalue arguments and copied from lvalues.
///
/// @param parts Bindings, such as `trellis::bind<I>.to<C>()`, and modules, in any order.
/// @return The module.
///
/// @par Constraints
/// - No argument is an override, `trellis::override<I>`, which only `trellis::make_injector`
///   takes; otherwise compilation fails with a static_assert that says so.
/// - Every argument is a binding or a module (or a struct derived from one); otherwise
///   compilation fails with a static_assert naming `make_module`.
/// - The bindings of an lvalue argument can be copied; otherwise compilation fails with a
///   static_assert that says to pass it with `std::move`.
///
/// @par Errors
/// None. Keys bound twice are reported when the module reaches `trellis::make_injector`:
/// `TRL-E003`.
///
/// @par Example
/// @code
/// inline auto storage_module(const Config& config) {
///   return trellis::make_module(
///       trellis::bind<Config>.to_ref(config),
///       trellis::bind<IRepository>.to<SqlRepository>().in(trellis::singleton));
/// }
/// @endcode
template <class... Parts>
[[nodiscard]] constexpr auto make_module([[maybe_unused]] Parts&&... parts)
    -> detail::flat_module_t<std::remove_cvref_t<Parts>...> {
  if constexpr (!detail::no_overrides<std::remove_cvref_t<Parts>...>) {
    // Before the check below, which an override fails too, so that this message is the one.
    static_assert(detail::no_overrides<std::remove_cvref_t<Parts>...>,
                  "trellis: make_module takes bindings and modules, not overrides: pass "
                  "trellis::override<I> to make_injector, where it replaces the binding of I");
    detail::unreachable();
  } else if constexpr (!detail::bindings_and_modules<std::remove_cvref_t<Parts>...>) {
    static_assert(detail::bindings_and_modules<std::remove_cvref_t<Parts>...>,
                  "trellis: make_module and make_injector take bindings, such as "
                  "trellis::bind<I>.to<C>(), and modules made with trellis::make_module");
    detail::unreachable();
  } else if constexpr (!detail::bindings_can_be_taken<Parts...>) {
    static_assert(
        detail::bindings_can_be_taken<Parts...>,
        "trellis: make_module and make_injector copy the bindings of an lvalue argument, and "
        "one of them can't be copied, such as a .to_value() of a move-only type; "
        "pass that argument with std::move");
    detail::unreachable();
  } else {
    return detail::flatten<std::remove_cvref_t<Parts>...>::template make<
        detail::flat_module_t<std::remove_cvref_t<Parts>...>>(std::forward<Parts>(parts)...);
  }
}

}  // namespace trellis::inline v1

#endif  // TRELLIS_DSL_MODULE_HPP
