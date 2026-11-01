// Trellis binding vocabulary: bind<I> and the bindings it makes (plan §6.1, §6.2, ADR-0004).
// Copyright (c) 2026 Aryan Patel
// SPDX-License-Identifier: BSL-1.0
//
// bind<I>.to<C>().in(singleton) reads as a chain, but it accumulates nothing: every step returns
// one binding type, binding<Key, Impl, Lifetime>, with one of its parts replaced (ADR-0010). A set
// of bindings is a module, a variadic pack (dsl/module.hpp), never a growing chain type.
//
//   bind<I>                  binding<I, I, unique_t>: binds I to itself, unique
//   .to<C>()                 binding<I, C, unique_t>: C implements I
//   .in(singleton)           binding<I, C, singleton_t>
//   .allow_copy()            binding<I, C, singleton_t, true>: by-value requests may copy
//   .to_ref(x), .to_ptr(p)   binding<I, I, external_t>, or <I, const I, ...> for a const object
//   .to_shared(sp)           binding<I, I, shared_external_t>, or <I, const I, ...>
//   .to_value(v)             binding<I, V, value_t>, where V is v's type: I or derived from I
//   .with_ctor<A...>()       binding<I, detail::declared_impl<C, ctor<A...>>, ...>: C's constructor
//                            declared, in C's place (see declared_impl)
//
// The external, shared and value bindings hold their object (a pointer, a std::shared_ptr or the
// value), so a binding is a value that a module carries to the injector. The others are empty.
#ifndef TRELLIS_DSL_BIND_HPP
#define TRELLIS_DSL_BIND_HPP

#include <trellis/config.hpp>
#include <trellis/discovery/ctor_traits.hpp>
#include <trellis/dsl/lifetimes.hpp>
#include <trellis/dsl/request_forms.hpp>

#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>

