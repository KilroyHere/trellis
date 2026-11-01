// compile-fail: a key bound by two modules and overridden once; the duplicate is reported with the two bindings as written
// expect-id: TRL-E003
// expect-regex: each key may be bound only once per injector
// expect-regex: E003<.*clock, ?trellis::(v1::)?binding<.*system_clock.*trellis::(v1::)?binding<.*atomic_clock|error C2338:
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 30
//
// An override replaces a binding; it doesn't resolve two bindings of one key, which are a mistake
// in the production wiring. So make_injector reports TRL-E003 as it would without the override,
// naming the two bindings the modules hold, not the override (dsl/override.hpp replaces nothing
// when keys repeat), and creating from the injector adds no error.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006).
// The regex on the error type also accepts MSVC's static_assert error, C2338, in its place: MSVC
// prints the literal but not the condition (ADR-0006; untested here, as MSVC runs only in CI).
#include <trellis/trellis.hpp>

#include <memory>

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
struct atomic_clock final : clock {
  [[nodiscard]] long now() const override { return 2; }
};
struct fixed_clock final : clock {
  [[nodiscard]] long now() const override { return 42; }
};

}  // namespace app

int main() {
  auto injector = trellis::make_injector(
      trellis::make_module(trellis::bind<app::clock>.to<app::system_clock>()),
      trellis::make_module(trellis::bind<app::clock>.to<app::atomic_clock>()),
      trellis::override<app::clock>.to<app::fixed_clock>());
  [[maybe_unused]] const std::unique_ptr<app::clock> time =
      injector.create<std::unique_ptr<app::clock>>();
}
