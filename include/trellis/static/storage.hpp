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
//   object_slot<shared_storage<Impl>, Mode>  singleton: a std::shared_ptr to the Impl, so that a
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
// The code a program gets per singleton is kept small (NFR-04, NFR-06; T1.14):
// - The state machine isn't a template. safe_slot_state and none_slot_state hold the state and
//   the slow path, which a program compiles once, whatever its singletons.
// - A slot doesn't know how to build its object. The slow path calls a make_fn that the
//   resolution code (static/resolve.hpp) passes in: one function per singleton, which builds the
//   object with the class's constructor and puts it in the slot's storage, through the slot's
//   make_with(). That keeps the slots apart from resolution, and it keeps the creation code of a
//   chain of singletons shallow: the make_fn is instantiated where its address is taken, in the
//   request's own resolve<R>(), not several levels down inside the slot or inside
//   std::allocate_shared (NFR-09, ADR-0014 item 3).
// - A std::shared_ptr to a singleton shares one control block type with every singleton of the
//   same size and alignment (make_shared_object).
// - Nothing a slot holds needs destroying except through the teardown stack, so the slots are
//   trivially destructible, and the injector's destructor has no code per slot.
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

// ---- the teardown stack (ADR-0005 item 4) -----------------------------------------------------

using destroy_fn = void (*)(void* object) noexcept;

struct teardown_entry {
  destroy_fn destroy;
  void* object;
};

// teardown_ref<ThreadSafe>: what a slot's slow path needs of the injector's teardown stack,
// whatever its capacity: where its entries are, and its counter. The slow path isn't a template on
// the injector, so it can't name teardown_stack<Capacity, ThreadSafe>; two pointers, passed in
// registers, stand for it.
template <bool ThreadSafe>
struct teardown_ref {
  using counter = std::conditional_t<ThreadSafe, std::atomic<std::size_t>, std::size_t>;

  teardown_entry* entries;
  counter* size;

  // Records a built object. At most Capacity pushes ever happen: one per slot. Under
  // threading::safe a relaxed fetch_add hands every pushing thread its own entry.
  void push(teardown_entry entry) const noexcept {
    std::size_t index = 0;
    if constexpr (ThreadSafe) {
      index = size->fetch_add(1, std::memory_order_relaxed);
    } else {
      index = (*size)++;
    }
    entries[index] = entry;
  }
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

  // Where a slot's slow path pushes its object.
  [[nodiscard]] teardown_ref<ThreadSafe> ref() noexcept { return {entries_.data(), &size_}; }

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
  typename teardown_ref<ThreadSafe>::counter size_{0};
};

// An injector without singletons has nothing to tear down.
template <bool ThreadSafe>
class teardown_stack<0, ThreadSafe> {
 public:
  void unwind() noexcept {}
  [[nodiscard]] static constexpr std::size_t size() noexcept { return 0; }
};

// ---- storage policies (ADR-0005 items 2 and 3) ------------------------------------------------

// A storage's construct(build) takes a Build: a callable that builds the object at the std::byte*
// it is given, the suitably sized and aligned storage of a slot or of a std::shared_ptr's control
// block, as the resolution code builds it (static/resolve.hpp, build<Node>::make_in).

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

  // Builds the object with build and says how to destroy it.
  template <class Build>
  teardown_entry construct(Build& build) {
    build(bytes_.data());
    return {&destroy, bytes_.data()};
  }
  T& object() noexcept { return *std::launder(reinterpret_cast<T*>(bytes_.data())); }

 private:
  static void destroy(void* object) noexcept { std::launder(static_cast<T*>(object))->~T(); }

  alignas(T) std::array<std::byte, sizeof(T)> bytes_;
};

// construct_with<T>: an allocator whose construct() builds the object with a Build, so that
// std::allocate_shared makes the object and its control block in one allocation, as
// std::make_shared would, while Trellis builds the object itself: with braces for an aggregate,
// with parentheses otherwise (ADR-0003), in place, so that the class needn't be copyable or
// movable. allocate_shared(a, args...) creates the object through allocator_traits<A>::construct(a,
// p, args...) ([util.smartptr.shared.create]/7.2), which calls this construct(p, &build): the
// Build travels as allocate_shared's argument, so the allocator is empty and the control block,
// which keeps a copy of it, is no larger than std::make_shared's. A shared_ptr made this way also
// sets up std::enable_shared_from_this, which an aliasing shared_ptr wouldn't, so
// make_shared_object uses it for the classes that share from this. Memory comes from
// std::allocator, so all construct_with allocators compare equal. Checked on libstdc++ 13 and 14
// and libc++ 18 (tests/unit/create_test.cpp); MS STL calls allocator_traits the same way, untested
// locally.
template <class T>
struct construct_with {
  using value_type = T;
  using is_always_equal = std::true_type;

