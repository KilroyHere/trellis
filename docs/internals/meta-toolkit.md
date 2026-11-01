# The meta toolkit

The compile-time utilities under `include/trellis/meta/` that the engine is built on (plan §7.1, §8.1 C2–C3): type lists, an inheritance-based type map, fixed strings, constexpr type names, and `result<T>`. `type_list.hpp` and `type_map.hpp` are internal (`trellis::detail`); `fixed_string`, `type_name`, `failure`, `result` and `make_unexpected` are public. Built in T1.1 and reworked after its review, which found argument-dependent lookup instantiating user types, a GCC deduction bug with reference keys, slow list queries and a result that could end up holding nothing.

Timings below are `-fsyntax-only` compiles, best of 3, on this project's 4-vCPU container, for 1,000 queries over a list or map of 1,000 entries (the review's `bench_lookup.cpp`), given as clang-18 / gcc-13.

## Rules that apply to every technique

- **No recursion over a pack, and no fold over an unbounded one.** Clang rejects a fold expression over more than 256 operands, fatally: `instantiating fold expression with 300 arguments exceeded expression nesting limit of 256` on clang-18 and clang-20 (`-fbracket-depth`; GCC has no such limit). A module's bindings can be longer, so the toolkit folds over no pack at all (ADR-0010). A fold remains fine where a pack can't exceed 256 operands.
- **Qualified calls only.** An unqualified call such as `find_entry<K>(map_ptr)` makes argument-dependent lookup look into the associated classes of its argument, and so instantiate every class template specialization among a map's keys and values. A value such as `std::optional<abstract_class>`, which fails to instantiate, then breaks lookups of other keys, and the instantiations cost about 1.5 times as much. Every call is qualified (`detail::...` or `Map::...`).
- **No deduction against base classes.** GCC 13 and 14 match an explicitly given `T&` against a base whose argument is `T&&`, also nested (`w<int&>` against `w<int&&>`, `std::function<void(int&)>` against `std::function<void(int&&)>`). A lookup that deduced from the matching base would find neither key, and a duplicate check that deduced would report one that isn't there. The toolkit selects bases by non-template overloads or by builtin traits instead. The one remaining deduction, `at_t`'s fallback, fixes the index, which only one base matches.
- **No element is instantiated.** The lists, maps and queries name types without instantiating them, so elements may be incomplete, abstract, `void`, references, or class templates whose instantiation fails.

## Flat expansions and `first_true`

**What it does.** Where a pack has to be looked at element by element, it is expanded once into a braced list of booleans, which is flat at any length, and scanned by an ordinary constexpr loop:

```cpp
constexpr std::size_t first_true(std::initializer_list<bool> flags) noexcept;  // index, or size
first_true({TRELLIS_IS_SAME(T, Ts)...})          // the first position of T in Ts
first_true({!std::is_convertible_v<...>...})     // the first key that isn't unique
```

`TRELLIS_IS_SAME` (config.hpp) is the `__is_same` builtin where there is one, which creates no variable-template specialization per pair: 250 queries over 600 types took gcc-13 0.34 s and 57 MB, against 1.37 s and 161 MB with `std::is_same_v` (clang-18: 0.52 s, 125 MB against 3.43 s, 231 MB).

**Why.** A fold would hit Clang's limit, and recursion would cost one instantiation level per element. The scan's cost is one expression of N elements per query, so it is the fallback, not the main path: `index_of_v` scanning every query cost 8.4 s and 627 MB / 4.1 s and 366 MB, against 3.0 s and 119 MB / 1.3 s and 47 MB through the overload set below.

**Where.** `first_true` and `positions::scan` (type_list.hpp), `keys_are_distinct` (type_map.hpp).

**Tests.** `tests/unit/type_list_test.cpp`: the `first_true` assertions, and every algorithm on `items<1000>`, a list longer than 256 elements. Replacing `contains_v` with the plan's original `(std::is_same_v<T, Ts> || ...)` makes clang-18 fail on that list.

## The indexed set of a list: `slots_of`

**What it does.** One class per list derives from an indexed slot per element, and each slot from a tag that stands for its type:

```cpp
template <class T> struct type_tag {};
template <std::size_t I, class T> struct slot : type_tag<T> { using type = T; };
struct slots<index_sequence<Is...>, Ts...> : slot<Is, Ts>... {};
```

A repeated type gives two slots with the same `type_tag` base, which C++ allows: the base is ambiguous, not duplicated. Three queries use the set:

