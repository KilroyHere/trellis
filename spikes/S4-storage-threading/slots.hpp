// Spike S4 (plan §12.1): candidate designs for lazily initialized singleton slots and for the
// teardown stack. Throwaway code that informs plan §7.5, §7.6, ADR-05 and ADR-12; it is not part
// of include/ and does not follow every library convention.
//
// Designs, all with the same interface `get(teardown, sync, make)`:
//   once_slot   (A) std::once_flag per slot and std::call_once.
//   cv_slot     (B) atomic state empty -> constructing -> ready; the slow path waits on a
//                   per-injector mutex and condition variable (`injector_sync`).
//   wait_slot   (C) the same atomic state; the slow path waits with C++20 atomic wait/notify
//                   on the state byte itself, so there is no per-injector synchronization object.
//   plain_slot  threading::none: a plain state byte.
//   eager_slot  eager mode: constructed up front, every access is a plain read.
// Storage policies: inline_storage<T> (singleton_inline) and shared_storage<T> (singleton).
#pragma once

#include <array>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <utility>

#if defined(__GNUC__)
#define S4_NOINLINE [[gnu::noinline]]
#else
#define S4_NOINLINE
#endif

namespace s4 {

// ---------------------------------------------------------------------------------------------
// R004: re-entrant construction of the same slot.
// ---------------------------------------------------------------------------------------------

struct reentrant_construction : std::logic_error {
  using std::logic_error::logic_error;
};

[[noreturn]] S4_NOINLINE inline void report_r004() {
  throw reentrant_construction(
      "TRL-R004: re-entrant construction of a singleton (a lazy dereferenced inside the "
      "constructor of the object it refers to)");
}

// Each thread keeps an intrusive stack of the slots it is constructing right now. A frame lives
// in the slow path's stack frame: pushing it costs a TLS read and two TLS writes, no allocation,
// and only the cold path pays. A thread that finds a slot in `constructing` walks its own stack;
// if the slot is on it, waiting would deadlock on itself, so it reports R004 instead.
struct construction_frame {
  const void* slot;
  construction_frame* prev;
};

inline thread_local construction_frame* constructing_top = nullptr;

class frame_guard {
 public:
  explicit frame_guard(const void* slot) noexcept : frame_{slot, constructing_top} {
    constructing_top = &frame_;
  }
  ~frame_guard() { constructing_top = frame_.prev; }
  frame_guard(const frame_guard&) = delete;
  frame_guard& operator=(const frame_guard&) = delete;

 private:
  construction_frame frame_;
};

struct no_frame_guard {
  explicit no_frame_guard(const void*) noexcept {}
};

inline bool this_thread_is_constructing(const void* slot) noexcept {
  for (const construction_frame* f = constructing_top; f != nullptr; f = f->prev)
    if (f->slot == slot) return true;
  return false;
}

// ---------------------------------------------------------------------------------------------
// Teardown stack.
// ---------------------------------------------------------------------------------------------

using destroy_fn = void (*)(void*) noexcept;

struct teardown_entry {
  destroy_fn destroy;
  void* object;
};

// Fixed capacity: one entry per owned slot, known at compile time, so no heap. A slot pushes
// exactly once, after its object is fully constructed and before it publishes `ready`. So the
// stack cannot overflow, failed constructions leave no entry, and a dependency (which is ready
// before its dependent's constructor runs) always sits below its dependent, even when different
// threads build them: the dependent's thread acquired the dependency's `ready`, which was
// published after the dependency's fetch_add, so coherence orders the two fetch_adds.
template <std::size_t Capacity, bool ThreadSafe>
class teardown_stack {
 public:
  teardown_stack() = default;
  teardown_stack(const teardown_stack&) = delete;
  teardown_stack& operator=(const teardown_stack&) = delete;
  ~teardown_stack() { unwind(); }

  void push(destroy_fn destroy, void* object) noexcept {
    std::size_t index = 0;
    if constexpr (ThreadSafe)
      index = size_.fetch_add(1, std::memory_order_relaxed);
    else
      index = size_++;
    assert(index < Capacity);
    entries_[index] = {destroy, object};
  }

