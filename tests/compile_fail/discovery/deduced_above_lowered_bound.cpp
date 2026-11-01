// compile-fail: deduction finds a constructor only above a lowered bound of 4 parameters
// expect-id: TRL-E010
// expect-regex: TRL-E010: constructor arity above max_ctor_arity
// expect-regex: E010<app::service, 4(UL)?>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-regex-msvc: TRL-E010: constructor arity above max_ctor_arity
// expect-regex-msvc: E010<T,4>[^\n]*\n *with\n *\[\n(?: +\w+=[^\n]*\n)*? +T=app::service(?:,|$)
// expect-regex-msvc: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// The bound is the injector's max_ctor_arity, which T1.5 will pass as the last argument of
// discovered_ctor_t; here an injector with max_ctor_arity = 4 meets a constructor of 5. The entry
// point is used twice, and the last regex allows no second error line.
#include <trellis/discovery/deduced.hpp>

namespace app {

struct part {};
struct service {
  service(part /*a*/, part /*b*/, part /*c*/, part /*d*/, part /*e*/) {}
};

}  // namespace app

using no_binding_ctor = trellis::detail::no_ctor_declared;

int main() {
  [[maybe_unused]] trellis::detail::discovered_ctor_t<app::service, no_binding_ctor, 4>* first =
      nullptr;
  [[maybe_unused]] trellis::detail::discovered_ctor_t<app::service, no_binding_ctor, 4>* second =
      nullptr;
}
