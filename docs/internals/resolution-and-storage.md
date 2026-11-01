# Resolution and storage

How `injector::create<T>()` works (T1.5): the check that decides at compile time whether a request can be served, the creation code that serves it, the failure pass that reports why it can't, and where the injector keeps the objects it owns. Headers: [`include/trellis/static/resolve.hpp`](../../include/trellis/static/resolve.hpp) (resolution), [`include/trellis/static/storage.hpp`](../../include/trellis/static/storage.hpp) (slots and teardown) and [`include/trellis/static/injector.hpp`](../../include/trellis/static/injector.hpp) (the injector, `create<T>()`, eager mode). Decisions: [ADR-0014](../adr/0014-resolution-memoization-and-cycles.md) (memoization, depth per hop), [ADR-0006](../adr/0006-diagnostics-contract.md) (one error per root cause), [ADR-0005](../adr/0005-state-and-storage.md) (storage, teardown) and [ADR-0012](../adr/0012-threading-model.md) (slots, threads); the slot design is spike [S4](../spikes/S4-storage-threading.md)'s.

`create<R>()` is ADR-0006 item 2's structure, with nothing after the `if`, which MSVC would flag as unreachable (C4702):

```cpp
template <class R>
detail::create_result_t<R> create() {
  using root = detail::context<injector, unique_t, /* access, see "Eager mode" */>;
  if constexpr (root::template can_resolve<R>) {
    root ctx{*this};
    return ctx.template resolve<R>();   // the creation code
  } else {
    detail::explain<injector, R>();     // the failure pass: one static_assert per cause
    detail::unreachable();
  }
}
```

`create_result_t<R>` is `request_result_t<R>` (a reference, or the request without `const` and `&&`), unless no function can return that type: then `void` (see "One error, not two" below).

## The check: a plan and a node per request

**What it does.** `context<Injector, Lifetime>::can_resolve<R>` says whether request `R`, made by a consumer whose lifetime is `Lifetime`, can be served. It is a variable template:

```cpp
template <class R>
static constexpr bool can_resolve = node_v<typename plan<Injector, R>::node>;
```

- `plan<Injector, R>` works out, once per (injector, request), everything that doesn't recurse: the binding of R's key through the binding map, or the implicit unique binding of an unbound class (ADR-0004); the verdict of the request-form table (`request_check`, T1.4); strict mode; for a class the injector builds, whether it is complete and not abstract, whether discovery finds its constructor (`discoverable_v`, T1.3) and whether a by-value request can be copied. Its `node` names what is left: `never_node<reason>` (false, with the reason the failure pass reports), `always_node` (an object a binding holds: true), or the construction of a class with every parameter resolved, `deduced_node<Impl, Ctx, Indices, Aggregate>` or `declared_node<Impl, Ctx, Aggregate, Args...>`.
- `node_v<Node>` evaluates a construction node directly. A deduced node is `__is_constructible(Impl, any_arg<Impl, Ctx>...)` (with braces in a requires-expression for an aggregate), whose overload resolution checks each placeholder's constraint `Ctx::can_resolve<T>`: the next hop. A declared node scans `Ctx::can_resolve<Args>...` in a braced list (no fold, plan §8.1 C3). `Ctx` is the class's own context: its binding's lifetime is its parameters' consumer lifetime.
- `select_node<Plan>()`, read through `decltype`, asks each question only once the ones before it are settled, in `if constexpr` branches, so a class with a missing binding is never probed.

**Why.** Memoization per (key, request form, consumer lifetime), and no path or depth in any type (plan §7.3, ADR-0014 item 1): `R` is the key and the form, the context carries the consumer lifetime, and each `(Injector, Lifetime, R)` triple is one variable template specialization, which compilers evaluate once. The consumer lifetime isn't consulted in Phase 1; it is in the key so that the captive rule (E006, Phase 2) fits.

