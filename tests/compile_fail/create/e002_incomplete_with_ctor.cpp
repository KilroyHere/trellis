// compile-fail: a binding's .with_ctor<Args...>() for a class that is only declared, requested twice; discovery reports the incomplete class once
// expect-id: TRL-E002
// expect-regex: TRL-E002: type not constructible: it is incomplete here
// expect-regex: trellis::error::E002<app::journal, ?trellis::(v1::)?detail::no_ctor_declared ?>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
//
// journal is only declared where the injector would build it, so its binding's declaration can't
// be checked, and the class can't be built. It is the report that deduction gives an incomplete
// class (discovery/deduced.hpp, discovery_report), naming no declaration, so that one class is one
// report however its constructor is declared: the second request form adds no error.
//
// The regexes accept MSVC's spelling of template arguments, without a space after the comma; the
// regex that starts with \A accepts only an output with exactly one error line.
#include <trellis/trellis.hpp>

#include <memory>

namespace app {

struct journal;

}  // namespace app

int main() {
  auto injector = trellis::make_injector(trellis::bind<int>.to_value(1),
                                         trellis::bind<app::journal>.with_ctor<int>());
  injector.create<std::unique_ptr<app::journal>>();
  [[maybe_unused]] const auto second = injector.create<std::shared_ptr<app::journal>>();
}
