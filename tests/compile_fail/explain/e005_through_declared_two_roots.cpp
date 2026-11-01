// compile-fail: a declared constructor two classes below create<T>() takes a unique object by reference, which nothing would keep alive; two roots fail on it
// expect-id: TRL-E005
// expect-regex: TRL-E005: request form incompatible with the lifetime: nothing would keep the object alive
// expect-regex: trellis::error::E005<app::timer, ?trellis::error::request<app::timer ?&>, ?trellis::(v1::)?unique_t>
// expect-regex: trellis::error::path<app::dashboard, ?app::scheduler>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
//
// scheduler declares a constructor that takes timer&, and timer is bound as unique: each request
// would get a new object that nothing keeps alive (plan §6.3). The explain pass reports the
// request with the path from create<dashboard>(), path<dashboard, scheduler>, and its 1-based
// parameter index; alarm reaches the same request through scheduler and adds no error.
//
// The regexes accept MSVC's spelling of template arguments, without a space after the comma; the
// regex that starts with \A accepts only an output with exactly one error line.
#include <trellis/trellis.hpp>

namespace app {

struct timer {};
struct scheduler {
  using trellis_ctor = trellis::ctor<int, timer&>;
  scheduler(int slots, timer& clock) : slots_(slots), clock_(&clock) {}
  int slots_;
  timer* clock_;
};
struct dashboard {
  explicit dashboard(scheduler& jobs) : jobs_(&jobs) {}
  scheduler* jobs_;
};
struct alarm {
  explicit alarm(scheduler& jobs) : jobs_(&jobs) {}
  scheduler* jobs_;
};

}  // namespace app

int main() {
  auto injector = trellis::make_injector(trellis::bind<app::timer>,
                                         trellis::bind<app::scheduler>.in(trellis::singleton),
                                         trellis::bind<int>.to_value(4));
  [[maybe_unused]] const app::dashboard first = injector.create<app::dashboard>();
  [[maybe_unused]] const app::alarm second = injector.create<app::alarm>();
}