The depth per hop decides the longest dependency chain that compiles at the compilers' default limits (NFR-09: 200 hops; GCC's `-ftemplate-depth` is 900, Clang's 1024). Everything in the plan finishes before the recursion starts, so a hop nests only `can_resolve<R>`, `node_v`, and the constraint check of the selected conversion operator: the lean shape of ADR-0014 item 3. The builtin matters: `TRELLIS_IS_CONSTRUCTIBLE` is `__is_constructible` where the compiler has it, and `std::is_constructible_v` costs one more level (it is a variable template), as does calling `deduced_constructible_v` (T1.3), which is the same check behind one more template. Measured in T1.5 on a chain of classes each taking the next, the smallest `-ftemplate-depth` that compiles 100 hops through `create<C0>()`, check and creation together:

| Node of a deduced class | gcc-13 | clang-18 | Deepest chain at the default limit, GCC / Clang |
|---|---|---|---|
| `node_v` with the builtin (T1.5) | 313 (3N + 13) | 415 (4N + 15) | 295 / 252, measured |
| `node_v` with `std::is_constructible_v` | 412 (4N + 12) | 514 (5N + 14) | about 222 / 202 |
| `node_v` through `deduced_constructible_v` | 511 (5N + 11) | 613 (6N + 13) | about 176 / 165 |

The deepest chain was measured by bisection on gcc-13 and gcc-14 (295 hops) and clang-18 and clang-20 (252), both for classes taken by value and for singletons taken by reference. At 100 hops the request form makes no difference: by value 313 / 415, `std::unique_ptr` 313 / 415, singletons by reference 315 / 415, singletons as `std::shared_ptr` 315 / 415. Declared constructors cost 2 levels per hop (214 / 217 by value, 216 / 223 for singletons by reference), as their node calls `can_resolve` directly. S2's lean stub reached 298 / 251 hops (ADR-0014 item 3). MSVC gets the builtin too, through `config.hpp` (it has no `__has_builtin`), and its own limits are in ADR-0014. T1.11 turns the measurement into a test.

**Tests:** `tests/unit/create_test.cpp` (every test reaches the check; `nodes_agree` checks `node_v` against `deduced_constructible_v` for deduced classes and aggregates, resolvable or not); `tests/unit/resolve_forms_test.cpp`, "NodesAgree", the same for each of S1's 32 deducible parameter forms on an injector of singletons and one of unique bindings, with `create<consumer<P>>()` built wherever the check accepts it, and the 3 undeducible forms rejected; the chain measurements came from compiling generated chains with `-fsyntax-only` (T1.11 adds the chain test).

## The creation code: three function bodies per hop

**What it does.** `context::resolve<R>()` serves the request, after the check passed, with one `if constexpr` branch per kind of binding and request form: a slot for a singleton (built on first use), `build<Node, Access>::value`, `fresh` or `place` for a new object (a prvalue, `new`, or in given storage), or the object a binding holds. `build`'s functions expand the placeholders themselves, `Impl(any_arg<Impl, Ctx>{ctx}...)`, with braces for an aggregate, so building a class is exactly what discovery checked (ADR-0003 item 5). A by-value request returns the prvalue, so a class that can't be copied or moved can be created.

The order in which a class's parameters are created is the compiler's order of evaluation of function arguments: unspecified for parentheses (GCC goes right to left, Clang left to right), left to right for braces. Dependencies always come before their consumer; teardown follows whatever order things were built in.

**Why.** GCC and Clang instantiate a function template's body where it is first used, so the creation code nests too, and its depth per hop counts against NFR-09 as the check's does (ADR-0014 item 3, §8.1 C7). Each hop nests exactly three bodies: `resolve`, the `build` function, and the placeholder's conversion operator, which calls the next `resolve`. Calling T1.3's `construct_deduced`, itself two functions, would add two levels.

A singleton is the place where the creation code could get deep: its slot's slow path and `std::allocate_shared`'s internals are a dozen function templates between one `resolve` and the next. So the slot never sees the creation code: `resolve` takes the address of `build<Node, checked>::place`, a plain `void (*)(void* injector, std::byte* where)` (`build_fn`), and passes it to the slot, which calls it through the pointer. Taking the address instantiates `place` right in `resolve`, so a singleton hop also nests three bodies, and the slot's code and `allocate_shared` stay off the chain. The indirect call happens once per singleton.

