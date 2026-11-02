// compile-fail: an unbound type requested by reference three hops below the root, through deduced, declared and aggregate classes
// expect-id: TRL-E012
// expect-regex: unbound concrete type requested by reference
// expect-regex: E012<.*sensor, trellis::error::request<const .*sensor ?&>|error C2338:
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 50
// expect-max-lines-msvc: 127
//
// The failure pass follows the failing request down, through a singleton with a declared
// constructor and a unique aggregate, to the cause, and reports only it: the classes above it
// fail only because of it (ADR-0006, one static_assert per root cause). T1.6 adds the path, which
// would name app -> service -> station -> const sensor&.
//
// The regex that starts with \A accepts only an output with exactly one error line. The regex on
// the error type also accepts MSVC's static_assert error, C2338, in its place: MSVC prints the
// literal but not the condition (ADR-0006; untested here, as MSVC runs only in CI).
#include <trellis/trellis.hpp>

#include <memory>
#include <utility>

namespace {

struct sensor {};
struct station {
  const sensor& input;
  int channel;
};
struct service {
  service(std::unique_ptr<station> line, int retries) : line_(std::move(line)), retries_(retries) {}
  std::unique_ptr<station> line_;
  int retries_;
};
struct app {
  explicit app(service& core) : core_(&core) {}
  service* core_;
};

}  // namespace

template <>
struct trellis::ctor_traits<service> : trellis::ctor<std::unique_ptr<station>, int> {};

int main() {
  auto injector = trellis::make_injector(trellis::bind<service>.in(trellis::singleton),
                                         trellis::bind<int>.to_value(3));
  [[maybe_unused]] const app built = injector.create<app>();
}