  // Destroys everything in reverse order of construction. Single-threaded by precondition: the
  // injector is being destroyed, so no other thread may use it.
  void unwind() noexcept {
    std::size_t n = size();
    while (n != 0) {
      --n;
      entries_[n].destroy(entries_[n].object);
    }
    if constexpr (ThreadSafe)
      size_.store(0, std::memory_order_relaxed);
    else
      size_ = 0;
  }

  std::size_t size() const noexcept {
    if constexpr (ThreadSafe)
      return size_.load(std::memory_order_relaxed);
    else
      return size_;
  }

 private:
  std::array<teardown_entry, Capacity> entries_{};
  std::conditional_t<ThreadSafe, std::atomic<std::size_t>, std::size_t> size_{0};
};

// ---------------------------------------------------------------------------------------------
// Storage policies. `make` is called with an emplace function and must call it exactly once
// (or throw); this mirrors the library resolving the constructor arguments inside the slot's
// construction and constructing the object in place.
// ---------------------------------------------------------------------------------------------

template <class T>
class inline_storage {
 public:
  using value_type = T;
  static constexpr bool shares_ownership = false;

  inline_storage() = default;
  inline_storage(const inline_storage&) = delete;
  inline_storage& operator=(const inline_storage&) = delete;

  template <class Make>
  void construct(Make& make) {
    make([this](auto&&... args) {
      ::new (static_cast<void*>(bytes_)) T(std::forward<decltype(args)>(args)...);
    });
  }
  T& ref() noexcept { return *std::launder(reinterpret_cast<T*>(bytes_)); }
  static void destroy(void* self) noexcept { static_cast<inline_storage*>(self)->ref().~T(); }

 private:
  alignas(T) std::byte bytes_[sizeof(T)];
};

template <class T>
class shared_storage {
 public:
  using value_type = T;
  static constexpr bool shares_ownership = true;

  shared_storage() = default;
  shared_storage(const shared_storage&) = delete;
  shared_storage& operator=(const shared_storage&) = delete;

  template <class Make>
  void construct(Make& make) {
    make([this](auto&&... args) {
      ptr_ = std::make_shared<T>(std::forward<decltype(args)>(args)...);
    });
  }
  T& ref() noexcept { return *ptr_; }
  std::shared_ptr<T> share() const noexcept { return ptr_; }
  // Teardown drops the injector's reference; holders of a shared_ptr keep the object alive.
  static void destroy(void* self) noexcept { static_cast<shared_storage*>(self)->ptr_.reset(); }

 private:
  std::shared_ptr<T> ptr_;
};

// ---------------------------------------------------------------------------------------------
// Slot states for designs B and C and for threading::none.
// ---------------------------------------------------------------------------------------------

namespace state {
inline constexpr std::uint8_t empty = 0;
inline constexpr std::uint8_t constructing = 1;
inline constexpr std::uint8_t ready = 2;
inline constexpr std::uint8_t phase_mask = 3;
// Set by a thread before it blocks, so the constructing thread knows it must wake someone.
// Without waiters the cold path is one CAS and one exchange: no lock, no notify call.
inline constexpr std::uint8_t waiters = 4;
}  // namespace state

// Design B's per-injector synchronization object. The other designs ignore it.
struct injector_sync {
  std::mutex mutex;
  std::condition_variable cv;
};

// ---------------------------------------------------------------------------------------------
// (A) std::call_once per slot. Re-entry cannot be detected without extra per-slot state (the
// once_flag is opaque), so a lazy dereference inside its own constructor deadlocks.
// ---------------------------------------------------------------------------------------------

template <class Storage>
class once_slot {
 public:
  using value_type = typename Storage::value_type;
  using storage_type = Storage;

  template <class Td, class Make>
  value_type& get(Td& td, injector_sync& /*unused*/, Make&& make) {
    std::call_once(flag_, [&] {
      storage_.construct(make);
      td.push(&Storage::destroy, &storage_);
    });
    return storage_.ref();
  }
  Storage& storage() noexcept { return storage_; }

