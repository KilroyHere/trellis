// Tests for include/trellis/meta/type_map.hpp: entry, type_map, lookup_t and has_duplicate_keys_v.
//
// Most checks are static_asserts, so a wrong answer breaks the build; the TESTs repeat the main
// cases in ordinary code. tests/compile_fail/meta/ has the misuse that must not compile: a lookup
// of a missing key, building a map with a repeated key, and has_duplicate_keys_v of a non-map.
#include <trellis/meta/type_map.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

namespace {

using trellis::detail::contains_key_v;
using trellis::detail::entry;
using trellis::detail::has_duplicate_keys_v;
using trellis::detail::lookup_or_t;
using trellis::detail::lookup_t;
using trellis::detail::type_map;

struct k1 {};
struct k2 {};
struct k3 {};
struct incomplete;
struct abstract {
  abstract() = default;
  abstract(const abstract&) = delete;
  abstract& operator=(const abstract&) = delete;
  virtual ~abstract() = default;
  virtual void run() = 0;
};

// A class template that fails to compile when instantiated. Nothing may instantiate a key or a
// value, also not argument-dependent lookup, which instantiates the class template
// specializations among a call's argument types (T1.1 review).
template <class T>
struct poison {
  static_assert(sizeof(T) == 0, "poison<T> must never be instantiated");
};

// Wraps a type, to test keys that differ only inside a template argument.
template <class T>
struct w {};

using map3 = type_map<entry<k1, int>, entry<k2, char>, entry<k3, k1>>;

// A map with N entries key<I> -> std::integral_constant<std::size_t, I>: long enough that
// anything linear per lookup or quadratic per map would show in the build time, and past the 256
// operands Clang allows a fold expression.
template <std::size_t I>
struct key {};
template <std::size_t I>
using index_value = std::integral_constant<std::size_t, I>;
template <std::size_t... Is>
auto make_map(std::index_sequence<Is...>) -> type_map<entry<key<Is>, index_value<Is>>...>;
template <std::size_t N>
using big_map = decltype(make_map(std::make_index_sequence<N>{}));
// The same with one more entry that repeats key<Repeated>.
template <std::size_t Repeated, std::size_t... Is>
auto make_map_repeating(std::index_sequence<Is...>)
    -> type_map<entry<key<Is>, index_value<Is>>..., entry<key<Repeated>, void>>;

// ---- entry ------------------------------------------------------------------------------------

static_assert(std::is_same_v<entry<k1, int>::key, k1>);
static_assert(std::is_same_v<entry<k1, int>::value, int>);

// ---- lookup_t ---------------------------------------------------------------------------------

static_assert(std::is_same_v<lookup_t<k1, map3>, int>);
static_assert(std::is_same_v<lookup_t<k2, map3>, char>);
static_assert(std::is_same_v<lookup_t<k3, map3>, k1>);
// Values can be any type: references, void, cv-qualified, incomplete or abstract.
using odd_values = type_map<entry<k1, int&>, entry<k2, void>, entry<k3, const int>,
                            entry<int, incomplete>, entry<long, abstract>>;
static_assert(std::is_same_v<lookup_t<k1, odd_values>, int&>);
static_assert(std::is_same_v<lookup_t<k2, odd_values>, void>);
static_assert(std::is_same_v<lookup_t<k3, odd_values>, const int>);
static_assert(std::is_same_v<lookup_t<int, odd_values>, incomplete>);
static_assert(std::is_same_v<lookup_t<long, odd_values>, abstract>);
// So can keys, and keys are exact types: k1, const k1 and k1& are three different keys.
using exact_keys = type_map<entry<k1, int>, entry<const k1, char>, entry<k1&, long>,
                            entry<incomplete, short>, entry<abstract, bool>, entry<void, double>>;
static_assert(std::is_same_v<lookup_t<k1, exact_keys>, int>);
static_assert(std::is_same_v<lookup_t<const k1, exact_keys>, char>);
static_assert(std::is_same_v<lookup_t<k1&, exact_keys>, long>);
static_assert(std::is_same_v<lookup_t<incomplete, exact_keys>, short>);
static_assert(std::is_same_v<lookup_t<abstract, exact_keys>, bool>);
static_assert(std::is_same_v<lookup_t<void, exact_keys>, double>);
// A struct derived from a map is a map for lookup_t, so that a long entry list can hide behind a
// short name (struct app : type_map<...> {}, ADR-0010).
struct app : map3 {};
static_assert(std::is_same_v<lookup_t<k2, app>, char>);
// One entry, the first and the last of a long map.
static_assert(std::is_same_v<lookup_t<k1, type_map<entry<k1, int>>>, int>);
static_assert(lookup_t<key<0>, big_map<1000>>::value == 0);
static_assert(lookup_t<key<500>, big_map<1000>>::value == 500);
static_assert(lookup_t<key<999>, big_map<1000>>::value == 999);

// ---- has_duplicate_keys_v ---------------------------------------------------------------------

static_assert(!has_duplicate_keys_v<type_map<>>);
static_assert(!has_duplicate_keys_v<type_map<entry<k1, int>>>);
static_assert(!has_duplicate_keys_v<map3>);
static_assert(!has_duplicate_keys_v<exact_keys>);
static_assert(!has_duplicate_keys_v<type_map<entry<k1, int>, entry<k2, int>>>);  // equal values
// The same key with different values, and the same entry twice.
static_assert(has_duplicate_keys_v<type_map<entry<k1, int>, entry<k1, char>>>);
static_assert(has_duplicate_keys_v<type_map<entry<k1, int>, entry<k1, int>>>);
static_assert(has_duplicate_keys_v<type_map<entry<k1, int>, entry<k2, int>, entry<k1, int>>>);
static_assert(has_duplicate_keys_v<
              type_map<entry<k1, int>, entry<k2, int>, entry<k3, int>, entry<k2, long>>>);
static_assert(has_duplicate_keys_v<type_map<entry<void, int>, entry<void, int>>>);
static_assert(has_duplicate_keys_v<type_map<entry<incomplete, int>, entry<incomplete, char>>>);
// Long maps, with and without a repeated key, wherever it is.
static_assert(!has_duplicate_keys_v<big_map<1000>>);
static_assert(
    has_duplicate_keys_v<decltype(make_map_repeating<0>(std::make_index_sequence<1000>{}))>);
static_assert(
    has_duplicate_keys_v<decltype(make_map_repeating<999>(std::make_index_sequence<1000>{}))>);
// Asking doesn't build the map: the map below would be a compile error if it were instantiated
// (tests/compile_fail/meta/type_map_duplicate_keys.cpp), so has_duplicate_keys_v can guard it.
using repeated = type_map<entry<k1, int>, entry<k1, char>>;
static_assert(has_duplicate_keys_v<repeated>);

// ---- the map itself ---------------------------------------------------------------------------

// A map is an empty type: it holds no values, only its entries as bases.
static_assert(std::is_empty_v<map3>);
static_assert(std::is_empty_v<type_map<>>);
static_assert(std::is_base_of_v<entry<k2, char>, map3>);

// ---- keys and values are never instantiated ---------------------------------------------------

// Not by building the map, by a lookup of another key, or by the duplicate check.
using poisoned = type_map<entry<k1, int>, entry<k2, poison<int>>, entry<poison<long>, char>>;
static_assert(!has_duplicate_keys_v<poisoned>);
static_assert(std::is_empty_v<poisoned>);
static_assert(std::is_same_v<lookup_t<k1, poisoned>, int>);
static_assert(std::is_same_v<lookup_t<k2, poisoned>, poison<int>>);
static_assert(std::is_same_v<lookup_t<poison<long>, poisoned>, char>);
static_assert(has_duplicate_keys_v<type_map<entry<poison<int>, int>, entry<poison<int>, long>>>);
// A real one: std::optional of an abstract class fails to instantiate.
using with_optional = type_map<entry<k1, int>, entry<k2, std::optional<abstract>>>;
static_assert(std::is_same_v<lookup_t<k1, with_optional>, int>);
static_assert(std::is_same_v<lookup_t<k2, with_optional>, std::optional<abstract>>);

// ---- keys that differ only in & and && --------------------------------------------------------

// GCC 13 and 14 confuse T& with T&& when deducing against base classes, nested ones included, so
// these keys would look repeated and be found by neither lookup if the map deduced (T1.1 review).
using refs =
    type_map<entry<int&, char>, entry<int&&, long>, entry<w<int&>, short>, entry<w<int&&>, bool>,
             entry<std::function<void(int&)>, float>, entry<std::function<void(int&&)>, double>,
             entry<const int&, unsigned>, entry<const int&&, int>>;
static_assert(!has_duplicate_keys_v<refs>);
static_assert(std::is_same_v<lookup_t<int&, refs>, char>);
static_assert(std::is_same_v<lookup_t<int&&, refs>, long>);
static_assert(std::is_same_v<lookup_t<w<int&>, refs>, short>);
static_assert(std::is_same_v<lookup_t<w<int&&>, refs>, bool>);
static_assert(std::is_same_v<lookup_t<std::function<void(int&)>, refs>, float>);
static_assert(std::is_same_v<lookup_t<std::function<void(int&&)>, refs>, double>);
static_assert(std::is_same_v<lookup_t<const int&, refs>, unsigned>);
static_assert(std::is_same_v<lookup_t<const int&&, refs>, int>);
static_assert(!has_duplicate_keys_v<type_map<entry<int&, int>, entry<int&&, int>>>);
static_assert(
    has_duplicate_keys_v<type_map<entry<int&&, int>, entry<int, int>, entry<int&&, long>>>);
static_assert(has_duplicate_keys_v<type_map<entry<w<int&>, int>, entry<w<int&>, long>>>);

// ---- lookup_or_t and contains_key_v: the lookup without its assertion -------------------------

struct none {};
// A present key gives its value; a missing one gives the fallback, without an error.
static_assert(std::is_same_v<lookup_or_t<k2, map3, none>, char>);
static_assert(std::is_same_v<lookup_or_t<long, map3, none>, none>);
static_assert(std::is_same_v<lookup_or_t<const k1, map3, none>, none>);  // exact keys
static_assert(std::is_same_v<lookup_or_t<k1, type_map<>, void>, void>);
static_assert(
    std::is_same_v<lookup_or_t<k2, odd_values, none>, void>);     // a void value isn't "none"
static_assert(std::is_same_v<lookup_or_t<k2, app, none>, char>);  // a derived map
static_assert(contains_key_v<k1, map3> && contains_key_v<k3, map3>);
static_assert(!contains_key_v<long, map3> && !contains_key_v<k1&, map3>);
static_assert(!contains_key_v<k1, type_map<>>);
static_assert(contains_key_v<key<999>, big_map<1000>> && !contains_key_v<key<1000>, big_map<1000>>);
// Keys that differ only in & and &&, plain and nested.
static_assert(std::is_same_v<lookup_or_t<int&&, refs, none>, long>);
static_assert(std::is_same_v<lookup_or_t<w<int&>, refs, none>, short>);
static_assert(std::is_same_v<lookup_or_t<std::function<void(int&&)>, refs, none>, double>);
static_assert(std::is_same_v<lookup_or_t<int&, type_map<entry<int&&, long>>, none>, none>);
static_assert(std::is_same_v<lookup_or_t<w<int&&>, type_map<entry<w<int&>, long>>, none>, none>);
static_assert(contains_key_v<std::function<void(int&)>, refs>);
static_assert(!contains_key_v<std::function<void(const int&)>, refs>);
// Values are never instantiated, also when the lookup falls back.
static_assert(std::is_same_v<lookup_or_t<k1, poisoned, none>, int>);
static_assert(std::is_same_v<lookup_or_t<k2, poisoned, none>, poison<int>>);
static_assert(std::is_same_v<lookup_or_t<k3, poisoned, none>, none>);
static_assert(std::is_same_v<lookup_or_t<k3, poisoned, poison<char>>, poison<char>>);
static_assert(contains_key_v<poison<long>, poisoned> && !contains_key_v<k3, poisoned>);

}  // namespace

