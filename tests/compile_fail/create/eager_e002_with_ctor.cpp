// compile-fail: options{.eager = true} and a singleton whose binding's .with_ctor<Args...>() matches no constructor; make_injector reports it once
// expect-id: TRL-E002
// expect-regex: TRL-E002: type not constructible: its trellis::ctor<\.\.\.> declaration matches no constructor
// expect-regex: trellis::error::E002<app::journal, ?trellis::(v1::)?ctor<const app::config ?&, ?int> ?>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-regex-msvc: TRL-E002: type not constructible: its trellis::ctor<\.\.\.> declaration matches no constructor
// expect-regex-msvc: trellis::error::E002<T,Declared>[^\n]*\n *with\n *\[\n(?: +\w+=[^\n]*\n)*? +T=app::journal,\n +Declared=trellis::v1::ctor<const app::config &,int>(?:,|$)
// expect-regex-msvc: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
//
// Eager mode builds every singleton inside make_injector (plan §7.5), so the declaration is
// checked there, as create<T>() would check it: the journal's binding declares a constructor of
// two parameters, and the journal has one of one. Nothing requests the journal, and a second
// make_injector call with the same bindings adds no error.
//
// The regexes accept MSVC's spelling of template arguments, without a space after the comma; the
// regex that starts with \A accepts only an output with exactly one error line.
#include <trellis/trellis.hpp>

namespace app {

struct config {
  int port = 0;
};
struct journal {
  explicit journal(const config& settings) : port(settings.port) {}
  int port;
};

}  // namespace app

int main() {
  [[maybe_unused]] auto first = trellis::make_injector<trellis::options{.eager = true}>(
      trellis::bind<app::config>.to_value(app::config{}), trellis::bind<int>.to_value(1),
      trellis::bind<app::journal>.with_ctor<const app::config&, int>().in(trellis::singleton));
  [[maybe_unused]] auto second = trellis::make_injector<trellis::options{.eager = true}>(
      trellis::bind<app::config>.to_value(app::config{}), trellis::bind<int>.to_value(1),
      trellis::bind<app::journal>.with_ctor<const app::config&, int>().in(trellis::singleton));
}
