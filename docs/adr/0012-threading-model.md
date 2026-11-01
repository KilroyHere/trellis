# ADR-0012: Threading model

- **Status:** Accepted (owner, 2026-10-30)
- **Plan:** [§4](../plan.md#4-architecture-decisions) ADR-12; §6.8, §7.6, §7.7; §16 D6; R18; risk K10
- **Spikes:** [S4 storage and threading](../spikes/S4-storage-threading.md), whose recommendation the owner accepted.
- **Amends:** plan §6.8 and §7.6 (the slot design, the retry after a throw, and R004 in all builds), §7.7 (R004 as the static engine's one runtime failure path), and, from T1.5, §12.2 (eager mode and the threading options take effect in Phase 1).

## Context

Lazy singletons are thread-safe by default (D6), and thread safety must be documented per operation and tested (R18: Kangaru and dingo don't document it, and inversify-cpp's singleton cache appears to race). Plan §7.6 left the slot's slow path open, "a per-injector mutex and condition variable, or `std::call_once` per slot, whichever S4 shows is smaller and faster", and detected re-entrant construction (R004) only in debug builds.

Spike S4 compared slot designs on gcc-13, gcc-14, clang-18, clang-20 and clang-18 with libc++ (S4 §1, §2):

- **A:** `std::call_once` with a `std::once_flag` per slot;
- **B:** an atomic state whose waiters use a per-injector mutex and condition variable;
- **C:** the same state, with waiters blocking on the state byte itself through C++20 `atomic::wait`;
- **C+spin:** C, but a waiter first re-reads the state briefly.

It measured sizes, speed with Google Benchmark, the include cost, semantics in stress tests, and ThreadSanitizer runs.

## Decision

1. **The modes** (unchanged):
   - `threading::safe` is the default: lazy singleton slots are initialized exactly once, even under concurrent `create()`;
   - `threading::none` removes the synchronization;
   - `eager = true` constructs all singletons inside `make_injector`, in dependency order;
   - scopes are owned by one thread unless configured otherwise;
   - the docs recommend eager mode for applications: it fails fast at startup and behaves the same whichever services are created first.
2. **The `threading::safe` slot is S4's C+spin** (S4 §4):

   ```cpp
   // state: empty 0, constructing 1, ready 2; bit 4 = a waiter is blocked
   template <class Storage>                 // shared_storage<Impl> or inline_storage<Impl>
   class slot {
     std::atomic<std::uint8_t> state_{empty};
     Storage storage_;
    public:
     auto& get(auto& teardown, auto&& make) {
       if (state_.load(std::memory_order_acquire) == ready) [[likely]] return storage_.ref();
       return get_slow(teardown, make);     // noinline
     }
     // get_slow, in a loop:
     //   CAS empty->constructing (acquire) succeeds:
     //     push a thread-local R004 frame; construct;
     //     on throw: publish(empty), rethrow;
     //     push the teardown entry; publish(ready)
     //   ready: return
     //   constructing and on this thread's frame stack: throw TRL-R004
     //   up to 128 times: cpu_relax(); reload; if no longer constructing, continue
     //   CAS to set the waiters bit; state_.wait(constructing | waiters)
     // publish(s): if (state_.exchange(s, release) & waiters) state_.notify_all();
   };
   ```

   - **Fast path:** one acquire load and a compare, a plain `mov` on x86: 0.25–0.31 ns warm, the same as B and `threading::none` (S4 §3.2).
   - **Uncontended first access:** one compare-exchange, one exchange and the teardown `fetch_add`, with no lock and no notify call: about 17 ns with inline storage and 30 ns with `make_shared`.
   - **Under contention,** with 8 threads on one slot: for trivial constructors it takes 1.05–3.7 times B's time, a few nanoseconds once per singleton; for 1 µs constructors, which are closer to real singletons, it is 1.4–4.1 times faster than B, because nobody blocks.
   - **The spin count** comes from S4's sweep on gcc-14, as ratios to plain C for trivial and 1 µs constructors: 8 spins 1.60× and 1.04×, 32 spins 0.64× and 1.01×, 128 spins 0.84× and 0.25×.
   - **No synchronization object per injector or per scope,** and only `<atomic>` is needed. A shared scope reuses the same slot type at no extra cost.
3. **Fallback where `__cpp_lib_atomic_wait` isn't defined,** which is libc++ with an Apple deployment target below macOS 11 or iOS 14: waiters use B's slow path, a per-injector mutex and condition variable. `config.hpp` gets `TRELLIS_HAS_ATOMIC_WAIT` and `TRELLIS_CPU_RELAX()`, and CI forces the fallback once.
4. **`std::call_once` is rejected** (see Alternatives).
5. **R004 is detected in all builds,** not only in debug builds (S4 §4):
   - each thread keeps an intrusive stack of the slots it is constructing, and a thread that finds a slot in `constructing` checks its own stack before waiting;
   - the cost is two thread-local writes, on the cold path only, which S4 couldn't measure: the variants without detection weren't faster (1.02–1.12×);
   - without detection, re-entry deadlocks silently;
   - under `threading::none` detection is free, because finding `constructing` always means re-entry.

   S4 tested it (S4 §3.4): a constructor that dereferences its own slot gets TRL-R004, the slot returns to `empty`, nothing is registered, and a later request succeeds; a lazy to a different slot works; and a second thread that arrives during a 50 ms construction waits and gets the same object, with no false R004.
6. **A throwing constructor leads to a retry,** as `call_once` and function-local statics specify:
   - the exception reaches only the thread that ran the constructor;
   - the slot returns to `empty`, blocked waiters wake, and the next caller tries again;
   - dependencies built during the failed attempt stay built and registered, which is §7.7's basic guarantee;
   - eager mode unwinds the teardown stack and rethrows, the strong guarantee (ADR-0005).

   Under contention, S4's constructor failed 3 times while 8 threads waited: exactly 3 exceptions reached one caller each, and then one object was built.
7. **`threading::none`** uses a plain `uint8_t` state. **Eager mode** reads are plain reads; the lazy check costs about 0.13 ns more than eager mode, on GCC only.
8. **The runtime container's singleton slots use the same mechanism,** and after `build()` its tables are immutable (§7.6, unchanged).
9. **Guarantees are documented per operation** (construct, `create`, `begin_scope`, `get`, teardown), each backed by a ThreadSanitizer stress test (§7.6, unchanged). S4's stress test, with 8 threads building a 64-node graph with nested dependencies for 200 rounds, is TSan-clean on clang-18 with libstdc++ and with libc++, and on gcc-14 (S4 §3.5).

## Alternatives

- **A, `std::call_once` per slot** (S4 §3.2, §3.4):
  - on libstdc++ it is 6–10 times slower warm: `call_once` has no inline fast path, and every call stores two thread-local variables and calls `pthread_once`;
  - it is 7 times slower cold, because glibc's `pthread_once` always issues a `FUTEX_WAKE`: 65 futex calls for 64 first accesses, against 1 for B or C;
  - it can't detect R004 without extra per-slot state, and deadlocks instead;
  - its retry after a throwing callable hangs under TSan with libstdc++, and on some non-x86 targets (GCC bug 66146).
- **B everywhere,** an atomic state with a per-injector mutex and condition variable: a defensible simpler choice, with one code path, no spin heuristic and no availability caveat, and the fastest design for trivial constructors under contention. It costs 88 bytes per injector and per shared scope on Linux (`std::mutex` 40 and `std::condition_variable` 48, larger on MSVC), plus 0.07–0.09 s per translation unit for `<mutex>` and `<condition_variable>` (S4 §3.3). It is kept as the fallback.
- **C without the spin:** losers set the waiters bit at once, so the owner calls `notify_all`, which is a futex syscall whenever libstdc++ has registered a waiter. It is slower than B under contention.
- **Poisoning the slot after a throw,** storing an `exception_ptr` and rethrowing it: 8–16 bytes per slot, and transient failures become permanent.
- **R004 in debug builds only** (the plan's draft): a release build deadlocks silently instead.

## Consequences

- **The spin count of 128 was tuned on one x86 VM** (S4 §5). `pause` costs 10–140 cycles depending on the core, and AArch64 uses `yield`, so on some CPUs the spin may need a time bound instead.
- **The Apple fallback is untested** here; CI must cover it.
- **MSVC** (S4 §5, assessed from the MS STL sources, not measured; risk low): `atomic<uint8_t>::wait` and `notify_all` map to `WaitOnAddress` and `WakeByAddressAll`. `TRELLIS_CPU_RELAX` needs `_mm_pause` or `YieldProcessor`, and the no-inline attribute needs `__declspec(noinline)`. `call_once` calls `InitOnceBeginInitialize` on every call, so A would be slow warm there too.
- **R004 across shared libraries** (S4 §5): the `inline thread_local` frame stack is one object per program only if its symbol isn't hidden, so re-entry through a lazy that crosses a `-fvisibility=hidden` boundary would deadlock instead of reporting R004. This matters for plugins (ADR-0002, ADR-0011).
- **R004 is a runtime error of the static engine in every build,** so the static engine does have a failure path at run time (§7.7). S4's prototype throws it; how it is reported under `-fno-exceptions` is open (ADR-0009) and is decided with `lazy` in Phase 2.
- **The frame stack is the one piece of state outside injectors and scopes** (ADR-0005). It holds only the slots its thread is constructing.
- **Contended numbers are noisy:** they come from a shared, oversubscribed 4-vCPU VM, so read them as orderings; the orderings held across two full runs (S4 §5). libc++'s speed wasn't measured, because the installed Google Benchmark is built against libstdc++.
- T1.5 implements the slots and the teardown stack. R004's tests arrive with `lazy` and `provider` in Phase 2.

## History

- **2026-10-30, plan draft 1:** drafted as ADR-12 in plan §4, "confirm in S4", with the slow path left open in §7.6 and R004 in debug builds.
- **2026-10-30, spike S4 (T0.6):** recommended C+spin with the mutex fallback, rejected `call_once`, kept the retry, and moved R004 detection to all builds.
- **2026-10-30, owner:** accepted S4's recommendation: atomic state-machine slots using `atomic::wait`, with a mutex fallback, and `std::call_once` rejected (docs/PROGRESS.md, "Owner decisions").
- **2026-10-31, T0.8:** recorded in this file. Plan §6.8 and §7.6 amended.
- **2026-10-31, T0.8 review:** plan §7.7 amended too, because R004 in every build contradicted its statement that the static engine has no failure path at run time.
- **2026-11-01, T1.5:** implemented as decided, in `include/trellis/static/storage.hpp`: item 2's C+spin slot with the waiters bit and 128 spins, item 3's fallback (`TRELLIS_HAS_ATOMIC_WAIT`, which a test program sets to 0, so the fallback runs locally too), item 6's retry, item 7's plain flag for `threading::none` and plain reads for an eager injector, and `TRELLIS_CPU_RELAX` and `TRELLIS_NOINLINE` in `config.hpp`. Item 5's R004 detection is left for Phase 2, with `lazy`, as the Consequences say: without `lazy` nothing re-enters a slot, as a dependency cycle doesn't compile (ADR-0014). `tests/unit/threading_test.cpp` runs S4's three concurrency cases (exactly once across 8 threads on a 16-node graph, waiting for a slow construction, 3 failures under contention); the tsan preset runs it under ThreadSanitizer. `eager` and `threading::none` take effect in Phase 1 rather than with Phase 2's options (plan §12.2).
