// compile-fail: a 21-class chain of deduced constructors behind four application headers, failing at the bottom, with the call to create<T>() over three lines
// expect-id: TRL-E012
// expect-regex: TRL-E012: unbound concrete type requested by reference
// expect-regex: trellis::error::E012<app::sensor, ?trellis::error::request<app::sensor ?&> ?>
// expect-regex: trellis::error::path<app::c0, ?app::c1, ?app::c2,[^<>]*app::c19, ?app::c20>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-regex-msvc: TRL-E012: unbound concrete type requested by reference
// expect-regex-msvc: trellis::error::E012<app::sensor,trellis::error::request<R>>[^\n]*\n *with\n *\[\n(?: +\w+=[^\n]*\n)*? +R=app::sensor &(?:,|$)
// expect-regex-msvc: trellis::error::path<app::c0, ?app::c1, ?app::c2,[^<>]*app::c19, ?app::c20>
// expect-regex-msvc: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
//
// NFR-07's 40 lines whatever the layout of the user's code (T1.6 review): the two costs together,
// five levels of include above <trellis/trellis.hpp> and a call over three lines, which GCC 14
// quotes in full.
//
// The regexes accept MSVC's spelling of template arguments, without a space after the comma; the
// regex that starts with \A accepts only an output with exactly one error line.
#include "layout/app_all.hpp"

int main() {
  auto injector = trellis::make_injector();
  // The call stays on three lines.
  // clang-format off
  [[maybe_unused]] const app::c0 first =
      injector
          .create<app::c0>();
  // clang-format on
}
