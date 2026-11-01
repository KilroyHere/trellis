// compile-fail: .with_ctor<Args...>() on a value binding of a move-only type, written twice; the binding is still moved along
// expect-regex: \.with_ctor<Args\.\.\.>\(\): the binding holds an object it was given
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// The rvalue overload of the value binding's step moves the binding along, so a binding that holds
// a std::unique_ptr, which can't be copied, adds no error of its own to the one static_assert,
// neither in the step nor in make_injector, which takes it by value.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006:
// one static_assert per root cause).
#include <trellis/trellis.hpp>

#include <memory>
#include <utility>

int main() {
  [[maybe_unused]] auto first =
      trellis::bind<std::unique_ptr<int>>.to_value(std::make_unique<int>(1)).with_ctor<int*>();
  [[maybe_unused]] auto injector = trellis::make_injector(
      trellis::bind<std::unique_ptr<int>>.to_value(std::make_unique<int>(2)).with_ctor<int*>());
  [[maybe_unused]] auto moved = std::move(first).with_ctor<int*>();
}
