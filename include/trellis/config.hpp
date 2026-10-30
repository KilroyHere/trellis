// Trellis configuration: version, language mode and feature detection.
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

#endif  // TRELLIS_CONFIG_HPP
