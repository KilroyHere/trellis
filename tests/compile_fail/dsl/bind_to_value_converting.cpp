// compile-fail: .to_value() with a value that only converts to the key
// expect-regex: bind<I>\.to_value\(v\): v must be an I, or of a class publicly derived from I
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// The binding stores the value with its own type, so the value must be of the key's type or
// derived from it: the caller converts a string literal for a std::string key, not the
// binding.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006:
// one static_assert per root cause).
#include <trellis/trellis.hpp>

#include <string>

int main() { [[maybe_unused]] auto binding = trellis::bind<std::string>.to_value("app"); }