namespace trellis::inline v1 {

template <class Key, class Impl, class Lifetime, bool AllowCopy = false>
class binding;

namespace detail {

// C can implement Key: Key itself, or a class publicly and unambiguously derived from it, in both
// cases without cv qualifiers. These checks are variables rather than concepts, so that the
// static_asserts on them report just the requirement, without Clang's notes on why a concept
// isn't satisfied.
template <class C, class Key>
inline constexpr bool is_implementation_of = TRELLIS_IS_SAME(C, std::remove_cv_t<C>) &&
                                             (TRELLIS_IS_SAME(C, Key) || std::derived_from<C, Key>);

// An object of type T can stand for Key: T is Key or derived from it, possibly const, not
// volatile. A const object gives a const binding, so constness carries through (plan §6.3).
template <class T, class Key>
inline constexpr bool is_object_of =
    !std::is_volatile_v<T> && is_implementation_of<std::remove_const_t<T>, Key>;

// The type an external or shared binding of Key to a T gives access to: Key, const if T is.
template <class Key, class T>
using bound_object_t = std::conditional_t<std::is_const_v<T>, const Key, Key>;

// declared_impl<C, Ctor>: the Impl of a binding that declares, with .with_ctor<Args...>(), the
// constructor of the class C that it builds; Ctor is that ctor<Args...>. The declaration rides in
// the implementation's place, so that every binding stays binding<Key, Impl, Lifetime, AllowCopy>
// and a binding without one keeps its type and its spelling exactly: a fifth template parameter,
// even a defaulted one or an empty pack, changes how Clang prints every binding type (it then
// spells out arguments it elides now), and with it the errors that name a binding (TRL-E003,
// TRL-E009). impl_parts reads it back; nothing else looks inside, and nothing makes an object
// of it.
template <class C, class Ctor>
struct declared_impl {};

// impl_parts<Impl>: what a binding's Impl stands for: `type`, the class the injector builds, or
// the type of the object that an external, shared or value binding holds; and `declared`, the
// constructor that the binding declares, which the static engine passes to discovery as its
// BindingCtor (discovery/ctor_traits.hpp, declared_ctor): a ctor<Args...>, or no_ctor_declared.
template <class Impl>
struct impl_parts {
  using type = Impl;
  using declared = no_ctor_declared;
};
template <class C, class Ctor>
struct impl_parts<declared_impl<C, Ctor>> {
  using type = C;
  using declared = Ctor;
};
template <class Impl>
using impl_class_t = typename impl_parts<Impl>::type;
template <class Impl>
using impl_ctor_t = typename impl_parts<Impl>::declared;

// reject_ctor_declaration<Binding, Args...>(): the one error for .with_ctor<Args...>() on a
// binding that holds its object, made by .to_ref(), .to_ptr(), .to_shared() or .to_value(): the
// injector builds nothing for it, so there is no constructor to declare. Such a binding's step
// calls it, the value binding's from both of its overloads, so that the misuse is one
// static_assert per binding type and declaration however it is written; and it is a function,
// which a failed assertion leaves valid, unlike a class template, which Clang would mark invalid
// and report again at every use (tests/compile_fail/dsl/with_ctor_on_*.cpp).
template <class Binding, class... Args>
constexpr void reject_ctor_declaration() noexcept {
  static_assert(always_false<Binding, Args...>,
                "trellis: .with_ctor<Args...>(): the binding holds an object it was given, by "
                ".to_ref(), .to_ptr(), .to_shared() or .to_value(), so the injector builds nothing "
                "and calls no constructor");
}

// binding_traits<B>: the parts of a binding type, so that the bindings themselves expose no
// members but their chain functions. The primary template means "not a binding".
template <class B>
struct binding_traits {
  static constexpr bool is_binding = false;
};
template <class Key, class Impl, class Lifetime, bool AllowCopy>
struct binding_traits<binding<Key, Impl, Lifetime, AllowCopy>> {
  static constexpr bool is_binding = true;
  using key = Key;
  using impl = Impl;
  using lifetime = Lifetime;
  static constexpr bool allow_copies = AllowCopy;
};

// Reads the object that an external, shared or value binding holds; the bindings befriend it.
struct binding_access;

// checked_key<Key>(): reports a key that isn't one (is_bindable_key), with one static_assert.
// binding's default constructor, which makes every binding object first, reads its return type
// through decltype, and the auto return type makes that instantiate the body: once per key,
// however the binding is reached (bind<I>, decltype(bind<I>...), a binding type spelled out).
// A failed assertion in a function read this way leaves the reader valid, on MSVC too, so
// nothing that uses the binding afterwards is an error of its own (bind.hpp's constructor;
// tests/compile_fail/dsl/bind_bad_key_*.cpp).
template <class Key>
constexpr auto checked_key() noexcept {
  static_assert(is_bindable_key<Key>,
                "trellis: bind<I>: I must be an object type without const, volatile or reference, "
                "and not an array");
  return true;
}

}  // namespace detail

/// @brief A binding of the key `Key` to objects of type `Impl` that the injector constructs, with
///        the lifetime `Lifetime`: `trellis::unique_t`, `trellis::singleton_t` or
///        `trellis::singleton_inline_t`.
///
/// `trellis::bind<I>` is the binding of `I` to itself, unique. Each function below returns a new
/// binding with one part changed; nothing accumulates in the type (ADR-0010). Pass bindings to
/// `trellis::make_module` or `trellis::make_injector`. The objects are constructed in Phase 1 by
/// `create<T>()` (T1.5), with constructors discovered as plan §7.2 describes, or with the one
/// that `.with_ctor<Args...>()` declares.
///
/// @tparam Key The key: the type that consumers request, qualifiers included.
/// @tparam Impl The class to construct: `Key`, or a class publicly derived from it. After
///              `.with_ctor<Args...>()`, `detail::declared_impl<C, ctor<Args...>>`, which names
///              that class `C` with the constructor declared for it.
/// @tparam Lifetime The lifetime.
/// @tparam AllowCopy Whether a shared object may be requested by value, as a copy.
///
/// @par Constraints
/// - `Key` is an object type without cv qualifiers that is not an array. The default constructor
///   checks it, with one static_assert; the functions below then pass a binding with a bad key
///   along unchanged.
///
/// @par Errors
/// None.
///
/// @par Example
/// @code
/// auto logger = trellis::bind<ILogger>.to<ConsoleLogger>().in(trellis::singleton);
/// auto service = trellis::bind<Service>.in(trellis::singleton);
/// @endcode
template <class Key, class Impl, class Lifetime, bool AllowCopy>
class binding {
  // Whether the key is one. The default constructor, which makes every binding object first,
  // reports a bad key; the chain functions then return the binding unchanged without checking
  // anything, so that no other binding type is constructed and a bad key is one error however the
  // binding is used. (A static_assert in the class body would fire again for every binding type in
  // the chain, and Clang drops the members of a class whose assertion failed, so each chain call
  // would add "no member named" to the key's error.)
  static constexpr bool valid_key = detail::is_bindable_key<Key>;

