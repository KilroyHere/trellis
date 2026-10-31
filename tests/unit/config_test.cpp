// Tests for include/trellis/config.hpp: the version macros and the TRELLIS_HAS_* feature macros.
//
// tests/unit/CMakeLists.txt compiles this file once per language mode the compiler supports
// (TRELLIS_TEST_STANDARD is 20, 23 or 26), so each feature macro is tested in a mode where it must
// be 0 and in one where it can be 1. Most checks are static_asserts, so a wrong macro breaks the
// build; the TESTs record in the test log which features were detected.
#include <trellis/config.hpp>

// Each mode includes only what its checks use.
#if TRELLIS_HAS_STATIC_ASSERT_MESSAGE
#include <cstddef>
#endif
#if TRELLIS_HAS_EXPECTED
#include <expected>
#endif
#if TRELLIS_HAS_PACK_INDEXING || TRELLIS_HAS_REFLECTION
#include <type_traits>
#endif
// The builtin and signature macros (end of this file) are checked in every mode.
#include <array>
#include <string_view>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#if !defined(TRELLIS_VERSION_MAJOR) || !defined(TRELLIS_VERSION_MINOR) || \
    !defined(TRELLIS_VERSION_PATCH) || !defined(TRELLIS_VERSION) || !defined(TRELLIS_CPLUSPLUS)
#error "config.hpp must define the version macros and TRELLIS_CPLUSPLUS"
#endif
#if !defined(TRELLIS_HAS_STATIC_ASSERT_MESSAGE) || !defined(TRELLIS_HAS_EXPECTED) || \
    !defined(TRELLIS_HAS_PACK_INDEXING) || !defined(TRELLIS_HAS_REFLECTION)
#error "config.hpp must define every TRELLIS_HAS_* macro"
#endif

// The language mode is the one the build asked for. A -std flag that was silently overridden would
// make the C++23 and C++26 runs test C++20 again.
#if TRELLIS_TEST_STANDARD == 20
static_assert(TRELLIS_CPLUSPLUS == 202002L);
#elif TRELLIS_TEST_STANDARD == 23
static_assert(TRELLIS_CPLUSPLUS > 202002L && TRELLIS_CPLUSPLUS <= 202302L);
#elif TRELLIS_TEST_STANDARD == 26
static_assert(TRELLIS_CPLUSPLUS > 202302L);
#else
#error "TRELLIS_TEST_STANDARD must be 20, 23 or 26"
#endif

// Each feature macro is 0 or 1, so both `#if TRELLIS_HAS_X` and `if constexpr (TRELLIS_HAS_X)`
// work.
static_assert(TRELLIS_HAS_STATIC_ASSERT_MESSAGE == 0 || TRELLIS_HAS_STATIC_ASSERT_MESSAGE == 1);
static_assert(TRELLIS_HAS_EXPECTED == 0 || TRELLIS_HAS_EXPECTED == 1);
static_assert(TRELLIS_HAS_PACK_INDEXING == 0 || TRELLIS_HAS_PACK_INDEXING == 1);
static_assert(TRELLIS_HAS_REFLECTION == 0 || TRELLIS_HAS_REFLECTION == 1);

// A feature newer than the language mode is off, even where the compiler offers it as an extension.
// Clang 20 accepts user-generated static_assert messages and pack indexing in C++20 mode, and says
// so in __cpp_static_assert and __cpp_pack_indexing, but warns that they are C++26 extensions, and
// -Werror turns those warnings into errors.
#if TRELLIS_TEST_STANDARD < 23
static_assert(TRELLIS_HAS_EXPECTED == 0);
#endif
#if TRELLIS_TEST_STANDARD < 26
static_assert(TRELLIS_HAS_STATIC_ASSERT_MESSAGE == 0);
static_assert(TRELLIS_HAS_PACK_INDEXING == 0);
static_assert(TRELLIS_HAS_REFLECTION == 0);
#endif

// A toolchain known to have a feature must detect it, so that a misspelt feature-test macro can't
// silently switch a feature off. The versions are the ones checked locally (GCC 13 and 14, Clang 18
// and 20, libstdc++ 14, libc++ 18). Apple Clang numbers its versions differently and is left out.
#if !defined(__apple_build_version__)
#if TRELLIS_TEST_STANDARD == 26 && defined(__clang__) && __clang_major__ >= 18
static_assert(TRELLIS_HAS_STATIC_ASSERT_MESSAGE == 1);
#endif
#if TRELLIS_TEST_STANDARD == 26 && !defined(__clang__) && defined(__GNUC__) && __GNUC__ >= 14
static_assert(TRELLIS_HAS_STATIC_ASSERT_MESSAGE == 1);
#endif
#if TRELLIS_TEST_STANDARD == 26 && defined(__clang__) && __clang_major__ >= 20
static_assert(TRELLIS_HAS_PACK_INDEXING == 1);
#endif
#if TRELLIS_TEST_STANDARD >= 23 && defined(_LIBCPP_VERSION) && _LIBCPP_VERSION >= 180000
static_assert(TRELLIS_HAS_EXPECTED == 1);
#endif
// libstdc++'s <expected> needs __cpp_concepts >= 202002L, which Clang 18 doesn't define.
#if TRELLIS_TEST_STANDARD >= 23 && defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE >= 13 && \
    (!defined(__clang__) || __clang_major__ >= 20)
