// Tests for include/trellis/meta/type_list.hpp: type_list, contains_v, index_of_v and at_t.
//
// Most checks are static_asserts, so a wrong answer breaks the build; the TESTs repeat the main
// cases in ordinary code. tests/compile_fail/meta/ has the misuse that must not compile: an index
// past the end, a type that isn't in the list, and an argument that isn't a type_list.
#include <trellis/config.hpp>
#include <trellis/meta/type_list.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

namespace {

using trellis::detail::at_t;
using trellis::detail::contains_v;
using trellis::detail::index_of_v;
using trellis::detail::type_list;

struct a {};
struct b {};
struct c {};
struct incomplete;
struct abstract {
  abstract() = default;
  abstract(const abstract&) = delete;
  abstract& operator=(const abstract&) = delete;
  virtual ~abstract() = default;
  virtual void run() = 0;
};

// A class template that fails to compile when instantiated. The algorithms must never instantiate
// an element, also not through argument-dependent lookup, which instantiates the class template
// specializations among a call's argument types (T1.1 review).
template <class T>
struct poison {
  static_assert(sizeof(T) == 0, "poison<T> must never be instantiated");
};

// Wraps a type, to test types that differ only inside a template argument.
template <class T>
struct w {};

// A list of N distinct types, item<0> to item<N - 1>: longer than the 256 operands Clang allows a
// fold expression, so a fold-based algorithm would fail to compile here.
template <std::size_t I>
struct item {};
template <std::size_t... Is>
auto make_items(std::index_sequence<Is...>) -> type_list<item<Is>...>;
template <std::size_t N>
using items = decltype(make_items(std::make_index_sequence<N>{}));

// ---- type_list --------------------------------------------------------------------------------

static_assert(type_list<>::size == 0);
static_assert(type_list<a>::size == 1);
static_assert(type_list<a, b, a>::size == 3);  // a repeated type counts each time
static_assert(items<1000>::size == 1000);
// A list is an empty type that is never needed as a value, so incomplete and abstract types,
// void, references and other lists can all be elements.
static_assert(type_list<incomplete, void, int&, type_list<>>::size == 4);
static_assert(std::is_empty_v<type_list<a, b>>);

// ---- contains_v -------------------------------------------------------------------------------

static_assert(contains_v<a, type_list<a, b>>);
static_assert(contains_v<b, type_list<a, b>>);
static_assert(!contains_v<c, type_list<a, b>>);
static_assert(!contains_v<a, type_list<>>);
// Exact matches only: cv-qualifiers and references make a different type.
static_assert(!contains_v<const a, type_list<a>>);
static_assert(!contains_v<a&, type_list<a>>);
static_assert(!contains_v<a, type_list<const a, a&, a&&, a*>>);
static_assert(contains_v<a&, type_list<a, a&>>);
static_assert(contains_v<void, type_list<int, void>>);
static_assert(contains_v<incomplete, type_list<int, incomplete>>);
static_assert(contains_v<type_list<>, type_list<int, type_list<>>>);
// Beyond 256 types.
static_assert(contains_v<item<0>, items<1000>>);
static_assert(contains_v<item<999>, items<1000>>);
static_assert(!contains_v<item<1000>, items<1000>>);

// ---- index_of_v -------------------------------------------------------------------------------

static_assert(index_of_v<a, type_list<a, b, c>> == 0);
static_assert(index_of_v<b, type_list<a, b, c>> == 1);
static_assert(index_of_v<c, type_list<a, b, c>> == 2);
static_assert(index_of_v<a, type_list<b, a, a>> == 1);  // the first occurrence
static_assert(index_of_v<const a, type_list<a, const a>> == 1);
static_assert(index_of_v<a&&, type_list<a&, a&&>> == 1);
static_assert(index_of_v<item<0>, items<1000>> == 0);
static_assert(index_of_v<item<700>, items<1000>> == 700);
static_assert(index_of_v<item<999>, items<1000>> == 999);

// ---- at_t -------------------------------------------------------------------------------------

static_assert(std::is_same_v<at_t<0, type_list<a, b, c>>, a>);
static_assert(std::is_same_v<at_t<1, type_list<a, b, c>>, b>);
static_assert(std::is_same_v<at_t<2, type_list<a, b, c>>, c>);
static_assert(std::is_same_v<at_t<0, type_list<int&>>, int&>);
static_assert(std::is_same_v<at_t<1, type_list<void, const int>>, const int>);
static_assert(std::is_same_v<at_t<1, type_list<a, incomplete>>, incomplete>);
static_assert(std::is_same_v<at_t<2, type_list<a, a, b>>, b>);
static_assert(std::is_same_v<at_t<0, items<1000>>, item<0>>);
static_assert(std::is_same_v<at_t<999, items<1000>>, item<999>>);
// at_t and index_of_v are inverses on a list of distinct types.
static_assert(index_of_v<at_t<417, items<1000>>, items<1000>> == 417);

// Each implementation of at_t's element lookup, tested on its own wherever the compiler has it:
// the indexed base classes everywhere, __type_pack_element on GCC 14+ and Clang, and pack
// indexing in C++26 mode (Clang 20). at_t uses the first available of builtin, pack indexing and
// bases, so on most compilers only one of them is exercised through at_t.
template <template <std::size_t, class...> class Element>
constexpr bool picks_by_index() {
  return std::is_same_v<Element<0, a, b, c>, a> && std::is_same_v<Element<1, a, b, c>, b> &&
         std::is_same_v<Element<2, a, b, c>, c> && std::is_same_v<Element<1, a, a, b>, a> &&
         std::is_same_v<Element<0, int&>, int&> && std::is_same_v<Element<1, a, void>, void> &&
         std::is_same_v<Element<1, a, incomplete>, incomplete> &&
         // Types that differ only in & and &&, and elements that must not be instantiated.
         std::is_same_v<Element<1, int&, int&&>, int&&> &&
         std::is_same_v<Element<0, w<int&>, w<int&&>>, w<int&>> &&
         std::is_same_v<Element<0, int, poison<int>>, int> &&
         std::is_same_v<Element<1, int, poison<int>>, poison<int>>;
}
template <template <std::size_t, class...> class Element, std::size_t... Is>
constexpr bool picks_from_long_pack(std::index_sequence<Is...>) {
  return std::is_same_v<Element<0, item<Is>...>, item<0>> &&
         std::is_same_v<Element<sizeof...(Is) - 1, item<Is>...>, item<sizeof...(Is) - 1>>;
}

static_assert(picks_by_index<trellis::detail::pack_element_by_bases>());
static_assert(
    picks_from_long_pack<trellis::detail::pack_element_by_bases>(std::make_index_sequence<1000>{}));
#if TRELLIS_HAS_TYPE_PACK_ELEMENT
static_assert(picks_by_index<trellis::detail::pack_element_by_builtin>());
static_assert(picks_from_long_pack<trellis::detail::pack_element_by_builtin>(
    std::make_index_sequence<1000>{}));
#endif
#if TRELLIS_HAS_PACK_INDEXING
static_assert(picks_by_index<trellis::detail::pack_element_by_indexing>());
static_assert(picks_from_long_pack<trellis::detail::pack_element_by_indexing>(
    std::make_index_sequence<1000>{}));
#endif
static_assert(picks_by_index<trellis::detail::pack_element>());

// ---- elements are never instantiated ----------------------------------------------------------

// Not by the list itself, by any algorithm, or by the lookups behind them.
static_assert(type_list<poison<int>>::size == 1);
static_assert(contains_v<int, type_list<poison<int>, int>>);
static_assert(!contains_v<long, type_list<poison<int>, int>>);
static_assert(contains_v<poison<int>, type_list<int, poison<int>>>);
static_assert(index_of_v<int, type_list<poison<int>, int>> == 1);
static_assert(index_of_v<poison<int>, type_list<int, poison<int>>> == 1);
static_assert(std::is_same_v<at_t<0, type_list<int, poison<int>>>, int>);
static_assert(std::is_same_v<at_t<1, type_list<int, poison<int>>>, poison<int>>);
// A real one: std::optional of an abstract class fails to instantiate.
using with_optional = type_list<int, std::optional<abstract>, a>;
static_assert(contains_v<a, with_optional>);
static_assert(index_of_v<a, with_optional> == 2);
static_assert(std::is_same_v<at_t<2, with_optional>, a>);

// ---- types that differ only in & and && -------------------------------------------------------

// GCC 13 and 14 confuse T& with T&& when deducing against base classes, nested ones included, so
// these lists would break an algorithm that deduced (T1.1 review).
using refs = type_list<int&, int&&, w<int&>, w<int&&>, std::function<void(int&)>,
                       std::function<void(int&&)>, const int&, const int&&>;
static_assert(!contains_v<int&, type_list<int&&>> && !contains_v<int&&, type_list<int&>>);
static_assert(!contains_v<w<int&>, type_list<w<int&&>>>);
static_assert(contains_v<w<int&&>, refs> && contains_v<std::function<void(int&&)>, refs>);
static_assert(index_of_v<int&, refs> == 0 && index_of_v<int&&, refs> == 1);
static_assert(index_of_v<w<int&>, refs> == 2 && index_of_v<w<int&&>, refs> == 3);
static_assert(index_of_v<std::function<void(int&)>, refs> == 4);
static_assert(index_of_v<std::function<void(int&&)>, refs> == 5);
static_assert(index_of_v<const int&, refs> == 6 && index_of_v<const int&&, refs> == 7);
static_assert(std::is_same_v<at_t<1, refs>, int&&> && std::is_same_v<at_t<3, refs>, w<int&&>>);
static_assert(std::is_same_v<at_t<5, refs>, std::function<void(int&&)>>);

// ---- first_true, the scan behind index_of_v and the map's duplicate check ---------------------

static_assert(trellis::detail::first_true({true}) == 0);
static_assert(trellis::detail::first_true({false, true}) == 1);
static_assert(trellis::detail::first_true({false, false, true, true}) == 2);
static_assert(trellis::detail::first_true({false, false}) == 2);  // none: the list's size
static_assert(trellis::detail::first_true({}) == 0);              // an empty pack's list

}  // namespace

