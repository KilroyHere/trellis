# Spike S4: storage and threading

Task T0.6, 2026-10-30. **Status: recommendation waiting for owner sign-off.** It decides ADR-12 and
plan §7.6 and confirms ADR-05 and §7.5.

## 1. Question

From plan §12.1: *`std::call_once` versus an atomic state machine: size, speed, TSan results.* In
full: how should Trellis store lazily created singletons, and how should it tear them down? The
candidates are:

- **A:** `std::call_once` with a `std::once_flag` per slot.
- **B:** an atomic state (`empty → constructing → ready`) whose slow path waits on a
  per-injector mutex and condition variable.
- **C:** anything clearly better. This turned out to be the same atomic state, waiting on the
  state byte itself with C++20 `atomic::wait`/`notify_all`, and **C+spin**, which first spins
  briefly.

The spike also covers retry after a throwing constructor, R004 (re-entrant construction), the
fixed-capacity teardown stack, and `shared_ptr` versus inline storage.

## 2. Method

**Code.** Everything is in `spikes/S4-storage-threading/`:

- `slots.hpp`: the designs.
- `bench.cpp`: Google Benchmark.
- `stress.cpp`: tests, stress tests and the `call_once` hazard demonstrations.
- `summarize.py` and `include_cost.py`: post-processing.
- `run.sh`: rebuilds every number below. Run `bash spikes/S4-storage-threading/run.sh` from the
  repository root; output goes to `build/spikes/S4-storage-threading/`.

The reference run quoted here is snapshotted in `spikes/S4-storage-threading/results/`: its logs,
raw JSON, `summary.md`, `sizes-*.md`, `include-cost.md`, `spin-sweep.md` and `syscalls.md`. The
summary of a second full run, used only to judge noise, is in `verify-run-summary.md`.

**Designs.** All designs share the same storage policies and teardown stack. Each has an inline
fast path and a `[[gnu::noinline]]` slow path.

| Name | Per-slot state | Slow path |
|---|---|---|
| A `once` | `std::once_flag` | `std::call_once` |
| B `cv` | `std::atomic<uint8_t>` | CAS `empty→constructing`; a waiter locks the injector's mutex, sets a `waiters` bit and waits on its condition variable |
| C `wait` | `std::atomic<uint8_t>` | Like B, but the waiter sets the `waiters` bit and calls `state.wait()`. No per-injector object. |
| C+spin `wait_spin` | `std::atomic<uint8_t>` | Like C, but first re-reads the state up to 128 times with a CPU `pause` |
| `none` | `uint8_t` | `threading::none` |
| `eager` | none | no check: constructed by `make_injector` |

In B and C, the constructing thread publishes with `exchange(ready, release)`. It locks or
notifies only if the old value had the `waiters` bit. So an uncontended first access costs one
CAS and one exchange, with no lock and no notify call. `*_nodetect` variants drop the R004
bookkeeping, to price it.

**Toolchains.** All on x86-64 Linux 6.18:

| Toolchain | Standard library |
|---|---|
| GCC 13.3.0 | libstdc++ 13 |
| GCC 14.2.0 | libstdc++ 14 |
| Clang 18.1.3 | libstdc++ 14 |
| Clang 20.1.2 | libstdc++ 14 |
| Clang 18.1.3 with `-stdlib=libc++` | libc++ 18.1 |

Flags:

- Common: `-std=c++20 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror`.
- Benchmarks: `-O2 -DNDEBUG`.
- Tests: `-O2`, with assertions on.
- ThreadSanitizer: `-O1 -g -fsanitize=thread`.

clang-18's TSan runtime was missing, so I installed `libclang-rt-18-dev` 18.1.3, which matches
the installed clang. `run.sh` checks for it and says how to install it.

**Machine.** A 4-vCPU 2.3 GHz VM, shared with other agents that were compiling throughout (load
average 1–4). Eight threads therefore means two threads per vCPU. Absolute numbers are noisy, so
compare ratios within one run.

**Benchmarks.** Google Benchmark 1.8.3, the Ubuntu package. Its "built as DEBUG" warning concerns
only the harness.

