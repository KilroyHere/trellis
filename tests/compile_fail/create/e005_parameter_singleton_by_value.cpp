// compile-fail: a constructor takes a singleton by value, without .allow_copy(); copies would hide the sharing
// expect-id: TRL-E005
// expect-regex: copies hide sharing
// expect-regex: E005<.*settings, trellis::error::request<.*settings>, trellis::(v1::)?singleton_t>|error C2338:
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
//
// A deduced parameter's request comes from the conversion operator that overload resolution
// selects, which the failure pass names without enumerating the parameters (static/resolve.hpp,
// explain_arg): here `settings` by value, which the table rejects for a singleton (plan §6.3).
// Two classes need it, and each is created twice: one error.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006).
// The regex on the error type also accepts MSVC's static_assert error, C2338, in its place: MSVC
// prints the literal but not the condition (ADR-0006; untested here, as MSVC runs only in CI).
#include <trellis/trellis.hpp>

namespace {

struct settings {
  settings() = default;
  int port = 80;
};
struct server {
  explicit server(settings config) : port(config.port) {}
  int port;
};
struct client {
  client(int /*unused*/, settings config) : port(config.port) {}
  int port;
};

}  // namespace

int main() {
  auto injector = trellis::make_injector(trellis::bind<settings>.in(trellis::singleton),
                                         trellis::bind<int>.to_value(1));
  [[maybe_unused]] const server first = injector.create<server>();
  [[maybe_unused]] const server second = injector.create<server>();
  [[maybe_unused]] const client third = injector.create<client>();
  [[maybe_unused]] const client fourth = injector.create<client>();
}
