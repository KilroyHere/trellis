// compile-fail: options{.eager = true} and a singleton that takes an unbound type by reference; make_injector reports it once
// expect-id: TRL-E012
// expect-regex: unbound concrete type requested by reference
// expect-regex: E012<.*sensor, trellis::error::request<.*sensor ?&>|error C2338:
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 50
// expect-max-lines-msvc: 102
//
// Eager mode builds every singleton inside make_injector (plan §7.5), so every singleton must be
// buildable, even one that nothing creates: a missing owner fails when the injector is made, as
// create<T>() would report it. Two singletons need the sensor: one error.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006).
// The regex on the error type also accepts MSVC's static_assert error, C2338, in its place: MSVC
// prints the literal but not the condition (ADR-0006; untested here, as MSVC runs only in CI).
#include <trellis/trellis.hpp>

namespace {

struct sensor {};
struct monitor {
  explicit monitor(sensor& input) : input_(&input) {}
  sensor* input_;
};
struct recorder {
  explicit recorder(sensor& input) : input_(&input) {}
  sensor* input_;
};

}  // namespace

int main() {
  [[maybe_unused]] auto injector = trellis::make_injector<trellis::options{.eager = true}>(
      trellis::bind<monitor>.in(trellis::singleton),
      trellis::bind<recorder>.in(trellis::singleton_inline));
}
