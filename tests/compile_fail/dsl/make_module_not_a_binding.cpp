// compile-fail: make_module with an argument that is neither a binding nor a module
// expect-regex: make_module and make_injector take bindings
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// The bindings around the misplaced argument add no errors of their own.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006:
// one static_assert per root cause).
#include <trellis/trellis.hpp>

namespace {

struct cache {};
struct config {};

const config the_config{};

}  // namespace

int main() {
  [[maybe_unused]] auto bindings =
      trellis::make_module(trellis::bind<cache>, 3.5, trellis::bind<config>.to_ref(the_config));
}