TEST(TypeList, Size) {
  EXPECT_EQ(type_list<>::size, 0U);
  EXPECT_EQ((type_list<a, b, a>::size), 3U);
  EXPECT_EQ(items<1000>::size, 1000U);
}

TEST(TypeList, Contains) {
  EXPECT_TRUE((contains_v<b, type_list<a, b>>));
  EXPECT_FALSE((contains_v<c, type_list<a, b>>));
  EXPECT_FALSE((contains_v<const a, type_list<a>>));
  EXPECT_FALSE((contains_v<a, type_list<>>));
  EXPECT_TRUE((contains_v<item<999>, items<1000>>));
}

TEST(TypeList, IndexOf) {
  EXPECT_EQ((index_of_v<c, type_list<a, b, c>>), 2U);
  EXPECT_EQ((index_of_v<a, type_list<b, a, a>>), 1U);
  EXPECT_EQ((index_of_v<item<700>, items<1000>>), 700U);
}

TEST(TypeList, At) {
  EXPECT_TRUE((std::is_same_v<at_t<1, type_list<a, b, c>>, b>));
  EXPECT_TRUE((std::is_same_v<at_t<0, type_list<int&>>, int&>));
  EXPECT_TRUE((std::is_same_v<at_t<999, items<1000>>, item<999>>));
}

TEST(TypeList, AtImplementations) {
  EXPECT_TRUE(picks_by_index<trellis::detail::pack_element_by_bases>());
#if TRELLIS_HAS_TYPE_PACK_ELEMENT
  EXPECT_TRUE(picks_by_index<trellis::detail::pack_element_by_builtin>());
#endif
#if TRELLIS_HAS_PACK_INDEXING
  EXPECT_TRUE(picks_by_index<trellis::detail::pack_element_by_indexing>());
#endif
}

TEST(TypeList, NeverInstantiatesElements) {
  EXPECT_TRUE((contains_v<int, type_list<poison<int>, int>>));
  EXPECT_EQ((index_of_v<int, type_list<poison<int>, int>>), 1U);
  EXPECT_TRUE((std::is_same_v<at_t<1, type_list<int, poison<int>>>, poison<int>>));
}

TEST(TypeList, TellsLvalueFromRvalueReferences) {
  EXPECT_FALSE((contains_v<int&, type_list<int&&>>));
  EXPECT_EQ((index_of_v<w<int&&>, refs>), 3U);
  EXPECT_TRUE((std::is_same_v<at_t<1, refs>, int&&>));
}