- **Settings:** 5 repetitions, random interleaving, 0.05 s minimum per repetition. The tables
  give the median and the [min–max] of the repetitions.
- **Unit:** nanoseconds per slot access.
- **Eight-thread benchmarks:** Google Benchmark divides wall time by the accesses of all threads,
  so the number is amortized; each thread's latency is about 8× it. The `eager` warm read scales
  4.4× from 1 to 8 threads, as 4 vCPUs should.
- **libc++:** not benchmarked, because the installed `libbenchmark` is built against libstdc++.
  libc++ is covered by the tests, TSan, the sizes and the hazard demonstrations.

The benchmarks:

| Benchmark | What it measures |
|---|---|
| `warm` | Fast path: the slot is already constructed |
| `warm8_same`, `warm8_distinct` | The same, from 8 threads: one slot, or adjacent slots |
| `share`, `share8_same` | A `shared_ptr<T>` request: copy and drop the pointer |
| `cold` | First access to 1,024 fresh slots per batch. Manual timing excludes the reset. |
| `cold8_same` | 8 threads race through the same 4,096 fresh slots; the constructor is trivial |
| `cold8_same_spin1us` | The same with a 1 µs constructor, so the threads that lose the race must wait |
| `cold8_distinct` | Each thread constructs its own interleaved slots: neighbours share cache lines and the teardown counter |

**Tests.** `stress.cpp` runs 30 rounds in plain builds and 200 under TSan:

- 8 threads build a 64-node graph with nested dependencies.
- Every node records constructions, destructions, and whether any dependency was already dead.

**Include cost.** For each design's header set: preprocessed lines and the median `-fsyntax-only`
time over 5 runs. The baseline is the headers a DI core includes anyway (`<array> <atomic>
<memory> <tuple> <type_traits> <utility>` …).

## 3. Results

### 3.1 Sizes (bytes, x86-64)

| | A `call_once` | B atomic+cv | C, C+spin | `none` |
|---|---|---|---|---|
| Synchronization per slot | 4 (libstdc++), 8 (libc++) | 1 | 1 | 1 |
| Synchronization per injector | 0 | 88 (`mutex` 40 + `condition_variable` 48) | 0 | 0 |
| Slot, `shared_ptr` storage (any `T`) | 24 | 24 | 24 | 24 |
| Slot, inline `T` of 1 B | 8 (libstdc++), 16 (libc++) | 2 | 2 | 2 |
| Slot, inline `T` of 8 B | 16 | 16 | 16 | 16 |
| Slot, inline `T` of 64 B | 68 (libstdc++), 72 (libc++) | 65 | 65 | 65 |

Notes:

- The teardown stack is 16 bytes per owned slot plus an 8-byte counter: `teardown_stack<100>` is
  1,608 bytes.
- With `shared_ptr` storage, padding makes every design the same size. The 1-byte state only pays
  off for small inline objects.
- Source: `results/sizes-*.md`.

### 3.2 Speed: median ns per access (ratio to C in the same run)

The full tables, with [min–max] for every compiler, are in `results/summary.md`. The contended
`cold8_*` rows vary a lot between runs (§5), so read them as orderings and ratios, not absolute
values.

