// Trellis static engine: where an injector keeps the objects it owns, the slots of its singletons,
// and its teardown stack (plan §7.5, §7.6, ADR-0005, ADR-0012, spike S4).
// Copyright (c) 2026 Aryan Patel
// SPDX-License-Identifier: BSL-1.0
//
// Internal (trellis::detail): the injector's storage, not user API. docs/internals/
// resolution-and-storage.md explains each technique and lists its tests.
//
// An injector owns the objects of its singleton and singleton_inline bindings, one per binding.
// Each such binding has a slot in the injector (ADR-0005):
//
//   object_slot<shared_storage<Impl>, Mode>  singleton: a std::shared_ptr<Impl>, so that a
//                                            shared_ptr request returns real shared ownership;
//                                            24 bytes.
//   object_slot<inline_storage<Impl>, Mode>  singleton_inline: the object's bytes, inside the
//                                            injector, with no heap allocation; sizeof(Impl) + 1,
//                                            rounded up to alignof(Impl).
//
// The first request of a slot's object builds it, and later requests read it. Under
// threading::safe (the default) a slot is spike S4's state machine, empty -> constructing ->
// ready, whose losers spin briefly and then block on the state byte with atomic::wait (ADR-0012
// item 2); under threading::none it is a plain flag. A constructor that throws leaves the slot
// empty, and the next request retries (ADR-0012 item 6).
//
// A slot doesn't know how to build its object: it calls a build_fn, a plain function pointer that
// the resolution code (static/resolve.hpp) passes in. That keeps the slots apart from resolution,
// and it keeps the creation code of a chain of singletons shallow: the function that builds a
// singleton is instantiated where its address is taken, in the request's own resolve<R>(), not
// several levels down inside the slot or inside std::allocate_shared (NFR-09, ADR-0014 item 3).
//
// Each slot pushes its object onto the injector's teardown stack once it is built, and the stack
// destroys them in reverse order of construction when the injector is destroyed (ADR-0005 item 4).
#ifndef TRELLIS_STATIC_STORAGE_HPP
#define TRELLIS_STATIC_STORAGE_HPP

#include <trellis/config.hpp>
#include <trellis/dsl/bind.hpp>
#include <trellis/dsl/lifetimes.hpp>
#include <trellis/dsl/module.hpp>
#include <trellis/dsl/options.hpp>
#include <trellis/meta/type_list.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>
#if !TRELLIS_HAS_ATOMIC_WAIT
#include <condition_variable>
#include <mutex>
#endif

namespace trellis::inline v1::detail {

// build_fn: builds an object at `where`, the suitably sized and aligned storage of a slot or of a
// std::shared_ptr's control block, resolving its constructor's requests from the injector at
// `injector`. static/resolve.hpp makes one per (class, binding lifetime). The storage is passed as
// bytes, a type of its own, so that the two pointers can't be swapped by mistake.
using build_fn = void (*)(void* injector, std::byte* where);

// ---- the teardown stack (ADR-0005 item 4) -----------------------------------------------------

using destroy_fn = void (*)(void* object) noexcept;

struct teardown_entry {
  destroy_fn destroy;
  void* object;
};

// teardown_stack<Capacity, ThreadSafe>: the objects an injector has built, in order of
// construction. Capacity is the number of slots, known at compile time, so the stack needs no
// heap: 16 bytes per entry and a counter. Each slot pushes exactly once, after its object's
// constructor succeeds and before it publishes `ready` (ADR-0005 item 4, rule 1). So the stack
// can't overflow, a failed construction leaves no entry, and a dependency, which is ready before
// its dependent's constructor runs, always sits below its dependents, even when different threads
// build them: the dependent's thread acquired the dependency's `ready`, published after the
// dependency's push. Under threading::safe the counter is an atomic incremented with a relaxed
// fetch_add, which hands every pushing thread its own entry.
//
// The injector declares its stack after its slots, so the stack is destroyed, and destroys the
// objects, before the slots' storage is (ADR-0005 item 4, rule 2).
template <std::size_t Capacity, bool ThreadSafe>
class teardown_stack {
 public:
  teardown_stack() = default;
  teardown_stack(const teardown_stack&) = delete;
  teardown_stack(teardown_stack&&) = delete;
  teardown_stack& operator=(const teardown_stack&) = delete;
  teardown_stack& operator=(teardown_stack&&) = delete;
  ~teardown_stack() { unwind(); }

