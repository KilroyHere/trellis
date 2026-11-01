// compile-fail: the binding class template spelled directly, with a const key
// expect-regex: bind<I>: I must be an object type without const, volatile or reference
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// Without bind<I>, the binding's constructor still checks the key: it is the constructor that
// makes every binding object first.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006:
// one static_assert per root cause).
#include <trellis/trellis.hpp>

namespace {

struct sensor {};

}  // namespace

int main() {
  [[maybe_unused]] auto injector =
      trellis::make_injector(trellis::binding<const sensor, const sensor, trellis::singleton_t>{});
}