  // Whether the binding is still bind<I> as written, apart from .allow_copy(): .to_ref(),
  // .to_ptr(), .to_value() and .to_shared() take the place of .to<C>() and .in().
  static constexpr bool untouched =
      TRELLIS_IS_SAME(Impl, Key) && TRELLIS_IS_SAME(Lifetime, unique_t);
  // Whether .to<C>() may still name an implementation: the class the binding builds is the key,
  // with or without a declared constructor. A variable, not the macro itself, in the
  // static_assert: GCC would report the error inside the macro and add its expansion notes.
  static constexpr bool no_implementation_yet = TRELLIS_IS_SAME(detail::impl_class_t<Impl>, Key);
  // Whether .with_ctor<Args...>() may still declare a constructor.
  static constexpr bool no_declaration_yet =
      TRELLIS_IS_SAME(detail::impl_ctor_t<Impl>, detail::no_ctor_declared);

 public:
  /// @brief Makes the binding, checking its key: `trellis::bind<I>` makes one this way.
  ///
  /// The check is here, in the constructor that makes every binding object first, so a bad key is
  /// reported however the binding is reached: through `bind<I>`, also when it appears only inside
  /// `decltype`, or through a binding type spelled directly. A constructor is instantiated once
  /// per binding type, so the key is reported once.
  ///
  /// @par Constraints
  /// - `Key` is an object type without cv qualifiers that is not an array; otherwise compilation
  ///   fails with a static_assert naming `bind<I>`.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// constexpr auto clock = trellis::bind<IClock>;  // bind<I> calls this constructor
  /// @endcode
  constexpr binding() noexcept {
    // The check is in detail::checked_key's body, read through decltype, not here (T1.6): with
    // the static_assert in this constructor, cl 19.51 treats the constant initializer of
    // bind<const I> as invalid after reporting it, and then reports every use of it again
    // (C3553, C2516; platforms CI). A constructor that only names checked_key's return type stays
    // valid, as explicit_ctor_t's readers do there. One instantiation per key, as before.
    static_cast<void>(sizeof(decltype(detail::checked_key<Key>())));
  }

  /// @brief Binds the key to the implementation `C`, which the injector constructs.
  ///
  /// @tparam C The implementation: the key itself, or a class publicly derived from it.
  /// @return The binding of the key to `C`, with the same lifetime.
  ///
  /// @par Constraints
  /// - `C` is the key or a class publicly derived from it, without cv qualifiers.
  /// - The binding doesn't name an implementation other than the key yet: after
  ///   `.to<C>()`, another `.to<D>()` is an error. (`.to<I>()` on `bind<I>` changes nothing.)
  ///
  /// Otherwise compilation fails with a static_assert naming `bind<I>.to<C>()`. A constructor
  /// that `.with_ctor<Args...>()` declared before this step is then declared for `C`.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// auto clock = trellis::bind<IClock>.to<SystemClock>();  // unique by default
  /// @endcode
  template <class C>
  [[nodiscard]] constexpr auto to() const noexcept {
    if constexpr (!valid_key) {
      return *this;  // the constructor has reported the key
    } else {
      static_assert(no_implementation_yet,
                    "trellis: bind<I>.to<C>(): the binding already names an implementation");
      static_assert(detail::is_implementation_of<C, Key>,
                    "trellis: bind<I>.to<C>(): C must be I, or a class publicly derived from I, "
                    "without const or volatile");
      if constexpr (!no_implementation_yet || !detail::is_implementation_of<C, Key>) {
        return *this;  // after the static_assert, so that nothing else is reported
      } else if constexpr (no_declaration_yet) {
        return binding<Key, C, Lifetime, AllowCopy>{};
      } else {
        // The declaration was made before .to<C>(): it is for the class the binding builds, C.
        return binding<Key, detail::declared_impl<C, detail::impl_ctor_t<Impl>>, Lifetime,
                       AllowCopy>{};
      }
    }
  }

  /// @brief Sets the lifetime: `trellis::unique` (the default), `trellis::singleton` or
  ///        `trellis::singleton_inline`.
  ///
  /// @param lifetime The lifetime value; only its type matters.
  /// @return The binding with that lifetime.
  ///
  /// @par Constraints
  /// - The lifetime is one of the three above; otherwise compilation fails with a static_assert
  ///   naming `.in(lifetime)`.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// auto repository = trellis::bind<IRepository>.to<SqlRepository>().in(trellis::singleton);
  /// @endcode
  template <class L>
  [[nodiscard]] constexpr auto in([[maybe_unused]] L lifetime) const noexcept {
    if constexpr (!valid_key) {
      return *this;  // the constructor has reported the key
    } else {
      static_assert(
          detail::is_constructed_lifetime<L>,
          "trellis: .in(lifetime): the lifetime must be trellis::unique, trellis::singleton "
          "or trellis::singleton_inline");
      if constexpr (detail::is_constructed_lifetime<L>) {
        return binding<Key, Impl, L, AllowCopy>{};
      } else {
        return *this;
      }
    }
  }

