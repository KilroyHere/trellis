// compile-fail: bind<I> with a const key, chained and passed to make_injector
// expect-regex: bind<I>: I must be an object type without const, volatile or reference
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// The key is checked where bind<I> is named, once, not again for each binding type the chain makes
// or that make_injector stores.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006:
// one static_assert per root cause).
#include <trellis/trellis.hpp>

namespace {

struct sensor {};

}  // namespace

int main() {
  [[maybe_unused]] auto injector =
      trellis::make_injector(trellis::bind<const sensor>.in(trellis::singleton));
}
