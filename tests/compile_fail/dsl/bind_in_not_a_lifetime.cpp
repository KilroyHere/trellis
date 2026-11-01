// compile-fail: .in() with a lifetime that .in() doesn't accept
// expect-regex: \.in\(lifetime\): the lifetime must be trellis::unique, trellis::singleton or trellis::singleton_inline
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// external_t is a lifetime, but one that .to_ref() and .to_ptr() set themselves.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006:
// one static_assert per root cause).
#include <trellis/trellis.hpp>

namespace {

struct cache {};

}  // namespace

int main() { [[maybe_unused]] auto binding = trellis::bind<cache>.in(trellis::external_t{}); }