- `contains_v<T, List>` is `std::is_base_of_v<type_tag<T>, slots>`, one builtin trait. `is_base_of` ignores ambiguity, so a repeated type counts as present.
- A map's duplicate check converts `const slots*` to `const type_tag<K>*` for each key. The conversion is valid only when that base is unambiguous, that is, when `K` occurs exactly once.
- `at_t`'s fallback deduces `T` from `slot<I, T>` with `I` fixed (below).

The set is reached through `list_slots<List>`, a class template keyed on the list. A query that named `slots_of<Ts...>` itself would expand the pack again every time: that version of `contains_v` cost clang-18 3.9 s and 713 MB.

**Why.** `contains_v` as a scan cost 6.10 s and 353 MB / 4.74 s and 496 MB; through the set it costs 0.37 s and 96 MB / 0.22 s and 34 MB. The duplicate check needs no deduction (the GCC bug above) and no instantiation per key pair: 0.72 s / 0.22 s for a 1,000-entry map, where deducing cost 0.87 s / 0.57 s.

**Where.** `type_tag`, `slot`, `slots`, `slots_of`, `list_slots` and `contains_v` (type_list.hpp); `keys_are_distinct` and `has_duplicate_keys_v` (type_map.hpp).

**Tests.** `type_list_test.cpp`, sections contains_v, "elements are never instantiated" and "types that differ only in & and &&"; `type_map_test.cpp`, sections has_duplicate_keys_v and "keys that differ only in & and &&".

## `index_of_v`: an overload set with a scan behind it

**What it does.** A second per-list class gathers one non-template overload per element, `static integral_constant<size_t, I> at(const type_tag<T>*)`, with `using position<Is, Ts>::at...;`. When `T` occurs once, exactly one overload matches, and the index comes from one overload resolution. A repeated `T` makes the call ambiguous and a missing one leaves it without a candidate; a `requires` check sees either and falls back to the flat scan, which finds the first position or reports the error.

**Why.** It is 2.8 times faster than a scan per query on clang-18, with a fifth of the memory, and avoids deduction. It lives in a class of its own: merged into `slot`, the overloads made `contains_v` and the duplicate check twice as slow for lists that never ask for a position.

**Where.** `position`, `positions`, `position_in` and `index_of_v` (type_list.hpp).

**Tests.** `type_list_test.cpp`, sections index_of_v, "elements are never instantiated" and "types that differ only in & and &&"; `tests/compile_fail/meta/type_list_index_of_missing.cpp`.

## `at_t`: a builtin, pack indexing, or indexed bases

**What it does.** `at_t<I, List>` uses `__type_pack_element<I, Ts...>` where the compiler has it (Clang, GCC 14 and later), C++26 pack indexing `Ts...[I]` where that exists instead, and otherwise the slots: `detail::select_index<I>(const slots*)` takes a `const slot<I, T>*`, so the call deduces `T` from the only slot whose index is `I` ([temp.deduct.call]/4.3). The index is clamped after the bounds `static_assert`, so that an index past the end is reported once, not again by the builtin or the deduction.

**Why.** The builtin instantiates nothing. The bases cost one overload resolution per lookup on a class built once per list, with no recursion.

**Where.** `pack_element_by_builtin`, `pack_element_by_indexing`, `pack_element_by_bases`, `pack_element`, `at` and `at_t` (type_list.hpp); `TRELLIS_HAS_TYPE_PACK_ELEMENT` (config.hpp).

**Tests.** `type_list_test.cpp`: `picks_by_index` and `picks_from_long_pack`, run on each implementation the compiler has (the bases everywhere, the builtin on GCC 14 and Clang, pack indexing in C++26 mode on clang-20); `TypeList.AtImplementations`; `tests/compile_fail/meta/type_list_at_out_of_range.cpp`.

## The type map and its lookups

**What it does.** `type_map<entry<K, V>...>` derives from its entries. Each entry declares one non-template overload, and the map gathers them beside a template fallback:

```cpp
template <class K, class V> struct entry {
  static std::type_identity<V> lookup(const type_tag<K>*);
};
struct map_bases<true, Es...> : Es... {
  using Es::lookup...;
  template <class K> static missing_key<K> lookup(const type_tag<K>*);
};
template <class K, class Map>
using lookup_t = typename decltype(Map::lookup(static_cast<const type_tag<K>*>(nullptr)))::type;
```

