// compile-fail: an override of a const key, which no binding has, beside the module that binds the key; then the injector used
// expect-regex: bind<I>: I must be an object type without const, volatile or reference
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// override<K> checks its key as bind<K> does, through the binding it wraps, with bind<I>'s
// message, once per key. Such an override has no key to replace, and make_injector adds no
// TRL-E009 for it: the key's error is its one error. The injector, which keeps the module's
// binding, then builds the reader as usual.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006:
// one static_assert per root cause).
#include <trellis/trellis.hpp>

namespace app {

struct sensor {
  sensor() = default;
};
struct reader {
  explicit reader(sensor& input) : input_(&input) {}
  sensor* input_;
};

}  // namespace app

int main() {
  const app::sensor fake{};
  auto injector = trellis::make_injector(
      trellis::make_module(trellis::bind<app::sensor>.in(trellis::singleton)),
      trellis::override<const app::sensor>.to_ref(fake));
  [[maybe_unused]] const app::reader built = injector.create<app::reader>();
}
