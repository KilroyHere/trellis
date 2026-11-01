// Trellis configuration: version, language mode, feature detection and compiler builtins.
// Copyright (c) 2026 Aryan Patel
// SPDX-License-Identifier: BSL-1.0
//
// This is the only header that defines macros (all prefixed TRELLIS_). Code that uses a feature
// newer than C++20 checks the TRELLIS_HAS_* macro here, never a feature-test macro directly.
#ifndef TRELLIS_CONFIG_HPP
#define TRELLIS_CONFIG_HPP

// The standard library's feature-test macros (__cpp_lib_*). They are only tested in #if, which
// include-cleaner doesn't count as a use.
#include <version>  // IWYU pragma: keep

// Semantic version. CMakeLists.txt reads the three numbers from here, and #if must be able to test
// them, so they stay macros rather than the enum clang-tidy suggests.
// NOLINTBEGIN(modernize-macro-to-enum)
#define TRELLIS_VERSION_MAJOR 0
#define TRELLIS_VERSION_MINOR 1
#define TRELLIS_VERSION_PATCH 0
// NOLINTEND(modernize-macro-to-enum)
// One number for #if comparisons: MAJOR * 10000 + MINOR * 100 + PATCH, so 1.2.3 is 10203.
#define TRELLIS_VERSION \
  (TRELLIS_VERSION_MAJOR * 10000 + TRELLIS_VERSION_MINOR * 100 + TRELLIS_VERSION_PATCH)

// The language mode, like __cplusplus. MSVC leaves __cplusplus at 199711L unless /Zc:__cplusplus
// is given, but always reports the mode in _MSVC_LANG.
#if defined(_MSVC_LANG) && _MSVC_LANG > __cplusplus
#define TRELLIS_CPLUSPLUS _MSVC_LANG
#else
#define TRELLIS_CPLUSPLUS __cplusplus
#endif

// Features newer than C++20. Each TRELLIS_HAS_* macro is 1 or 0.
//
// Library features rely on the __cpp_lib_* macro alone: a standard library defines it only in the
// modes where the feature is usable. Core-language features also require the mode they were
// standardized in, because a compiler may report a feature it offers only as an extension: Clang 20
// defines __cpp_static_assert as 202306L and __cpp_pack_indexing in C++20 mode, then warns that
// both are C++26 extensions, and -Werror turns those warnings into errors. C++26 has no final
// __cplusplus value yet (GCC 14 and Clang 18 to 20 use 202400L), so anything above C++23's 202302L
// counts as C++26.

// User-generated static_assert messages (C++26, P2741R3), for the diagnostics in plan §7.8.
#if TRELLIS_CPLUSPLUS > 202302L && defined(__cpp_static_assert) && __cpp_static_assert >= 202306L
#define TRELLIS_HAS_STATIC_ASSERT_MESSAGE 1
#else
#define TRELLIS_HAS_STATIC_ASSERT_MESSAGE 0
#endif

// std::expected (C++23, P0323R12), for result<T> (plan §7.1).
#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
#define TRELLIS_HAS_EXPECTED 1
#else
#define TRELLIS_HAS_EXPECTED 0
#endif

// Pack indexing, Ts...[N] (C++26, P2662R3).
#if TRELLIS_CPLUSPLUS > 202302L && defined(__cpp_pack_indexing) && __cpp_pack_indexing >= 202311L
#define TRELLIS_HAS_PACK_INDEXING 1
#else
#define TRELLIS_HAS_PACK_INDEXING 0
#endif

// Static reflection (C++26, P2996): both the language part (^^T, splices) and the library part
// (<meta>), as plan §6.12 requires. Today that means GCC 16 with -std=c++26 -freflection.
#if TRELLIS_CPLUSPLUS > 202302L && defined(__cpp_impl_reflection) && defined(__cpp_lib_reflection)
#define TRELLIS_HAS_REFLECTION 1
#else
#define TRELLIS_HAS_REFLECTION 0
#endif

