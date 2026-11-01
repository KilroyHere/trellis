// compile-fail: make_injector with an lvalue override whose binding holds a move-only value
// expect-regex: pass that argument with std::move
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// make_injector copies an lvalue argument's binding, an override's included, and a .to_value() of
// a std::unique_ptr can't be copied: one error that says to pass the override with std::move, as
// for a module (tests/compile_fail/dsl/make_injector_lvalue_move_only.cpp).
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006:
// one static_assert per root cause).
#include <trellis/trellis.hpp>

#include <memory>
#include <utility>

int main() {
  auto production =
      trellis::make_module(trellis::bind<std::unique_ptr<int>>.to_value(std::make_unique<int>(1)));
  auto replacement = trellis::override<std::unique_ptr<int>>.to_value(std::make_unique<int>(2));
  [[maybe_unused]] auto injector = trellis::make_injector(std::move(production), replacement);
}