  construct_with() = default;
  // Rebinding: allocate_shared allocates its control block through a copy for another type.
  template <class U>
  constexpr construct_with(const construct_with<U>& /*other*/) noexcept {}

  [[nodiscard]] T* allocate(std::size_t count) { return std::allocator<T>{}.allocate(count); }
  void deallocate(T* pointer, std::size_t count) noexcept {
    std::allocator<T>{}.deallocate(pointer, count);
  }
  template <class U, class Build>
  void construct(U* where, Build* build) {
    (*build)(reinterpret_cast<std::byte*>(where));
  }
};

template <class T, class U>
constexpr bool operator==(const construct_with<T>& /*left*/,
                          const construct_with<U>& /*right*/) noexcept {
  return true;
}

// shares_from_this<T>: T has an unambiguous and accessible base std::enable_shared_from_this<U>,
// which a std::shared_ptr made for a T sets up ([util.smartptr.shared.const]/1). The requirement
// deduces U from T* through T's bases, which fails for two such bases; and it converts T* to the
// base's pointer from here, which fails for a base that isn't public. So it holds exactly where
// std::shared_ptr enables shared_from_this.
template <class U>
void enable_shared_from_this_base(const std::enable_shared_from_this<U>* base);
template <class T>
concept shares_from_this = requires(T* object) { detail::enable_shared_from_this_base(object); };

// shared_holder<Size, Align>: the bytes of an object of that size and alignment, and the function
// that destroys it, set once the object is built: what a std::shared_ptr to a singleton owns,
// unless the singleton's class shares from this (make_shared_object). Its default constructor is
// user-provided, so that std::make_shared's value-initialization leaves the bytes alone.
//
// The bytes come first, and there are as many of them as end the holder right after the function
// pointer: at least Size, rounded up so that the pointer is aligned and the holder's size is a
// multiple of its alignment. So the alignment specifier pads nothing, which MSVC would report
// for a class aligned beyond 8 bytes (C4324, a level-4 warning), and the holder is as large as
// with the pointer first: Size and the pointer, rounded up to 8 bytes, or Size plus Align for a
// class aligned beyond them, as its size is a multiple of its alignment (create_test.cpp checks
// the sizes).
template <std::size_t Size, std::size_t Align>
struct shared_holder {
  static constexpr std::size_t alignment = Align > alignof(destroy_fn) ? Align
                                                                       : alignof(destroy_fn);
  static constexpr std::size_t bytes_size =
      (Size + sizeof(destroy_fn) + alignment - 1) / alignment * alignment - sizeof(destroy_fn);

  shared_holder() noexcept {}  // NOLINT(modernize-use-equals-default): see above
  shared_holder(const shared_holder&) = delete;
  shared_holder& operator=(const shared_holder&) = delete;
  ~shared_holder() {
    if (destroy != nullptr) {
      destroy(bytes.data());
    }
  }

  alignas(Align) std::array<std::byte, bytes_size> bytes;
  destroy_fn destroy = nullptr;
};

// Destroys the T built at `object`.
template <class T>
void destroy_object(void* object) noexcept {
  std::launder(static_cast<T*>(object))->~T();
}

// make_shared_object<T, Element>(build): a new T, built by build, owned by a
// std::shared_ptr<Element> (Element is T, a base of T, or void) that holds it and its control
// block in one allocation, as std::make_shared does (ADR-0005 item 2).
//
// A class that shares from this gets the shared_ptr from std::allocate_shared<T>, whose control
// block sets up enable_shared_from_this (construct_with). Any other class goes into a
// shared_holder made by std::make_shared, and the shared_ptr aliases the object in it: it shares
// the holder's control block, which destroys the object when the last owner lets go, as T's own
// control block would. The holder's type depends only on T's size and alignment, so classes of
// one size share one control block type with its virtual functions, and std::allocator<void>'s
// allocator_traits, which libstdc++ specializes: a control block per class, with construct_with's
// allocator_traits, costs about as much to compile as the rest of a singleton's creation code
// (T1.14).
template <class T, class Element, class Build>
std::shared_ptr<Element> make_shared_object(Build& build) {
  if constexpr (shares_from_this<T>) {
    return std::allocate_shared<T>(construct_with<T>{}, std::addressof(build));
  } else {
    auto holder = std::make_shared<shared_holder<sizeof(T), alignof(T)>>();
    build(holder->bytes.data());
    holder->destroy = &destroy_object<T>;
    Element* const object = std::launder(reinterpret_cast<T*>(holder->bytes.data()));
    return std::shared_ptr<Element>(std::move(holder), object);
  }
}

