// compile-fail: .with_ctor<Args...>() on the binding that .to_shared(sp) makes, which builds nothing; written twice
// expect-regex: \.with_ctor<Args\.\.\.>\(\): the binding holds an object it was given
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// .to_shared(sp) binds an object that the caller shares, so the injector constructs nothing, and a
// constructor declaration has nothing to apply to. The misuse, written twice, is one error. The
// step returns the binding unchanged, which make_injector then takes without another error.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006:
// one static_assert per root cause).
#include <trellis/trellis.hpp>

#include <memory>

namespace app {

struct clock {
  long ticks = 0;
};

}  // namespace app

int main() {
  auto shared = std::make_shared<app::clock>();
  [[maybe_unused]] auto first = trellis::bind<app::clock>.to_shared(shared).with_ctor<long>();
  [[maybe_unused]] auto second = trellis::bind<app::clock>.to_shared(shared).with_ctor<long>();
  [[maybe_unused]] auto injector = trellis::make_injector(first);
}
