// compile-fail: a binding's .with_ctor<int>() that matches no constructor, one class below two roots made with create<T>(); discovery's report, once
// expect-id: TRL-E002
// expect-regex: TRL-E002: type not constructible: its trellis::ctor<\.\.\.> declaration matches no constructor
// expect-regex: trellis::error::E002<app::meter, ?trellis::(v1::)?ctor<int> ?>
// expect-regex: trellis::error::path<app::panel>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-regex-msvc: TRL-E002: type not constructible: its trellis::ctor<\.\.\.> declaration matches no constructor
// expect-regex-msvc: trellis::error::E002<T,Declared>[^\n]*\n *with\n *\[\n(?: +\w+=[^\n]*\n)*? +T=app::meter,\n +Declared=trellis::v1::ctor<int>(?:,|$)
// expect-regex-msvc: trellis::error::path<app::dial>
// expect-regex-msvc: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
//
// meter's own member alias is right, but its binding declares trellis::ctor<int>, which wins, and
// meter's constructor takes a double and an int. The explain pass reaches meter from
// create<panel>() as panel's parameter 1, path<panel>, and raises the report that discovery's own
// check raises for the binding's declaration (TRL-E002 naming it), so it is one error: dial takes
// the same class, by std::shared_ptr where panel takes a reference, and adds none.
//
// The regexes accept MSVC's spelling of template arguments, without a space after the comma; the
// regex that starts with \A accepts only an output with exactly one error line.
#include <trellis/trellis.hpp>

#include <memory>
#include <utility>

namespace app {

struct meter {
  using trellis_ctor = trellis::ctor<double, int>;
  meter(double scale, int offset) : value_(scale + offset) {}
  double value_;
};
struct panel {
  explicit panel(meter& gauge) : gauge_(&gauge) {}
  meter* gauge_;
};
struct dial {
  explicit dial(std::shared_ptr<meter> gauge) : gauge_(std::move(gauge)) {}
  std::shared_ptr<meter> gauge_;
};

}  // namespace app

int main() {
  auto injector =
      trellis::make_injector(trellis::bind<double>.to_value(1.5), trellis::bind<int>.to_value(2),
                             trellis::bind<app::meter>.with_ctor<int>().in(trellis::singleton));
  [[maybe_unused]] const app::panel first = injector.create<app::panel>();
  [[maybe_unused]] const app::dial second = injector.create<app::dial>();
}