**Tests:** `tests/unit/create_test.cpp`, `DeducedParametersAreResolvedInEveryForm`, `DeclaredConstructorsAndAggregatesAreBuiltAsDiscovered` and the request-form tests; the chain measurements above.

## Shared storage in one allocation

**What it does.** A `trellis::singleton` is held in a `std::shared_ptr<Impl>`, made by `std::allocate_shared` with `construct_with<Impl>`, an allocator whose `construct(p)` calls the build function:

```cpp
template <class U>
void construct(U* where) { build(injector, reinterpret_cast<std::byte*>(where)); }
```

`allocate_shared<T>(alloc)` creates the object through `allocator_traits<A>::construct(a, p)` with no further arguments ([util.smartptr.shared.create]/7.2), which calls this.

**Why.** ADR-0005 item 2 prices the default at one allocation per singleton, as `std::make_shared` makes. `make_shared` can't build the object the way Trellis must: it would use parentheses for an aggregate, and only a constructor call, not the placeholders' prvalue. `std::shared_ptr<Impl>(new Impl(...))` costs two allocations; an aliasing pointer into a holder would own the object but not set up `std::enable_shared_from_this`, which singletons often use. The allocator gives one allocation, in-place construction with Trellis's rule, any class, copyable or not, and a plain `shared_ptr` that `enable_shared_from_this` and `weak_ptr` work with. It works on libstdc++ 13 and 14 and libc++ 18 (checked in T1.5); MS STL calls `allocator_traits` the same way (untested here).

A unique binding requested as `std::shared_ptr<T>` uses the same allocator, so it costs one allocation too.