For a key with an entry, both the entry's overload and the template match exactly, and overload resolution prefers the non-template. The other entries' overloads aren't viable, because `type_tag<K>*` converts to no other `type_tag`. A missing key gets `missing_key<K>`, whose `static_assert` is `lookup_t`'s one error. `lookup_or_t<K, Map, Fallback>` and `contains_key_v<K, Map>` read the same call without asserting: a partial specialization on `std::type_identity<V>` tells a hit from `missing_key<K>`, which is named but never instantiated.

A map whose keys repeat derives from `map_bases<false, ...>` instead of its entries. Deriving from two identical entries would be an error of its own (the same base twice), so its one error is a `static_assert`. `has_duplicate_keys_v` answers without building the map, so the engine checks it first and reports TRL-E003 in its own words (T1.4).

**Why.** One overload resolution per lookup, with no search and no deduction (the GCC bug), and through a qualified call (argument-dependent lookup). 1,000 lookups cost 2.33 s / 0.92 s, against 2.63 s / 1.17 s for the earlier deducing version. Matching a map with repeated keys by its exact type, so that Clang would also stay silent on a lookup into it, made every lookup 5.7 times as expensive on clang-18, so that wasn't done.

**Limit.** Clang drops the members of a class whose instantiation failed. A lookup into a map that has already reported its repeated key therefore reports a second error on Clang (one on GCC).

**Where.** type_map.hpp.

**Tests.** `type_map_test.cpp`: every section, including poison values and keys, `std::optional` of an abstract class, `&`/`&&` keys plain and nested, a struct derived from a map, `lookup_or_t` and `contains_key_v`; `tests/compile_fail/meta/type_map_*.cpp`.

## `type_name`: the compiler's signature, cut to size

**What it does.** `detail::probe::signature_of<T>()` returns `__PRETTY_FUNCTION__` (GCC, Clang) or `__FUNCSIG__` (MSVC), through `TRELLIS_FUNCTION_SIGNATURE`. The text before `T`'s spelling and after it is the same for every `T`, so comparing the signatures for `int` and `double` measures both lengths, with no offsets hard-coded for any compiler; a `static_assert` checks the result for `int` and `char` when the header is included. The name is then copied into `type_name_storage<T>`, an inline `fixed_string` per type, so that the view points to just the name, followed by a `'\0'`, in static storage. On MSVC a constexpr pass first drops the class keys (`struct `, `class `, `enum `, `union `) that `__FUNCSIG__` writes, and trailing spaces.

**Why.** Diagnostics, C++26 `static_assert` messages and the name-based `type_key` need names as constant expressions (plan §7.1). The probe function has a namespace of its own, one that declares no types, because GCC leaves out the qualification of a type declared in the function's own namespace: in `trellis::v1::detail`, `trellis::detail::type_list<int>` came out as `type_list<int>`.

**Spellings measured** (the unit test prints them): gcc-13 and gcc-14 write `const tn::widget&`, `tn::widget* const`, `tn::box<tn::box<int> >`, `int [3]`, `{anonymous}::hidden` and `trellis::v1::detail::type_list<int>`. clang-18, clang-20 and clang-18 with libc++ write `const tn::widget &`, `tn::widget *const`, `tn::box<tn::box<int>>`, `int[3]`, `(anonymous namespace)::hidden` and `trellis::detail::type_list<int>`. The tests compare spellings modulo insignificant whitespace.

**Member pointers.** Two compilers misspell a member pointer type in their signatures, measured in the platforms CI: Apple clang 21 (clang-2100.1.1.101) writes `int widget::*` for `int tn::widget::*`, dropping the class's scope, and cl 19.51 writes `inttn::widget::*`, without the space before the class. Clang 18 and 20 are right. The likely cause on Clang is llvm-project #132401 (March 2025), which made a member pointer type keep its class as a nested-name-specifier, printed without its prefix; so the gate for upstream Clang 21 and later is inferred, not measured, as no upstream Clang 21 is available here or in CI. On MSVC only cl 19.51 was measured. Where `TRELLIS_TYPE_NAME_RESPELLS_MEMBER_POINTERS` is 1 (Clang 21 and later, and MSVC), `type_name` builds a member pointer's name, cv-qualified or not, from the compiler's spelling of the class itself, which both compilers get right. Where the class sits in the member pointer's spelling is found as `signature_prefix` finds `T`: from the spellings of the same member pointer type with two marker classes, `member_marker::p` and `member_marker::q`, whose names differ in the first and last character. The text those two share at either end surrounds the class, minus the markers' scope where the compiler spells it (MSVC, GCC, Clang 20), so the method works whatever the compiler does to the class and whatever the declarator (`void (tn::widget::*)(int)`, `int tn::widget::*const`). A space goes before the class where the text before it ends in an identifier. The respelling is defined on every compiler, and the tests compare it with the compiler's own spelling where that is right; `respell_member_pointer` itself is plain string code, tested on Clang 21's and MSVC's spellings everywhere.

