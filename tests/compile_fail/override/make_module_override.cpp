// compile-fail: an override passed to make_module, twice; overrides are arguments of make_injector only
// expect-regex: make_module takes bindings and modules, not overrides
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// A module is a set of bindings; an override replaces a binding of the injector, so it goes to
// make_injector (dsl/override.hpp). make_module says so in a message of its own, ahead of the one
// for arguments that are neither bindings nor modules, and is instantiated once for the argument
// list, so the second call adds no error.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006:
// one static_assert per root cause).
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

}  // namespace app

int main() {
  [[maybe_unused]] auto first =
      trellis::make_module(trellis::bind<app::clock>.to<app::system_clock>(),
                           trellis::override<app::clock>.to<app::fixed_clock>());
  [[maybe_unused]] auto second =
      trellis::make_module(trellis::bind<app::clock>.to<app::system_clock>(),
                           trellis::override<app::clock>.to<app::fixed_clock>());
}
