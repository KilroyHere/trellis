// compile-fail: two aggregates hold, by value, an aggregate that deduction refuses
// expect-id: TRL-E004
// expect-regex: TRL-E004: ambiguous constructor: deduction can't initialize every member
// expect-regex: trellis::error::E004<app::settings>
// expect-regex: trellis::error::path<app::(service|monitor)>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-regex-msvc: TRL-E004: ambiguous constructor: deduction can't initialize every member
// expect-regex-msvc: trellis::error::E004<T>[^\n]*\n *with\n *\[\n(?: +\w+=[^\n]*\n)*? +T=app::settings(?:,|$)
// expect-regex-msvc: trellis::error::path<app::(service|monitor)>
// expect-regex-msvc: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
//
// Deduction refuses settings: std::optional ends its deduced members (TRL-E004). service and
// monitor each hold a settings by value. Their nodes are checked with parentheses, which never
// elide braces (static/resolve.hpp; config.hpp, TRELLIS_AGGREGATE_CHECKS_WITH_BRACES), so the
// failing request for settings fails them on every compiler. With braces, Clang took the deleted
// twin that the request selects for no conversion, elided braces into settings and built it from
// its members, value-initializing retries without a word, where GCC reported the error. The
// explain pass reaches settings from a root and raises discovery's report, which is unique to the
// class, so the other root adds no error.
//
// The regex that starts with \A accepts only an output with exactly one error line.
#include <trellis/trellis.hpp>

#include <optional>

namespace app {

struct endpoint {};
struct settings {
  endpoint host;
  std::optional<int> retries;
};
struct service {
  settings config;
};
struct monitor {
  settings config;
};

}  // namespace app

int main() {
  auto injector = trellis::make_injector();
  [[maybe_unused]] const app::service first = injector.create<app::service>();
  [[maybe_unused]] const app::monitor second = injector.create<app::monitor>();
}
