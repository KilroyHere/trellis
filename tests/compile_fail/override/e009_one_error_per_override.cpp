// compile-fail: two overrides of two keys that no other argument binds, and one that applies: one TRL-E009 for each of the two
// expect-id: TRL-E009
// expect-regex: TRL-E009[\s\S]*TRL-E009
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 60
// expect-fail-msvc: cl reports a static_assert that fails in several specializations of one function template only once per translation unit
//
// Each override without a binding to replace is a mistake of its own, a root cause, so each gets
// its error (ADR-0006), and the override that applies gets none.
//
// The second regex accepts the ID twice, and the regex that starts with \A at most two error
// lines.
#include <trellis/trellis.hpp>

namespace app {

struct clock {};
struct fixed_clock : clock {
  fixed_clock() = default;
};
struct sensor {};
struct fake_sensor : sensor {
  fake_sensor() = default;
};
struct logger {};
struct quiet_logger : logger {
  quiet_logger() = default;
};

}  // namespace app

int main() {
  [[maybe_unused]] auto injector = trellis::make_injector(
      trellis::bind<app::logger>, trellis::override<app::clock>.to<app::fixed_clock>(),
      trellis::override<app::logger>.to<app::quiet_logger>(),
      trellis::override<app::sensor>.to<app::fake_sensor>());
}
