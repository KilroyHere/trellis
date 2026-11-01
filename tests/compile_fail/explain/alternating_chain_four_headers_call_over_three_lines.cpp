// compile-fail: a 20-class chain of deduced constructors whose requests alternate between by value and by reference, behind four application headers, failing at the bottom, with the call to create<T>() over three lines
// expect-id: TRL-E012
// expect-regex: TRL-E012: unbound concrete type requested by reference
// expect-regex: trellis::error::E012<app::sensor, ?trellis::error::request<app::sensor ?&> ?>
// expect-regex: trellis::error::path<app::c0, ?app::c1, ?app::c2,[^<>]*app::c18, ?app::c19>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines-nfr07: 40
//
// NFR-07's 40 lines as owner decision 1A counts them, without the include chain and the excerpt
// of the user's call site (plan §3.2): the two costs of the user's layout together, five levels
// of include above <trellis/trellis.hpp> and a call over three lines, which GCC 14 quotes in full;
// the largest output of the four layouts. The chain's notes alternate between two lines of
// explain.hpp, which the compilers don't fold, so it reaches their backtrace limit
// (docs/internals/explain-pass.md, "What still grows").
//
// The ID and the message hold on MSVC too. The regexes on the error type, the path and the one
// error line, and the line budget, are GCC's and Clang's: cl spells the types in "with" blocks of
// its own, and NFR-07's 40 lines are for GCC and Clang.
#include "layout/alternating_app_all.hpp"

int main() {
  auto injector = trellis::make_injector(app::alternating_module());
  // The call stays on three lines.
  // clang-format off
  [[maybe_unused]] const app::c0 first =
      injector
          .create<app::c0>();
  // clang-format on
}
