// compile-fail: .with_ctor<Args...>() on the bindings that .to_ref(x) and .to_ptr(p) make, which build nothing; written twice
// expect-regex: \.with_ctor<Args\.\.\.>\(\): the binding holds an object it was given
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// .to_ref(x) and .to_ptr(p) bind an object that the caller owns, so the injector constructs
// nothing, and a constructor declaration has nothing to apply to. Both make the same binding type,
// so the misuse, written once through each, is one error. The step returns the binding unchanged,
// which make_injector then takes without another error.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006:
// one static_assert per root cause).
#include <trellis/trellis.hpp>

namespace app {

struct config {
  int port = 0;
};

}  // namespace app

int main() {
  app::config settings{};
  [[maybe_unused]] auto by_reference = trellis::bind<app::config>.to_ref(settings).with_ctor<int>();
  [[maybe_unused]] auto by_pointer = trellis::bind<app::config>.to_ptr(&settings).with_ctor<int>();
  [[maybe_unused]] auto injector = trellis::make_injector(by_reference);
}
