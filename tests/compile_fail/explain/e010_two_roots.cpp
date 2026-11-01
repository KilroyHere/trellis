// compile-fail: options{.max_ctor_arity = 2} and a class whose constructor takes 3 parameters, one class below create<T>(); two roots need it
// expect-id: TRL-E010
// expect-regex: TRL-E010: constructor arity above max_ctor_arity
// expect-regex: trellis::error::E010<app::mixer, ?2(UL)?>
// expect-regex: trellis::error::path<app::studio>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
//
// Deduction finds mixer's constructor only above the injector's bound (TRL-E010). The explain pass
// reaches it from create<studio>(), path<studio>, and raises discovery's report, which is unique
// to the class and the bound, so stage, which needs a mixer too, adds no error.
//
// The regexes accept MSVC's spelling of template arguments, without a space after the comma; the
// regex that starts with \A accepts only an output with exactly one error line.
#include <trellis/trellis.hpp>

namespace app {

struct channel {};
struct mixer {
  mixer(channel /*left*/, channel /*right*/, channel /*center*/) {}
};
struct studio {
  explicit studio(mixer desk) : desk_(desk) {}
  mixer desk_;
};
struct stage {
  explicit stage(mixer desk) : desk_(desk) {}
  mixer desk_;
};

}  // namespace app

int main() {
  auto injector = trellis::make_injector<trellis::options{.max_ctor_arity = 2}>();
  [[maybe_unused]] const app::studio first = injector.create<app::studio>();
  [[maybe_unused]] const app::stage second = injector.create<app::stage>();
}