// Destroys the std::shared_ptr<void> that a shared_storage built at `object`: the injector's
// reference to its singleton. One function for every singleton, whatever its class.
inline void destroy_shared_pointer(void* object) noexcept {
  std::launder(static_cast<std::shared_ptr<void>*>(object))->~shared_ptr();
}

// shared_storage<T>: a std::shared_ptr to a T that the injector holds (singleton). Teardown drops
// the injector's reference; a shared_ptr that a consumer holds keeps the object alive after it,
// and weak_ptrs expire when the last one goes (ADR-0005 item 2).
//
// The pointer is a std::shared_ptr<void> to the T, kept in raw bytes, built by construct() and
// destroyed by the teardown stack, as inline_storage does with its object: so the storage needs no
// destructor of its own, the slots, being trivially destructible, cost the injector's destructor
// no code, and destroying the pointer is one function for every T (destroy_shared_pointer).
template <class T>
class shared_storage {
 public:
  using value_type = T;

  // Builds the object, and the pointer that owns it, and says how to destroy the pointer.
  template <class Build>
  teardown_entry construct(Build& build) {
    ::new (static_cast<void*>(bytes_.data()))
        std::shared_ptr<void>(detail::make_shared_object<T, void>(build));
    return {&destroy_shared_pointer, bytes_.data()};
  }
  T& object() noexcept { return *static_cast<T*>(held().get()); }
  // The object as Result, a std::shared_ptr<E> or std::weak_ptr<E> where E is T or a base of T,
  // owned with the injector's reference: the aliasing constructor shares its control block, so a
  // shared_ptr is one more owner of the same object. Converting straight from the held pointer
  // costs less to compile than making a std::shared_ptr<T> and converting that: shared_ptr's
  // converting constructors are constrained, the aliasing constructor isn't (T1.14).
  template <class Result>
  [[nodiscard]] Result share() const noexcept {
    using element = typename Result::element_type;
    const std::shared_ptr<void>& held_pointer = held();
    element* const object = static_cast<T*>(held_pointer.get());
    if constexpr (TRELLIS_IS_SAME(Result, std::shared_ptr<element>)) {
      return Result(held_pointer, object);
    } else {
      return Result(std::shared_ptr<element>(held_pointer, object));
    }
  }

 private:
  [[nodiscard]] const std::shared_ptr<void>& held() const noexcept {
    return *std::launder(reinterpret_cast<const std::shared_ptr<void>*>(bytes_.data()));
  }

  alignas(std::shared_ptr<void>) std::array<std::byte, sizeof(std::shared_ptr<void>)> bytes_;
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
#if TRELLIS_HAS_ATOMIC_WAIT
using safe_sync = no_sync;
#else
struct injector_sync {
  std::mutex mutex;
  std::condition_variable woken;
};
using safe_sync = injector_sync;
#endif

template <threading Mode>
struct sync_of {
  using type = no_sync;
};
template <>
struct sync_of<threading::safe> {
  using type = safe_sync;
};

// make_fn<State>: builds a slot's object in the slot's storage, given the slot's state and the
// injector, and returns the teardown entry that destroys it. The resolution code makes one per
// singleton (static/resolve.hpp, build<Node>::make_in), which reaches the storage through the
// slot's make_with (below).
template <class State>
using make_fn = teardown_entry (*)(State& state, void* injector);

// safe_slot_state: the state of a threading::safe slot, and its slow path: spike S4's C+spin
// (ADR-0012 item 2), with retry after a throw (item 6). It isn't a template, so a program compiles
// the slow path once, however many singletons it has, and each slot's own code is its fast path.
// Detecting re-entry by the constructing thread itself, R004 in plan §7.8, comes with lazy, in
// Phase 2. Without lazy nothing can re-enter a slot, as a cycle doesn't compile (ADR-0014 item 2).
class safe_slot_state {
 public:
  safe_slot_state() = default;
  safe_slot_state(const safe_slot_state&) = delete;
  safe_slot_state(safe_slot_state&&) = delete;
  safe_slot_state& operator=(const safe_slot_state&) = delete;
  safe_slot_state& operator=(safe_slot_state&&) = delete;
  ~safe_slot_state() = default;

  // The fast path: an acquire load and a compare, a plain load on x86. It synchronizes with the
  // constructing thread's publish, so the object is visible after it.
  [[nodiscard]] bool ready() const noexcept {
    return state_.load(std::memory_order_acquire) == slot_state::ready;
  }

