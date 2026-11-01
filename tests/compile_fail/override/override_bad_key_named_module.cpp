// compile-fail: a test injector's type spelled through decltype, from a named module and an override of a const key
// expect-regex: bind<I>: I must be an object type without const, volatile or reference
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// The documented testing pattern names a test injector only inside decltype, as in
// trellis::testing::wireable<decltype(...), App> (plan §6.10), beside a named module whose own
// bindings are spelled through decltype (ADR-0010). override<K> is an auto variable, so its key is
// checked there too, once (dsl/override.hpp), as bind<I>'s is in a named module
// (tests/compile_fail/dsl/bind_bad_key_named_module.cpp). wireable is false for that injector type,
// as make_injector can't make it, and says so without an error of its own.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006:
// one static_assert per root cause).
#include <trellis/testing/wiring.hpp>
#include <trellis/trellis.hpp>

#include <utility>

namespace app {

struct sensor {
  sensor() = default;
};
struct reader {
  explicit reader(sensor& input) : input_(&input) {}
  sensor* input_;
};

struct app_module : decltype(trellis::make_module(trellis::bind<sensor>.in(trellis::singleton))){};

}  // namespace app

int main() {
  using test_injector = decltype(trellis::make_injector(
      app::app_module{},
      trellis::override<const app::sensor>.to_ref(std::declval<const app::sensor&>())));
  static_assert(!trellis::testing::wireable<test_injector, app::reader>);
  static_assert(
      trellis::testing::wireable<decltype(trellis::make_injector(app::app_module{})), app::reader>);
}
