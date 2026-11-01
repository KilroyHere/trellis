// compile-fail: a singleton whose deduced constructor takes std::unique_ptr of the singleton above it, a request the table rejects before any cycle; reported once, as TRL-E005
// expect-id: TRL-E005
// expect-regex: TRL-E005: request form incompatible with the lifetime: std::unique_ptr would duplicate ownership of a shared object
// expect-regex: trellis::error::E005<app::a, ?trellis::error::request<std::unique_ptr<app::a(, ?std::default_delete<app::a> ?)?> ?>, ?trellis::(v1::)?singleton_t ?>
// expect-regex: trellis::error::path<app::a, ?app::b>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
//
// b's parameter names a, a class already on the path from create<a&>(), but its request,
// std::unique_ptr<a> of a singleton, is rejected by the request-form table before a's check
// could recurse: there is no cycle for the compiler to report, and the explain pass reports the
// request (T1.6 review). T1.6's first cycle stop counted any request of a class on the path, so
// this graph compiled without an error and aborted at run time. The root is created twice.
//
// The regexes accept MSVC's spelling of template arguments, without a space after the comma; the
// regex that starts with \A accepts only an output with exactly one error line.
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
  explicit b(std::unique_ptr<a> owned) : owned_(std::move(owned)) {}
  std::unique_ptr<a> owned_;
};

}  // namespace app

int main() {
  auto injector = trellis::make_injector(trellis::bind<app::a>.in(trellis::singleton),
                                         trellis::bind<app::b>.in(trellis::singleton));
  [[maybe_unused]] app::a& first = injector.create<app::a&>();
  [[maybe_unused]] app::a& second = injector.create<app::a&>();
}
