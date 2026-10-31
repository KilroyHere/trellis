// Tests for include/trellis/meta/fixed_string.hpp: fixed_string<N>, a string usable as a template
// argument (named qualifiers, plan §6.5) and as a compile-time message buffer (plan §7.8).
//
// Most checks are static_asserts, so a wrong answer breaks the build; the TESTs check the same
// operations at run time.
#include <trellis/config.hpp>
#include <trellis/meta/fixed_string.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>

#include <gtest/gtest.h>

namespace {

using trellis::fixed_string;
using namespace std::string_view_literals;

// ---- construction -----------------------------------------------------------------------------

// From a string literal; N counts the characters, not the terminating '\0'.
constexpr fixed_string hello{"hello"};
static_assert(std::is_same_v<decltype(hello), const fixed_string<5>>);
static_assert(hello.size() == 5);
static_assert(hello.view() == "hello");
static_assert(hello.data()[5] == '\0');  // always terminated, so data() works as a C string
static_assert(hello[0] == 'h' && hello[4] == 'o');

constexpr fixed_string empty{""};
static_assert(std::is_same_v<decltype(empty), const fixed_string<0>>);
static_assert(empty.size() == 0 && empty.view().empty() && empty.data()[0] == '\0');

// An embedded '\0' is a character like any other.
constexpr fixed_string with_nul{"a\0b"};
static_assert(with_nul.size() == 3 && with_nul.view() == "a\0b"sv);

// Default construction gives N '\0' characters, to be filled in.
static_assert(fixed_string<3>{}.view() == "\0\0\0"sv);
static_assert(fixed_string<3>{}.data()[3] == '\0');

// Filling in a buffer of known size: how a message is built in two passes, one that counts the
// characters and one that writes them (ADR-0006, item 4).
constexpr fixed_string<3> abc = [] {
  fixed_string<3> text;
  text[0] = 'a';
  text[1] = 'b';
  text.data()[2] = 'c';
  return text;
}();
static_assert(abc.view() == "abc");

// ---- comparison -------------------------------------------------------------------------------

static_assert(hello == fixed_string{"hello"});
static_assert(hello != fixed_string{"help!"});
static_assert(hello != fixed_string{"hell"});  // different sizes compare unequal, not ill-formed
static_assert(hello == "hello" && "hello" == hello);
static_assert(hello != "hell" && "hello!" != hello);
static_assert(hello == "hello"sv && "hello"sv == hello);
static_assert(empty == "" && empty != "x");

// ---- concatenation ----------------------------------------------------------------------------

static_assert(std::is_same_v<decltype(hello + fixed_string{"!"}), fixed_string<6>>);
static_assert((hello + fixed_string{", world"}) == "hello, world");
static_assert((hello + "!") == "hello!");
static_assert(("<" + hello + ">") == "<hello>");
static_assert((empty + empty).size() == 0);
static_assert((hello + empty) == hello && (empty + hello) == hello);
static_assert((fixed_string{"TRL-"} + "E001").data()[8] == '\0');

// ---- iteration --------------------------------------------------------------------------------

constexpr std::size_t count_of(char wanted) {
  std::size_t count = 0;
  for (const char letter : hello) {
    count += letter == wanted ? 1 : 0;
  }
  return count;
}
static_assert(count_of('l') == 2 && count_of('z') == 0);
static_assert(hello.end() - hello.begin() == 5);

// ---- as a template argument -------------------------------------------------------------------

// A fixed_string is a structural type, so it can be a template argument, as named qualifiers
// need (trellis::named<"replica">, plan §6.5). Equal strings name the same specialization.
template <fixed_string Name>
struct named {
  static constexpr std::string_view name = Name.view();
};
static_assert(named<"replica">::name == "replica");
static_assert(std::is_same_v<named<"replica">, named<"replica">>);
static_assert(!std::is_same_v<named<"replica">, named<"primary">>);
static_assert(!std::is_same_v<named<"replica">, named<"replica2">>);
static_assert(named<"">::name.empty());

// ---- as a static_assert message (C++26, P2741) ------------------------------------------------

#if TRELLIS_HAS_STATIC_ASSERT_MESSAGE
// size() and data() are all P2741 asks of a message; a false assertion would print the text.
static_assert(true, hello + " from a generated message");
#endif

}  // namespace

TEST(FixedString, ConstructsFromLiterals) {
  const fixed_string text{"trellis"};
  EXPECT_EQ(text.size(), 7U);
  EXPECT_EQ(text.view(), "trellis");
  EXPECT_EQ(std::string(text.data()), "trellis");
  EXPECT_EQ(text.data()[7], '\0');
}

TEST(FixedString, IsWritable) {
  fixed_string text{"abc"};
  text[1] = 'x';
  EXPECT_EQ(text, "axc");
  fixed_string<2> blank;
  EXPECT_EQ(blank.view(), "\0\0"sv);
  blank[0] = 'o';
  blank[1] = 'k';
  EXPECT_EQ(blank, "ok");
}

TEST(FixedString, Compares) {
  const fixed_string text{"abc"};
  EXPECT_EQ(text, fixed_string{"abc"});
  EXPECT_NE(text, fixed_string{"abd"});
  EXPECT_NE(text, fixed_string{"ab"});
  EXPECT_EQ(text, "abc");
  EXPECT_EQ(text, std::string_view("abc"));
}

TEST(FixedString, Concatenates) {
  const fixed_string id{"E001"};
  const auto message = "trellis [TRL-" + id + "]";
  EXPECT_EQ(message.size(), 18U);
  EXPECT_EQ(message, "trellis [TRL-E001]");
  EXPECT_EQ(std::string(message.begin(), message.end()), "trellis [TRL-E001]");
}

TEST(FixedString, NamesTemplateSpecializations) {
  EXPECT_EQ(named<"replica">::name, "replica");
  EXPECT_FALSE((std::is_same_v<named<"replica">, named<"primary">>));
}
