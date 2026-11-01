// compile-fail: one missing binding requested in three forms, sensor&, const sensor& and sensor*, by the classes below one root; one error
// expect-id: TRL-E012
// expect-regex: TRL-E012: unbound concrete type requested by reference, pointer or shared_ptr: nothing would own it
// expect-regex: trellis::error::E012<app::sensor, ?trellis::error::request<app::sensor ?&> ?>
// expect-regex: trellis::error::path<app::station, ?app::reader>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-regex-msvc: TRL-E012: unbound concrete type requested by reference, pointer or shared_ptr: nothing would own it
// expect-regex-msvc: trellis::error::E012<app::sensor,trellis::error::request<R>>[^\n]*\n *with\n *\[\n(?: +\w+=[^\n]*\n)*? +R=app::sensor &(?:,|$)
// expect-regex-msvc: trellis::error::path<app::station, ?app::reader>
// expect-regex-msvc: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
// expect-max-lines-msvc: 127
//
// sensor has no binding, so reader's sensor&, logger's const sensor& and probe's sensor* are three
// requests that nothing would own (TRL-E012), with one root cause. The explain pass follows only
// the first failing parameter of each class (plan §7.8), station's first, reader, and reader's
// sensor&, so the other two forms are never reached: one error, with the path from
// create<station>(), path<station, reader>. The root is created twice.
//
// The regexes accept MSVC's spelling of template arguments, without a space after the comma; the
// regex that starts with \A accepts only an output with exactly one error line.
#include <trellis/trellis.hpp>

namespace app {

struct sensor {};
struct reader {
  explicit reader(sensor& input) : input_(&input) {}
  sensor* input_;
};
struct logger {
  explicit logger(const sensor& input) : input_(&input) {}
  const sensor* input_;
};
struct probe {
  explicit probe(sensor* input) : input_(input) {}
  sensor* input_;
};
struct station {
  station(reader first, logger second, probe third)
      : first_(first), second_(second), third_(third) {}
  reader first_;
  logger second_;
  probe third_;
};

}  // namespace app

int main() {
  auto injector = trellis::make_injector();
  [[maybe_unused]] const app::station first = injector.create<app::station>();
  [[maybe_unused]] const app::station second = injector.create<app::station>();
}
