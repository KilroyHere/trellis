// Spike S3 (throwaway): a mock of the static resolution pipeline of plan §7.3 and §7.8,
// with just enough machinery to measure how wiring errors render. It is not the
// library: constructor discovery is a hand-written `using inject = ctor<...>`, singleton
// storage is a function-local static, and there are only two lifetimes.
//
// Rendering variants, selected with -DS3_VARIANT=<n>:
//   1 naive       static_assert(resolvable<...>) and the creation code is still instantiated
//   2 leaf        the creation code checks each node and asserts where it meets the failure
//   3 explain     recommended C++20: `if constexpr` guard, explain pass, one static_assert whose
//                 condition names the error type, then a [[noreturn]] call instead of a return
//   4 noret       like 3 without the [[noreturn]] tail
//   5 incomplete  like 3, but reports by naming a member of an incomplete error class
//   6 ext         like 7 in C++20 mode: P2741 message used as a compiler extension
//   7 msg26       recommended C++26: explain pass + P2741 message built at compile time
//   8 delete      explain pass in a default template argument of a deleted overload of
//                 create(); = delete("reason") (P2573) where __cpp_deleted_function allows,
//                 plain = delete otherwise (run.sh calls it delete26 in C++26 mode)
//   9 guard       `if constexpr` guard + static_assert(resolvable<...>), no explain pass
#pragma once

#include <cstddef>
#include <cstdlib>
#include <memory>
#include <string_view>
#include <tuple>
#include <type_traits>

#include "type_name.hpp"

#define S3_NAIVE 1
#define S3_LEAF 2
#define S3_EXPLAIN 3
#define S3_NORET 4
#define S3_INCOMPLETE 5
#define S3_EXT 6
#define S3_MSG26 7
#define S3_DELETE26 8
#define S3_GUARD 9

#ifndef S3_VARIANT
#define S3_VARIANT S3_EXPLAIN
#endif

