// compile-fail: a second .with_ctor<Args...>() on a binding that declares a constructor already; written twice
// expect-regex: \.with_ctor<Args\.\.\.>\(\): the binding declares a constructor already
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// A binding declares one constructor, as it names one implementation (.to<C>() twice is an error
// too). The second step returns the binding with its first declaration, so the injector builds the
// service with that one, and the misuse, written twice, is one error.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006:
// one static_assert per root cause).
#include <trellis/trellis.hpp>

namespace app {

struct config {
  int port = 0;
};
struct service {
  explicit service(const config& settings) : port(settings.port) {}
  service(const config& settings, int offset) : port(settings.port + offset) {}
  int port;
};

}  // namespace app

int main() {
  [[maybe_unused]] auto first =
      trellis::bind<app::service>.with_ctor<const app::config&>().with_ctor<const app::config&, int>();
  auto injector = trellis::make_injector(
      trellis::bind<app::config>.to_value(app::config{}),
      trellis::bind<app::service>.with_ctor<const app::config&>().with_ctor<const app::config&, int>());
  [[maybe_unused]] const app::service built = injector.create<app::service>();
}
