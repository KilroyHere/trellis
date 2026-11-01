// compile-fail: .to_value() of a const lvalue of a move-only type, which can't be copied
// expect-regex: bind<I>\.to_value\(v\): the binding can't store a copy of v
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// The binding would copy the value, and a const std::unique_ptr can be neither copied nor moved
// from.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006:
// one static_assert per root cause).
#include <trellis/trellis.hpp>

#include <memory>

int main() {
  const auto pointer = std::make_unique<int>(1);
  [[maybe_unused]] auto binding = trellis::bind<std::unique_ptr<int>>.to_value(pointer);
}
