// compile-fail: a constructor parameter std::unique_ptr<base> of bind<base>.to<derived>(), base's destructor not virtual, one class below two roots
// expect-id: TRL-E014
// expect-regex: TRL-E014: owning request would delete the implementation through a base class whose destructor isn't virtual
// expect-regex: trellis::error::E014<app::base, ?trellis::error::request<std::unique_ptr<app::base(, ?std::default_delete<app::base> ?)?> ?>, ?app::derived>
// expect-regex: trellis::error::path<app::owner>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-regex-msvc: TRL-E014: owning request would delete the implementation through a base class whose destructor isn't virtual
// expect-regex-msvc: trellis::error::E014<app::base,trellis::error::request<R>,app::derived>[^\n]*\n *with\n *\[\n(?: +\w+=[^\n]*\n)*? +R=std::unique_ptr<app::base,std::default_delete<app::base>>(?:,|$)
// expect-regex-msvc: trellis::error::path<app::keeper>
// expect-regex-msvc: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
//
// std::unique_ptr<base> would delete the derived object through base's destructor, which isn't
// virtual (TRL-E014, provisional ID). The explain pass reports owner's parameter 1 with the path
// from create<owner>(), path<owner>; keeper takes the same request and adds no error.
//
// The regexes accept MSVC's spelling of template arguments, without a space after the comma; the
// regex that starts with \A accepts only an output with exactly one error line.
#include <trellis/trellis.hpp>

#include <memory>
#include <utility>

namespace app {

struct base {
  int value = 0;
};
struct derived : base {
  derived() = default;
};
struct owner {
  explicit owner(std::unique_ptr<base> part) : part_(std::move(part)) {}
  std::unique_ptr<base> part_;
};
struct keeper {
  explicit keeper(std::unique_ptr<base> part) : part_(std::move(part)) {}
  std::unique_ptr<base> part_;
};

}  // namespace app

int main() {
  auto injector = trellis::make_injector(trellis::bind<app::base>.to<app::derived>());
  [[maybe_unused]] const app::owner first = injector.create<app::owner>();
  [[maybe_unused]] const app::keeper second = injector.create<app::keeper>();
}