| Benchmark | Design | gcc-13 | gcc-14 | clang-18 | clang-20 |
|---|---|---|---|---|---|
| warm, inline | eager | 0.129 (0.51×) | 0.131 (0.43×) | 0.262 (0.99×) | 0.265 (1.00×) |
| warm, inline | A once | 2.49 (9.9×) | 2.12 (6.9×) | 1.60 (6.0×) | 2.35 (8.9×) |
| warm, inline | B cv | 0.261 (1.04×) | 0.286 (0.93×) | 0.264 (1.00×) | 0.261 (0.99×) |
| warm, inline | C wait | 0.252 | 0.306 | 0.265 | 0.265 |
| warm, shared | A once | 2.47 (9.5×) | 2.05 (7.8×) | 2.03 (7.0×) | 2.49 (9.6×) |
| warm, shared | C wait | 0.260 | 0.264 | 0.288 | 0.260 |
| warm, 8 threads, same slot | A once | 0.941 (9.6×) | 0.813 (7.1×) | 0.562 (4.7×) | 1.02 (9.3×) |
| warm, 8 threads, same slot | C wait | 0.0979 | 0.115 | 0.121 | 0.109 |
| `shared_ptr` request, 1 thread | C wait | 16.7 | 16.4 | 16.6 | 15.9 |
| `shared_ptr` request, 8 threads, same slot | C wait | 69.5 | 54.0 | 58.0 | 66.1 |
| cold, inline | none | 6.61 (0.39×) | 6.86 (0.39×) | 6.71 (0.37×) | 6.57 (0.37×) |
| cold, inline | A once | 128 (7.5×) | 126 (7.1×) | 130 (7.2×) | 129 (7.3×) |
| cold, inline | B cv | 17.5 (1.03×) | 17.3 (0.97×) | 18.7 (1.03×) | 17.5 (0.99×) |
| cold, inline | C wait | 17.1 | 17.8 | 18.1 | 17.7 |
| cold, inline | C, no R004 | 17.5 (1.02×) | 18.2 (1.02×) | 19.2 (1.06×) | 18.8 (1.06×) |
| cold, inline | C+spin | 16.9 (0.99×) | 17.5 (0.99×) | 17.9 (0.99×) | 17.8 (1.00×) |
| cold, shared | A once | 141 (4.5×) | 139 (4.7×) | 146 (4.7×) | 142 (4.5×) |
| cold, shared | C wait | 31.1 | 29.7 | 31.1 | 31.2 |
| cold, 8 threads, same slot | A once | 53.8 (5.8×) | 46.2 (4.0×) | 45.8 (6.3×) | 55.7 (5.0×) |
| cold, 8 threads, same slot | B cv | 3.84 (0.41×) | 2.57 (0.23×) | 5.44 (0.75×) | 3.22 (0.29×) |
| cold, 8 threads, same slot | C wait | 9.32 | 11.4 | 7.29 | 11.1 |
| cold, 8 threads, same slot | C+spin | 5.51 (0.59×) | 9.56 (0.84×) | 5.76 (0.79×) | 5.40 (0.49×) |
| cold, 8 threads, same slot, 1 µs constructor | A once | 284 (1.50×) | 253 (1.33×) | 326 (1.79×) | 303 (1.65×) |
| cold, 8 threads, same slot, 1 µs constructor | B cv | 111 (0.59×) | 153 (0.80×) | 161 (0.88×) | 145 (0.79×) |
| cold, 8 threads, same slot, 1 µs constructor | C wait | 189 | 190 | 182 | 183 |
| cold, 8 threads, same slot, 1 µs constructor | C+spin | 79.7 (0.42×) | 48.3 (0.25×) | 39.2 (0.21×) | 42.1 (0.23×) |
| cold, 8 threads, distinct slots | A once | 38.0 (1.48×) | 29.6 (1.20×) | 36.5 (1.61×) | 40.5 (2.03×) |
| cold, 8 threads, distinct slots | C wait | 25.7 | 24.6 | 22.7 | 20.0 |

Spread for gcc-14, as [min–max] of 5 repetitions:

| Design | warm, inline | cold, inline | cold, 8 threads, same slot | same slot, 1 µs constructor |
|---|---|---|---|---|
| A | [2.01–2.16] | [121–129] | [14.4–61.4] | [238–327] |
| B | [0.27–0.36] | [16.4–17.6] | [1.76–5.21] | [143–203] |
| C | [0.29–0.36] | [16.9–18.0] | [4.88–17.5] | [184–201] |
| C+spin | — | [16.7–21.4] | [6.13–12.1] | [39.2–108] |

What the numbers show:

- **The fast path.** B, C and `none` are the same: one acquire load and a compare, which is a
  plain `mov` on x86. Eager is the loop floor, which matters on gcc only.
- **A is slow warm on libstdc++.** Its `call_once` has no inline fast path. Every call stores two
  thread-local variables (`__once_callable`, `__once_call`) and calls `pthread_once`.
