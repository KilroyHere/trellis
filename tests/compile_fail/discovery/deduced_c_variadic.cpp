// compile-fail: deduction meets a C variadic constructor, C(...)
// expect-id: TRL-E004
// expect-regex: TRL-E004: ambiguous constructor: a constructor template or variadic constructor
// expect-regex: E004<app::c_variadic>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-regex-msvc: TRL-E004: ambiguous constructor: a constructor template or variadic constructor
// expect-regex-msvc: E004<T>[^\n]*\n *with\n *\[\n(?: +\w+=[^\n]*\n)*? +T=app::c_variadic(?:,|$)
// expect-regex-msvc: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// An ellipsis accepts any arguments, as a variadic template does (spike S1 §3.5). The entry point
// is used twice, and the last regex allows no second error line.
#include <trellis/discovery/deduced.hpp>

namespace app {

struct c_variadic {
  explicit c_variadic(...) {}
};

}  // namespace app

int main() {
  [[maybe_unused]] trellis::detail::discovered_ctor_t<app::c_variadic>* first = nullptr;
  [[maybe_unused]] trellis::detail::discovered_ctor_t<app::c_variadic>* second = nullptr;
}
