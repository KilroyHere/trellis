// compile-fail: bind<I> with a const key, then .to_ref() on it
// expect-regex: bind<I>: I must be an object type without const, volatile or reference
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// A request names its key without cv qualifiers, so bind<const sensor> could never be requested.
// .to_ref() after it adds no error of its own.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006:
// one static_assert per root cause).
#include <trellis/trellis.hpp>

namespace {

struct sensor {};

const sensor the_sensor{};

}  // namespace

int main() { [[maybe_unused]] auto binding = trellis::bind<const sensor>.to_ref(the_sensor); }