  /// @brief Declares the constructor that the injector calls for the class the binding builds:
  ///        the one that takes `Args...`.
  ///
  /// The declaration concerns this binding only, and wins over what the class declares for
  /// itself, a `trellis::ctor_traits` specialization or a member alias `trellis_ctor`, and over
  /// deduction (plan §7.2, ADR-0003). It is for the class the binding builds, whether `.to<C>()`
  /// comes before it or after it, and it combines with `.in(lifetime)` and `.allow_copy()` in
  /// any order. Each declared parameter is a request, served as the request-form table says (plan
  /// §6.3); a parameter of type `X&&` or `const X&&` is the request `X` (ADR-0003). Bindings that
  /// hold an object, made by `.to_ref()`, `.to_ptr()`, `.to_shared()` and `.to_value()`, build
  /// nothing and reject the step.
  ///
  /// @tparam Args The constructor's parameter types, as the constructor spells them, such as
  ///              `const Config&` or `std::unique_ptr<IClock>`; none for the default constructor.
  /// @return The binding with the declaration.
  ///
  /// @par Constraints
  /// - The binding declares no constructor yet; otherwise compilation fails with a static_assert
  ///   naming `.with_ctor<Args...>()`.
  /// - The injector can build the class from `Args...` as it will build it: an aggregate with
  ///   braces, so no argument may narrow, and any other class with parentheses, as
  ///   `std::is_constructible_v<C, Args...>` checks. Otherwise `TRL-E002` where the injector uses
  ///   the declaration.
  ///
  /// @par Errors
  /// None itself. A declaration that matches no constructor of the class, or for an aggregate one
  /// that doesn't brace-initialize it, is `TRL-E002`, raised by `create<T>()`, and by
  /// `trellis::make_injector` with `options{.eager = true}`, once for the class and the
  /// declaration.
  ///
  /// @par Example
  /// @code
  /// // SqlRepository also has a constructor for tests; the injector calls this one:
  /// auto repository = trellis::bind<IRepository>.to<SqlRepository>()
  ///                       .with_ctor<const Config&, ILogger&>()
  ///                       .in(trellis::singleton);
  /// @endcode
  template <class... Args>
  [[nodiscard]] constexpr auto with_ctor() const noexcept {
    if constexpr (!valid_key) {
      return *this;  // the constructor has reported the key
    } else {
      static_assert(no_declaration_yet,
                    "trellis: .with_ctor<Args...>(): the binding declares a constructor already");
      if constexpr (no_declaration_yet) {
        return binding<Key, detail::declared_impl<Impl, ctor<Args...>>, Lifetime, AllowCopy>{};
      } else {
        return *this;  // after the static_assert, so that nothing else is reported
      }
    }
  }

  /// @brief Lets consumers request the shared object by value, as a copy.
  ///
  /// The request-form table rejects a by-value request of a shared object, because a copy hides
  /// the sharing (plan §6.3). This opts in. It has no effect on requests that the table allows
  /// anyway, such as by-value requests of a unique binding.
  ///
  /// @return The same binding, allowing copies.
  ///
  /// @par Constraints
  /// None.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// auto settings = trellis::bind<Settings>.in(trellis::singleton).allow_copy();
  /// @endcode
  [[nodiscard]] constexpr auto allow_copy() const noexcept {
    if constexpr (!valid_key) {
      return *this;  // the constructor has reported the key
    } else {
      return binding<Key, Impl, Lifetime, true>{};
    }
  }

  /// @brief Binds the key to an object that the caller owns, by reference.
  ///
  /// The binding holds the object's address. The caller keeps the object alive for as long as
  /// the injector uses it; the injector never copies or destroys it. A const object can only be
  /// requested as `const T&`, `const T*` or, with `.allow_copy()`, a copy (plan §6.3).
  ///
  /// @param object The object: an object of the key's type, or of a class publicly derived from
  ///               it.
  /// @return The external binding of the key to `object`.
  ///
  /// @par Constraints
  /// - `object` is of the key's type or of a class publicly derived from it, not volatile.
  /// - The binding is still `bind<I>` as written: `.to_ref()` replaces `.to<C>()` and `.in()`.
  ///
  /// Otherwise compilation fails with a static_assert naming `bind<I>.to_ref(x)`. An rvalue
  /// doesn't compile: the binding would dangle.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// const Config config = load_config();
  /// auto injector = trellis::make_injector(trellis::bind<Config>.to_ref(config));
  /// @endcode
  template <class T>
  [[nodiscard]] constexpr auto to_ref([[maybe_unused]] T& object) const noexcept {
    if constexpr (!valid_key) {
      return *this;  // the constructor has reported the key
    } else {
      static_assert(
          untouched,
          "trellis: bind<I>.to_ref(x): .to_ref() replaces .to<C>() and .in(), so it comes "
          "right after bind<I>");
      static_assert(detail::is_object_of<T, Key>,
                    "trellis: bind<I>.to_ref(x): x must be an I, or of a class publicly derived "
                    "from I");
      if constexpr (untouched && detail::is_object_of<T, Key>) {
        return binding<Key, detail::bound_object_t<Key, T>, external_t, AllowCopy>(
            std::addressof(object));
      } else {
        return *this;
      }
    }
  }

