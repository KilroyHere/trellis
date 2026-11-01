// compile-fail: a class with a constructor template that accepts anything, two classes below two roots that need it
// expect-id: TRL-E004
// expect-regex: TRL-E004: ambiguous constructor: a constructor template or variadic constructor accepts any argument
// expect-regex: trellis::error::E004<app::greedy>
// expect-regex: trellis::error::path<app::shell, ?app::engine>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
//
// Deduction can't tell what greedy's constructor template needs (TRL-E004). The explain pass
// reaches it from create<shell>() through engine and raises discovery's report, which is unique to
// the class, so console, which needs a greedy too, through reporter, adds no error. Both roots are
// two classes above the cause, so Clang and GCC both show shell's path (see
// e002_no_constructor_equal_roots.cpp).
//
// The regexes accept MSVC's spelling of template arguments, without a space after the comma; the
// regex that starts with \A accepts only an output with exactly one error line.
#include <trellis/trellis.hpp>

namespace app {

struct greedy {
  template <class T>
  explicit greedy(T&& /*anything*/) {}
};
struct engine {
  explicit engine(greedy part) : part_(part) {}
  greedy part_;
};
struct shell {
  explicit shell(engine core) : core_(core) {}
  engine core_;
};
struct reporter {
  explicit reporter(greedy part) : part_(part) {}
  greedy part_;
};
struct console {
  explicit console(reporter output) : output_(output) {}
  reporter output_;
};

}  // namespace app

int main() {
  auto injector = trellis::make_injector();
  [[maybe_unused]] const app::shell first = injector.create<app::shell>();
  [[maybe_unused]] const app::console second = injector.create<app::console>();
}
