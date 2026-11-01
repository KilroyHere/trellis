// compile-fail: .to_ref() of a temporary, which would dangle
// expect-regex: to_ref
// expect-regex: delete
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// The binding would hold the address of an object destroyed at the end of the statement, so the
// overload for rvalues is deleted.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006:
// one static_assert per root cause).
#include <trellis/trellis.hpp>

namespace {

struct settings {};

}  // namespace

int main() { [[maybe_unused]] auto binding = trellis::bind<settings>.to_ref(settings{}); }