namespace trellis {

// The error vocabulary sits outside the versioned inline namespace so that compilers
// that print inline namespaces (GCC does) don't add "v1::" to every error type.
namespace error {
template <class... Consumers>
struct path {};  // the consumers from the root down to the failing request, root first
template <class Requested>
struct request {};  // the request form exactly as the consumer spelled it
struct see_backtrace {};  // variant 2 has no path; the compiler's backtrace stands in for it

// One class template per catalog entry (plan §7.8). `reported` is false so that a
// static_assert naming it fails, and the compiler prints its template arguments.
template <class T, class Request, class Path>
struct E001 {
  static constexpr bool reported = false;
};
template <class T, class Request, class Lifetime, class Path>
struct E005 {
  static constexpr bool reported = false;
};
template <class T, class Request, class Path>
struct E012 {
  static constexpr bool reported = false;
};

template <class Error>
struct incomplete;  // variant 5: declared, never defined
}  // namespace error

inline namespace v1 {

[[noreturn]] inline void unreachable() { std::abort(); }  // never runs: the TU fails to compile

struct unique {};
struct singleton {};

template <class Key, class Impl, class Lifetime>
struct binding {
  using key = Key;
  using impl = Impl;
  using lifetime = Lifetime;
};

// Inheritance-based map: one find() overload per binding, so a lookup is a single
// overload resolution instead of a recursive search; void means "not bound".
template <class K>
struct key_tag {};
template <class B>
struct map_entry {
  static B find(key_tag<typename B::key>);
};
template <class... Bs>
struct bindings : map_entry<Bs>... {
  using map_entry<Bs>::find...;
  static void find(...);
};
template <class Map, class T>
using lookup_t = decltype(Map::find(key_tag<T>{}));

// Stand-in for constructor discovery (spike S1): a component lists its parameters.
template <class... Ps>
struct ctor {};
template <class T>
struct ctor_of {
  using type = ctor<>;
};
template <class T>
  requires requires { typename T::inject; }
struct ctor_of<T> {
  using type = typename T::inject;
};
template <class Deps>
struct arity;
template <class... Ps>
struct arity<ctor<Ps...>> : std::integral_constant<std::size_t, sizeof...(Ps)> {};
template <std::size_t I, class Deps>
struct nth_dep;
template <std::size_t I, class... Ps>
struct nth_dep<I, ctor<Ps...>> {
  using type = std::tuple_element_t<I, std::tuple<Ps...>>;
};

enum class form : unsigned char { value, ref, cref, ptr, cptr, unique_ptr, shared_ptr };
template <class R>
struct form_of {
  using type = R;
  static constexpr form value = form::value;
};
template <class T>
struct form_of<T&> {
  using type = T;
  static constexpr form value = form::ref;
};
template <class T>
struct form_of<const T&> {
  using type = T;
  static constexpr form value = form::cref;
};
template <class T>
struct form_of<T*> {
  using type = T;
  static constexpr form value = form::ptr;
};
template <class T>
struct form_of<const T*> {
  using type = T;
  static constexpr form value = form::cptr;
};
template <class T>
struct form_of<std::unique_ptr<T>> {
  using type = T;
  static constexpr form value = form::unique_ptr;
};
template <class T>
struct form_of<std::shared_ptr<T>> {
  using type = T;
  static constexpr form value = form::shared_ptr;
};

// ok: resolvable; e00x: this node fails; dep: this node is fine but a dependency fails
enum class status : unsigned char { ok, e001, e005, e012, dep };

// The request-form table of plan §6.3, reduced to two lifetimes.
template <bool Bound, bool Abstract, class Lifetime, form F>
constexpr status classify() {
  constexpr bool sharing = F == form::ref || F == form::cref || F == form::ptr || F == form::cptr;
  constexpr bool owning = F == form::value || F == form::unique_ptr;
  if constexpr (!Bound) {
    if constexpr (Abstract) return status::e001;
    return owning ? status::ok : status::e012;  // unbound concrete types are implicitly unique
  } else if constexpr (std::is_same_v<Lifetime, unique>) {
    return sharing ? status::e005 : status::ok;  // nothing would keep the object alive
  } else {
    return owning ? status::e005 : status::ok;  // copies hide sharing / duplicate ownership
  }
}

template <class Map, class Deps, class Consumer>
struct deps_ok;

// Normal pass: one instantiation per (bindings, request form, consumer lifetime). No
// path and no depth in the key (plan §7.3), so the compiler memoizes it.
template <class Map, class R, class Consumer>
struct node {
  using type = typename form_of<R>::type;
  static constexpr form f = form_of<R>::value;
  using found = lookup_t<Map, type>;
  static constexpr bool bound = !std::is_void_v<found>;
  using bind = std::conditional_t<bound, found, binding<type, type, unique>>;
  using impl = typename bind::impl;
  using lifetime = typename bind::lifetime;
  using deps = typename ctor_of<impl>::type;
  static constexpr status self = classify<bound, std::is_abstract_v<type>, lifetime, f>();
  // conjunction: the dependencies are only looked at when the node itself is fine.
  static constexpr bool ok =
      std::conjunction_v<std::bool_constant<self == status::ok>, deps_ok<Map, deps, lifetime>>;
};

template <class Map, class... Ps, class Consumer>
struct deps_ok<Map, ctor<Ps...>, Consumer>
    : std::bool_constant<(node<Map, Ps, Consumer>::ok && ...)> {};

template <class Map, class R, class Consumer = unique>
concept resolvable = node<Map, R, Consumer>::ok;

// ---- Explain pass (plan §7.8) ----------------------------------------------------
// Runs only after resolvable<> failed. Starting at the root it reads the memoized
// node<>::ok of each constructor parameter, descends into the first failing one with
// the path extended, and classifies the deepest failure. Cost: one explain_at<> per hop
// and one memo read per parameter on that path, O(depth x arity), and nothing at all
// on the normal pass.
template <class Map, class Consumer, class... Ps>
constexpr std::size_t first_failing(ctor<Ps...>) {
  constexpr bool ok[] = {node<Map, Ps, Consumer>::ok..., false};  // false: sentinel
  std::size_t i = 0;
  while (ok[i]) ++i;
  return i;
}

template <class Map, class R, class Consumer>
constexpr status explain_status() {
  using N = node<Map, R, Consumer>;
  if constexpr (N::ok) return status::ok;
  return N::self == status::ok ? status::dep : N::self;
}

template <status S, class Map, class R, class Consumer, class Path, std::size_t Param>
struct explain_at;

template <class Map, class R, class Consumer = unique, class Path = error::path<>,
          std::size_t Param = 0>
using explain = explain_at<explain_status<Map, R, Consumer>(), Map, R, Consumer, Path, Param>;

// Common part of a classified failure.
template <status S, class Map, class R, class Consumer, class Path, std::size_t Param>
struct failure {
  using N = node<Map, R, Consumer>;
  static constexpr status code = S;
  using failing = typename N::type;
  using requested = R;
  using lifetime = typename N::lifetime;
  using path = Path;
  static constexpr std::size_t param = Param;  // 1-based parameter of the last consumer; 0: root
  static constexpr std::size_t hops = 0;
  static constexpr std::size_t reads = 0;
};

template <class Map, class R, class C, class Path, std::size_t Param>
struct explain_at<status::ok, Map, R, C, Path, Param> {
  using type = void;  // nothing to explain (used by the deleted overload in variant 8)
};
template <class Map, class R, class C, class Path, std::size_t Param>
struct explain_at<status::e001, Map, R, C, Path, Param>
    : failure<status::e001, Map, R, C, Path, Param> {
  using type = error::E001<typename node<Map, R, C>::type, error::request<R>, Path>;
};
template <class Map, class R, class C, class Path, std::size_t Param>
struct explain_at<status::e005, Map, R, C, Path, Param>
    : failure<status::e005, Map, R, C, Path, Param> {
  using type = error::E005<typename node<Map, R, C>::type, error::request<R>,
                           typename node<Map, R, C>::lifetime, Path>;
};
template <class Map, class R, class C, class Path, std::size_t Param>
struct explain_at<status::e012, Map, R, C, Path, Param>
    : failure<status::e012, Map, R, C, Path, Param> {
  using type = error::E012<typename node<Map, R, C>::type, error::request<R>, Path>;
};

// This node is fine but a dependency is not: descend into the first failing parameter.
template <class Map, class R, class C, class... Consumers, std::size_t Param>
struct explain_at<status::dep, Map, R, C, error::path<Consumers...>, Param> {
  using N = node<Map, R, C>;
  static constexpr std::size_t index = first_failing<Map, typename N::lifetime>(typename N::deps{});
  using next = typename nth_dep<index, typename N::deps>::type;
  using inner = explain<Map, next, typename N::lifetime,
                        error::path<Consumers..., typename N::impl>, index + 1>;

  using type = typename inner::type;
  static constexpr status code = inner::code;
  using failing = typename inner::failing;
  using requested = typename inner::requested;
  using lifetime = typename inner::lifetime;
  using path = typename inner::path;
  static constexpr std::size_t param = inner::param;
  static constexpr std::size_t hops = inner::hops + 1;
  static constexpr std::size_t reads = inner::reads + arity<typename N::deps>::value;
};

// ---- C++26 rendering: the message is built at compile time (P2741) -----------------
// Two passes over the same renderer: one counts the characters, the second writes
// them into a buffer of exactly that size. Usable in C++20 too, e.g. for printing.
struct char_counter {
  std::size_t n = 0;
  constexpr void operator()(std::string_view s) { n += s.size(); }
};
template <std::size_t N>
struct fixed_string {
  char buf[N + 1]{};
  std::size_t len = 0;
  constexpr void operator()(std::string_view s) {
    for (char c : s) buf[len++] = c;
  }
  constexpr std::size_t size() const { return len; }
  constexpr const char* data() const { return buf; }
};

template <class Out>
constexpr void render_number(Out& out, std::size_t v) {
  char digits[20]{};
  std::size_t n = 0;
  do {
    digits[n++] = static_cast<char>('0' + v % 10);
    v /= 10;
  } while (v != 0);
  char text[20]{};
  for (std::size_t i = 0; i < n; ++i) text[i] = digits[n - 1 - i];
  out(std::string_view(text, n));
}

template <class Out, class... Cs>
constexpr void render_path(Out& out, error::path<Cs...>) {
  ((out(s3::type_name<Cs>()), out(" -> ")), ...);
}
template <class Out, class... Cs>
constexpr void render_consumer(Out& out, error::path<Cs...>) {
  if constexpr (sizeof...(Cs) == 0) {
    out("create<>()");
  } else {
    using last = std::tuple_element_t<sizeof...(Cs) - 1, std::tuple<Cs...>>;
    out("'");
    out(s3::type_name<last>());
    out("'");
  }
}

template <class X, class Out>
constexpr void render(Out& out) {
  constexpr std::string_view t = s3::type_name<typename X::failing>();
  constexpr std::string_view r = s3::type_name<typename X::requested>();
  if constexpr (X::code == status::e001) {
    out("trellis [TRL-E001]: no binding for abstract type '");
    out(t);
    out("'");
  } else if constexpr (X::code == status::e005) {
    out("trellis [TRL-E005]: '");
    out(r);
    if constexpr (std::is_same_v<typename X::lifetime, unique>)
      out("' cannot be served by a unique binding: nothing would keep the object alive");
    else
      out("' cannot be served by a singleton binding: it would copy or re-own the shared object");
  } else {
    out("trellis [TRL-E012]: unbound concrete type '");
    out(t);
    out("' requested by reference, pointer or shared_ptr: nobody would own it");
  }
  out("\n  requested as '");
  out(r);
  out("' by ");
  render_consumer(out, typename X::path{});
  if constexpr (X::param != 0) {
    out(" (parameter ");
    render_number(out, X::param);
    out(")");
  }
  out("\n  path: ");
  render_path(out, typename X::path{});
  out(t);
  out("\n  fix: ");
  if constexpr (X::code == status::e001) {
    out("add bind<");
    out(t);
    out(">.to<YourImplementation>() (docs: errors/E001)");
  } else if constexpr (X::code == status::e005) {
    if constexpr (std::is_same_v<typename X::lifetime, unique>) {
      out("bind '");
      out(t);
      out("' in singleton, or request it by value or std::unique_ptr (docs: errors/E005)");
    } else {
      out("request it as T& or std::shared_ptr<T>, or opt in with .allow_copy() (docs: errors/E005)");
    }
  } else {
    out("bind '");
    out(t);
    out("' with an owning lifetime such as singleton, or request it by value (docs: errors/E012)");
  }
}

template <class X>
constexpr std::size_t message_size() {
  char_counter c;
  render<X>(c);
  return c.n;
}
template <class X>
constexpr auto make_message() {
  fixed_string<message_size<X>()> s;
  render<X>(s);
  return s;
}
template <class X>
inline constexpr auto message = make_message<X>();

// ---- C++20 rendering: one static_assert per error kind, each with its own literal ----
// Written out inline (no helper function, no macro) because each extra frame adds
// "required from" / "in expansion of macro" lines. Only the branch for the actual kind
// is instantiated, so exactly one assertion fires.

template <class Module>
class injector {
 public:
  using map = typename Module::bindings;

#if S3_VARIANT == S3_DELETE26
  template <class R>
    requires resolvable<map, R>
  R create() {
    return make<R, false>();
  }
  // The explain pass runs in a default template argument, so the error type shows up
  // in the compiler's "candidate ... [with ...]" note. The reason must be a literal.
  template <class R, class Error = typename explain<map, R>::type>
    requires(!resolvable<map, R>)
  R create()
#if defined(__cpp_deleted_function) && __cpp_deleted_function >= 202403L
      = delete("trellis: wiring error; the error ID, type, request and path are in 'Error' below");
#else
      = delete;
#endif
#else
  template <class R>
  R create() {
#if S3_VARIANT == S3_NAIVE
    static_assert(resolvable<map, R>, "trellis: the object graph cannot be created");
    return make<R, false>();
#elif S3_VARIANT == S3_LEAF
    return make<R, true>();
#elif S3_VARIANT == S3_GUARD
    if constexpr (resolvable<map, R>) {
      return make<R, false>();
    } else {
      static_assert(resolvable<map, R>, "trellis: the object graph cannot be created");
      unreachable();
    }
#else
    if constexpr (resolvable<map, R>) {
      return make<R, false>();
    } else {
      using X = explain<map, R>;
#if S3_VARIANT == S3_EXPLAIN || S3_VARIANT == S3_NORET
      using err = typename X::type;
      if constexpr (X::code == status::e001)
        static_assert(err::reported, "TRL-E001: no binding for an abstract type");
      else if constexpr (X::code == status::e005)
        static_assert(err::reported, "TRL-E005: request form incompatible with the lifetime");
      else
        static_assert(err::reported, "TRL-E012: unbound concrete type requested by reference");
#elif S3_VARIANT == S3_INCOMPLETE
      static_assert(error::incomplete<typename X::type>::reported);
#elif S3_VARIANT == S3_EXT
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wc++26-extensions"
      static_assert(false, message<X>);
#pragma GCC diagnostic pop
#elif S3_VARIANT == S3_MSG26
      static_assert(false, message<X>);
#endif
#if S3_VARIANT != S3_NORET
      unreachable();  // no return statement is needed after a [[noreturn]] call
#endif
    }
#endif
  }
#endif

 private:
  // Creation code. Written for valid requests only; with Checked (variant 2) each node
  // checks itself first, so the failure is reported where the creation code meets it.
  template <class R, bool Checked>
  R make() {
    using N = node<map, R, unique>;
    if constexpr (Checked && N::self != status::ok) {
      using err = typename explain_at<N::self, map, R, unique, error::see_backtrace, 0>::type;
      if constexpr (N::self == status::e001)
        static_assert(err::reported, "TRL-E001: no binding for an abstract type");
      else if constexpr (N::self == status::e005)
        static_assert(err::reported, "TRL-E005: request form incompatible with the lifetime");
      else
        static_assert(err::reported, "TRL-E012: unbound concrete type requested by reference");
      unreachable();
    } else if constexpr (N::f == form::value) {
      return build_value<typename N::impl, Checked>(typename N::deps{});
    } else if constexpr (N::f == form::unique_ptr) {
      return build_unique<typename N::impl, Checked>(typename N::deps{});
    } else if constexpr (N::f == form::shared_ptr) {
      if constexpr (std::is_same_v<typename N::lifetime, singleton>)
        return slot<typename N::bind, Checked>();
      else
        return build_shared<typename N::impl, Checked>(typename N::deps{});
    } else if constexpr (N::f == form::ref || N::f == form::cref) {
      return *slot<typename N::bind, Checked>();
    } else {
      return slot<typename N::bind, Checked>().get();
    }
  }
  template <class Impl, bool C, class... Ps>
  Impl build_value(ctor<Ps...>) {
    return Impl(make<Ps, C>()...);
  }
  template <class Impl, bool C, class... Ps>
  std::unique_ptr<Impl> build_unique(ctor<Ps...>) {
    return std::make_unique<Impl>(make<Ps, C>()...);
  }
  template <class Impl, bool C, class... Ps>
  std::shared_ptr<Impl> build_shared(ctor<Ps...>) {
    return std::make_shared<Impl>(make<Ps, C>()...);
  }
  // Mock singleton storage (the real design keeps slots in the injector, ADR-05).
  template <class B, bool C>
    requires std::is_same_v<typename B::lifetime, singleton>
  std::shared_ptr<typename B::impl>& slot() {
    static std::shared_ptr<typename B::impl> s =
        build_shared<typename B::impl, C>(typename ctor_of<typename B::impl>::type{});
    return s;
  }
};

}  // namespace v1
}  // namespace trellis
