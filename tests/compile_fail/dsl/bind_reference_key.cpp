// compile-fail: bind<I> with a reference type as the key
// expect-regex: bind<I>: I must be an object type without const, volatile or reference
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// A request names its key without references or cv qualifiers (const Config& asks for Config),
// so a key that has them could never be requested.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006:
// one static_assert per root cause).
#include <trellis/trellis.hpp>

int main() { [[maybe_unused]] const auto& binding = trellis::bind<int&>; }
