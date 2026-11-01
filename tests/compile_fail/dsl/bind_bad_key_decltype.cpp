// compile-fail: decltype(bind<I>) with a reference key, as a type only
// expect-regex: bind<I>: I must be an object type without const, volatile or reference
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// Only the type of bind<I> is named, and no binding object is ever made: the key is still checked,
// because deducing bind<I>'s type instantiates its initializer.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006:
// one static_assert per root cause).
#include <trellis/trellis.hpp>

namespace {

struct sensor {};

}  // namespace

int main() {
  using reference_key = decltype(trellis::bind<sensor&>);
  [[maybe_unused]] const reference_key* none = nullptr;
}