// Whether exceptions are enabled: not under -fno-exceptions (GCC, Clang) or without /EHsc (MSVC).
// The library works either way (NFR-11); code that catches an exception, such as the assignment
// of result<T>'s fallback, checks this. GCC and Clang define __cpp_exceptions when exceptions are
// on, MSVC _CPPUNWIND.
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
#define TRELLIS_HAS_EXCEPTIONS 1
#else
#define TRELLIS_HAS_EXCEPTIONS 0
#endif

// Compiler builtins that answer a type question without instantiating a template (plan §8.1 C3,
// ADR-0014), each with a portable fallback. GCC 10+ and Clang have __has_builtin; MSVC doesn't, so
// it takes the fallbacks (untested locally: MSVC runs only in CI). The #if is nested because a
// compiler without __has_builtin can't parse the call.
#if defined(__has_builtin)
// __type_pack_element<I, Ts...>, the I-th type of Ts: Clang, and GCC from 14. meta/type_list.hpp
// falls back to C++26 pack indexing or to indexed base classes.
#if __has_builtin(__type_pack_element)
#define TRELLIS_HAS_TYPE_PACK_ELEMENT 1
#endif
// TRELLIS_IS_SAME(A, B) is std::is_same_v<A, B>, but where the compiler has the __is_same builtin
// it creates no variable-template specialization per pair of types, which matters when a type is
// compared with every element of a long list. Measured in T1.1: 250 lookups in a 600-type list
// took 1.37 s and 161 MB on gcc-13 with std::is_same_v, 0.34 s and 57 MB with the builtin
// (clang-18: 3.43 s and 231 MB, 0.52 s and 125 MB). It is variadic so that a type argument that
// contains a comma passes through whole. The fallback needs <type_traits>, which every header
// that uses the macro includes.
#if __has_builtin(__is_same)
#define TRELLIS_IS_SAME(...) __is_same(__VA_ARGS__)
#endif
#endif
#ifndef TRELLIS_HAS_TYPE_PACK_ELEMENT
#define TRELLIS_HAS_TYPE_PACK_ELEMENT 0
#endif
#ifndef TRELLIS_IS_SAME
#define TRELLIS_IS_SAME(...) ::std::is_same_v<__VA_ARGS__>
#endif

// TRELLIS_FUNCTION_SIGNATURE: the enclosing function's signature as a string literal, with its
// template arguments spelled out, from which meta/type_name.hpp reads a type's name (plan §7.1,
// spike S3 §3.4). GCC and Clang (clang-cl too) have __PRETTY_FUNCTION__, MSVC has __FUNCSIG__.
// __FUNCSIG__ spells class types with their class keys ("struct app::service", "class
// std::vector<...>", "enum app::mode"), which TRELLIS_FUNCTION_SIGNATURE_CLASS_KEYS tells
// type_name to drop (spike S3 §3.8; untested locally, as MSVC runs only in CI). With neither, the
// macro is left undefined and meta/type_name.hpp refuses to compile.
#if defined(__clang__) || defined(__GNUC__)
#define TRELLIS_FUNCTION_SIGNATURE __PRETTY_FUNCTION__
#define TRELLIS_FUNCTION_SIGNATURE_CLASS_KEYS 0
#elif defined(_MSC_VER)
#define TRELLIS_FUNCTION_SIGNATURE __FUNCSIG__
#define TRELLIS_FUNCTION_SIGNATURE_CLASS_KEYS 1
#else
#define TRELLIS_FUNCTION_SIGNATURE_CLASS_KEYS 0
#endif