TEST(TypeMap, Lookup) {
  EXPECT_TRUE((std::is_same_v<lookup_t<k1, map3>, int>));
  EXPECT_TRUE((std::is_same_v<lookup_t<k3, map3>, k1>));
  EXPECT_TRUE((std::is_same_v<lookup_t<k2, odd_values>, void>));
  EXPECT_TRUE((std::is_same_v<lookup_t<const k1, exact_keys>, char>));
  EXPECT_TRUE((std::is_same_v<lookup_t<k2, app>, char>));
  EXPECT_EQ((lookup_t<key<999>, big_map<1000>>::value), 999U);
}

TEST(TypeMap, HasDuplicateKeys) {
  EXPECT_FALSE(has_duplicate_keys_v<type_map<>>);
  EXPECT_FALSE(has_duplicate_keys_v<map3>);
  EXPECT_TRUE((has_duplicate_keys_v<type_map<entry<k1, int>, entry<k1, char>>>));
  EXPECT_TRUE((has_duplicate_keys_v<type_map<entry<k1, int>, entry<k1, int>>>));
  EXPECT_FALSE(has_duplicate_keys_v<big_map<1000>>);
  EXPECT_TRUE(
      has_duplicate_keys_v<decltype(make_map_repeating<500>(std::make_index_sequence<1000>{}))>);
}

