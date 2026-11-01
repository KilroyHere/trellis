// compile-fail: make_module with an lvalue binding that holds a move-only value
// expect-regex: pass that argument with std::move
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// The same rule as for make_injector: an lvalue argument is copied, and this binding can't be.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006:
// one static_assert per root cause).
#include <trellis/trellis.hpp>

#include <memory>

namespace {

struct cache {};

}  // namespace

int main() {
  auto pointer = trellis::bind<std::unique_ptr<int>>.to_value(std::make_unique<int>(1));
  [[maybe_unused]] auto bindings = trellis::make_module(trellis::bind<cache>, pointer);
}