  // Records a built object. At most Capacity pushes ever happen: one per slot.
  void push(destroy_fn destroy, void* object) noexcept {
    std::size_t index = 0;
    if constexpr (ThreadSafe) {
      index = size_.fetch_add(1, std::memory_order_relaxed);
    } else {
      index = size_++;
    }
    entries_[index] = teardown_entry{destroy, object};
  }

  // Destroys the objects in reverse order of construction. Called by the destructor, which no other
  // thread may run concurrently with a request: the injector is going away.
  void unwind() noexcept {
    std::size_t remaining = size();
    while (remaining != 0) {
      --remaining;
      entries_[remaining].destroy(entries_[remaining].object);
    }
    if constexpr (ThreadSafe) {
      size_.store(0, std::memory_order_relaxed);
    } else {
      size_ = 0;
    }
  }

  // The number of objects built so far.
  [[nodiscard]] std::size_t size() const noexcept {
    if constexpr (ThreadSafe) {
      return size_.load(std::memory_order_relaxed);
    } else {
      return size_;
    }
  }

 private:
  std::array<teardown_entry, Capacity> entries_{};
  std::conditional_t<ThreadSafe, std::atomic<std::size_t>, std::size_t> size_{0};
};

// An injector without singletons has nothing to tear down.
template <bool ThreadSafe>
class teardown_stack<0, ThreadSafe> {
 public:
  void unwind() noexcept {}
  [[nodiscard]] static constexpr std::size_t size() noexcept { return 0; }
};

// ---- storage policies (ADR-0005 items 2 and 3) ------------------------------------------------

// inline_storage<T>: the bytes of a T, inside the injector (singleton_inline). Whether they hold
// a T is the slot's state, which serves as the engaged flag, so there is no flag here. The
// teardown stack destroys the object; the storage's own destructor does nothing.
template <class T>
class inline_storage {
 public:
  using value_type = T;

  inline_storage() = default;
  inline_storage(const inline_storage&) = delete;
  inline_storage(inline_storage&&) = delete;
  inline_storage& operator=(const inline_storage&) = delete;
  inline_storage& operator=(inline_storage&&) = delete;
  ~inline_storage() = default;

  void construct(build_fn build, void* injector) { build(injector, bytes_.data()); }
  T& object() noexcept { return *std::launder(reinterpret_cast<T*>(bytes_.data())); }
  static void destroy(void* self) noexcept { static_cast<inline_storage*>(self)->object().~T(); }

 private:
  alignas(T) std::array<std::byte, sizeof(T)> bytes_;
};

// construct_with<T>: an allocator whose construct() builds the object with a build_fn, so that
// std::allocate_shared makes the object and its control block in one allocation, as
// std::make_shared would, while Trellis builds the object itself: with braces for an aggregate,
// with parentheses otherwise (ADR-0003), in place, so that the class needn't be copyable or
// movable. allocate_shared creates the object through allocator_traits<A>::construct(a, p) with
// no further arguments ([util.smartptr.shared.create]/7.2), which calls this construct(p). A
// shared_ptr made this way also sets up std::enable_shared_from_this, which an aliasing
// shared_ptr wouldn't. Memory comes from std::allocator, so all construct_with allocators compare
// equal. Checked on libstdc++ 13 and 14 and libc++ 18 (tests/unit/create_test.cpp); MS STL calls
// allocator_traits the same way, untested locally.
template <class T>
struct construct_with {
  using value_type = T;
  using is_always_equal = std::true_type;

  constexpr construct_with(build_fn build_with, void* from) noexcept
      : build(build_with), injector(from) {}
  // Rebinding: allocate_shared allocates its control block through a copy for another type.
  template <class U>
  constexpr construct_with(const construct_with<U>& other) noexcept
      : build(other.build), injector(other.injector) {}

  [[nodiscard]] T* allocate(std::size_t count) { return std::allocator<T>{}.allocate(count); }
  void deallocate(T* pointer, std::size_t count) noexcept {
    std::allocator<T>{}.deallocate(pointer, count);
  }
  template <class U>
  void construct(U* where) {
    build(injector, reinterpret_cast<std::byte*>(where));
  }

