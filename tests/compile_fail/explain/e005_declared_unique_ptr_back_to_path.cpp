// compile-fail: a singleton whose declared constructor takes std::unique_ptr of the singleton above it, a request the table rejects before any cycle; reported once, as TRL-E005
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
// expect-max-lines-msvc: 109
//
// As explain/e005_unique_ptr_back_to_path.cpp, with b's constructor declared, which the explain
// pass follows without a note of its own: the declared request std::unique_ptr<a> names a, a
// class on the path, but the request-form table rejects it first, so it is no cycle and the
// pass reports it (T1.6 review). The root is created twice.
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
  using trellis_ctor = trellis::ctor<std::unique_ptr<a>>;
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