// TRELLIS_TYPE_NAME_RESPELLS_MEMBER_POINTERS is 1 on Clang 21 and later and on MSVC (not
// clang-cl), whose signatures misspell a member pointer type:
// - Apple clang 21.0.0 (clang-2100.1.1.101, platforms CI) prints int tn::widget::* as
//   `int widget::*`, without the class's scope, so member pointers into two classes of the same
//   name in different namespaces would get one name; Clang 18 and 20 print `int tn::widget::*`.
//   The likely cause is llvm-project #132401 (March 2025), which made a member pointer type keep
//   its class as a nested-name-specifier (not checked: the LLVM tracker is unreachable here). The
//   gate for upstream Clang is inferred from that, not measured: no upstream Clang 21 is available
//   here or in CI. Where a Clang it covers spells the scope after all, the respelling gives the
//   same name.
// - cl 19.51's __FUNCSIG__ leaves out the space before the class: `inttn::widget::*`. No upstream
//   report found; godbolt.org and the GCC/LLVM/MSVC trackers are unreachable here. The gate
//   covers every cl from NFR-13's 19.38, but only 19.51 was measured.
// Where this is 1, type_name builds a member pointer type's name from the compiler's spelling of
// the class itself, which both get right (meta/type_name.hpp, tests/unit/type_name_test.cpp).
// GCC spells member pointers right.
#if (defined(__clang__) && __clang_major__ >= 21) || (defined(_MSC_VER) && !defined(__clang__))
#define TRELLIS_TYPE_NAME_RESPELLS_MEMBER_POINTERS 1
#else
#define TRELLIS_TYPE_NAME_RESPELLS_MEMBER_POINTERS 0
#endif

// The compiler workarounds of deduced constructor discovery (discovery/deduced.hpp, ADR-0003 item
// 6, docs/internals/deduction.md): two for GCC, two for MSVC. All are tested in
// tests/unit/deduced_test.cpp.
//
// TRELLIS_PROBE_DECLARES_TWINS is 1 on GCC before 14. GCC 13 picks the discovery probe's
// `operator T&() const` for a parameter of type X&&, const X&&, std::unique_ptr<X>&& or
// std::reference_wrapper<X>, and then rejects it ("invalid user-defined conversion ... near
// match"), so discovery misses those constructors (spike S1, note b). GCC 14 is fixed; no bug
// report was found, as gcc.gnu.org can't be reached from here. When the probe also declares the
// deleted twins of the resolving placeholder's operators, constrained so that no constructor
// parameter selects them, GCC 13 chooses as GCC 14, Clang and MSVC do (measured in T1.3 on
// gcc-13.3). S1 recommended a probe-only `operator T&&() const` instead, which also made GCC 13's
// discovery accept std::optional<T> and std::tuple<T> parameters and aggregate members; the twins
// don't (amended by ADR-0003, 2026-10-31, T1.3).
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 14
#define TRELLIS_PROBE_DECLARES_TWINS 1
#else
#define TRELLIS_PROBE_DECLARES_TWINS 0
#endif

// TRELLIS_PLACEHOLDER_CONVERSIONS_BEGIN and _END surround the code that builds an object from
// resolving placeholders. GCC's -Wconversion warns at every by-value parameter ("choosing
// 'operator T()' over 'operator T&() const' ... because conversion sequence for the argument is
// better"): it flags exactly the tie-break on the implicit object parameter that deduction relies
// on ([over.ics.rank]/3.2.6, spike S1 §3.3), in Trellis's code rather than the user's, so the pair
// switches that warning off there and only there. Clang doesn't warn, so the pair is empty there,
// as everywhere but GCC.
#if defined(__GNUC__) && !defined(__clang__)
#define TRELLIS_PLACEHOLDER_CONVERSIONS_BEGIN \
  _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wconversion\"")
#define TRELLIS_PLACEHOLDER_CONVERSIONS_END _Pragma("GCC diagnostic pop")
#else
#define TRELLIS_PLACEHOLDER_CONVERSIONS_BEGIN
#define TRELLIS_PLACEHOLDER_CONVERSIONS_END
#endif

