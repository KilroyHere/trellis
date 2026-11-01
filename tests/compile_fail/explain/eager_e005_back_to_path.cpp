// compile-fail: options{.eager = true} and a singleton whose deduced constructor takes std::unique_ptr of the singleton above it; make_injector reports it once, as TRL-E005
// expect-id: TRL-E005
// expect-regex: TRL-E005: request form incompatible with the lifetime: std::unique_ptr would duplicate ownership of a shared object
// expect-regex: trellis::error::E005<app::a, ?trellis::error::request<std::unique_ptr<app::a(, ?std::default_delete<app::a> ?)?> ?>, ?trellis::(v1::)?singleton_t ?>
// expect-regex: trellis::error::path<app::a, ?app::b>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-regex-msvc: TRL-E005: request form incompatible with the lifetime: std::unique_ptr would duplicate ownership of a shared object
// expect-regex-msvc: trellis::error::E005<app::a,trellis::error::request<R>,trellis::v1::singleton_t>[^\n]*\n *with\n *\[\n(?: +\w+=[^\n]*\n)*? +R=std::unique_ptr<app::a,std::default_delete<app::a>>(?:,|$)
// expect-regex-msvc: trellis::error::path<app::a, ?app::b>
// expect-regex-msvc: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
// expect-max-lines-msvc: 180
//
// As explain/e005_unique_ptr_back_to_path.cpp, found by make_injector, which runs the explain
// pass for the first singleton it can't build: the rejected request back to a is no cycle, and
// the pass reports it (T1.6 review). The injector is made twice.
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

constexpr trellis::options eager{.eager = true};

}  // namespace app

int main() {
  [[maybe_unused]] auto first = trellis::make_injector<app::eager>(
      trellis::bind<app::a>.in(trellis::singleton), trellis::bind<app::b>.in(trellis::singleton));
  [[maybe_unused]] auto second = trellis::make_injector<app::eager>(
      trellis::bind<app::a>.in(trellis::singleton), trellis::bind<app::b>.in(trellis::singleton));
}
