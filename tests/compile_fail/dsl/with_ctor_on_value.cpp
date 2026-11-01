// compile-fail: .with_ctor<Args...>() on the binding that .to_value(v) makes, which builds nothing; written on an lvalue and an rvalue
// expect-regex: \.with_ctor<Args\.\.\.>\(\): the binding holds an object it was given
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// .to_value(v) binds the injector's copy of a value, so the injector constructs nothing, and a
// constructor declaration has nothing to apply to. The value binding's step has two overloads, for
// an lvalue and an rvalue binding, as .allow_copy() does, so that a move-only value is moved
// along; both raise the same report, so the misuse, written through each, is one error.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006:
// one static_assert per root cause).
#include <trellis/trellis.hpp>

namespace app {

struct config {
  int port = 0;
};

}  // namespace app

int main() {
  const auto value = trellis::bind<app::config>.to_value(app::config{8080});
  [[maybe_unused]] auto from_lvalue = value.with_ctor<int>();
  [[maybe_unused]] auto from_rvalue =
      trellis::bind<app::config>.to_value(app::config{8080}).with_ctor<int>();
  [[maybe_unused]] auto injector = trellis::make_injector(from_rvalue);
}
