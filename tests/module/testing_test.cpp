// Smoke test of the C++20 module trellis.testing (plan §6.10, §10): the testing helpers through
// `import trellis.testing;` alone, which also gives the rest of the library, as
// <trellis/testing/wiring.hpp> does.
#include <gtest/gtest.h>

#include "app.hpp"

import trellis.testing;

namespace {

// A class with a deduced constructor that needs a bound mod::clock_source.
class station {
 public:
  explicit station(mod::clock_source& clock) : clock_(&clock) {}
  [[nodiscard]] long now() const { return clock_->now(); }

 private:
  mod::clock_source* clock_;
};

inline auto make_station_injector() {
  return trellis::make_injector(
      trellis::bind<mod::clock_source>.to<mod::system_clock>().in(trellis::singleton));
}

// wireable reports nothing: false for an injector that can't build the root, which needs an
// abstract class that nothing binds.
static_assert(trellis::testing::wireable<decltype(make_station_injector()), station>);
static_assert(!trellis::testing::wireable<decltype(trellis::make_injector()), station>);

}  // namespace

TEST(ModuleTesting, CreatesEverySingleton) {
  auto injector = make_station_injector();
  trellis::testing::create_all(injector);
  EXPECT_EQ(injector.create<station>().now(), 42);
}