  // The deleted to_ref(const T&&) below returns a binding, not void, so that the deleted call is
  // the one error: given void, MSVC also rejects the variable the call initializes (cl 19.51,
  // C3313), as in tests/compile_fail/dsl/bind_to_ref_temporary.cpp.
  /// @brief Rejects a temporary: a binding to it would dangle.
  ///
  /// @par Constraints
  /// - Never selected for a valid program: `.to_ref()` takes an lvalue.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// // trellis::bind<Config>.to_ref(Config{});  // error: deleted function
  /// @endcode
  template <class T>
  [[nodiscard]] binding to_ref(const T&& object) const = delete;

  /// @brief Binds the key to an object that the caller owns, through a pointer.
  ///
  /// The same as `.to_ref(*object)`.
  ///
  /// @param object A pointer to the object, which must not be null.
  /// @return The external binding of the key to `*object`.
  ///
  /// @par Constraints
  /// - `object` points to an object of the key's type or of a class publicly derived from it,
  ///   not volatile.
  /// - The binding is still `bind<I>` as written.
  ///
  /// Otherwise compilation fails with a static_assert naming `bind<I>.to_ptr(p)`. A null pointer
  /// is undefined behaviour once the binding is used.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// Metrics* metrics = host.metrics();
  /// auto injector = trellis::make_injector(trellis::bind<Metrics>.to_ptr(metrics));
  /// @endcode
  template <class T>
  [[nodiscard]] constexpr auto to_ptr([[maybe_unused]] T* object) const noexcept {
    if constexpr (!valid_key) {
      return *this;  // the constructor has reported the key
    } else {
      static_assert(
          untouched,
          "trellis: bind<I>.to_ptr(p): .to_ptr() replaces .to<C>() and .in(), so it comes "
          "right after bind<I>");
      static_assert(detail::is_object_of<T, Key>,
                    "trellis: bind<I>.to_ptr(p): p must point to an I, or to an object of a class "
                    "publicly derived from I");
      if constexpr (untouched && detail::is_object_of<T, Key>) {
        return binding<Key, detail::bound_object_t<Key, T>, external_t, AllowCopy>(object);
      } else {
        return *this;
      }
    }
  }

  /// @brief Binds the key to a copy of `value`, which the injector owns.
  ///
  /// The binding stores the value with the type of the argument, so a value whose type is a class
  /// derived from the key isn't sliced. A derived object passed through a reference to the key is
  /// copied as the key, as any copy through such a reference is. Requests by value get a copy,
  /// and requests by reference or pointer the stored object (plan §6.3).
  ///
  /// @param value The value, copied or moved into the binding. Its type, without references and
  ///              cv qualifiers, is the key or a class publicly derived from it.
  /// @return The value binding.
  ///
  /// @par Constraints
  /// - The value's type is the key or a class publicly derived from it. A value that merely
  ///   converts to the key, such as a string literal for `std::string`, doesn't qualify: convert
  ///   it first.
  /// - The binding can store the copy: the value's type isn't abstract, and `value` can be
  ///   copied or moved into it. An abstract key reached through a reference doesn't qualify:
  ///   pass the object with its concrete type, or bind it with `.to_ref()`.
  /// - The binding is still `bind<I>` as written.
  ///
  /// Otherwise compilation fails with a static_assert naming `bind<I>.to_value(v)`.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// auto injector = trellis::make_injector(
  ///     trellis::bind<Config>.to_value(Config{"postgres://db"}),
  ///     trellis::bind<std::string>.to_value(std::string("app")));
  /// @endcode
  template <class V>
  [[nodiscard]] constexpr auto to_value([[maybe_unused]] V&& value) const
      noexcept(std::is_nothrow_constructible_v<std::remove_cvref_t<V>, V>) {
    if constexpr (!valid_key) {
      return *this;  // the constructor has reported the key
    } else {
      using stored = std::remove_cvref_t<V>;
      // A copy needs a concrete type that the argument copies or moves into. Checked only for an
      // implementation of the key, so that each mistake gets one static_assert.
      constexpr bool storable = !detail::is_implementation_of<stored, Key> ||
                                (!std::is_abstract_v<stored> && std::is_constructible_v<stored, V>);
      static_assert(untouched,
                    "trellis: bind<I>.to_value(v): .to_value() replaces .to<C>() and .in(), so it "
                    "comes right after bind<I>");
      static_assert(detail::is_implementation_of<stored, Key>,
                    "trellis: bind<I>.to_value(v): v must be an I, or of a class publicly derived "
                    "from I; convert it to I first");
      static_assert(storable,
                    "trellis: bind<I>.to_value(v): the binding can't store a copy of v, whose type "
                    "is abstract or can't be copied or moved from v; pass the object with its "
                    "concrete type, or bind it with .to_ref()");
      if constexpr (untouched && detail::is_implementation_of<stored, Key> && storable) {
        return binding<Key, stored, value_t, AllowCopy>(std::forward<V>(value));
      } else {
        return *this;
      }
    }
  }

