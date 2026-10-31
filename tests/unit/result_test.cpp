// Tests for include/trellis/meta/result.hpp: trellis::failure, trellis::result<T> and
// trellis::make_unexpected (ADR-0009).
//
// tests/unit/CMakeLists.txt compiles this file in C++20 mode, where result<T> is the library's
// fallback, and in C++23 and C++26 modes, where it is std::expected<T, failure> wherever the
// standard library has it. The checks use only the operations both provide, so they cover both.
#include <trellis/config.hpp>
#include <trellis/meta/result.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#if TRELLIS_HAS_EXPECTED
#include <expected>
#endif

#include <gtest/gtest.h>

namespace {

using trellis::failure;
using trellis::make_unexpected;
using trellis::result;

// ---- which type result<T> is ------------------------------------------------------------------

#if TRELLIS_HAS_EXPECTED
static_assert(std::is_same_v<result<int>, std::expected<int, failure>>);
static_assert(std::is_same_v<result<void>, std::expected<void, failure>>);
#else
static_assert(std::is_same_v<result<int>, trellis::detail::fallback_result<int>>);
static_assert(std::is_same_v<result<void>, trellis::detail::fallback_result<void>>);
#endif
static_assert(std::is_same_v<result<int>::value_type, int>);
static_assert(std::is_same_v<result<int>::error_type, failure>);

// ---- failure ----------------------------------------------------------------------------------

constexpr failure oops{"oops"};
static_assert(oops.message() == "oops");
static_assert(oops == failure{"oops"});
static_assert(oops != failure{"other"});
static_assert(failure{std::string_view{}}.message().empty());
// A small value: a view of a message in static storage, copied freely.
static_assert(std::is_trivially_copyable_v<failure>);
static_assert(std::is_nothrow_copy_constructible_v<failure>);
// Explicit, so that no string becomes a failure by accident.
static_assert(!std::is_convertible_v<std::string_view, failure>);

// ---- a value ----------------------------------------------------------------------------------

constexpr result<int> forty_two = 42;
static_assert(forty_two.has_value());
static_assert(static_cast<bool>(forty_two));
static_assert(*forty_two == 42);
static_assert(forty_two.value_or(0) == 42);

struct point {
  int x;
  int y;
};
constexpr result<point> origin = point{0, 7};
static_assert(origin->y == 7);

// ---- a failure --------------------------------------------------------------------------------

constexpr result<int> failed = make_unexpected(failure{"no value"});
static_assert(!failed.has_value());
static_assert(!static_cast<bool>(failed));
static_assert(failed.error().message() == "no value");
static_assert(failed.error() == failure{"no value"});
static_assert(failed.value_or(7) == 7);

// ---- constant expressions, on temporaries too -------------------------------------------------

// GCC used to refuse these with the fallback, whose accessors went through std::get_if.
static_assert(*result<int>(42) == 42);
static_assert(result<int>(42).value_or(0) == 42);
static_assert(result<int>(make_unexpected(failure{"x"})).value_or(7) == 7);
static_assert(result<int>(make_unexpected(failure{"x"})).error().message() == "x");
static_assert(*result<failure>(failure{"v"}) == failure{"v"});
static_assert(result<failure>(failure{"v"}).value_or(failure{"w"}) == failure{"v"});

// ---- result<void> -----------------------------------------------------------------------------

constexpr result<void> done{};
static_assert(done.has_value());
constexpr result<void> not_done = make_unexpected(failure{"not done"});
static_assert(!not_done.has_value() && not_done.error() == failure{"not done"});

// ---- copying and moving follow T --------------------------------------------------------------

static_assert(std::is_copy_constructible_v<result<std::string>>);
static_assert(std::is_nothrow_move_constructible_v<result<std::string>>);
static_assert(!std::is_copy_constructible_v<result<std::unique_ptr<int>>>);
static_assert(std::is_move_constructible_v<result<std::unique_ptr<int>>>);
// What C++23 guarantees for std::expected, and the fallback matches: trivial when T is.
static_assert(std::is_trivially_copy_constructible_v<result<int>>);
static_assert(std::is_trivially_destructible_v<result<int>>);
static_assert(!std::is_trivially_destructible_v<result<std::string>>);
// A value converts implicitly when T converts implicitly, and only explicitly otherwise.
static_assert(std::is_convertible_v<const char*, result<std::string>>);
static_assert(std::is_constructible_v<result<std::unique_ptr<int>>, int*>);
static_assert(!std::is_convertible_v<int*, result<std::unique_ptr<int>>>);
// A failure converts to no result: it goes through make_unexpected.
static_assert(!std::is_constructible_v<result<int>, failure>);
static_assert(!std::is_convertible_v<failure, result<int>>);

// A function that can fail, written once for both implementations.
result<int> parse_digit(char letter) {
  if (letter < '0' || letter > '9') {
    return make_unexpected(failure{"not a digit"});
  }
  return letter - '0';
}

// A value whose copy and move constructors throw once armed. std::expected allows such a T, as
// its failure type is nothrow-movable, and keeps the old failure when assigning a value throws.
bool fragile_armed = false;
struct fragile {
  int value = 0;
  explicit fragile(int initial) : value(initial) {}
  fragile(const fragile& other) : value(other.value) { throw_if_armed(); }
  // Throwing on purpose: the test is about a move constructor that can.
  // NOLINTNEXTLINE(performance-noexcept-move-constructor,bugprone-exception-escape)
  fragile(fragile&& other) : value(other.value) { throw_if_armed(); }
  fragile& operator=(const fragile&) = default;
  fragile& operator=(fragile&&) = default;
  ~fragile() = default;

