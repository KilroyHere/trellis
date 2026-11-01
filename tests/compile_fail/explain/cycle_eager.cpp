// compile-fail: options{.eager = true} and two singletons whose deduced constructors take each other by reference, a dependency cycle; at most six errors, and no recursion limit
// expect-regex: depends on itself|is not usable in a constant expression|resulted in a non-constant expression|must be initialized by a constant expression
// expect-regex: app::b
// expect-regex: \A(?![\s\S]*(?:recursive template instantiation exceeded|template instantiation depth exceeds|too many errors emitted))
// expect-regex: \A(?!(?:[\s\S]*?(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: ){7})
// expect-regex-msvc: error C7608: atomic constraint should be a constant expression|error C2131: expression did not evaluate to a constant
// expect-regex-msvc: app::b
// expect-regex-msvc: \A(?![\s\S]*(?:recursive template instantiation exceeded|template instantiation depth exceeds|too many errors emitted|\bC1202\b|\bC2999\b|\bC1003\b))
// expect-regex-msvc: \A(?!(?:[\s\S]*?(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: ){7})
// expect-max-lines: 217
//
// make_injector checks every singleton first and runs the explain pass for the first that can't
// be built. The normal pass has reported the cycle with the compiler's own errors (ADR-0014 item
// 2: 2 to 6 errors in 20 to 217 lines), and the explain pass stops at the cycle and adds none.
// T1.6's first explain pass followed the cycle round: Clang gave 20 errors.
//
// The regex with {7} accepts at most six error lines; the one before it rejects a recursion limit
// and Clang's error limit.
#include <trellis/trellis.hpp>

namespace app {

struct b;
struct a {
  explicit a(b& other) : other_(&other) {}
  b* other_;
};
struct b {
  explicit b(a& other) : other_(&other) {}
  a* other_;
};

constexpr trellis::options eager{.eager = true};

}  // namespace app

int main() {
  [[maybe_unused]] auto injector = trellis::make_injector<app::eager>(
      trellis::bind<app::a>.in(trellis::singleton), trellis::bind<app::b>.in(trellis::singleton));
}
