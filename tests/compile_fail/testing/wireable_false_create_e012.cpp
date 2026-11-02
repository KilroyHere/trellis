// compile-fail: wireable is false for a reference to an unbound type, and create<T>() of it then reports TRL-E012, once
// expect-id: TRL-E012
// expect-regex: unbound concrete type requested by reference
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
// expect-max-lines-msvc: 102
//
// trellis::testing::wireable<Injector, Root> asks the check that create<Root>() makes itself
// (testing/wiring.hpp): an unbound concrete type is built by value, but nothing would own it for
// a reference (ADR-0004). The static_asserts on wireable compile, both ways, and the one error is
// create<T>()'s.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006).
#include <trellis/testing/wiring.hpp>
#include <trellis/trellis.hpp>

namespace app {

struct widget {
  widget() = default;
};
struct holder {
  explicit holder(widget& held) : held_(&held) {}
  widget* held_;
};

}  // namespace app

int main() {
  auto injector = trellis::make_injector();
  static_assert(trellis::testing::wireable<decltype(injector), app::widget>);
  static_assert(!trellis::testing::wireable<decltype(injector), app::holder>);
  [[maybe_unused]] const app::widget made = injector.create<app::widget>();
  [[maybe_unused]] const app::holder held = injector.create<app::holder>();
}