**Limits, for ADR-0011 and Phase 5.** A name-based `type_key` needs distinct names for distinct types. Clang prints a local class without its enclosing function (`local`, where GCC prints `main()::local`), so two local classes of the same name in different functions get the same name, and it puts the file path and position into lambda names (`(lambda at file.cpp:37:23)`). GCC prints a namespace-scope lambda as just `<lambda()>`. A member pointer inside another type (a pointer or reference to one, an array of them, a template argument) keeps the compiler's spelling: on Clang 21 its class has no scope, so `std::unique_ptr<int a::w::*>` and `std::unique_ptr<int b::w::*>` get one name, and MSVC leaves out the space before the class at the outermost level (`inttn::widget::* *`), though not inside a template argument. MSVC also names `std::nullptr_t` `nullptr`. MSVC's class-key stripping is tested on MSVC-style strings on every compiler.

**Where.** type_name.hpp; `TRELLIS_FUNCTION_SIGNATURE`, `TRELLIS_FUNCTION_SIGNATURE_CLASS_KEYS` and `TRELLIS_TYPE_NAME_RESPELLS_MEMBER_POINTERS` (config.hpp).

**Tests.** `tests/unit/type_name_test.cpp`, including its "member pointers" section; `TypeName.PrintsWhatThisCompilerProduces` records each compiler's spellings in the test log, with the raw spelling of a member pointer.

## `fixed_string`: a structural string

**What it does.** `fixed_string<N>` holds `char chars[N + 1]` as a public member, which makes it a structural type, usable as a template argument (`named<"replica">`, plan §6.5). A deduction guide takes `N` from a literal. `size()` and `data()` make it a C++26 `static_assert` message, and a default-constructed one is a buffer to fill in, for messages built in two passes (count, then write; ADR-0006).

**Why a C array.** A `std::array` member would also be structural, but compilers would print it as a nested `std::array{...}` wherever they show a `fixed_string` template argument.

**Tests.** `tests/unit/fixed_string_test.cpp`, including a `static_assert` with a generated message in C++26 mode.

## `result<T>` without `std::expected`

**What it does.** `result<T>` is `std::expected<T, failure>` when `TRELLIS_HAS_EXPECTED`, and otherwise `detail::fallback_result<T>`, a `std::variant<T, failure>` (an empty placeholder for `void`) with the part of `std::expected`'s interface that portable code uses. `make_unexpected(failure)` builds the failure state in both modes, as `std::unexpected` is C++23 only.

- **Assignment** follows [expected.object.assign]: a value assigned over a failure is constructed while a copy of the failure is kept, and the failure is put back if that throws. The variant's own assignment would leave it valueless, and `error()` was then undefined behaviour, which UBSan reported. The `try`/`catch` sits behind `TRELLIS_HAS_EXCEPTIONS`, so the header builds with `-fno-exceptions`.
- **Accessors** use `std::get`: GCC refuses `std::get_if`'s null check on a temporary's member in a constant expression, so `*result<int>(42)` wasn't constant there. A wrong-state access throws `std::bad_variant_access`, where `std::expected` leaves it undefined.
- **Invalid `T`** (a reference, function or array) gets one `static_assert`; the members then store a placeholder, so that no second error follows.

**Precondition.** Every translation unit of a program sees the same `TRELLIS_HAS_EXPECTED`. `make_unexpected` has the same signature in both modes but returns a different type, so mixing them is a silent ODR violation, not a link error.

**Where.** result.hpp; `TRELLIS_HAS_EXPECTED` and `TRELLIS_HAS_EXCEPTIONS` (config.hpp).

**Tests.** `tests/unit/result_test.cpp`, compiled in C++20, C++23 and C++26 mode, so that the same checks run against the fallback and `std::expected` (clang-18 with libstdc++ uses the fallback in every mode); `Result.AssigningAValueThatThrowsKeepsTheFailure`; `unit.no_exceptions` (`tests/unit/no_exceptions_test.cpp`, built with `-fno-exceptions -fno-rtti`); `tests/compile_fail/meta/result_of_reference.cpp`; `tests/unit/error_namespace_test.cpp`, which keeps the name `trellis::error` free for the compile-error vocabulary.