static_assert(TRELLIS_HAS_EXPECTED == 1);
#endif
#endif

// Where a macro is 1, the feature works under the project warning set with -Werror.
namespace {

#if TRELLIS_HAS_STATIC_ASSERT_MESSAGE
// A message produced by a constant expression: any object with constexpr size() and data()
// (P2741R3).
struct generated_message {
  [[nodiscard]] constexpr std::size_t size() const { return 2; }
  [[nodiscard]] constexpr const char* data() const { return "ok"; }
};
static_assert(true, generated_message{});
#endif

#if TRELLIS_HAS_PACK_INDEXING
template <class... Ts>
using first_of = Ts...[0];
static_assert(std::is_same_v<first_of<int, char>, int>);
#endif

#if TRELLIS_HAS_EXPECTED
constexpr std::expected<int, int> halve(int n) {
  if (n % 2 != 0) {
    return std::unexpected(n);
  }
  return n / 2;
}
static_assert(halve(8).value() == 4);
static_assert(halve(3).error() == 3);
#endif

#if TRELLIS_HAS_REFLECTION
// Untested locally: no installed compiler implements P2996. clang-format 18 doesn't know the
// reflection operator ^^ or splices, so it must not touch these lines.
// clang-format off
static_assert(^^int == ^^int);
static_assert(std::is_same_v<typename [: ^^int :], int>);
// clang-format on
#endif

}  // namespace

TEST(Config, VersionMatchesTheCMakeProject) {
  EXPECT_EQ(TRELLIS_VERSION_MAJOR, TRELLIS_TEST_PROJECT_VERSION_MAJOR);
  EXPECT_EQ(TRELLIS_VERSION_MINOR, TRELLIS_TEST_PROJECT_VERSION_MINOR);
  EXPECT_EQ(TRELLIS_VERSION_PATCH, TRELLIS_TEST_PROJECT_VERSION_PATCH);
}

TEST(Config, VersionNumberOrdersReleases) {
  // MAJOR * 10000 + MINOR * 100 + PATCH, so `#if TRELLIS_VERSION >= 10200` means 1.2.0 or later.
  EXPECT_EQ(TRELLIS_VERSION,
            TRELLIS_VERSION_MAJOR * 10000 + TRELLIS_VERSION_MINOR * 100 + TRELLIS_VERSION_PATCH);
  EXPECT_LT(TRELLIS_VERSION_MINOR, 100);
  EXPECT_LT(TRELLIS_VERSION_PATCH, 100);
}

TEST(Config, StaticAssertMessage) {
#if TRELLIS_HAS_STATIC_ASSERT_MESSAGE
  SUCCEED() << "a static_assert with a generated message compiled";
#else
  GTEST_SKIP() << "user-generated static_assert messages are not available in this mode";
#endif
}

TEST(Config, Expected) {
#if TRELLIS_HAS_EXPECTED
  EXPECT_EQ(halve(10).value_or(0), 5);
  EXPECT_FALSE(halve(7).has_value());
#else
  GTEST_SKIP() << "std::expected is not available in this mode";
#endif
}

TEST(Config, PackIndexing) {
#if TRELLIS_HAS_PACK_INDEXING
  EXPECT_TRUE((std::is_same_v<first_of<char, int>, char>));
#else
  GTEST_SKIP() << "pack indexing is not available in this mode";
#endif
}

TEST(Config, Reflection) {
#if TRELLIS_HAS_REFLECTION
  SUCCEED() << "reflection expressions compiled";
#else
  GTEST_SKIP() << "static reflection is not available in this mode";
#endif
}

// ---- Compiler builtins and the function signature, for the meta toolkit (T1.1) ----------------

// TRELLIS_HAS_EXCEPTIONS follows the build: the unit tests build with exceptions, and
// tests/unit/no_exceptions_test.cpp checks the other way.
#if !defined(TRELLIS_HAS_EXCEPTIONS)
#error "config.hpp must define TRELLIS_HAS_EXCEPTIONS"
#endif
static_assert(TRELLIS_HAS_EXCEPTIONS == 1);