  build_fn build;
  void* injector;
};

template <class T, class U>
constexpr bool operator==(const construct_with<T>& /*left*/,
                          const construct_with<U>& /*right*/) noexcept {
  return true;
}

// shared_storage<T>: a std::shared_ptr<T> that the injector holds (singleton). Teardown drops the
// injector's reference; a shared_ptr that a consumer holds keeps the object alive after it, and
// weak_ptrs expire when the last one goes (ADR-0005 item 2).
template <class T>
class shared_storage {
 public:
  using value_type = T;

  void construct(build_fn build, void* injector) {
    pointer_ = std::allocate_shared<T>(construct_with<T>(build, injector));
  }
  T& object() noexcept { return *pointer_; }
  [[nodiscard]] const std::shared_ptr<T>& pointer() const noexcept { return pointer_; }
  static void destroy(void* self) noexcept { static_cast<shared_storage*>(self)->pointer_.reset(); }

 private:
  std::shared_ptr<T> pointer_;
};

// ---- slots (ADR-0012) -------------------------------------------------------------------------

// A threading::safe slot's state: empty, constructing or ready in the low two bits, and a bit
// that a thread sets before it blocks, so that the constructing thread knows to wake it. Without
// waiters, a first access costs one compare-exchange and one exchange: no lock and no notify.
namespace slot_state {
inline constexpr std::uint8_t empty = 0;
inline constexpr std::uint8_t constructing = 1;
inline constexpr std::uint8_t ready = 2;
inline constexpr std::uint8_t phase = 3;
inline constexpr std::uint8_t waiters = 4;
}  // namespace slot_state

// How resolution reaches a slot's object: checked, building it on its first request (a lazy
// injector, and an eager injector while make_injector builds every singleton); or ready, reading it
// directly, with no state to test, once an eager injector has built them all (plan §7.6: "after
// make_injector returns, every access is a plain read"; ADR-0012 item 7).
enum class slot_access : unsigned char { checked, ready };

// How often a thread that finds a slot under construction re-reads its state, with a CPU pause in
// between, before it blocks: spike S4's sweep on gcc-14 (ADR-0012 item 2). A short construction
// then finishes without anybody blocking, so its thread skips notify_all, a futex call.
inline constexpr int slot_spin_limit = 128;

// What the slots of an injector share for blocking: nothing where atomic::wait exists, otherwise a
// mutex and a condition variable per injector (ADR-0012 item 3).
struct no_sync {};
#if !TRELLIS_HAS_ATOMIC_WAIT
struct injector_sync {
  std::mutex mutex;
  std::condition_variable woken;
};
#endif

template <threading Mode>
struct sync_of {
  using type = no_sync;
};
#if !TRELLIS_HAS_ATOMIC_WAIT
template <>
struct sync_of<threading::safe> {
  using type = injector_sync;
};
#endif

// object_slot<Storage, Mode>: the storage of one object the injector owns, and whether it is built.
// ready() and object() are the fast path, inlined into the request; construct() is the slow path,
// out of line, which builds the object exactly once.
template <class Storage, threading Mode>
class object_slot;

// threading::safe: spike S4's C+spin (ADR-0012 item 2), with retry after a throw (item 6).
// Detecting re-entry by the constructing thread itself, R004 in plan §7.8, comes with lazy, in
// Phase 2. Without lazy nothing can re-enter a slot, as a cycle doesn't compile (ADR-0014 item 2).
template <class Storage>
class object_slot<Storage, threading::safe> {
 public:
  using value_type = typename Storage::value_type;

  object_slot() = default;
  object_slot(const object_slot&) = delete;
  object_slot(object_slot&&) = delete;
  object_slot& operator=(const object_slot&) = delete;
  object_slot& operator=(object_slot&&) = delete;
  ~object_slot() = default;

  // The fast path: an acquire load and a compare, a plain load on x86. It synchronizes with the
  // constructing thread's publish, so the object is visible after it.
  [[nodiscard]] bool ready() const noexcept {
    return state_.load(std::memory_order_acquire) == slot_state::ready;
  }
  value_type& object() noexcept { return storage_.object(); }
  Storage& storage() noexcept { return storage_; }

