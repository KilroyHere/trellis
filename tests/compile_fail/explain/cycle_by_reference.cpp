// compile-fail: two singletons whose deduced constructors take each other by reference, a dependency cycle; at most six errors, and no recursion limit
// expect-regex: depends on itself|is not usable in a constant expression|resulted in a non-constant expression|must be initialized by a constant expression
// expect-regex: app::b
// expect-regex: \A(?![\s\S]*(?:recursive template instantiation exceeded|template instantiation depth exceeds|too many errors emitted))
// expect-regex: \A(?!(?:[\s\S]*?(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: ){7})
// expect-max-lines: 217
//
// In deduced mode the normal pass reports a cycle with the compiler's own errors: the check of
// a& re-enters itself (ADR-0014 item 2, which allows 2 to 6 errors in 20 to 217 lines and lists
// these signatures for TRL-E007's page). The explain pass then stops at b, whose constructor
// requests a class already on the path from the root, without reading the cycle's checks again.
// T1.6's first explain pass followed the cycle round and round: Clang gave 20 errors and "too
// many errors emitted".
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

}  // namespace app

int main() {
  auto injector = trellis::make_injector(trellis::bind<app::a>.in(trellis::singleton),
                                         trellis::bind<app::b>.in(trellis::singleton));
  [[maybe_unused]] app::a& first = injector.create<app::a&>();
}
