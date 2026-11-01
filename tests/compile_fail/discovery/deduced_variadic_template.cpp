// compile-fail: deduction meets a variadic constructor template
// expect-id: TRL-E004
// expect-regex: TRL-E004: ambiguous constructor: a constructor template or variadic constructor
// expect-regex: E004<app::variadic>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// The constructor takes any number of arguments of any type, so every arity up to the bound
// "works" and none says what the class needs (spike S1 §3.5). The entry point is used twice, and
// the last regex allows no second error line.
#include <trellis/discovery/deduced.hpp>

namespace app {

struct variadic {
  template <class... Us>
  explicit variadic(Us&&... /*any*/) {}
};

}  // namespace app

int main() {
  [[maybe_unused]] trellis::detail::discovered_ctor_t<app::variadic>* first = nullptr;
  [[maybe_unused]] trellis::detail::discovered_ctor_t<app::variadic>* second = nullptr;
}