  // The slow path: builds the object with build, unless another thread builds it first; then
  // returns once it is ready. If the constructor throws, the slot returns to empty, waiting threads
  // wake, the exception propagates to this caller only, and the next request retries.
  template <class Teardown, class Sync>
  TRELLIS_NOINLINE void construct(Teardown& teardown, [[maybe_unused]] Sync& sync, build_fn build,
                                  void* injector) {
    for (;;) {
      std::uint8_t observed = slot_state::empty;
      if (state_.compare_exchange_strong(observed, slot_state::constructing,
                                         std::memory_order_acquire)) {
        build_and_publish(teardown, sync, build, injector);
        return;
      }
      if (observed == slot_state::ready) {
        return;
      }
      for (int spin = 0;
           spin != slot_spin_limit && (observed & slot_state::phase) == slot_state::constructing;
           ++spin) {
        TRELLIS_CPU_RELAX();
        observed = state_.load(std::memory_order_acquire);
      }
      if ((observed & slot_state::phase) == slot_state::constructing) {
        wait_while_constructing(sync, observed);
      }
      // Ready, so the next compare-exchange fails and returns; or empty again after a throw, so
      // this thread may build it now.
    }
  }

 private:
  template <class Teardown, class Sync>
  void build_and_publish(Teardown& teardown, Sync& sync, build_fn build, void* injector) {
#if TRELLIS_HAS_EXCEPTIONS
    try {
      storage_.construct(build, injector);
    } catch (...) {
      publish(sync, slot_state::empty);
      throw;
    }
#else
    storage_.construct(build, injector);
#endif
    teardown.push(&Storage::destroy, &storage_);
    publish(sync, slot_state::ready);
  }

#if TRELLIS_HAS_ATOMIC_WAIT
  // Sets the waiters bit, unless set already, and blocks until the state changes. A failed
  // compare-exchange means the state changed meanwhile, so the caller just looks again.
  void wait_while_constructing(no_sync& /*sync*/, std::uint8_t observed) {
    if ((observed & slot_state::waiters) == 0) {
      const auto announced = static_cast<std::uint8_t>(observed | slot_state::waiters);
      if (!state_.compare_exchange_weak(observed, announced, std::memory_order_acquire)) {
        return;
      }
      observed = announced;
    }
    state_.wait(observed, std::memory_order_acquire);
  }

  void publish(no_sync& /*sync*/, std::uint8_t state) noexcept {
    if ((state_.exchange(state, std::memory_order_release) & slot_state::waiters) != 0) {
      state_.notify_all();
    }
  }
#else
  // The fallback (ADR-0012 item 3): the waiters bit is set while holding the injector's mutex, and
  // the constructing thread locks that mutex before it notifies, which it can only do once the
  // waiter is inside wait(), so no wake-up is lost.
  void wait_while_constructing(injector_sync& sync, std::uint8_t observed) {
    std::unique_lock<std::mutex> lock(sync.mutex);
    observed = state_.load(std::memory_order_acquire);
    while ((observed & slot_state::phase) == slot_state::constructing) {
      if ((observed & slot_state::waiters) == 0 &&
          !state_.compare_exchange_weak(observed,
                                        static_cast<std::uint8_t>(observed | slot_state::waiters),
                                        std::memory_order_acquire)) {
        continue;
      }
      sync.woken.wait(lock);
      observed = state_.load(std::memory_order_acquire);
    }
  }

  void publish(injector_sync& sync, std::uint8_t state) noexcept {
    if ((state_.exchange(state, std::memory_order_release) & slot_state::waiters) != 0) {
      {
        const std::lock_guard<std::mutex> lock(sync.mutex);
      }
      sync.woken.notify_all();
    }
  }
#endif

  std::atomic<std::uint8_t> state_{slot_state::empty};
  Storage storage_;
};

// threading::none: a plain flag (ADR-0012 item 7). One thread at a time uses the injector.
template <class Storage>
class object_slot<Storage, threading::none> {
 public:
  using value_type = typename Storage::value_type;

