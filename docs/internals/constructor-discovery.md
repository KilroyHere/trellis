# Constructor discovery

Constructor discovery tells the static engine which constructor of an implementation type to call, and with what ([plan §5.3](../plan.md#53-how-injectorcreateapp-compiles-static-engine), step 3.3). [ADR-0003](../adr/0003-constructor-discovery.md) puts three strategies behind one trait, in this order: explicit, reflection (C++26, Phase 6) and deduced (T1.3). This note covers the explicit strategy, in [`include/trellis/discovery/ctor_traits.hpp`](../../include/trellis/discovery/ctor_traits.hpp), and the state it hands to the next strategy. The deduction tricks of spike S1, and `discovered_ctor_t`, the one entry point for every mode, are in [deduction.md](deduction.md).

## The explicit strategy

The user lists the constructor's parameter types as a `trellis::ctor<Args...>`, in one of three places. The first that applies wins:

1. the binding: `bind<I>.to<C>().with_ctor<Args...>()`, which discovery accepts but the binding DSL doesn't offer yet: T1.4's DSL has no `.with_ctor`, and no task in `docs/tasks.json` builds it;
2. a specialization, full or partial: `template <> struct trellis::ctor_traits<C> : trellis::ctor<Args...> {};`
3. a public member alias in `C`: `using trellis_ctor = trellis::ctor<Args...>;`

`ctor<Args...>` exposes the parameter types as `args`, a `detail::type_list<Args...>`, which the static engine walks. The types stay as declared: references, cv-qualifiers and `X&&` included. Mapping a parameter type to a request is the request vocabulary's job (plan §6.3, T1.4 and T1.5), and there `X&&` and `const X&&` are the request `X` in every discovery mode (ADR-0003, item 3). The check agrees with that: a fresh `X` binds to an `X&&` parameter.

The engine's entry points, all in `trellis::detail`:

| Entity | What it gives | Assertion |
|---|---|---|
| `declared_ctor_t<T, BindingCtor>` | the declaration that applies, or `no_ctor_declared` | none |
| `ctor_matches_v<T, Declared>` | whether the declaration matches how a `T` will be built | none |
| `explicit_ctor_t<T, BindingCtor>` | the same as `declared_ctor_t`, checked | TRL-E002, once per declaration |

`BindingCtor` is what the binding declares: a `ctor<Args...>`, or `no_ctor_declared` when it declares nothing, which is the default. All three ignore `T`'s cv-qualifiers, so `const X` finds the declaration written for `X`, specialization included, and shares its checked result: `explicit_ctor_t` names `X` without them before anything is instantiated.

### Precedence by construction

**What it does.** A specialization of `ctor_traits<T>` wins over `T::trellis_ctor`, with no code that compares the two: only the primary template reads the member alias, and any specialization, full or partial, replaces the primary template.

```cpp
template <class T>
struct ctor_traits : detail::member_ctor<T>::type {};  // T::trellis_ctor, or no_ctor_declared
```

**Why.** Detecting whether a specialization exists isn't possible portably, and a check of both sources would have to instantiate the member alias's lookup for every class. Here the language's own rules give the order, and the documented form of a specialization, deriving from `ctor<Args...>` (ADR-0003), is exactly what the primary template does with the alias.

The same rules make a few specializations declare nothing, each documented on `ctor_traits` and pinned by a test: one that derives from no `ctor`, or from several (the deduction below fails), and one that is declared but never defined (an incomplete class shows no bases). Each also hides the member alias, since it replaces the primary template. A specialization declared as a `class` derives privately, and the compiler reports the inaccessible base with an error of its own on every compiler.

**Tests:** `tests/unit/ctor_traits_test.cpp`: `ct::both` (both sources, the specialization wins), `ct::family<U>` (a partial specialization wins over the template's alias), `ct::empty_specialization` and `ct::undefined_specialization` (they declare nothing and hide the alias); `TEST(CtorTraits, SpecializationWinsOverMemberAlias)`.

### Reading the declaration from a base class

**What it does.** `declared_ctor<T>` reads the `ctor<Args...>` that `ctor_traits<T>` derives from with one overload resolution:

```cpp
template <class... Args>
ctor<Args...> ctor_base_of(const ctor<Args...>*);  // deduces Args from the base
no_ctor_declared ctor_base_of(const void*);        // no ctor base: nothing declared

using type = decltype(detail::ctor_base_of(static_cast<const ctor_traits<T>*>(nullptr)));
```

The first overload deduces `Args` from a base class ([temp.deduct.call]/4.3), and its derived-to-base conversion ranks above the conversion to `const void*` ([over.ics.rank]/4.4). Without a `ctor` base the deduction fails, and so it does with two of them, as ambiguous; then only the second overload is viable. The call is qualified, so argument-dependent lookup never searches the namespaces of `T` and its template arguments, which could instantiate class templates there or find a user's function of the same name.

GCC 13 and 14 match `T&` against a `T&&` base when a template argument is given and compared with the bases, nested ones included; that is why T1.1 made its `type_map` lookups deduction-free. `ctor_base_of` gives no argument: all of `Args` comes from the one base, and every compiler reads it exactly, as the tests of parameter types that differ only in `&` and `&&` check, plain, nested and inside `std::function`.

**Why.** It reads the documented form of a specialization directly, a class derived from `ctor<Args...>`, also through a class of the user's own (`ct::indirect` in the tests), at the cost of one overload resolution per class, whatever the arity. A member type such as `ctor_traits<T>::type` would have worked too, but would put a generic name into the user's specializations' interface.

**Tests:** `tests/unit/ctor_traits_test.cpp`, the static_asserts under "ctor_traits and declared_ctor_t: the type's own declaration".

### Finding the member alias

**What it does.** `member_ctor<T>` has a partial specialization constrained on `requires { typename T::trellis_ctor; }`. A requires-expression checks access, so a private alias is invisible, as it is to all code outside the class: such a class declares nothing. An alias that names something other than a `ctor<...>`, such as a function signature, meets a `static_assert`, which is the one error; its `type` is then `no_ctor_declared`, so nothing fails after it.

Name lookup also finds an alias that the class inherits, and C++20 can't tell it from the class's own. So a derived class without a declaration of its own uses its base's:

| Derived class | Result |
|---|---|
| inherits the constructors too, `using Base::Base;` | the base's declaration, which matches: the intended use |
| has a constructor of its own | TRL-E002, and deduction (T1.3) never sees the class |
| has a constructor of its own and one that takes the base's parameters | the base's choice, silently |
| has two bases with aliases | nothing declared: the name is ambiguous |
| has an alias of its own, or a specialization | its own declaration, the documented fix |

Specializations aren't inherited at all. The table is documented on `ctor_traits` and on the TRL-E002 page.

`member_ctor` stays a class template, unlike the check below, because only one thing reads `member_ctor<T>::type`: the primary `ctor_traits<T>`, which is instantiated once per class, however often the class is used. After a failed assertion, Clang reports every later read of the specialization's member as an error of its own.

**Why.** Deriving from `T::trellis_ctor` without the check would ignore an alias that names a class other than a `ctor` without a word, and give the compiler's own error for one that isn't a class. The private and the inherited cases can't be detected in standard C++, so they are documented instead.

**Tests:** `tests/unit/ctor_traits_test.cpp` (`ct::with_alias`, `ct::private_alias`, and under "inherited member aliases" one class per row of the table); `tests/compile_fail/discovery/member_alias_not_a_ctor.cpp` and `member_alias_not_a_ctor_used_twice.cpp`.

### The binding's declaration

**What it does.** `declared_ctor<T, ctor<Args...>>` is a partial specialization of its own that yields the binding's declaration and never names `ctor_traits<T>`, so a binding's `.with_ctor<Args...>()` wins without instantiating anything the type declares. An undefined primary template rejects any `BindingCtor` other than a `ctor<...>` or `no_ctor_declared`; only the binding DSL passes it.

**Why.** The binding is the most specific declaration: it concerns one binding of one injector, where the other two concern the type everywhere. The plan doesn't rank it against them, so this is the simplest reading, recorded for the owner. Not instantiating `ctor_traits<T>` also keeps a broken type-level declaration from mattering where a binding overrides it.

**Tests:** `tests/unit/ctor_traits_test.cpp`, under "declared_ctor_t: the binding's declaration", including `ct::malformed_alias`, whose `ctor_traits` would fail to compile if it were instantiated; `TEST(CtorTraits, BindingWinsOverType)`; `tests/compile_fail/discovery/binding_ctor_wrong_types.cpp` and `binding_ctor_used_twice.cpp`.

### The check: a declaration can't lie

**What it does.** `explicit_ctor_t` is `decltype` of a call to `checked_ctor<T, Declared>()`, a function with a deduced return type that holds the one assertion, rendered as ADR-0006 item 3 says, and returns a `Declared`:

```cpp
template <class T, class Declared>
constexpr auto checked_ctor() noexcept {
  if constexpr (!ctor_matches_v<T, Declared>) {
    // one static_assert for an aggregate, one for any other class, each with its own literal:
    static_assert(trellis::error::E002<T, Declared>::reported,
                  "TRL-E002: type not constructible: its trellis::ctor<...> declaration matches "
                  "no constructor");
  }
  return Declared{};
}
```

Deducing the return type makes the compiler instantiate the body, and with it the check, wherever `explicit_ctor_t` is named. Clang prints the condition, with the type and the declaration, on the error line; GCC prints the message, with the ID, on the error line and the condition in the note after it. `ctor_matches_v` gives the same answer without an assertion, for the resolver's normal pass, which only asks (ADR-0006 item 2).

**The rule it checks, which T1.5's creation code follows** (`builds_from`): an aggregate is built with braces, `T{args...}`, as deduced discovery builds one (ADR-0003), so a narrowing argument is rejected and brace elision works; any other class is built with parentheses, `T(args...)`, which `std::is_constructible` checks, so an `initializer_list` constructor never takes the arguments over (`std::vector<int>(n, value)` holds *n* copies, where braces would make a list, or fail to narrow a `std::size_t`). A requires-expression turns an ill-formed `T{args...}`, narrowing included, into `false`. Checking one way and building the other would let a declaration pass that the build then rejects, or builds differently.

**Why a function, not a class template.** The first version was a class template whose `false` specialization held the assertion and still defined `type`. Clang marks a specialization whose `static_assert` failed as invalid, and then reported every later read of its `type` as an error of its own ("no type named 'type'"), one per consumer of the declaration: two nodes that need the same class would give two errors (found in T1.2's review). A deduced return type survives the failed assertion, and later uses reuse it, so the error comes once on every compiler, however often the declaration is used.

**Keeping the output short.** `explicit_ctor_t` names `declared_ctor<...>::type` directly rather than through `declared_ctor_t`: each alias template in between adds an instantiation note to every error raised inside `ctor_traits<T>` (3 lines on Clang). And it takes a third, defaulted parameter, `T` without cv-qualifiers, so that its definition fits on one line: GCC 14 quotes every source line of it in each error's backtrace, 4 lines more when it took three. The result: 8 to 12 lines for every single TRL-E002 case and 9 to 15 for a declaration used twice, on gcc-13, gcc-14, clang-18 and clang-20, within NFR-07's 40.

**Cost.** One `is_constructible`, or one brace check, per declaration, which spike S1 measured at −0.12 to +0.23 ms per class, within noise (ADR-0003), plus one function instantiation per (type, declaration). The plan names no ID for a lying declaration; TRL-E002 is the catalog's "type not constructible", recorded for the owner.

**Tests:** `tests/unit/ctor_traits_test.cpp`, under "ctor_matches_v" (the build rule: narrowing, brace elision, `std::vector<int>`) and "explicit_ctor_t"; `tests/compile_fail/discovery/`: wrong types and wrong arity for a specialization and for a member alias, wrong types for a binding, the same declarations used by two consumers (`*_used_twice.cpp`), and a narrowing declaration for an aggregate. Each expects TRL-E002 on the first error line and allows no second error line.

### Nothing declared

**What it does.** Where no declaration applies, `declared_ctor_t` and `explicit_ctor_t` yield `detail::no_ctor_declared`, and the primary `ctor_traits<T>` derives from it when `T` has no member alias. `ctor_matches_v` is true for it: there is nothing to check.

**Why.** It is the state that the next strategy takes over: the engine deduces the constructor of a type whose explicit result is `no_ctor_declared`, without any change to `ctor`, `ctor_traits` or the member alias. `discovered_ctor_t` ([deduction.md](deduction.md#the-entry-point)) puts the two together: the declaration when one applies, otherwise the deduced constructor.

**Tests:** `tests/unit/ctor_traits_test.cpp`: `ct::undeclared`, `int`, `void` and an abstract class; `TEST(CtorTraits, NothingDeclared)`.

## Open points

- The compile-fail tests here reach the check through `detail::explicit_ctor_t`, which T1.6 left reporting as it did: the assertion stays in the check, raised through `detail::raise`, and `create<T>()`'s explain pass raises the same report, with the path from the root, for a class it reaches ([explain-pass.md](explain-pass.md); `tests/compile_fail/explain/e002_declaration_mismatch_two_roots.cpp`).
- T1.5 builds a declared type by the rule above: braces for an aggregate, parentheses otherwise.
- An abstract class with a member alias gives TRL-E002 here, where plan §5.3 reports an abstract type as TRL-E001. T1.5 settled it: `create<T>()` looks for the binding, and reports TRL-E001, before it discovers a constructor; discovery's own entry points keep TRL-E002.