  // The slow path: builds the object with make, unless another thread builds it first; then
  // returns once it is ready. If the constructor throws, the slot returns to empty, waiting
  // threads wake, the exception propagates to this caller only, and the next request retries. Out
  // of line and cold: it runs once per singleton, and the fast path stays a load and a compare
  // where it is inlined.
  TRELLIS_NOINLINE TRELLIS_COLD void build_once(make_fn<safe_slot_state> make, void* injector,
                                                teardown_ref<true> teardown,
                                                [[maybe_unused]] safe_sync& sync) {
    for (;;) {
      std::uint8_t observed = slot_state::empty;
      if (state_.compare_exchange_strong(observed, slot_state::constructing,
                                         std::memory_order_acquire)) {
        build_and_publish(make, injector, teardown, sync);
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
  // Publishes `empty` if the construction throws, while the exception passes on to the caller,
  // so that waiting threads wake and the next request retries (ADR-0012 item 6). A destructor
  // rather than catch (...) and a rethrow: the exception passes through a cleanup, as through any
  // destructor, and the binary needs none of the runtime's catch functions (NFR-06).
  class publish_empty_unless_built {
   public:
    publish_empty_unless_built(safe_slot_state& slot, safe_sync& sync) noexcept
        : slot_(&slot), sync_(&sync) {}
    publish_empty_unless_built(const publish_empty_unless_built&) = delete;
    publish_empty_unless_built& operator=(const publish_empty_unless_built&) = delete;
    ~publish_empty_unless_built() {
      if (slot_ != nullptr) {
        slot_->publish(*sync_, slot_state::empty);
      }
    }
    void built() noexcept { slot_ = nullptr; }

   private:
    safe_slot_state* slot_;
    safe_sync* sync_;
  };

  void build_and_publish(make_fn<safe_slot_state> make, void* injector, teardown_ref<true> teardown,
                         safe_sync& sync) {
    publish_empty_unless_built guard(*this, sync);
    const teardown_entry entry = make(*this, injector);
    guard.built();
    teardown.push(entry);
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
};

// none_slot_state: the state of a threading::none slot, a plain flag (ADR-0012 item 7). One thread
// at a time uses the injector. Not a template either.
class none_slot_state {
 public:
  [[nodiscard]] bool ready() const noexcept { return ready_; }

  // Builds the object. A throw leaves the flag clear, so the next request retries.
  TRELLIS_NOINLINE TRELLIS_COLD void build_once(make_fn<none_slot_state> make, void* injector,
                                                teardown_ref<false> teardown) {
    teardown.push(make(*this, injector));
    ready_ = true;
  }

 private:
  bool ready_ = false;
};

// object_slot<Storage, Mode>: the storage of one object the injector owns, and whether it is built.
// ready() and object() are the fast path, inlined into the request; construct() calls the slow
// path, out of line, which builds the object exactly once.
//
// The state is a base class, so that the slow path, which knows only the state, can hand it to
// the make_fn, which reaches the storage through make_with: a static_cast from the base to the
// derived class the object really is. The base is one byte and comes first, so the slot's size is
// what it was as two members: S4's sizes (tests/unit/create_test.cpp).
template <class Storage, threading Mode>
class object_slot
    : public std::conditional_t<Mode == threading::safe, safe_slot_state, none_slot_state> {
 public:
  using state_type = std::conditional_t<Mode == threading::safe, safe_slot_state, none_slot_state>;
  using value_type = typename Storage::value_type;

  object_slot() = default;
  object_slot(const object_slot&) = delete;
  object_slot(object_slot&&) = delete;
  object_slot& operator=(const object_slot&) = delete;
  object_slot& operator=(object_slot&&) = delete;
  ~object_slot() = default;

  value_type& object() noexcept { return storage_.object(); }
  Storage& storage() noexcept { return storage_; }

  // The slow path: builds the object with make, exactly once (safe_slot_state::build_once).
  template <class Teardown, class Sync>
  void construct(Teardown& teardown, [[maybe_unused]] Sync& sync, make_fn<state_type> make,
                 void* injector) {
    if constexpr (Mode == threading::safe) {
      this->build_once(make, injector, teardown.ref(), sync);
    } else {
      this->build_once(make, injector, teardown.ref());
    }
  }

  // Builds the object in the storage of the slot whose state is `state`, with build (see the
  // storage policies above); for a make_fn.
  template <class Build>
  static teardown_entry make_with(state_type& state, Build& build) {
    return static_cast<object_slot&>(state).storage_.construct(build);
  }

 private:
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
