// compile-fail: a named module, spelled through decltype, whose binding has a const key
// expect-regex: bind<I>: I must be an object type without const, volatile or reference
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
// expect-fail-msvc: cl's error recovery adds follow-on errors after bind<I>'s key check
//
// The documented named-module pattern names bind<I> only inside decltype. bind<I> is an auto
// variable, so its initializer, which constructs the binding and so checks the key, is
// instantiated there too.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006:
// one static_assert per root cause).
#include <trellis/trellis.hpp>

namespace {

struct sensor {};

}  // namespace

int main() {
  using bad_module =
      decltype(trellis::make_module(trellis::bind<const sensor>.in(trellis::singleton)));
  struct app : bad_module {};
  [[maybe_unused]] auto injector = trellis::make_injector(app{});
}