**Tests:** `tests/unit/create_test.cpp`, `SingletonGivesTheSameObjectInEverySharingForm`, `ASharedPtrOutlivesTheInjector`, `UniqueGivesANewObjectPerRequest`, `OverAlignedClassesKeepTheirAlignment` (an `alignas(128)` class in a shared slot, an inline slot, a unique `std::unique_ptr` and a unique `std::shared_ptr`) and `EnableSharedFromThisSharesTheControlBlock` (through an interface's singleton binding and a unique `std::shared_ptr` request); CI runs them on MS STL too.

## Slots: spike S4's state machine

**What it does.** `object_slot<Storage, threading::safe>` is S4's C+spin (ADR-0012 item 2): a one-byte atomic state, `empty`, `constructing` or `ready`, plus a waiters bit. The fast path, inlined into `resolve`, is an acquire load and a compare. The slow path (`TRELLIS_NOINLINE`): compare-exchange `empty` to `constructing` and build; or find `ready` and return; or spin up to 128 times with `TRELLIS_CPU_RELAX()`, then set the waiters bit and block in `atomic::wait`. The building thread pushes the teardown entry, then publishes `ready` with an exchange, and calls `notify_all` only if the waiters bit was set. If the constructor throws, the slot is published `empty`, waiters wake, the exception reaches only this caller, and the next request retries (ADR-0012 item 6). Without `__cpp_lib_atomic_wait` (`TRELLIS_HAS_ATOMIC_WAIT` is 0), waiters block on a mutex and a condition variable per injector instead (item 3). `object_slot<Storage, threading::none>` is a plain `bool`.

Inline storage (`singleton_inline`) is the object's bytes, `alignas(T) std::array<std::byte, sizeof(T)>`, and the slot's state is its engaged flag, so a slot is `sizeof(T) + 1` rounded up to `alignof(T)`: 2, 16 and 65 bytes for 1, 8 and 64-byte objects, as S4 measured. A shared slot is 24 bytes.

Re-entry of a slot by the thread that is building it (TRL-R004) can only happen through `lazy`, which Phase 2 brings with the detection (ADR-0012 item 5); in Phase 1 a dependency cycle is a compile error (ADR-0014 item 2).

**Why.** ADR-0012 chose C+spin over `std::call_once` (slower warm, no R004, a TSan hang on retry) and over the mutex everywhere (88 bytes per injector, more include cost, slower with microsecond constructors).

**Tests:** `tests/unit/threading_test.cpp`: 8 threads request a 16-node graph with nested dependencies in different orders, 30 rounds, with exactly one construction per node and one address per node across threads; teardown after such a race destroys each node before the nodes it depends on, whichever threads built them (30 rounds); threads that arrive during a 20 ms construction wait for it; a constructor that fails 3 times while 8 threads wait gives exactly 3 exceptions and one object. The tsan preset runs them under ThreadSanitizer, and `trellis_wait_fallback_tests` runs them again with `TRELLIS_HAS_ATOMIC_WAIT=0` (test names `wait_fallback.*`). `tests/unit/create_test.cpp` checks the slot sizes and the retry on one thread; `tests/unit/no_exceptions_create_test.cpp` builds the slots without `try` and `catch`.

## The teardown stack and the slot store

**What it does.** `teardown_stack<Capacity, ThreadSafe>` is a `std::array` of `{destroy function, object}` pairs and a counter (an atomic with relaxed `fetch_add` under `threading::safe`), with one entry per slot. A slot pushes after its object's constructor succeeds and before it publishes `ready` (ADR-0005 item 4, rule 1), so a dependency's entry lands below its dependents', even across threads, and a failed construction pushes nothing. The injector declares the stack after the slots, so the stack's destructor destroys the objects, in reverse order, before the slots' storage goes (rule 2), and after the bindings' values, which singletons may refer to, are still alive.

The slots are `slot_store<Mode, Indices, Bindings...>`, one base `slot_holder<J, B, Mode>` per binding position J, like T1.4's binding store: the binding map gives the position and the binding type together, so a slot is a `static_cast` away. A binding the injector doesn't own (unique, external, value) gets an empty holder with no base. The empty holders of one store are distinct types, so GCC and Clang lay all of them out at offset 0, as the Itanium ABI does for distinct empty bases, and they take no space.

**Why.** Plan §7.5 sketched the slots as a `std::tuple<slot<B>...>` (amended by ADR-0005, 2026-11-01, T1.5): `std::tuple`'s recursive implementation nests a class per element, which T1.4 avoided for the bindings for the same reason, and indexed bases need no lookup at all. MSVC gives each empty base after the first a byte unless the class is declared `__declspec(empty_bases)`, so `slot_store` and T1.4's `binding_store` carry `TRELLIS_EMPTY_BASES`, and T1.4's stored bindings spell their attribute `TRELLIS_NO_UNIQUE_ADDRESS`, `[[msvc::no_unique_address]]` on MSVC (`config.hpp`; untested here; `create_test.cpp` checks that a store of unowned bindings and a module of empty bindings are 1 byte).

**Tests:** `tests/unit/create_test.cpp`: `TeardownReversesTheOrderOfConstruction` (shared and inline singletons with nested dependencies and a unique member, checked against the construction order recorded), `AThrowingConstructorLeavesTheSlotRetryable` (S4's `E(F, G)` case: `F` stays built, the unique object built for the failed attempt is destroyed, and teardown is `E, G, F, H`), `SingletonsArePerInjector`, `UnrequestedSingletonsAreNeverBuilt`, and static asserts on the stack's size (1,608 bytes for 100 entries) and on a store of unowned bindings being empty.

## Eager mode

**What it does.** With `options{.eager = true}`, the injector's constructor requests every binding it owns by reference, in binding order, through checked contexts: each request builds its dependencies first, so the singletons are built in dependency order, and the teardown stack records the order they were built in. If a constructor throws, the exception leaves the injector's constructor, and the members' destructors tear down what was built, in reverse order: `make_injector`'s strong guarantee (plan §7.5). Afterwards `create<T>()` uses contexts with `slot_access::ready`, whose `resolve` reads a singleton's object without testing its slot and without instantiating its build function: plan §7.6's "every access is a plain read" (ADR-0012 item 7).

**Why.** The access is a template parameter of the context, `context<Injector, Lifetime, Access>`, which changes only `resolve`: `can_resolve<R>` evaluates the same node for every access, and the nodes name checked contexts, so the check is memoized once. An eager injector instantiates the singletons' creation code once, in its constructor.

**Tests:** `tests/unit/create_test.cpp`, `EagerModeBuildsEverySingletonInMakeInjector` and `EagerModeGivesTheStrongGuarantee` (S4's case: node 4 throws, nodes 3, 2, 1 and 0 are destroyed in that order); `tests/compile_fail/create/eager_e012.cpp` (a singleton that can't be built fails in `make_injector`, once).

## The failure pass

**What it does.** `explain<Injector, R>()` reports why request R can't be served, with one `static_assert` per reason, each in its own `if constexpr` branch with its own literal (ADR-0006 items 2 and 3):

| The plan's reason | Reported |
|---|---|
| keys repeat | nothing: `make_injector` has reported TRL-E003 |
| the table rejects the request | `report_request_form` (T1.4): TRL-E005 or TRL-E012 |
| an abstract type without a binding | TRL-E001 |
| an owning request of a type without a binding, under `options{.strict = true}` | TRL-E013 (provisional ID); the table's verdict comes first, so a reference to an unbound type stays TRL-E012 |
| an abstract type bound to itself (`bind<I>` alone) | TRL-E001, before discovery is asked, with or without a declaration |
| `std::unique_ptr<Key>` of a derived implementation, `Key`'s destructor not virtual | TRL-E014 (provisional ID) |
| the class's constructor can't be found, or the class is only declared | discovery's own assertion: TRL-E002, TRL-E004 or TRL-E010 |
| a by-value request whose copy can't be made | TRL-E002 |
| the class's node fails | its failing parameters, each through `explain` |

For a declared class, the failing parameters are the declared requests that `can_resolve` rejects. A deduced class's parameter types can't be listed (ADR-0006, "Deduced mode limits the explain pass"), so the pass builds the class from `explain_arg<Impl, Ctx>` placeholders in a function that is instantiated, never called:

```cpp
::new (static_cast<void*>(storage.data())) Impl(repeat_t<Is, explain_arg<Impl, Ctx>>{}...);
```

`explain_arg` has the probe's operator shapes, so overload resolution selects, for each parameter, the operator that `any_arg` selects, with the same deduced request; the selected operator's body is instantiated, and it calls `explain` for its request if that fails.

**Why.** Each `explain<Injector, R>` is instantiated once, so a failing request is reported once however many consumers make it, and each distinct failing request, a root cause, is one error. The assertions are in function bodies, which Clang doesn't mark invalid the way it marks a class whose `static_assert` failed, so later uses add no error (T1.2's and T1.4's lesson); every message has a compile-fail test that uses it at least twice. `explain` always reports when `can_resolve<R>` is false: the plan's reasons cover R itself, and a construction node that fails has a parameter that fails, since with every parameter resolvable the discovered constructor is viable and still the best (the placeholders' constraints only remove candidates). So `create<R>()`'s failing branch never compiles silently.

The static_assert fires inside the descent, so the compiler's instantiation notes trace the path back to `create<T>()`, at about 3 lines per note on Clang and 1 on GCC, capped by the default backtrace limit of 10: 26–31 lines for a failure one hop down, and up to 41 (Clang 18) and 40 (GCC 14) for one three hops down or in eager mode. NFR-07's 40-line budget and the path in the error type are T1.6's.

**Tests:** the 23 tests in `tests/compile_fail/create/`, each of which allows exactly one error line; `tests/unit/resolve_forms_test.cpp`, "FailurePassAgrees": for each of S1's 32 deducible forms, building `consumer<P>` from `explain_arg` asks exactly the request S1 recorded, which `any_arg` resolves (`tests/unit/deduced_test.cpp`), and `owner_arg` accepts the form, so the two placeholders can't drift from the probe's shapes.

## One error, not two

T1.5's review found three requests that gave a second error beside the failure pass's one; each is now one error.

- **A by-value request of an abstract class,** such as `create<IShape>()`, unbound, bound to an implementation or bound to itself. A function can't be declared to return an abstract class, so naming `request_result_t<R>` as `create<R>()`'s return type was an error of its own. `create_result_t<R>` is `void` where the result isn't `returnable` (an abstract or incomplete class, an array, a function type, or a `std::unique_ptr` to an incomplete class, which a call can't return as its temporary can't be destroyed); no such request can be served, so the valid ones keep their type. `returnable` asks `std::is_abstract_v` only after `complete_class`, in a concept's conjunction, which checks its second constraint only when the first holds. Tests: `e001_root_abstract_by_value.cpp`, `e002_root_abstract_bound_by_value.cpp` and `e001_root_abstract_bound_to_itself_by_value.cpp`.
- **A type that is only declared,** such as the pointee of a `database*` parameter. The request check asked `std::is_abstract_v<Key>` of every unbound key, which is an error for an incomplete one, and the cascade reached the plan's `if constexpr` chain. `complete_abstract<Key>` (`static/validate.hpp`) checks completeness first, the same way, so such a request goes on to the table (TRL-E012 for a pointer or reference) or to discovery (its one TRL-E002, "it is incomplete here"). Test: `e012_incomplete_by_pointer.cpp`.
- **A parameter `std::unique_ptr<X>` of an incomplete `X`.** Building the consumer from `explain_arg` instantiates the parameter's destructor, and `std::default_delete<X>` asserts that `X` is complete: a second error after discovery's. So `explain_node` first asks, in an unevaluated construction, whether the class builds from `owner_arg<Impl, Ctx, false>`, a placeholder with the probe's shapes whose by-value operator refuses such an owner (`owns_incomplete`). If it doesn't, the class has one, and the pass reports through `owner_arg<Impl, Ctx, true>` instead, unevaluated too: that operator's constraint calls `owner_converts`, which takes the address of `explain<Injector, T>` for the owner's request `T`, and the odr-use instantiates it, so the incomplete class is reported once, by the same specialization a by-value parameter would reach, and no destructor is instantiated. The class's other failing parameters are reported once `X` is complete. Test: `e002_incomplete_unique_ptr_parameter.cpp`, with the request at the root, as a parameter and as an aggregate's member.

## Choices for types without a constructor to discover

- **Unbound non-class types,** such as `int`, are implicitly unique for the request-form table, as T1.4 treats them, and have no constructor to discover: a by-value request is discovery's TRL-E002, "deduction needs a class type; bind the type to a value", and a reference is TRL-E012. They are never value-initialized silently, which would inject a 0 into a constructor that expects configuration ([NEEDS CLARIFICATION] in T1.5's progress entry).
- **An aggregate's fields are requests,** base classes first (ADR-0003 item 5). A class without a constructor of its own that derives from a base is an aggregate, so its base is a field and is requested by value; a class that should be built with no arguments needs a user-declared constructor, even `X() = default;`.
- **Deleting through a base class** (T1.5's review; plan §6.3 amended by ADR-0004). `std::unique_ptr<Key>` of a binding whose implementation derives from `Key` is TRL-E014 when `Key`'s destructor isn't virtual (`deletes_through_base`), as `std::default_delete<Key>` would delete the implementation through `Key`. It is asked only for a derived implementation, so a key that is its own implementation needn't be complete. A `std::shared_ptr<Key>` is safe, as its control block deletes the class it was made for, and so are slots and by-value requests (`create_test.cpp`, `NonVirtualBaseFormsDestroyTheDerivedClass`; `e014_unique_ptr_without_virtual_destructor.cpp`).

## Open points

- T1.6: the path from the root, NFR-07's 40 lines for deep failures (the notes above), and goldens. Forms of one missing binding, such as `sensor&`, `const sensor&` and `sensor*`, are distinct requests and so distinct errors.
- A `std::variant<X, int>` parameter is rejected when exactly one alternative resolves: the variant's converting constructor then accepts the placeholder too, and the conversion is ambiguous (`resolve_forms_test.cpp`, row 35 on the injector of singletons). The check and the creation code agree on it; it is a limit of the same kind as S1's rows 20, 33 and 34.
- Phase 2: R004 in the slot's slow path (the frame stack of ADR-0012 item 5), the captive rule on the consumer lifetime already in the key, and `lazy`, whose re-entry the slots must report.
- T1.11: the chain test, and the depth on MSVC, where `config.hpp` gives the builtin untested.