 private:
  std::once_flag flag_;
  Storage storage_;
};

// ---------------------------------------------------------------------------------------------
// (B) Atomic state machine; the slow path blocks on the injector's mutex and condition variable.
// ---------------------------------------------------------------------------------------------

template <class Storage, bool DetectReentry = true>
class cv_slot {
 public:
  using value_type = typename Storage::value_type;
  using storage_type = Storage;

  template <class Td, class Make>
  value_type& get(Td& td, injector_sync& sync, Make&& make) {
    if (state_.load(std::memory_order_acquire) == state::ready) [[likely]]
      return storage_.ref();
    return get_slow(td, sync, make);
  }
  Storage& storage() noexcept { return storage_; }

 private:
  template <class Td, class Make>
  S4_NOINLINE value_type& get_slow(Td& td, injector_sync& sync, Make& make) {
    for (;;) {
      std::uint8_t s = state::empty;
      if (state_.compare_exchange_strong(s, state::constructing, std::memory_order_acquire)) {
        construct(td, sync, make);
        return storage_.ref();
      }
      if (s == state::ready) return storage_.ref();
      if constexpr (DetectReentry)
        if (this_thread_is_constructing(this)) report_r004();
      std::unique_lock lock(sync.mutex);
      s = state_.load(std::memory_order_acquire);
      while ((s & state::phase_mask) == state::constructing) {
        // Set the waiters bit while holding the mutex. The owner locks the mutex after it sees
        // the bit, which can only happen once this thread is inside wait(): no lost wake-up.
        if ((s & state::waiters) == 0 &&
            !state_.compare_exchange_weak(s, static_cast<std::uint8_t>(s | state::waiters),
                                          std::memory_order_acquire))
          continue;
        sync.cv.wait(lock);
        s = state_.load(std::memory_order_acquire);
      }
      if (s == state::ready) return storage_.ref();
      // Back to empty: the owner's constructor threw. Retry: this thread may construct it.
    }
  }

  template <class Td, class Make>
  void construct(Td& td, injector_sync& sync, Make& make) {
    using guard_type = std::conditional_t<DetectReentry, frame_guard, no_frame_guard>;
    [[maybe_unused]] guard_type guard(this);
    try {
      storage_.construct(make);
    } catch (...) {
      publish(sync, state::empty);
      throw;
    }
    td.push(&Storage::destroy, &storage_);
    publish(sync, state::ready);
  }

  void publish(injector_sync& sync, std::uint8_t to) noexcept {
    if ((state_.exchange(to, std::memory_order_release) & state::waiters) != 0) {
      {
        std::lock_guard lock(sync.mutex);
      }
      sync.cv.notify_all();
    }
  }

  std::atomic<std::uint8_t> state_{state::empty};
  Storage storage_;
};

// ---------------------------------------------------------------------------------------------
// (C) Atomic state machine; the slow path waits on the state byte with C++20 atomic wait.
// Spin > 0 (variant C+spin): before announcing itself, a waiter re-reads the state up to Spin
// times with a CPU pause in between. A short construction then finishes without anybody setting
// the waiters bit, so its owner skips notify_all, which costs a futex syscall whenever a waiter
// is registered (libstdc++ counts spinning waiters too).
// ---------------------------------------------------------------------------------------------

inline void cpu_relax() noexcept {
#if defined(__x86_64__) || defined(__i386__)
  __builtin_ia32_pause();
#elif defined(__aarch64__)
  asm volatile("yield");
#endif
}

template <class Storage, bool DetectReentry = true, int Spin = 0>
class wait_slot {
 public:
  using value_type = typename Storage::value_type;
  using storage_type = Storage;

  template <class Td, class Make>
  value_type& get(Td& td, injector_sync& /*unused*/, Make&& make) {
    if (state_.load(std::memory_order_acquire) == state::ready) [[likely]]
      return storage_.ref();
    return get_slow(td, make);
  }
  Storage& storage() noexcept { return storage_; }

