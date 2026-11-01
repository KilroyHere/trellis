// compile-fail: bind<I> with a reference key, then .to<C>() and .in() on it
// expect-regex: bind<I>: I must be an object type without const, volatile or reference
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// bind<I> reports the key once. The chain functions pass a binding with a bad key along without
// checks of their own, so the calls after it add no errors.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006:
// one static_assert per root cause).
#include <trellis/trellis.hpp>

namespace {

struct sensor {};

}  // namespace

int main() {
  [[maybe_unused]] auto binding = trellis::bind<sensor&>.to<sensor>().in(trellis::singleton);
}