  /// @brief Binds the key to an object that the caller shares through a `std::shared_ptr`.
  ///
  /// The binding holds a `std::shared_ptr` to the object, so the object lives at least as long
  /// as the injector. A `std::shared_ptr<const T>` gives a const binding.
  ///
  /// @param object The shared pointer, which must not be null. It may point to an object of a
  ///               class publicly derived from the key.
  /// @return The shared external binding.
  ///
  /// @par Constraints
  /// - `object` points to an object of the key's type or of a class publicly derived from it,
  ///   not volatile.
  /// - The binding is still `bind<I>` as written.
  ///
  /// Otherwise compilation fails with a static_assert naming `bind<I>.to_shared(sp)`. A null
  /// pointer is undefined behaviour once the binding is used.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// auto injector =
  ///     trellis::make_injector(trellis::bind<IClock>.to_shared(std::make_shared<SystemClock>()));
  /// @endcode
  template <class T>
  [[nodiscard]] auto to_shared([[maybe_unused]] std::shared_ptr<T> object) const noexcept {
    if constexpr (!valid_key) {
      return *this;  // the constructor has reported the key
    } else {
      static_assert(untouched,
                    "trellis: bind<I>.to_shared(sp): .to_shared() replaces .to<C>() and .in(), so "
                    "it comes right after bind<I>");
      static_assert(detail::is_object_of<T, Key>,
                    "trellis: bind<I>.to_shared(sp): sp must point to an I, or to an object of a "
                    "class publicly derived from I");
      if constexpr (untouched && detail::is_object_of<T, Key>) {
        return binding<Key, detail::bound_object_t<Key, T>, shared_external_t, AllowCopy>(
            std::shared_ptr<detail::bound_object_t<Key, T>>(std::move(object)));
      } else {
        return *this;
      }
    }
  }
};

/// @brief A binding of the key `Key` to an object that the caller owns, made by `.to_ref()` or
///        `.to_ptr()`.
///
/// It holds the object's address. `Object` is `Key`, or `const Key` when the object is const.
///
/// @tparam Key The key.
/// @tparam Object The object's type as the binding sees it: `Key` or `const Key`.
/// @tparam AllowCopy Whether the object may be requested by value, as a copy.
///
/// @par Constraints
/// None. Made only by `.to_ref()` and `.to_ptr()`.
///
/// @par Errors
/// None.
///
/// @par Example
/// @code
/// const Config config = load_config();
/// auto binding = trellis::bind<Config>.to_ref(config);
/// // decltype(binding) is trellis::binding<Config, const Config, trellis::external_t>
/// @endcode
template <class Key, class Object, bool AllowCopy>
class binding<Key, Object, external_t, AllowCopy> {
 public:
  /// @brief Lets consumers request the object by value, as a copy.
  ///
  /// @return The same binding, allowing copies.
  ///
  /// @par Constraints
  /// None.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// auto config_binding = trellis::bind<Config>.to_ref(config).allow_copy();
  /// @endcode
  [[nodiscard]] constexpr binding<Key, Object, external_t, true> allow_copy() const noexcept {
    return binding<Key, Object, external_t, true>(object_);
  }

  /// @brief Rejects a constructor declaration: the binding holds the caller's object, so the
  ///        injector builds nothing.
  ///
  /// @return The same binding, unchanged.
  ///
  /// @par Constraints
  /// - Never valid: compilation fails with a static_assert naming `.with_ctor<Args...>()`, once
  ///   per binding type and declaration.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// // trellis::bind<Config>.to_ref(config).with_ctor<int>();  // error: builds nothing
  /// @endcode
  template <class... Args>
  [[nodiscard]] constexpr binding with_ctor() const noexcept {
    detail::reject_ctor_declaration<binding, Args...>();
    return *this;
  }

