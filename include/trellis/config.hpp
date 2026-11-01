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

#endif  // TRELLIS_CONFIG_HPP