TEST(TypeMap, NeverInstantiatesKeysOrValues) {
  EXPECT_FALSE(has_duplicate_keys_v<poisoned>);
  EXPECT_TRUE((std::is_same_v<lookup_t<k1, poisoned>, int>));
  EXPECT_TRUE((std::is_same_v<lookup_t<k1, with_optional>, int>));
}

TEST(TypeMap, TellsLvalueFromRvalueReferenceKeys) {
  EXPECT_FALSE(has_duplicate_keys_v<refs>);
  EXPECT_TRUE((std::is_same_v<lookup_t<int&, refs>, char>));
  EXPECT_TRUE((std::is_same_v<lookup_t<int&&, refs>, long>));
  EXPECT_TRUE((std::is_same_v<lookup_t<std::function<void(int&&)>, refs>, double>));
}

TEST(TypeMap, LookupOrFallsBack) {
  EXPECT_TRUE((std::is_same_v<lookup_or_t<k2, map3, none>, char>));
  EXPECT_TRUE((std::is_same_v<lookup_or_t<long, map3, none>, none>));
  EXPECT_TRUE((contains_key_v<k1, map3>));
  EXPECT_FALSE((contains_key_v<long, map3>));
  EXPECT_TRUE((std::is_same_v<lookup_or_t<k3, poisoned, none>, none>));
}