 private:
  template <class, class, class, bool>
  friend class binding;
  friend struct detail::binding_access;

  constexpr explicit binding(Object* object) noexcept : object_(object) {}

  Object* object_;
};

/// @brief A binding of the key `Key` to an object that the caller shares, made by `.to_shared()`.
///
/// It holds a `std::shared_ptr` to the object. `Object` is `Key`, or `const Key` for a
/// `std::shared_ptr` to a const object.
///
/// @tparam Key The key.
/// @tparam Object The object's type as the binding sees it: `Key` or `const Key`.
/// @tparam AllowCopy Whether the object may be requested by value, as a copy.
///
/// @par Constraints
/// None. Made only by `.to_shared()`.
///
/// @par Errors
/// None.
///
/// @par Example
/// @code
/// auto binding = trellis::bind<IClock>.to_shared(std::make_shared<SystemClock>());
/// // decltype(binding) is trellis::binding<IClock, IClock, trellis::shared_external_t>
/// @endcode
template <class Key, class Object, bool AllowCopy>
class binding<Key, Object, shared_external_t, AllowCopy> {
 public:
  /// @brief Lets consumers request the object by value, as a copy.
  ///
  /// @return The same binding, allowing copies.
  ///
  /// @par Constraints
  /// None.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// auto clock_binding = trellis::bind<Clock>.to_shared(std::make_shared<Clock>()).allow_copy();
  /// @endcode
  [[nodiscard]] binding<Key, Object, shared_external_t, true> allow_copy() const& noexcept {
    return binding<Key, Object, shared_external_t, true>(object_);
  }
  /// @brief Lets consumers request the object by value, as a copy; moves the pointer along.
  ///
  /// @return The same binding, allowing copies.
  ///
  /// @par Constraints
  /// None.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// auto clock_binding = trellis::bind<Clock>.to_shared(std::make_shared<Clock>()).allow_copy();
  /// @endcode
  [[nodiscard]] binding<Key, Object, shared_external_t, true> allow_copy() && noexcept {
    return binding<Key, Object, shared_external_t, true>(std::move(object_));
  }

  /// @brief Rejects a constructor declaration: the binding holds the caller's shared object, so
  ///        the injector builds nothing.
  ///
  /// @return The same binding, unchanged.
  ///
  /// @par Constraints
  /// - Never valid: compilation fails with a static_assert naming `.with_ctor<Args...>()`, once
  ///   per binding type and declaration.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// // trellis::bind<IClock>.to_shared(clock).with_ctor<int>();  // error: builds nothing
  /// @endcode
  template <class... Args>
  [[nodiscard]] binding with_ctor() const noexcept {
    detail::reject_ctor_declaration<binding, Args...>();
    return *this;
  }

 private:
  template <class, class, class, bool>
  friend class binding;
  friend struct detail::binding_access;

  explicit binding(std::shared_ptr<Object> object) noexcept : object_(std::move(object)) {}

  std::shared_ptr<Object> object_;
};

/// @brief A binding of the key `Key` to a value that the injector owns, made by `.to_value()`.
///
/// It holds the value, of type `Value`: the key, or a class publicly derived from it.
///
/// @tparam Key The key.
/// @tparam Value The stored value's type.
/// @tparam AllowCopy Has no effect: a value binding can always be requested by value.
///
/// @par Constraints
/// None. Made only by `.to_value()`.
///
/// @par Errors
/// None.
///
/// @par Example
/// @code
/// auto binding = trellis::bind<Config>.to_value(Config{"postgres://db"});
/// // decltype(binding) is trellis::binding<Config, Config, trellis::value_t>
/// @endcode
template <class Key, class Value, bool AllowCopy>
class binding<Key, Value, value_t, AllowCopy> {
 public:
  /// @brief Returns the same binding: a value binding allows copies already.
  ///
  /// @return The same binding, with `AllowCopy` set.
  ///
  /// @par Constraints
  /// - `Value` is copy constructible.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// auto config_binding = trellis::bind<Config>.to_value(Config{}).allow_copy();
  /// @endcode
  [[nodiscard]] constexpr binding<Key, Value, value_t, true> allow_copy() const& {
    return binding<Key, Value, value_t, true>(value_);
  }
  /// @brief Returns the same binding: a value binding allows copies already. Moves the value.
  ///
  /// @return The same binding, with `AllowCopy` set.
  ///
  /// @par Constraints
  /// - `Value` is move constructible.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// auto config_binding = trellis::bind<Config>.to_value(Config{}).allow_copy();
  /// @endcode
  [[nodiscard]] constexpr binding<Key, Value, value_t, true> allow_copy() && {
    return binding<Key, Value, value_t, true>(std::move(value_));
  }

