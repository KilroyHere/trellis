// compile-fail: deduction meets a constructor template that accepts any argument
// expect-id: TRL-E004
// expect-regex: TRL-E004: ambiguous constructor: a constructor template or variadic constructor
// expect-regex: E004<app::greedy>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// The only constructor is a template that takes anything, so the one argument it would get says
// nothing about what the class needs (spike S1 §3.5). The entry point is used twice, and the last
// regex allows no second error line: the type is reported once. Until the injector exists (T1.5),
// discovery is reached through discovered_ctor_t, the engine's way into it.
#include <trellis/discovery/deduced.hpp>

namespace app {

struct greedy {
  template <class U>
  explicit greedy(U&& /*any*/) {}
};

}  // namespace app

int main() {
  [[maybe_unused]] trellis::detail::discovered_ctor_t<app::greedy>* first = nullptr;
  [[maybe_unused]] trellis::detail::discovered_ctor_t<const app::greedy>* second = nullptr;
}
