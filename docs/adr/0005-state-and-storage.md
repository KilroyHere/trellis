# ADR-0005: State lives only in injector and scope objects; storage is honest

- **Status:** Accepted (owner, 2026-10-30)
- **Plan:** [§4](../plan.md#4-architecture-decisions) ADR-05; §6.2, §7.5, §7.7; FR-02, FR-13
- **Spikes:** [S4 storage and threading](../spikes/S4-storage-threading.md), whose recommendation the owner accepted. S4 confirmed this ADR and added two teardown rules.
- **Amends:** plan §7.5 (the inline slot layout, the teardown ordering rules, and, from T1.5, slots as indexed bases rather than a `std::tuple`).

## Context

Boost.DI's singletons are function-local statics shared by every injector in the process, so tests leak state into each other (issue #459, R6). Fruit injects `shared_ptr`s that alias an object they don't own (R7). Trellis wants each injector to own its singletons, a `shared_ptr<T>` request to mean real shared ownership, and teardown in reverse order of construction (FR-13). Spike S4 measured the storage policies and the teardown stack together with the threading designs of ADR-0012.

## Decision

1. **All object state lives in injector and scope objects.** No process-wide static holds an object.
2. **`singleton` storage is `std::shared_ptr<Impl>`, held by the injector,** so a `shared_ptr<T>` request returns real shared ownership. The slot is 24 bytes in every slot design S4 measured, because of padding (S4 §3.1), and costs one allocation per singleton. A `shared_ptr` the caller holds outlives the injector's teardown; then the object dies and `weak_ptr`s expire (S4 §3.4).
3. **`singleton_inline` stores the object inside the injector,** with no heap allocation. S4's slot keeps it in aligned raw bytes, and the slot's one-byte state serves as its engaged flag, so a slot costs `sizeof(T)` plus 1 byte, rounded up to `alignof(T)`: 2 bytes for a 1-byte `T`, 16 for 8 bytes, 65 for 64 bytes (S4 §3.1). A `shared_ptr<T>` request is a compile error (E005).
4. **The teardown stack** is a `std::array` of `{destroy function, object}` pairs plus a counter. Its capacity is the number of owned bindings, known at compile time, so it needs no heap. It costs 16 bytes per owned slot plus an 8-byte counter: `teardown_stack<100>` is 1,608 bytes. Teardown runs in reverse order of construction for each injector and scope, and two rules make that hold (S4 §4):
   1. **Push after construction succeeds and before publishing `ready`.** A direct dependency is ready before its dependent's constructor runs, so its entry always lands below its dependents', even when different threads build them: cache coherence orders the two `fetch_add`s. The counter is an `atomic<size_t>` with relaxed `fetch_add` under `threading::safe`, and a plain `size_t` under `threading::none`. Failed attempts push nothing, so the capacity can never overflow.
   2. **Unwind before the slots' storage is destroyed:** declare the stack after the slots, or unwind it in `~injector`.
5. **Exception guarantees** (§7.7), as S4 tested them (S4 §3.4):
   - `create<T>()` gives the basic guarantee: dependencies built during a failed attempt stay built and registered. In `E(F, G)` where `G` throws, `F` stays built, `E` and `G` are retried later, and teardown runs `E, G, F, H`.
   - An eager `make_injector` gives the strong guarantee: when node 4 throws, nodes 3, 2, 1 and 0 are destroyed in that order and the exception propagates.
   - What happens to the slot whose constructor threw (a retry) is ADR-0012's decision.

## Alternatives

- **Function-local statics** (Boost.DI #459): state shared by every injector.
- **Aliasing `shared_ptr`s that don't own their object** (Fruit): a held pointer can dangle.
- **`std::atomic<std::shared_ptr>` storage** (S4 §4): libc++ 18 doesn't provide it, and libstdc++ implements it with a lock bit.

## Consequences

- **A `shared_ptr<T>` request costs atomic reference counting:** about 16 ns, 60 times a reference request, and 54–70 ns amortized when 8 threads share one singleton (S4 §3.2). The docs should steer hot paths to `T&`.
- The default costs one heap allocation per singleton; embedded users opt into inline storage.
- **Teardown follows construction order, not the dependency graph.** A lazy or provider dependency that is first built after its dependent lands above it on the stack, so it is destroyed first.
- **The thread-local frame stack that detects R004** (ADR-0012) is the one piece of state outside injector and scope objects. It holds only pointers to the slots its thread is constructing right now and is empty whenever no construction is running, so it shares nothing between injectors.
- **A possible P2 optimization** (S4 §4): 2-byte slot indices with a static table of destroy functions per injector type, 2N + 8 bytes instead of 16N + 8.
- The documented precondition stands (§6.3): objects that hold an injector's singletons by reference must not outlive it, and holding them by `shared_ptr` is the way to outlive it.
- T1.5 implements storage and teardown, with recorded-destructor tests.

## History

- **2026-10-30, plan draft 1:** drafted as ADR-05 in plan §4, with the storage layout in §7.5.
- **2026-10-30, spike S4 (T0.6):** confirmed both storage policies and the teardown stack, added the two ordering rules, and rejected `std::atomic<std::shared_ptr>` storage.
- **2026-10-30, owner:** accepted S4's recommendation (docs/PROGRESS.md, "Owner decisions").
- **2026-10-31, T0.8:** recorded in this file. Plan §7.5 amended with the ordering rules.
- **2026-10-31, T0.8 review:** plan §7.5's slot text amended to match item 3 (the draft said `std::optional<Impl>`). Item 4's first rule now speaks of direct dependencies, because a lazy or provider dependency can be built after its dependent.
- **2026-11-01, T1.5:** implemented in `include/trellis/static/storage.hpp` ([docs/internals/resolution-and-storage.md](../internals/resolution-and-storage.md)), with S4's sizes: an inline slot is 2, 16 and 65 bytes for 1, 8 and 64-byte objects, a shared slot 24 bytes, a stack of 100 entries 1,608 bytes. Two implementation choices, neither changing a decision:
  - The `std::shared_ptr` of item 2 is made by `std::allocate_shared` with an allocator whose `construct` builds the object in place, so a singleton still costs one allocation while Trellis builds it by its own construction rule (ADR-0003), whatever its copy and move constructors, and `std::enable_shared_from_this` works.
  - Plan §7.5's `std::tuple<slot<B>...>` is a store of indexed bases instead, one per binding position, empty for a binding the injector doesn't own (plan §7.5 amended): a slot is then a `static_cast` from the position the binding map gives, and no recursive tuple nests a class per binding, which T1.4 avoided for the bindings too. On MSVC the empty bases, and T1.4's empty stored bindings, take no space only with `__declspec(empty_bases)` and `[[msvc::no_unique_address]]`, which `config.hpp` gives as `TRELLIS_EMPTY_BASES` and `TRELLIS_NO_UNIQUE_ADDRESS` (untested locally).
  - Item 5's guarantees are tested as S4 tested them: the `E(F, G)` case, and the eager build that throws at node 4.
