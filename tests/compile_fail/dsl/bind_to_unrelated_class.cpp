// compile-fail: bind<I>.to<C>() with a class C that doesn't derive from I
// expect-regex: bind<I>\.to<C>\(\): C must be I, or a class publicly derived from I
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// The injector would construct a C where an I is requested.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006:
// one static_assert per root cause).
#include <trellis/trellis.hpp>

namespace {

struct printer {
  printer() = default;
  printer(const printer&) = delete;
  printer& operator=(const printer&) = delete;
  virtual ~printer() = default;
  virtual void print() = 0;
};
struct scanner {};

}  // namespace

int main() { [[maybe_unused]] auto binding = trellis::bind<printer>.to<scanner>(); }
