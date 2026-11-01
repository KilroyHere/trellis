// compile-fail: an override whose key no other argument of make_injector binds, in two identical calls, with both injectors used afterwards
// expect-id: TRL-E009
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
//
// An override replaces the binding of its key that make_injector's other arguments provide (plan
// §6.10), and the module binds only the logger: nothing to replace, TRL-E009. make_injector is
// instantiated once for the argument list, so the second call adds no error; and an injector whose
// overrides don't apply resolves nothing more, so creating a service that needs the clock, or the
// clock itself, adds none either (static/injector_traits.hpp).
//
// The test checks the ID and that the error is the only one, not the message's wording, which the
// diagnostics engine (T1.6) owns. The regex that starts with \A accepts only an output with
// exactly one error line (ADR-0006: one static_assert per root cause).
#include <trellis/trellis.hpp>

namespace app {

struct clock {
  clock() = default;
  clock(const clock&) = delete;
  clock& operator=(const clock&) = delete;
  virtual ~clock() = default;
  [[nodiscard]] virtual long now() const = 0;
};
struct fixed_clock final : clock {
  [[nodiscard]] long now() const override { return 42; }
};
struct logger {
  logger() = default;
};
struct service {
  service(clock& time, logger& log) : time_(&time), log_(&log) {}
  clock* time_;
  logger* log_;
};

inline auto app_module() {
  return trellis::make_module(trellis::bind<logger>.in(trellis::singleton));
}

}  // namespace app

int main() {
  app::fixed_clock fixed;
  auto first =
      trellis::make_injector(app::app_module(), trellis::override<app::clock>.to_ref(fixed));
  auto second =
      trellis::make_injector(app::app_module(), trellis::override<app::clock>.to_ref(fixed));
  [[maybe_unused]] const app::service built = first.create<app::service>();
  [[maybe_unused]] app::clock& time = second.create<app::clock&>();
}