- **A is slow cold.** glibc's `pthread_once` issues an unconditional `FUTEX_WAKE` after
  initializing. `strace` counts 65 futex calls for 64 first accesses with A, against 1 (the
  runtime's own, as for `none`) with B or C (`results/syscalls.md`).
- **libc++ inlines an acquire-load fast path for `call_once`.** So A's warm penalty is specific to
  libstdc++, and probably to MSVC (§5). Its slow path takes one process-wide mutex.
- **Uncontended cold path.** B and C cost one CAS plus one exchange, plus a fetch_add on the
  teardown counter: about 17 ns inline and 30 ns with `make_shared`. `none` costs 6.7 ns.
- **R004 detection** (the thread-local frame) has no measurable cost: the `no R004` variants are
  not faster (1.02–1.12×).
- **Same slot, trivial constructor.** B is fastest. Its losers lock the mutex before announcing
  themselves, and by then the owner is usually done, so nobody waits.
- **Why plain C is slower.** C's losers set the `waiters` bit at once. That makes the owner call
  `notify_all`, which is a futex syscall whenever libstdc++ has registered a waiter, spinning or
  not.
- **C+spin.** It re-reads the state 128 times with `pause` before announcing itself.
  - Trivial constructors: it narrows the gap to B, from 1.3–4.4× B's time (plain C) to 1.05–3.7×.
  - 1 µs constructors: it is 1.4–4.1× faster than B, because nobody blocks.
- **Spin sweep** (`results/spin-sweep.md`, gcc-14), ratio to C for the trivial and the 1 µs
  constructor:
  - 8 spins don't help: 1.60× and 1.04×.
  - 32 spins help only the trivial case: 0.64× and 1.01×.
  - 128 spins help both: 0.84× and 0.25×.
- **`shared_ptr<T>` requests** cost an atomic increment and decrement: about 16 ns, 60× a reference
  request. From 8 threads on one singleton the refcount line bounces, giving 54–70 ns amortized,
  430–560 ns per thread.

### 3.3 Include cost

| Compiler | Headers | Preprocessed lines | `-fsyntax-only`, median of 5 |
|---|---|---|---|
| g++-14 | baseline (enough for C) | 37,718 | 0.294 s |
| g++-14 | + `<mutex>` (A) | 40,863 (+8%) | 0.325 s |
| g++-14 | + `<mutex>`, `<condition_variable>` (B) | 42,415 (+12%) | 0.362 s (+0.07 s) |
| clang++-18 | baseline (enough for C) | 37,696 | 0.350 s |
| clang++-18 | + `<mutex>`, `<condition_variable>` (B) | 42,389 (+12%) | 0.438 s (+0.09 s) |

B's headers alone would cost 0.07–0.09 s in every translation unit that includes Trellis, which is
a quarter of NFR-03's +0.3 s budget. C needs only `<atomic>`.

### 3.4 Semantics: tests in `stress.cpp`

All pass on gcc-13, gcc-14, clang-18, clang-20 and clang-18 with libc++, with 104,388 checks per
build.

| Test | A | B | C | C+spin | none |
|---|---|---|---|---|---|
| Exactly once: 8 threads, 64-node graph with nested dependencies, random orders, all threads see one address | ✓ | ✓ (inline and shared) | ✓ (inline and shared) | ✓ | — |
| Teardown is the exact reverse of construction, including a dependency built inside its dependent's construction | ✓ | ✓ | ✓ (shared) | — | ✓ |
| Throw partway: in E(F, G), G throws, so F stays built (basic guarantee); E and G retry; teardown is E, G, F, H | ✓ (✗ under TSan with libstdc++: hangs) | ✓ | ✓ | ✓ | ✓ (shared) |
| Eager build throws at node 4: nodes 3, 2, 1, 0 destroyed in that order, exception propagates (strong guarantee) | — | — | ✓ | — | — |
| Retry under contention: the constructor fails 3 times while 8 threads wait; exactly 3 exceptions, each reaching one caller; 1 object | ✓ (✗ under TSan with libstdc++: hangs) | ✓ | ✓ | ✓ | — |
| R004: a constructor dereferences its own slot, so TRL-R004 is thrown, the slot returns to empty, nothing is registered, and a later request succeeds; a lazy to a different slot works | ✗ deadlocks | ✓ | ✓ | ✓ | ✓ |
| No false R004: a second thread arriving during a 50 ms construction waits and gets the same object | — | ✓ | ✓ | ✓ | — |
| `shared_ptr` storage: real ownership, so the object outlives the injector's teardown while held, then dies, and the `weak_ptr` expires | ✓ (compiles) | ✓ (compiles) | ✓ | — | — |
| Inline storage answers `shared_ptr` requests | compile-time rejection (`static_assert` on a concept), for every design | | | | |

**`std::call_once` hazards** (`stress --hazards`, each case run under a watchdog):

| Library, build | Retry after a throwing callable | Re-entrant `call_once` on the same flag |
|---|---|---|
| libstdc++, plain (all 4 compilers) | retries | deadlock, nothing reported |
| libstdc++, TSan (clang-18, gcc-14) | **hangs**: TSan's `pthread_once` interceptor never releases its guard when the callable throws | deadlock |
| libc++, plain and TSan | retries | deadlock |

GCC bug 66146 reports the same hang (the next call blocks) for libstdc++ on several non-x86
targets, whose `pthread_once` doesn't survive an exception. It isn't reproducible here.

### 3.5 ThreadSanitizer

Each configuration ran `stress --rounds 200` with `TSAN_OPTIONS=halt_on_error=1`, and
`WARNING: ThreadSanitizer` was searched for in the log:

| Build | Result |
|---|---|
| clang-18 + libstdc++ | **clean**, 694,943 checks |
| clang-18 + libc++ | **clean**, 694,968 checks |
| gcc-14 + libstdc++ | **clean**, 694,943 checks |

On libstdc++, A's two exception tests are skipped because they hang (§3.4). B, C and C+spin run
everything.

## 4. Recommendation

**Slot (ADR-12, §7.6): adopt C+spin.**

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

- **Why C+spin.**
  - Its fast path and uncontended cold path equal B's.
  - It needs no per-injector or per-scope synchronization object, and no `<mutex>` or
    `<condition_variable>`, saving 12% of preprocessed lines and 0.07–0.09 s per translation
    unit.
  - Under contention, with trivial constructors it takes 1.05–3.7× B's time, which is a few
    nanoseconds, once per singleton. With microsecond constructors, which are closer to real
    singletons, it is 1.4–4.1× faster than B.
- **Fallback where `__cpp_lib_atomic_wait` is not defined.** That is libc++ with an Apple
  deployment target below macOS 11 or iOS 14, where libc++ withholds the macro. There, use B's
  slow path: a per-injector mutex and condition variable. `config.hpp` gets
  `TRELLIS_HAS_ATOMIC_WAIT` and `TRELLIS_CPU_RELAX()`, and CI should force the fallback once.
- **Reject A.** On libstdc++ it is 6–10× slower warm and 7× slower cold, with a syscall per first
  access. It cannot detect R004 without extra per-slot state, and deadlocks instead. Its retry
  after an exception hangs under TSan and, per GCC bug 66146, on some targets.
- **Cost of choosing B everywhere instead.** 88 bytes per injector and per shared scope, plus
  0.07–0.09 s per translation unit. B needs no spin heuristic and has no availability caveat. It
  is a defensible simpler choice if the owner prefers one code path.

**R004: detect it in all builds, not only debug builds.** This changes §6.8 and §7.6.

- Each thread keeps an intrusive stack of the slots it is constructing.
- A thread that finds a slot in `constructing` checks its own stack before waiting.
- The cost is two TLS writes on the cold path only, which isn't measurable here.
- Without it, re-entry deadlocks silently.
- Under `threading::none` detection is free: seeing `constructing` always means re-entry.

**Exceptions: keep retry.** This is what `call_once` and function-local statics specify.

- A throwing constructor's exception reaches only the thread that ran it.
- The slot returns to `empty`, blocked waiters wake, and the next caller tries again.
- Dependencies built during the failed attempt stay built and registered, which is the basic
  guarantee of §7.7.
- Rejected alternative: poisoning the slot (storing an `exception_ptr` and rethrowing it). It
  costs 8–16 bytes per slot and makes transient failures permanent.
- Eager mode unwinds the teardown stack and rethrows (the strong guarantee), as tested.

**Teardown (§7.5): confirmed as planned**, a `std::array<{destroy_fn, void*}, N>` with a counter,
plus two rules the spike found necessary:

1. **Push after construction succeeds and before publishing `ready`.** Then a dependency's entry
   always lands below its dependents', even when different threads build them: coherence orders
   the two `fetch_add`s. The counter is `atomic<size_t>` with relaxed `fetch_add` under
   `threading::safe`, and a plain `size_t` under `none`. Failed attempts push nothing, so capacity
   N can never overflow.
2. **Unwind before the slots' storage is destroyed.** Declare the stack after the slots, or unwind
   in `~injector`.

Possible P2 optimization: 2-byte slot indices with a static table of destroy functions per
injector type, 2N + 8 bytes instead of 16N + 8.

**Storage (ADR-05): confirmed.**

- **`shared_ptr` storage.** 24-byte slot and one allocation per singleton. It gives real
  ownership: a held `shared_ptr` outlives teardown. The cost is atomic refcounting per
  `shared_ptr<T>` request (16 ns, and contended across threads), so the docs should steer hot
  paths to `T&`.
- **Inline storage.** Costs `sizeof(T)` plus 1 byte, rounded up to `alignof(T)`, with no heap. A
  `shared_ptr<T>` request is a compile error (E005), as the `shares_ownership` constraint shows.
- **Rejected: `std::atomic<std::shared_ptr>` storage.** libc++ 18 doesn't provide it, and
  libstdc++ implements it with a lock bit.

**Unchanged:**

- `threading::none` uses a plain `uint8_t` state.
- `eager` access is a plain read. The lazy check costs about 0.13 ns over eager, gcc only.
- Scopes stay single-thread by default. A shared scope reuses the same slot type at no extra cost
  under C+spin.

## 5. Limits and open questions

- **MSVC (not available here; assessed from MS STL sources, not measured):**
  - `std::once_flag` is 8 bytes. `call_once` calls `__std_init_once_begin_initialize`
    (`InitOnceBeginInitialize`) on every call, so expect A's warm penalty there too.
  - `atomic<uint8_t>::wait` and `notify_all` map to `WaitOnAddress` and `WakeByAddressAll`, which
    is supported, and the `waiters` bit avoids the wake call when nobody waits.
  - `std::mutex` and `std::condition_variable` are much larger than on Linux (about 80 and 72
    bytes, from memory), which favours C.
  - Needed in `config.hpp`: `_mm_pause` or `YieldProcessor` for `TRELLIS_CPU_RELAX`, and
    `__declspec(noinline)`.
  - Risk: low.
- **Apple.** The availability fallback (§4) is untested here; CI must cover it.
- **Untested hardware.** AArch64 (`yield` as the relax instruction) and other CPUs: the spin
  count of 128 was tuned on one x86 VM. `pause` costs 10–140 cycles depending on the core, so on
  some CPUs the spin may need to be bounded by time instead.
- **Noise.** Contended numbers come from a shared, oversubscribed 4-vCPU VM, and individual
  medians move between runs. Plain C's same-slot number on clang-20 was 11 ns in the reference
  run and 38 ns in a later full run (`results/verify-run-summary.md`). The ordering held in both
  runs:
  - trivial constructors: A slowest, then C, then C+spin, with B fastest;
  - 1 µs constructors: A slowest, then C, then B, with C+spin fastest.

  Warm and uncontended cold ratios moved by under 10%.
- **libc++ speed is not measured.** The installed Google Benchmark is a libstdc++ build.
- **Re-entry across shared libraries.** The `inline thread_local` frame stack is one object per
  program only if the symbol isn't hidden. Re-entry through a lazy that crosses a
  `-fvisibility=hidden` shared-library boundary would deadlock instead of reporting R004. This
  matters for the runtime engine's plugins (S6).
- **Environment.** The setup script should install `libclang-rt-18-dev`; without it clang-18 TSan
  builds don't link.
