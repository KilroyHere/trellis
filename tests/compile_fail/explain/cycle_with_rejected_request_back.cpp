// compile-fail: a dependency cycle between two singletons, and beside it a request back to the first that the table rejects; the cycle still stops the explain pass: at most six errors, and no recursion limit
// expect-regex: depends on itself|is not usable in a constant expression|resulted in a non-constant expression|must be initialized by a constant expression
// expect-regex: app::b
// expect-regex: \A(?![\s\S]*(?:recursive template instantiation exceeded|template instantiation depth exceeds|too many errors emitted))
// expect-regex: \A(?!(?:[\s\S]*?(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: ){7})
// expect-max-lines: 217
//
// b takes a& (a real cycle: a's check recurses into b's, and b's back into a's) and
// std::unique_ptr<a>, which the request-form table rejects (TRL-E005) without recursing. The
// normal pass reports the cycle with the compiler's own errors (ADR-0014 item 2), and the explain
// pass stops at b, whose a& is a class node on the path, without reading the cycle's checks: a
// rejected request doesn't make a cycle (explain/e005_unique_ptr_back_to_path.cpp), and a real
// one beside it still does. TRL-E005 comes once the cycle is gone.
//
// The regex with {7} accepts at most six error lines; the one before it rejects a recursion limit
// and Clang's error limit.
#include <trellis/trellis.hpp>

#include <memory>
#include <utility>

namespace app {

struct b;
struct a {
  explicit a(b& other) : other_(&other) {}
  b* other_;
};
struct b {
  b(a& other, std::unique_ptr<a> owned) : other_(&other), owned_(std::move(owned)) {}
  a* other_;
  std::unique_ptr<a> owned_;
};

}  // namespace app

int main() {
  auto injector = trellis::make_injector(trellis::bind<app::a>.in(trellis::singleton),
                                         trellis::bind<app::b>.in(trellis::singleton));
  [[maybe_unused]] app::a& first = injector.create<app::a&>();
}
