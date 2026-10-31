// compile-fail: trellis::result of a reference type
// expect-regex: result<T> needs an object type or void
// expect-max-lines: 20
//
// Compile-fail tests build in C++20 mode, where result<T> is the library's fallback; it rejects
// references, as std::expected does from C++23 on, so code means the same in every mode. Using the
// type is enough for the one error; constructing one would add GCC's own conversion error.
#include <trellis/meta/result.hpp>

bool holds_value(const trellis::result<int&>& reference) { return reference.has_value(); }

int main() {}
