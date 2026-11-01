// compile-fail: override<I>.to<C>() with a class C that doesn't derive from I, passed to make_injector
// expect-regex: bind<I>\.to<C>\(\): C must be I, or a class publicly derived from I
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// An override's chain is bind<I>'s, with its checks and their messages (dsl/override.hpp): the
// injector would construct a C where an I is requested. The step then returns the override
// unchanged, an override of printer by itself, which still has a binding to replace, so
// make_injector adds no error.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006:
// one static_assert per root cause).
#include <trellis/trellis.hpp>

namespace app {

struct printer {
  printer() = default;
  printer(const printer&) = delete;
  printer& operator=(const printer&) = delete;
  virtual ~printer() = default;
  virtual void print() = 0;
};
struct laser_printer final : printer {
  void print() override {}
};
struct scanner {};

}  // namespace app

int main() {
  [[maybe_unused]] auto injector =
      trellis::make_injector(trellis::bind<app::printer>.to<app::laser_printer>(),
                             trellis::override<app::printer>.to<app::scanner>());
}
