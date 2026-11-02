// compile-fail: options{.strict = true} and a constructor parameter whose type has no binding
// expect-id: TRL-E013
// expect-regex: unbound concrete type requested in strict mode
// expect-regex: E013<.*clock_part, trellis::error::request<.*clock_part> ?>|error C2338:
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
// expect-max-lines-msvc: 102
//
// Strict mode turns off the implicit unique binding of unbound concrete types (ADR-0004, plan
// §6.13): every type the injector creates needs a binding, and a missing one is TRL-E013
// (provisional ID, T1.5's review). Two roots need the clock: one error.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006).
// The regex on the error type also accepts MSVC's static_assert error, C2338, in its place: MSVC
// prints the literal but not the condition (ADR-0006; untested here, as MSVC runs only in CI).
#include <trellis/trellis.hpp>

namespace {

struct clock_part {
  clock_part() = default;
  int ticks = 0;
};
struct timer {
  explicit timer(clock_part source) : ticks(source.ticks) {}
  int ticks;
};
struct bell {
  explicit bell(clock_part source) : ticks(source.ticks) {}
  int ticks;
};

}  // namespace

int main() {
  auto injector = trellis::make_injector<trellis::options{.strict = true}>(trellis::bind<timer>,
                                                                           trellis::bind<bell>);
  [[maybe_unused]] const timer first = injector.create<timer>();
  [[maybe_unused]] const bell second = injector.create<bell>();
}
