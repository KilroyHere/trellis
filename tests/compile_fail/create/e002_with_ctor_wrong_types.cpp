// compile-fail: a binding's .with_ctor<int>() that matches no constructor, requested in two forms and through two injectors; one error
// expect-id: TRL-E002
// expect-regex: TRL-E002: type not constructible: its trellis::ctor<\.\.\.> declaration matches no constructor
// expect-regex: trellis::error::E002<app::service, ?trellis::(v1::)?ctor<int> ?>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-regex-msvc: TRL-E002: type not constructible: its trellis::ctor<\.\.\.> declaration matches no constructor
// expect-regex-msvc: trellis::error::E002<T,Declared>[^\n]*\n *with\n *\[\n(?: +\w+=[^\n]*\n)*? +T=app::service,\n +Declared=trellis::v1::ctor<int>(?:,|$)
// expect-regex-msvc: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
//
// service's own member alias is right, but its binding's declaration wins over it (ADR-0003 item
// 1), and that one says int. create<T>() raises discovery's report for the binding's declaration,
// TRL-E002 naming it, which is unique to the class and the declaration: the second request form
// and the second injector, whose binding is a singleton with the same declaration, add none.
//
// The regexes accept MSVC's spelling of template arguments, without a space after the comma; the
// regex that starts with \A accepts only an output with exactly one error line.
#include <trellis/trellis.hpp>

#include <memory>

namespace app {

struct config {};
struct service {
  using trellis_ctor = trellis::ctor<const config&>;
  explicit service(const config& /*settings*/) {}
};

}  // namespace app

int main() {
  auto unique = trellis::make_injector(trellis::bind<app::config>.to_value(app::config{}),
                                       trellis::bind<app::service>.with_ctor<int>());
  [[maybe_unused]] const app::service first = unique.create<app::service>();
  [[maybe_unused]] const auto second = unique.create<std::unique_ptr<app::service>>();
  auto shared =
      trellis::make_injector(trellis::bind<app::config>.to_value(app::config{}),
                             trellis::bind<app::service>.with_ctor<int>().in(trellis::singleton));
  [[maybe_unused]] const app::service& third = shared.create<const app::service&>();
}
