// Spike S1: a minimal injector on top of deduce.hpp, for spike S2 to build on.
//
//   s1::injector<s1::singleton<Config>, s1::singleton<Log>> inj;
//   App app = inj.create<App>();
//
// Rules (a subset of plan section 6.3): unbound concrete classes are unique;
// singleton<T> makes T one object per injector, created on first use.
//   T                  unique only           (a singleton by value would be a copy)
//   T&, const T&       singleton only        (a unique object would dangle)
//   T*, const T*       singleton only
//   unique_ptr<T>      unique only
//   shared_ptr<T>      unique only (sole owner) in this stub
// Anything else (int, std::function, ...) is not resolvable here.
//
// Memoization: can_resolve<R> is a variable template, evaluated once per
// (injector, request type). The consumer lifetime is not in the key yet (S2).
#pragma once

#include <exception>
#include <memory>
#include <new>
#include <tuple>
#include <type_traits>

#include "deduce.hpp"

namespace s1 {

template <class T>
struct singleton {};

enum class form { value, ref, ptr, unique_ptr, shared_ptr, other };

// What a request type asks for: its form and the class it is about.
template <class R>
struct request {
  static constexpr form kind = std::is_class_v<R> ? form::value : form::other;
  using key = std::remove_cv_t<R>;
};
template <class R>
struct request<R&> {
  static constexpr form kind = std::is_class_v<std::remove_cv_t<R>> ? form::ref : form::other;
  using key = std::remove_cv_t<R>;
};
template <class R>
struct request<R*> {
  static constexpr form kind = std::is_class_v<std::remove_cv_t<R>> ? form::ptr : form::other;
  using key = std::remove_cv_t<R>;
};
template <class R>
struct request<std::unique_ptr<R>> {
  static constexpr form kind = form::unique_ptr;
  using key = R;
};
template <class R>
struct request<std::shared_ptr<R>> {
  static constexpr form kind = form::shared_ptr;
  using key = R;
};

// One lazily built object. Built from a prvalue, so T need not be movable.
template <class T>
class slot {
 public:
  slot() = default;
  slot(const slot&) = delete;
  slot& operator=(const slot&) = delete;
  ~slot() {
    if (ptr_ != nullptr) ptr_->~T();
  }
  template <class Make>
  T& get(Make&& make) {
    if (ptr_ == nullptr) ptr_ = ::new (static_cast<void*>(storage_)) T(make());
    return *ptr_;
  }

 private:
  alignas(T) unsigned char storage_[sizeof(T)];
  T* ptr_ = nullptr;
};

template <class... Bindings>
class injector;

template <class... Singletons>
class injector<singleton<Singletons>...> {
  template <class K>
  static constexpr bool is_singleton = (std::is_same_v<K, Singletons> || ...);

  template <class K>
  static consteval ctor_status status_of() {
    if constexpr (std::is_class_v<K>)
      return ctor_info<K>::status;
    else
      return ctor_status::not_class;
  }

  template <class R>
  static consteval bool can_resolve_impl() {
    using q = request<R>;
    using K = typename q::key;
    if constexpr (q::kind == form::other)
      return false;
    else if constexpr (q::kind == form::ref || q::kind == form::ptr)
      return is_singleton<K> && constructible_from<K, injector>();
    else
      return !is_singleton<K> && constructible_from<K, injector>();
  }

 public:
  injector() = default;
  injector(const injector&) = delete;  // any_arg and the slots refer to this object
  injector& operator=(const injector&) = delete;

  template <class R>
  static constexpr bool can_resolve = can_resolve_impl<R>();

  template <class R>
  R resolve() {
    using q = request<R>;
    using K = typename q::key;
    if constexpr (q::kind == form::ref)
      return std::get<slot<K>>(slots_).get([this] { return construct<K>(*this); });
    else if constexpr (q::kind == form::ptr)
      return &std::get<slot<K>>(slots_).get([this] { return construct<K>(*this); });
    else if constexpr (q::kind == form::unique_ptr)
      return std::unique_ptr<K>(construct_new<K>(*this));
    else if constexpr (q::kind == form::shared_ptr)
      return std::shared_ptr<K>(construct_new<K>(*this));
    else
      return construct<K>(*this);
  }

  // The root request. Each root cause gives exactly one static_assert with an
  // error ID, and nothing else is instantiated after it (no cascade). Which
  // parameter failed is the explain pass's job (plan 7.8), not this stub's.
  template <class R>
  R create() {
    using K = typename request<R>::key;
    constexpr form f = request<R>::kind;
    constexpr ctor_status st = status_of<K>();
    constexpr bool by_ref = f == form::ref || f == form::ptr;
    if constexpr (st == ctor_status::not_class) {
      static_assert(st != ctor_status::not_class, "TRL-E002: non-class types must be bound");
    } else if constexpr (st == ctor_status::abstract) {
      static_assert(st != ctor_status::abstract, "TRL-E001: no binding for an abstract type");
    } else if constexpr (st == ctor_status::greedy) {
      static_assert(st != ctor_status::greedy,
                    "TRL-E004: a constructor template or variadic constructor accepts any argument; "
                    "declare the constructor with ctor_traits");
    } else if constexpr (st == ctor_status::over_limit) {
      static_assert(st != ctor_status::over_limit, "TRL-E010: constructor arity above max_ctor_arity");
    } else if constexpr (st == ctor_status::not_constructible) {
      static_assert(st != ctor_status::not_constructible,
                    "TRL-E002: no accessible, unambiguous constructor (private, deleted, or same-arity "
                    "overloads?)");
    } else if constexpr (by_ref && !is_singleton<K>) {
      static_assert(!by_ref, "TRL-E005: a unique object requested by reference or pointer would dangle");
    } else if constexpr (!by_ref && is_singleton<K>) {
      static_assert(by_ref, "TRL-E005: a singleton requested by value or as an owner would be copied");
    } else if constexpr (!can_resolve<R>) {
      static_assert(can_resolve<R>, "TRL-E002: a constructor parameter is not resolvable");
    } else {
      return resolve<R>();
    }
    std::terminate();  // reached only after a static_assert above has failed
  }

 private:
  std::tuple<slot<Singletons>...> slots_;
};

}  // namespace s1
