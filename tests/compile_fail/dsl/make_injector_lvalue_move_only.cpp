// compile-fail: make_injector with an lvalue module whose binding holds a move-only value
// expect-regex: pass that argument with std::move
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// make_injector copies the bindings of an lvalue argument, and a .to_value() of a
// std::unique_ptr can't be copied: one error that says to pass the module with std::move.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006:
// one static_assert per root cause).
#include <trellis/trellis.hpp>

#include <memory>

int main() {
  auto bindings =
      trellis::make_module(trellis::bind<std::unique_ptr<int>>.to_value(std::make_unique<int>(1)));
  [[maybe_unused]] auto injector = trellis::make_injector(bindings);
}
