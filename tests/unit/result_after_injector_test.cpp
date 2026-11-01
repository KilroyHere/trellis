// result<T> and make_unexpected with meta/result.hpp included after the injector's headers, in
// every language mode (tests/unit/CMakeLists.txt), as result_test.cpp checks them with result.hpp
// first. trellis::detail declares a failure of its own, the enum of static/resolve.hpp, and the
// std::expected aliases of result.hpp, used from C++23 on, named failure unqualified, which then
// found the enum: make_unexpected didn't compile in this order until result.hpp qualified it.
// clang-format off: the order of the two includes is the subject of the test.
#include <trellis/static/injector.hpp>  // IWYU pragma: keep: included first on purpose
#include <trellis/meta/result.hpp>
// clang-format on

#include <type_traits>

#include <gtest/gtest.h>

namespace {

using trellis::failure;
using trellis::make_unexpected;
using trellis::result;

static_assert(std::is_same_v<result<int>::error_type, failure>);
static_assert(std::is_same_v<result<void>::error_type, failure>);

constexpr result<int> failed = make_unexpected(failure{"failed"});
static_assert(!failed.has_value());
static_assert(failed.error() == failure{"failed"});

}  // namespace

TEST(ResultAfterInjector, HoldsAValueOrAFailure) {
  const result<int> value = 7;
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(*value, 7);
  const result<void> missing = make_unexpected(failure{"missing"});
  ASSERT_FALSE(missing.has_value());
  EXPECT_EQ(missing.error().message(), "missing");
}
