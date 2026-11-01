// compile-fail: .with_ctor<Args...>() on an override made with .to_ref(x), which builds nothing; written on an lvalue and an rvalue override
// expect-regex: \.with_ctor<Args\.\.\.>\(\): the binding holds an object it was given
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// An override that holds an object rejects a constructor declaration as the binding it wraps does
// (dsl/bind.hpp): with bind<I>'s static_assert and message, once per binding type and declaration,
// whether the override is an lvalue or an rvalue. The step returns the override unchanged, which
// still has a binding to replace, so make_injector adds no error.
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
struct fake_clock final : clock {
  [[nodiscard]] long now() const override { return 2; }
};

}  // namespace app

int main() {
  app::fake_clock fake;
  const auto clock_override = trellis::override<app::clock>.to_ref(fake);
  [[maybe_unused]] auto from_lvalue = clock_override.with_ctor<int>();
  [[maybe_unused]] auto injector =
      trellis::make_injector(trellis::bind<app::clock>.to<app::system_clock>(),
                             trellis::override<app::clock>.to_ref(fake).with_ctor<int>());
}
