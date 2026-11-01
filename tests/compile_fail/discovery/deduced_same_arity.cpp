// compile-fail: deduction meets two constructors with the same number of parameters
// expect-id: TRL-E002
// expect-regex: TRL-E002: type not constructible: deduction found no public, non-deleted, unambiguous constructor
// expect-regex: same-arity overloads are ambiguous
// expect-regex: E002<app::router, trellis::(v1::)?detail::no_ctor_declared ?>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-regex-msvc: TRL-E002: type not constructible: deduction found no public, non-deleted, unambiguous constructor
// expect-regex-msvc: same-arity overloads are ambiguous
// expect-regex-msvc: E002<T,trellis::v1::detail::no_ctor_declared>[^\n]*\n *with\n *\[\n(?: +\w+=[^\n]*\n)*? +T=app::router(?:,|$)
// expect-regex-msvc: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// The placeholders fit both constructors equally, so overload resolution is ambiguous at that
// arity, and deduction can't tell that apart from a private, deleted or missing constructor (spike
// S1 §3.5, ADR-0003 item 5). The entry point is used twice, and the last regex allows no second
// error line.
#include <trellis/discovery/deduced.hpp>

namespace app {

struct table {};
struct cache {};
struct router {
  router(table /*routes*/, cache /*memo*/) {}
  router(cache /*memo*/, table /*routes*/) {}
};

}  // namespace app

int main() {
  [[maybe_unused]] trellis::detail::discovered_ctor_t<app::router>* first = nullptr;
  [[maybe_unused]] trellis::detail::discovered_ctor_t<app::router>* second = nullptr;
}
