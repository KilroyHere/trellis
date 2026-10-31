// The compile-error vocabulary lives in namespace trellis::error, outside inline namespace v1
// (ADR-0006). Because v1 is inline, nothing public in trellis::v1 may be named error: GCC then
// calls every use of trellis::error ambiguous, and Clang rejects the namespace as a redefinition
// of the name. This test declares such a namespace next to the whole public API, through the
// umbrella header, and names both, so that a clash can't come back. It is compiled in every
// language mode, as result<T> is a different type in C++23.
#include <trellis/trellis.hpp>

#include <type_traits>

#include <gtest/gtest.h>

namespace trellis::error {

template <class...>
struct probe {};

}  // namespace trellis::error

namespace {

static_assert(std::is_empty_v<trellis::error::probe<int>>);
// The runtime error value is trellis::failure, and result<T> still reaches it as error_type.
static_assert(std::is_same_v<trellis::result<int>::error_type, trellis::failure>);

}  // namespace

TEST(ErrorNamespace, CoexistsWithThePublicApi) {
  const trellis::result<int> missing = trellis::make_unexpected(trellis::failure{"none"});
  EXPECT_FALSE(missing.has_value());
  EXPECT_EQ(missing.error().message(), "none");
  EXPECT_TRUE(std::is_empty_v<trellis::error::probe<int>>);
}