 private:
  template <class Td, class Make>
  S4_NOINLINE value_type& get_slow(Td& td, Make& make) {
    for (;;) {
      std::uint8_t s = state::empty;
      if (state_.compare_exchange_strong(s, state::constructing, std::memory_order_acquire)) {
        construct(td, make);
        return storage_.ref();
      }
      if (s == state::ready) return storage_.ref();
      if constexpr (DetectReentry)
        if (this_thread_is_constructing(this)) report_r004();
      if constexpr (Spin > 0) {
        for (int k = 0; k < Spin && (s & state::phase_mask) == state::constructing; ++k) {
          cpu_relax();
          s = state_.load(std::memory_order_acquire);
        }
        if ((s & state::phase_mask) != state::constructing) continue;  // ready, or retry
      }
      // Announce a waiter, then block until the value changes (ready, or empty after a throw).
      if ((s & state::waiters) == 0 &&
          !state_.compare_exchange_weak(s, static_cast<std::uint8_t>(s | state::waiters),
                                        std::memory_order_acquire))
        continue;
      state_.wait(state::constructing | state::waiters, std::memory_order_acquire);
    }
  }

  template <class Td, class Make>
  void construct(Td& td, Make& make) {
    using guard_type = std::conditional_t<DetectReentry, frame_guard, no_frame_guard>;
    [[maybe_unused]] guard_type guard(this);
    try {
      storage_.construct(make);
    } catch (...) {
      publish(state::empty);
      throw;
    }
    td.push(&Storage::destroy, &storage_);
    publish(state::ready);
  }

  void publish(std::uint8_t to) noexcept {
    if ((state_.exchange(to, std::memory_order_release) & state::waiters) != 0) state_.notify_all();
  }

  std::atomic<std::uint8_t> state_{state::empty};
  Storage storage_;
};

// ---------------------------------------------------------------------------------------------
// threading::none: plain state. Single-threaded, so seeing `constructing` always means re-entry,
// and R004 detection is free.
// ---------------------------------------------------------------------------------------------

template <class Storage>
class plain_slot {
 public:
  using value_type = typename Storage::value_type;
  using storage_type = Storage;

  template <class Td, class Make>
  value_type& get(Td& td, injector_sync& /*unused*/, Make&& make) {
    if (state_ == state::ready) [[likely]]
      return storage_.ref();
    return get_slow(td, make);
  }
  Storage& storage() noexcept { return storage_; }

 private:
  template <class Td, class Make>
  S4_NOINLINE value_type& get_slow(Td& td, Make& make) {
    if (state_ == state::constructing) report_r004();
    state_ = state::constructing;
    try {
      storage_.construct(make);
    } catch (...) {
      state_ = state::empty;
      throw;
    }
    td.push(&Storage::destroy, &storage_);
    state_ = state::ready;
    return storage_.ref();
  }

  std::uint8_t state_ = state::empty;
  Storage storage_;
};

// ---------------------------------------------------------------------------------------------
// Eager mode: make_injector constructs everything, so access needs no state check at all.
// ---------------------------------------------------------------------------------------------

template <class Storage>
class eager_slot {
 public:
  using value_type = typename Storage::value_type;
  using storage_type = Storage;

  template <class Td, class Make>
  value_type& get(Td& /*unused*/, injector_sync& /*unused*/, Make&& /*unused*/) noexcept {
    return storage_.ref();
  }
  template <class Td, class Make>
  void prime(Td& td, Make&& make) {
    storage_.construct(make);
    td.push(&Storage::destroy, &storage_);
  }
  Storage& storage() noexcept { return storage_; }

 private:
  Storage storage_;
};

// A shared_ptr request. Only storage that really owns the object on the heap can answer it;
// for inline storage the requirement fails, which the library reports as a compile error.
template <class Slot, class Td, class Make>
  requires Slot::storage_type::shares_ownership
std::shared_ptr<typename Slot::value_type> share(Slot& slot, Td& td, injector_sync& sync,
                                                 Make&& make) {
  slot.get(td, sync, make);
  return slot.storage().share();
}

}  // namespace s4
