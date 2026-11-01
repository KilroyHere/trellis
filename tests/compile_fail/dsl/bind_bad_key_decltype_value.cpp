// compile-fail: a binding type with a const key, taken by decltype and constructed
// expect-regex: bind<I>: I must be an object type without const, volatile or reference
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// bind<I> reports the key. The chain, .allow_copy() included, keeps that binding type, whose
// constructor has already reported its key, so constructing it again adds nothing.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006:
// one static_assert per root cause).
#include <trellis/trellis.hpp>

namespace {

struct sensor {};

}  // namespace

int main() {
  using bad = decltype(trellis::bind<const sensor>.in(trellis::singleton).allow_copy());
  [[maybe_unused]] auto injector = trellis::make_injector(bad{});
}