  /// @brief Rejects a constructor declaration: the binding holds the injector's copy of a value,
  ///        so the injector builds nothing.
  ///
  /// @return The same binding, unchanged.
  ///
  /// @par Constraints
  /// - Never valid: compilation fails with a static_assert naming `.with_ctor<Args...>()`, once
  ///   per binding type and declaration, whichever overload is called.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// // trellis::bind<Config>.to_value(Config{}).with_ctor<int>();  // error: builds nothing
  /// @endcode
  template <class... Args>
  [[nodiscard]] constexpr const binding& with_ctor() const& noexcept {
    detail::reject_ctor_declaration<binding, Args...>();
    return *this;
  }
  /// @brief Rejects a constructor declaration, as the overload above does; returns the binding
  ///        as an rvalue, so that a move-only value is moved along, not copied.
  ///
  /// @return The same binding, unchanged.
  ///
  /// @par Constraints
  /// - Never valid: compilation fails with a static_assert naming `.with_ctor<Args...>()`, once
  ///   per binding type and declaration, whichever overload is called.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// // trellis::bind<Config>.to_value(Config{}).with_ctor<int>();  // error: builds nothing
  /// @endcode
  template <class... Args>
  [[nodiscard]] constexpr binding&& with_ctor() && noexcept {
    detail::reject_ctor_declaration<binding, Args...>();
    return std::move(*this);
  }

 private:
  template <class, class, class, bool>
  friend class binding;
  friend struct detail::binding_access;

  // Constrained, so that it never stands in for the copy and move constructors: for a non-const
  // binding lvalue, an unconstrained V&& would match better than const binding&.
  template <class V>
    requires(!TRELLIS_IS_SAME(std::remove_cvref_t<V>, binding))
  constexpr explicit binding(V&& value) noexcept(std::is_nothrow_constructible_v<Value, V>)
      : value_(std::forward<V>(value)) {}

  Value value_;
};

namespace detail {

// The object an external, shared or value binding holds, for the injector's storage (T1.5) and
// for tests: a pointer, the std::shared_ptr, or the value.
struct binding_access {
  template <class Key, class Object, bool AllowCopy>
  static constexpr Object* object(
      const binding<Key, Object, external_t, AllowCopy>& external) noexcept {
    return external.object_;
  }
  template <class Key, class Object, bool AllowCopy>
  static const std::shared_ptr<Object>& object(
      const binding<Key, Object, shared_external_t, AllowCopy>& shared) noexcept {
    return shared.object_;
  }
  template <class Key, class Value, bool AllowCopy>
  static constexpr const Value& object(
      const binding<Key, Value, value_t, AllowCopy>& value) noexcept {
    return value.value_;
  }
  // The injector owns a value binding's object, which the table lets consumers request as T&.
  template <class Key, class Value, bool AllowCopy>
  static constexpr Value& object(binding<Key, Value, value_t, AllowCopy>& value) noexcept {
    return value.value_;
  }
};

}  // namespace detail

// bind<I> below is declared auto, not with its type spelled out: the type is then deduced from
// the initializer, which is therefore instantiated wherever bind<I> is named, decltype included,
// and its constructor call checks the key. With the type spelled out, decltype(bind<I>) would
// never look at it.
/// @brief Starts a binding of the key `I`: the binding of `I` to itself, unique.
///
/// Chain `.to<C>()`, `.in(lifetime)`, `.allow_copy()` and `.with_ctor<Args...>()`, or
/// `.to_ref(x)`, `.to_ptr(p)`, `.to_value(v)` and `.to_shared(sp)`, then pass the binding to
/// `trellis::make_module` or `trellis::make_injector`. `bind<I>` alone is a binding too.
///
/// @tparam I The key: the type that consumers request, qualifiers included.
///
/// @par Constraints
/// - `I` is an object type without cv qualifiers that is not an array, such as `ILogger`, never
///   `const ILogger` or `ILogger&`; otherwise compilation fails with a static_assert naming
///   `bind<I>`. A request names its key without them: `const ILogger&` asks for the key
///   `ILogger`.
///
/// @par Errors
/// None. A key bound twice in one injector is `TRL-E003`, raised by `trellis::make_injector`.
///
/// @par Example
/// @code
/// auto injector = trellis::make_injector(
///     trellis::bind<ILogger>.to<ConsoleLogger>().in(trellis::singleton),
///     trellis::bind<IClock>.to<SystemClock>(),
///     trellis::bind<Config>.to_ref(config));
/// @endcode
template <class I>
inline constexpr auto bind = binding<I, I, unique_t>{};

}  // namespace trellis::inline v1

#endif  // TRELLIS_DSL_BIND_HPP
