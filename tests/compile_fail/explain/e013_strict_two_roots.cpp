// compile-fail: options{.strict = true} and an unbound class taken by value two classes below create<T>(), through a declared singleton; two roots fail on it
// expect-id: TRL-E013
// expect-regex: TRL-E013: unbound concrete type requested in strict mode
// expect-regex: trellis::error::E013<app::gear, ?trellis::error::request<app::gear> ?>
// expect-regex: trellis::error::path<app::car, ?app::engine>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
//
// Strict mode turns off the implicit unique binding, so engine's declared parameter gear, which
// has no binding, can't be served (TRL-E013, provisional ID). The explain pass reaches it from
// create<car>() through engine, a declared singleton, path<car, engine>; truck reaches the same
// request and adds no error.
//
// The regexes accept MSVC's spelling of template arguments, without a space after the comma; the
// regex that starts with \A accepts only an output with exactly one error line.
#include <trellis/trellis.hpp>

namespace app {

struct gear {};
struct engine {
  using trellis_ctor = trellis::ctor<gear>;
  explicit engine(gear part) : part_(part) {}
  gear part_;
};
struct car {
  explicit car(engine& motor) : motor_(&motor) {}
  engine* motor_;
};
struct truck {
  explicit truck(engine& motor) : motor_(&motor) {}
  engine* motor_;
};

constexpr trellis::options strict{.strict = true};

}  // namespace app

int main() {
  auto injector =
      trellis::make_injector<app::strict>(trellis::bind<app::engine>.in(trellis::singleton),
                                          trellis::bind<app::car>, trellis::bind<app::truck>);
  [[maybe_unused]] const app::car first = injector.create<app::car>();
  [[maybe_unused]] const app::truck second = injector.create<app::truck>();
}
