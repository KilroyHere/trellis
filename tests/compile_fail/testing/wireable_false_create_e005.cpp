// compile-fail: wireable is false for a reference to a unique object, and create<T>() of it then reports TRL-E005, once
// expect-id: TRL-E005
// expect-regex: nothing would keep the object alive
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
//
// trellis::testing::wireable<Injector, Root> asks the check that create<Root>() makes itself
// (testing/wiring.hpp), so it is false exactly where create<Root>() fails to compile, and it adds
// no error of its own: the static_asserts on it compile, both ways, and the one error is
// create<T>()'s. tests/unit/testing_test.cpp checks wireable against every other error of
// create<T>().
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006).
#include <trellis/testing/wiring.hpp>
#include <trellis/trellis.hpp>

namespace app {

struct widget {
  widget() = default;
};

}  // namespace app

int main() {
  auto injector = trellis::make_injector(trellis::bind<app::widget>);
  static_assert(trellis::testing::wireable<decltype(injector), app::widget>);
  static_assert(!trellis::testing::wireable<decltype(injector), app::widget&>);
  [[maybe_unused]] const app::widget made = injector.create<app::widget>();
  [[maybe_unused]] app::widget& used = injector.create<app::widget&>();
}
