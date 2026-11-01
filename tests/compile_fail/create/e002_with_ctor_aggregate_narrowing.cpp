// compile-fail: a binding's .with_ctor<double, double>() for an aggregate whose first member is an int, which braces can't narrow; requested twice
// expect-id: TRL-E002
// expect-regex: TRL-E002: type not constructible: its trellis::ctor<\.\.\.> declaration doesn't brace-initialize the aggregate
// expect-regex: trellis::error::E002<app::endpoint, ?trellis::(v1::)?ctor<double, ?double> ?>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
//
// An aggregate is checked, and would be built, with braces, as discovery builds one (plan §7.2's
// rule as amended, ADR-0003): a double for the int member narrows, which braces reject, though
// parentheses would accept it in C++20. The binding's declaration is checked as a type's own
// declaration is, once for the class and the declaration: the second request adds no error.
//
// The regexes accept MSVC's spelling of template arguments, without a space after the comma; the
// regex that starts with \A accepts only an output with exactly one error line.
#include <trellis/trellis.hpp>

#include <memory>

namespace app {

struct endpoint {
  int port;
  double timeout;
};

}  // namespace app

int main() {
  auto injector = trellis::make_injector(trellis::bind<app::endpoint>.with_ctor<double, double>());
  [[maybe_unused]] const app::endpoint first = injector.create<app::endpoint>();
  [[maybe_unused]] const auto second = injector.create<std::shared_ptr<app::endpoint>>();
}