// TRELLIS_REPEAT_THROUGH_CLASS is 1 on MSVC (not clang-cl). Deduced discovery repeats a
// placeholder N times by expanding repeat_t<Is, A>... over an index pack, where repeat_t<I, A> is
// an alias template for A that ignores I. cl 19.51 (VS 2026) loses the pack when such an
// expansion of one element makes a class template's arguments: ctor<repeat_t<Is, A>...> with
// Is = {0} fails with C3546 ("there are no parameter packs available to expand") inside ctor, the
// deduced declaration of every one-parameter constructor (T1.3, platforms CI). Where this is 1,
// repeat_t goes through a class template, repeat<I, A>::type, which keeps the pattern dependent on
// I. Elsewhere it stays a plain alias, which instantiates nothing. No upstream report found;
// godbolt.org and the GCC/LLVM/MSVC trackers are unreachable here. The gate covers every cl from
// NFR-13's 19.38, but only 19.51 was measured.
#if defined(_MSC_VER) && !defined(__clang__)
#define TRELLIS_REPEAT_THROUGH_CLASS 1
#else
#define TRELLIS_REPEAT_THROUGH_CLASS 0
#endif

// TRELLIS_ARRAY_CHECK_WITH_BRACES is 1 on MSVC (not clang-cl). Deduction refuses an aggregate with
// an array member (TRL-E004), which GCC and Clang count element by element and MSVC as one field.
// GCC and Clang find one by initializing the aggregate with parentheses from placeholders that
// can't become an array (discovery/deduced.hpp, arrays_free). cl 19.51's parenthesized aggregate
// initialization fails for every member that is itself an aggregate class, such as std::array or
// an empty struct, although the placeholder converts to it (T1.3, platforms CI), so that check
// refused aggregates without arrays. Where this is 1, the check uses braces, with a placeholder
// whose conversion to an array reference is deleted: MSVC initializes an array member from a
// conversion function that yields an array reference (spike S1's MSVC addendum), so it selects the
// deleted one exactly when the aggregate has an array member, of any size. No upstream report
// found for the parentheses; godbolt.org and the GCC/LLVM/MSVC trackers are unreachable here. The
// gate covers every cl from NFR-13's 19.38, but the parentheses' failure was measured on 19.51
// only; the array-reference initialization that the check relies on was seen on 19.51 and 19.44
// (T1.13).
#if defined(_MSC_VER) && !defined(__clang__)
#define TRELLIS_ARRAY_CHECK_WITH_BRACES 1
#else
#define TRELLIS_ARRAY_CHECK_WITH_BRACES 0
#endif

// ---- the static engine's resolution and storage (T1.5) ------------------------------------------

// TRELLIS_IS_CONSTRUCTIBLE(T, Args...) is std::is_constructible_v<T, Args...>, but where the
// compiler has the __is_constructible builtin it instantiates no variable template. The resolution
// check (static/resolve.hpp) asks it once per hop of a dependency chain, and each template it
// instantiates there is one more level of instantiation depth per hop, against NFR-09's 200 hops
// (ADR-0014 item 4; the measurements are in docs/internals/resolution-and-storage.md). GCC 13 and
// 14 and Clang 18 and 20 report the builtin through __has_builtin (checked in T1.5). MSVC has no
// __has_builtin, but has the builtin (T1.13 ran spike S2's stub, which uses it, on cl 19.44 and
// 19.51), so it gets it too; untested locally, as MSVC runs only in CI, where
// tests/unit/create_test.cpp checks that it agrees with discovery's std::is_constructible_v.
// Variadic, like TRELLIS_IS_SAME, so that template arguments with commas pass through whole. The
// fallback needs <type_traits>, which every header that uses the macro includes.
#if defined(__has_builtin)
#if __has_builtin(__is_constructible)
#define TRELLIS_IS_CONSTRUCTIBLE(...) __is_constructible(__VA_ARGS__)
#endif
#elif defined(_MSC_VER)
#define TRELLIS_IS_CONSTRUCTIBLE(...) __is_constructible(__VA_ARGS__)
#endif
#ifndef TRELLIS_IS_CONSTRUCTIBLE
#define TRELLIS_IS_CONSTRUCTIBLE(...) ::std::is_constructible_v<__VA_ARGS__>
#endif