#if !defined(TRELLIS_HAS_TYPE_PACK_ELEMENT) || !defined(TRELLIS_IS_SAME) || \
    !defined(TRELLIS_FUNCTION_SIGNATURE_CLASS_KEYS)
#error \
    "config.hpp must define TRELLIS_HAS_TYPE_PACK_ELEMENT, TRELLIS_IS_SAME and TRELLIS_FUNCTION_SIGNATURE_CLASS_KEYS"
#endif
static_assert(TRELLIS_HAS_TYPE_PACK_ELEMENT == 0 || TRELLIS_HAS_TYPE_PACK_ELEMENT == 1);
static_assert(TRELLIS_FUNCTION_SIGNATURE_CLASS_KEYS == 0 ||
              TRELLIS_FUNCTION_SIGNATURE_CLASS_KEYS == 1);

// A toolchain known to have __type_pack_element must detect it, and GCC 13, which lacks it, must
// not claim it. Apple Clang numbers its versions differently and is left out.
#if !defined(__apple_build_version__)
#if defined(__clang__) && __clang_major__ >= 18
static_assert(TRELLIS_HAS_TYPE_PACK_ELEMENT == 1);
#endif
#if !defined(__clang__) && defined(__GNUC__) && __GNUC__ >= 14
static_assert(TRELLIS_HAS_TYPE_PACK_ELEMENT == 1);
#endif
#if !defined(__clang__) && defined(__GNUC__) && __GNUC__ == 13
static_assert(TRELLIS_HAS_TYPE_PACK_ELEMENT == 0);
#endif
#endif

namespace {

#if TRELLIS_HAS_TYPE_PACK_ELEMENT
// Where the macro is 1, the builtin works under the project warning set with -Werror.
static_assert(std::is_same_v<__type_pack_element<1, int, char, long>, char>);
#endif

// TRELLIS_IS_SAME(A, B) is std::is_same_v<A, B>, through a builtin where there is one.
static_assert(TRELLIS_IS_SAME(int, int));
static_assert(!TRELLIS_IS_SAME(int, const int));
static_assert(!TRELLIS_IS_SAME(int, int&));
static_assert(!TRELLIS_IS_SAME(int, long));
static_assert(TRELLIS_IS_SAME(void, void));
// A type argument may contain a comma: the macro is variadic and passes it through whole.
static_assert(TRELLIS_IS_SAME(std::pair<int, char>, std::pair<int, char>));
static_assert(!TRELLIS_IS_SAME(std::pair<int, char>, std::pair<char, int>));
// It expands inside a pack expansion, as the type-list algorithms use it.
template <class T, class... Ts>
constexpr std::array<bool, sizeof...(Ts)> matches = {TRELLIS_IS_SAME(T, Ts)...};
static_assert(matches<int, char, int>[1] && !matches<int, char, int>[0]);

// TRELLIS_FUNCTION_SIGNATURE spells the enclosing function, with its template arguments.
#if defined(TRELLIS_FUNCTION_SIGNATURE)
template <class T>
constexpr std::string_view signature_of() {
  return TRELLIS_FUNCTION_SIGNATURE;
}
static_assert(signature_of<double>().find("double") != std::string_view::npos);
static_assert(signature_of<double>().find("signature_of") != std::string_view::npos);
#endif
#if defined(__clang__) || defined(__GNUC__) || defined(_MSC_VER)
#if !defined(TRELLIS_FUNCTION_SIGNATURE)
#error "GCC, Clang and MSVC each have a function-signature string, so TRELLIS_FUNCTION_SIGNATURE"
#endif
#endif
// Only MSVC's __FUNCSIG__ writes class keys ("struct x"); __PRETTY_FUNCTION__ doesn't.
#if defined(__clang__) || defined(__GNUC__)
static_assert(TRELLIS_FUNCTION_SIGNATURE_CLASS_KEYS == 0);
#elif defined(_MSC_VER)
static_assert(TRELLIS_FUNCTION_SIGNATURE_CLASS_KEYS == 1);
#endif

}  // namespace

TEST(Config, TypePackElement) {
#if TRELLIS_HAS_TYPE_PACK_ELEMENT
  EXPECT_TRUE((std::is_same_v<__type_pack_element<0, char, int>, char>));
#else
  GTEST_SKIP() << "__type_pack_element is not available; at_t uses indexed base classes";
#endif
}

TEST(Config, IsSame) {
  EXPECT_TRUE(TRELLIS_IS_SAME(int, int));
  EXPECT_FALSE(TRELLIS_IS_SAME(int, unsigned));
}