  static void throw_if_armed() {
    if (fragile_armed) {
      throw std::runtime_error("fragile");
    }
  }
};

}  // namespace

TEST(Result, HoldsAValue) {
  const result<int> digit = parse_digit('7');
  ASSERT_TRUE(digit.has_value());
  EXPECT_TRUE(digit);
  EXPECT_EQ(*digit, 7);
  EXPECT_EQ(digit.value_or(-1), 7);
}

TEST(Result, HoldsAnError) {
  const result<int> digit = parse_digit('x');
  ASSERT_FALSE(digit.has_value());
  EXPECT_FALSE(digit);
  EXPECT_EQ(digit.error().message(), "not a digit");
  EXPECT_EQ(digit.value_or(-1), -1);
}

TEST(Result, HoldsNonTrivialValues) {
  result<std::string> text = std::string("abc");
  ASSERT_TRUE(text.has_value());
  EXPECT_EQ(text->size(), 3U);
  text->append("def");
  EXPECT_EQ(*text, "abcdef");

  result<std::string> copy = text;
  EXPECT_EQ(*copy, "abcdef");
  copy = make_unexpected(failure{"gone"});
  ASSERT_FALSE(copy.has_value());
  EXPECT_EQ(copy.error().message(), "gone");
  copy = text;
  ASSERT_TRUE(copy.has_value());
  EXPECT_EQ(*copy, "abcdef");

  const result<std::string> moved = std::move(text);
  EXPECT_EQ(*moved, "abcdef");
  EXPECT_EQ(moved.value_or("fallback"), "abcdef");
  const result<std::string> missing = make_unexpected(failure{"missing"});
  EXPECT_EQ(missing.value_or("fallback"), "fallback");
}

TEST(Result, HoldsMoveOnlyValues) {
  result<std::unique_ptr<int>> owner = std::make_unique<int>(5);
  ASSERT_TRUE(owner.has_value());
  EXPECT_EQ(**owner, 5);
  // Moving the result moves its value.
  result<std::unique_ptr<int>> moved = std::move(owner);
  ASSERT_TRUE(moved.has_value());
  EXPECT_EQ(**moved, 5);
  // Moving out of the value through an rvalue result.
  const std::unique_ptr<int> taken = *std::move(moved);
  ASSERT_NE(taken, nullptr);
  EXPECT_EQ(*taken, 5);
}

TEST(Result, VoidResults) {
  result<void> ok{};
  EXPECT_TRUE(ok.has_value());
  ok = make_unexpected(failure{"failed"});
  ASSERT_FALSE(ok.has_value());
  EXPECT_EQ(ok.error().message(), "failed");
  ok = result<void>{};
  EXPECT_TRUE(ok.has_value());
}

TEST(Result, AssigningAValueThatThrowsKeepsTheFailure) {
  result<fragile> target = make_unexpected(failure{"kept"});
  const result<fragile> copy_source = fragile{1};
  result<fragile> move_source = fragile{2};
  fragile_armed = true;
  EXPECT_THROW(target = copy_source, std::runtime_error);
  ASSERT_FALSE(target.has_value());
  EXPECT_EQ(target.error().message(), "kept");
  EXPECT_THROW(target = std::move(move_source), std::runtime_error);
  ASSERT_FALSE(target.has_value());
  EXPECT_EQ(target.error().message(), "kept");
  fragile_armed = false;
  target = copy_source;
  ASSERT_TRUE(target.has_value());
  EXPECT_EQ(target->value, 1);
}

TEST(Result, AssignmentCoversEveryPairOfStates) {
  result<std::string> value_a = std::string("a");
  result<std::string> value_b = std::string("b");
  result<std::string> failing = make_unexpected(failure{"f"});
  value_a = value_b;  // value over value
  EXPECT_EQ(*value_a, "b");
  value_a = failing;  // failure over value
  ASSERT_FALSE(value_a.has_value());
  EXPECT_EQ(value_a.error().message(), "f");
  value_a = value_b;  // value over failure
  EXPECT_EQ(*value_a, "b");
  result<std::string>& self = failing;
  failing = self;  // failure over itself
  ASSERT_FALSE(failing.has_value());
  EXPECT_EQ(failing.error().message(), "f");
}