  object_slot() = default;
  object_slot(const object_slot&) = delete;
  object_slot(object_slot&&) = delete;
  object_slot& operator=(const object_slot&) = delete;
  object_slot& operator=(object_slot&&) = delete;
  ~object_slot() = default;

  [[nodiscard]] bool ready() const noexcept { return ready_; }
  value_type& object() noexcept { return storage_.object(); }
  Storage& storage() noexcept { return storage_; }

  // Builds the object. A throw leaves the flag clear, so the next request retries.
  template <class Teardown, class Sync>
  TRELLIS_NOINLINE void construct(Teardown& teardown, Sync& /*sync*/, build_fn build,
                                  void* injector) {
    storage_.construct(build, injector);
    teardown.push(&Storage::destroy, &storage_);
    ready_ = true;
  }

 private:
  bool ready_ = false;
  Storage storage_;
};

// ---- the slots of an injector -----------------------------------------------------------------

// owns_object<Binding>: whether the injector owns the binding's object, in a slot: singleton and
// singleton_inline. Unique objects belong to their consumers, external ones to the caller, and a
// value binding holds its value itself (dsl/bind.hpp).
template <class Binding>
inline constexpr bool owns_object = false;
template <class Key, class Impl, bool AllowCopy>
inline constexpr bool owns_object<binding<Key, Impl, singleton_t, AllowCopy>> = true;
template <class Key, class Impl, bool AllowCopy>
inline constexpr bool owns_object<binding<Key, Impl, singleton_inline_t, AllowCopy>> = true;

// slot_holder<J, Binding, Mode>: the slot of the binding at position J of the injector's
// flattened list, or nothing. A binding the injector doesn't own gets an empty class with no base,
// so the empty holders of one store are distinct types, and GCC and Clang lay all of them out at
// offset 0, taking no space (the Itanium C++ ABI). MSVC gives each empty base after the first a
// byte unless the class is declared __declspec(empty_bases), which slot_store is, through
// TRELLIS_EMPTY_BASES (config.hpp; untested locally).
template <std::size_t J, class Binding, threading Mode>
struct slot_holder {};
template <std::size_t J, class Key, class Impl, bool AllowCopy, threading Mode>
struct slot_holder<J, binding<Key, Impl, singleton_t, AllowCopy>, Mode> {
  object_slot<shared_storage<Impl>, Mode> cell;
};
template <std::size_t J, class Key, class Impl, bool AllowCopy, threading Mode>
struct slot_holder<J, binding<Key, Impl, singleton_inline_t, AllowCopy>, Mode> {
  object_slot<inline_storage<Impl>, Mode> cell;
};

// slot_store: one holder per binding, as indexed bases, like the binding store of dsl/module.hpp,
// so that the slot of the binding at position J is a static_cast away: the binding map gives J
// and the binding type together (static/binding_map.hpp). One class per injector type, built by a
// flat expansion, never a recursive std::tuple.
template <threading Mode, class Indices, class... Bindings>
struct slot_store;
template <threading Mode, std::size_t... J, class... Bindings>
struct TRELLIS_EMPTY_BASES slot_store<Mode, std::index_sequence<J...>, Bindings...>
    : slot_holder<J, Bindings, Mode>... {
  template <std::size_t I, class Binding>
  auto& at() noexcept {
    return static_cast<slot_holder<I, Binding, Mode>&>(*this).cell;
  }
};

// storage_for<Mode, type_list<B...>>: the storage types of an injector whose flattened bindings
// are B..., under the threading mode Mode: its slots, its teardown stack, sized to the number of
// slots, and what its slots share for blocking.
template <threading Mode, class Bindings>
struct storage_for;
template <threading Mode, class... Bindings>
struct storage_for<Mode, type_list<Bindings...>> {
  static constexpr std::size_t owned =
      detail::total({std::size_t{owns_object<Bindings> ? 1U : 0U}...});
  using slots = slot_store<Mode, std::index_sequence_for<Bindings...>, Bindings...>;
  using teardown = teardown_stack<owned, Mode == threading::safe>;
  using sync = typename sync_of<Mode>::type;
};

}  // namespace trellis::inline v1::detail

#endif  // TRELLIS_STATIC_STORAGE_HPP
