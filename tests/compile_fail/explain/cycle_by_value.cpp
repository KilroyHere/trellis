// compile-fail: two unbound classes whose deduced constructors take each other by value, a dependency cycle; at most six errors, and no recursion limit
// expect-regex: depends on itself|is not usable in a constant expression|resulted in a non-constant expression|must be initialized by a constant expression
// expect-regex: app::b
// expect-regex: \A(?![\s\S]*(?:recursive template instantiation exceeded|template instantiation depth exceeds|too many errors emitted))
// expect-regex: \A(?!(?:[\s\S]*?(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: ){7})
// expect-max-lines: 217
//
// Each a would need a new b, and each b a new a. The normal pass reports the cycle with the
// compiler's own errors (ADR-0014 item 2: 2 to 6 errors in 20 to 217 lines), and the explain pass
// stops at b, whose constructor requests a class already on the path, and adds none.
//
// The regex with {7} accepts at most six error lines; the one before it rejects a recursion limit
// and Clang's error limit.
#include <trellis/trellis.hpp>

namespace app {

struct b;
struct a {
  explicit a(b other);
};
struct b {
  explicit b(a other);
};

}  // namespace app

int main() {
  auto injector = trellis::make_injector();
  [[maybe_unused]] const app::a first = injector.create<app::a>();
}
