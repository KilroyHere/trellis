// Smoke test: GoogleTest and gMock work, and the umbrella header provides the configuration macros.
#include <trellis/trellis.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#if !defined(TRELLIS_VERSION) || !defined(TRELLIS_HAS_STATIC_ASSERT_MESSAGE) || \
    !defined(TRELLIS_HAS_EXPECTED) || !defined(TRELLIS_HAS_PACK_INDEXING) ||    \
    !defined(TRELLIS_HAS_REFLECTION)
#error "<trellis/trellis.hpp> must provide the configuration macros"
#endif

namespace {

class clock_source {
 public:
  virtual ~clock_source() = default;
  [[nodiscard]] virtual long now() const = 0;
};

class mock_clock_source : public clock_source {
 public:
  MOCK_METHOD(long, now, (), (const, override));
};

}  // namespace

TEST(Smoke, UmbrellaHeaderDefinesTheVersion) {
  EXPECT_EQ(TRELLIS_VERSION,
            TRELLIS_VERSION_MAJOR * 10000 + TRELLIS_VERSION_MINOR * 100 + TRELLIS_VERSION_PATCH);
}

TEST(Smoke, GoogleMockWorks) {
  mock_clock_source clock;
  EXPECT_CALL(clock, now()).WillOnce(::testing::Return(42L));
  EXPECT_EQ(clock.now(), 42L);
}
