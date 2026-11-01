// compile-fail: a key bound twice, and the injector then used with create<T>(); the duplicate stays one error
// expect-id: TRL-E003
// expect-regex: duplicate binding for a key
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 30
//
// make_injector reports a repeated key as TRL-E003 (T1.4). Nothing then resolves, and nothing more
// is reported: create<T>()'s plan sees the repeat before it looks anything up (static/resolve.hpp,
// plan), so neither the requests that use the repeated key nor the others add an error.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006).
#include <trellis/trellis.hpp>

namespace {

struct timer {};
struct service {
  explicit service(timer& clock) : clock_(&clock) {}
  timer* clock_;
};

}  // namespace

int main() {
  auto injector = trellis::make_injector(trellis::bind<timer>.in(trellis::singleton),
                                         trellis::bind<timer>.in(trellis::singleton_inline));
  [[maybe_unused]] const service built = injector.create<service>();
  [[maybe_unused]] timer& clock = injector.create<timer&>();
}
