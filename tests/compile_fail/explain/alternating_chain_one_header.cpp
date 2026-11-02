// compile-fail: a 20-class chain of deduced constructors whose requests alternate between by value and by reference, in an application header, failing at the bottom
// expect-id: TRL-E012
// expect-regex: TRL-E012: unbound concrete type requested by reference
// expect-regex: trellis::error::E012<app::sensor, ?trellis::error::request<app::sensor ?&> ?>
// expect-regex: trellis::error::path<app::c0, ?app::c1, ?app::c2,[^<>]*app::c18, ?app::c19>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-regex-msvc: TRL-E012: unbound concrete type requested by reference
// expect-regex-msvc: trellis::error::E012<app::sensor,trellis::error::request<R>>[^\n]*\n *with\n *\[\n(?: +\w+=[^\n]*\n)*? +R=app::sensor &(?:,|$)
// expect-regex-msvc: trellis::error::path<app::c0, ?app::c1, ?app::c2,[^<>]*app::c18, ?app::c19>
// expect-regex-msvc: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines-nfr07: 40
// expect-max-lines-msvc: 612
//
// NFR-07's 40 lines as owner decision 1A counts them, without the include chain and the excerpt
// of the user's call site (plan §3.2). The explain pass's notes for a request by value and for a
// request by reference come from two lines of explain.hpp, which neither GCC nor Clang folds when
// they alternate, so this chain reaches the compilers' backtrace limit, unlike the by-value chain
// of deep_chain_*.cpp (docs/internals/explain-pass.md, "What still grows"). The header is
// included directly and the call fits on one line: the layout that the user's code adds least to.
//
// The ID and the message hold on MSVC too. The regexes on the error type, the path and the one
// error line, and the line budget, are GCC's and Clang's: cl spells the types in "with" blocks of
// its own, and NFR-07's 40 lines are for GCC and Clang.
#include "layout/alternating_chain.hpp"

int main() {
  auto injector = trellis::make_injector(app::alternating_module());
  [[maybe_unused]] const app::c0 first = injector.create<app::c0>();
}
