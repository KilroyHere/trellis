// compile-fail: deduction meets a class whose only constructor is its copy constructor
// expect-id: TRL-E002
// expect-regex: TRL-E002: type not constructible: deduction found no public, non-deleted, unambiguous constructor
// expect-regex: E002<app::token, trellis::(v1::)?detail::no_ctor_declared ?>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-regex-msvc: TRL-E002: type not constructible: deduction found no public, non-deleted, unambiguous constructor
// expect-regex-msvc: E002<T,trellis::v1::detail::no_ctor_declared>[^\n]*\n *with\n *\[\n(?: +\w+=[^\n]*\n)*? +T=app::token(?:,|$)
// expect-regex-msvc: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// The placeholders never convert to the class under construction, which excludes its copy and move
// constructors (spike S1 §3.5); nothing else is left to call. The entry point is used twice, and
// the last regex allows no second error line.
#include <trellis/discovery/deduced.hpp>

namespace app {

struct token {
  token(const token& /*other*/) {}
};

}  // namespace app

int main() {
  [[maybe_unused]] trellis::detail::discovered_ctor_t<app::token>* first = nullptr;
  [[maybe_unused]] trellis::detail::discovered_ctor_t<app::token>* second = nullptr;
}