// TRELLIS_HAS_ATOMIC_WAIT is 1 where std::atomic<T>::wait and notify_all exist (C++20, P1135R6).
// Threads that find a singleton under construction block on the slot's state byte with them
// (ADR-0012 item 2). libc++ withholds __cpp_lib_atomic_wait for Apple deployment targets below
// macOS 11 and iOS 14; there the waiters use a mutex and a condition variable per injector instead
// (ADR-0012 item 3). A build may define the macro as 0 to force that fallback, as
// tests/unit/CMakeLists.txt does for one test program. Every translation unit of a program must
// then define it alike: it changes the injector's members and the slots' code, so mixing the two
// violates the one-definition rule.
#ifndef TRELLIS_HAS_ATOMIC_WAIT
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
#define TRELLIS_HAS_ATOMIC_WAIT 1
#else
#define TRELLIS_HAS_ATOMIC_WAIT 0
#endif
#endif

// TRELLIS_CPU_RELAX() tells the CPU that the thread is spinning: `pause` on x86, `yield` on
// AArch64, nothing elsewhere. A thread that finds a singleton under construction re-reads its
// state up to 128 times with it before it blocks (ADR-0012 item 2, spike S4: the spin count was
// tuned on x86). MSVC spells the instructions as the intrinsics _mm_pause and __yield, declared in
// <intrin.h>; untested locally, as MSVC runs only in CI.
#if defined(_MSC_VER) && !defined(__clang__) && (defined(_M_IX86) || defined(_M_X64))
#include <intrin.h>
#define TRELLIS_CPU_RELAX() _mm_pause()
#elif defined(_MSC_VER) && !defined(__clang__) && defined(_M_ARM64)
#include <intrin.h>
#define TRELLIS_CPU_RELAX() __yield()
#elif (defined(__GNUC__) || defined(__clang__)) && (defined(__x86_64__) || defined(__i386__))
#define TRELLIS_CPU_RELAX() __builtin_ia32_pause()
#elif (defined(__GNUC__) || defined(__clang__)) && defined(__aarch64__)
#define TRELLIS_CPU_RELAX() __asm__ __volatile__("yield")
#else
#define TRELLIS_CPU_RELAX() static_cast<void>(0)
#endif

// TRELLIS_NOINLINE keeps a function out of line: the slow path of a singleton's slot, which runs
// once per singleton, so that the fast path, a load and a compare, stays small where it is inlined
// (ADR-0012 item 2). MSVC spells it __declspec(noinline) (untested locally).
#if defined(_MSC_VER) && !defined(__clang__)
#define TRELLIS_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#define TRELLIS_NOINLINE __attribute__((noinline))
#else
#define TRELLIS_NOINLINE
#endif

// TRELLIS_NO_UNIQUE_ADDRESS on an empty member and TRELLIS_EMPTY_BASES on a class with several
// empty bases let them take no space. An injector holds one stored binding per binding, most of
// them empty (dsl/module.hpp), and one slot holder per binding, empty for each binding it doesn't
// own (static/storage.hpp). GCC and Clang honour [[no_unique_address]] and lay out distinct empty
// bases at offset 0, as the Itanium C++ ABI says, so there the second macro is empty. MSVC, and
// clang-cl, which follows its layout, ignore the standard attribute and give each empty base after
// the first a byte of its own: they need the attribute spelled [[msvc::no_unique_address]] (cl
// 19.29 and later) and the class declared __declspec(empty_bases) (platform fixes, 2026-11-01).
// Untested locally, as MSVC runs only in CI, where tests/unit/create_test.cpp checks the sizes.
#if defined(_MSC_VER)
#define TRELLIS_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#define TRELLIS_EMPTY_BASES __declspec(empty_bases)
#else
#define TRELLIS_NO_UNIQUE_ADDRESS [[no_unique_address]]
#define TRELLIS_EMPTY_BASES
#endif

#endif  // TRELLIS_CONFIG_HPP
