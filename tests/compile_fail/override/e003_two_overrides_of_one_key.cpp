// compile-fail: two overrides of one key in one make_injector call, made twice, with both injectors used afterwards
// expect-id: TRL-E003
// expect-regex: duplicate binding for a key: two overrides
// expect-regex: E003<.*clock, ?trellis::(v1::)?override_binding<.*fixed_clock.*trellis::(v1::)?override_binding<.*slow_clock|error C2338:
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
//
// Each key may be overridden once per injector: with two overrides, neither is the one to use, so
// make_injector reports TRL-E003, with the error type naming the key and both overrides in argument
// order (dsl/override.hpp). make_injector is instantiated once for the argument list, so the
// second call adds no error, and the injectors resolve nothing more, so creating from them adds
// none either.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006).
// The regex on the error type also accepts MSVC's static_assert error, C2338, in its place: MSVC
// prints the literal but not the condition (ADR-0006; untested here, as MSVC runs only in CI).
#include <trellis/trellis.hpp>

namespace app {

struct clock {
  clock() = default;
  clock(const clock&) = delete;
  clock& operator=(const clock&) = delete;
  virtual ~clock() = default;
  [[nodiscard]] virtual long now() const = 0;
};
struct system_clock final : clock {
  [[nodiscard]] long now() const override { return 1; }
};
struct fixed_clock final : clock {
  [[nodiscard]] long now() const override { return 42; }
};
struct slow_clock final : clock {
  [[nodiscard]] long now() const override { return 7; }
};
struct service {
  explicit service(clock& time) : time_(&time) {}
  clock* time_;
};

inline auto app_module() {
  return trellis::make_module(trellis::bind<clock>.to<system_clock>().in(trellis::singleton));
}

}  // namespace app

int main() {
  auto first = trellis::make_injector(app::app_module(),
                                      trellis::override<app::clock>.to<app::fixed_clock>(),
                                      trellis::override<app::clock>.to<app::slow_clock>());
  auto second = trellis::make_injector(app::app_module(),
                                       trellis::override<app::clock>.to<app::fixed_clock>(),
                                       trellis::override<app::clock>.to<app::slow_clock>());
  [[maybe_unused]] const app::service built = first.create<app::service>();
  [[maybe_unused]] app::clock& time = second.create<app::clock&>();
}
