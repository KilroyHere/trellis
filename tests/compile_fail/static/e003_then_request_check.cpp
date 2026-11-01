// compile-fail: an injector with a key bound twice, then a verdict read from it
// expect-id: TRL-E003
// expect-regex: duplicate binding for a key
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// The verdict for an injector whose keys repeat comes without a map lookup, for a bound key and
// for an unbound one alike, so TRL-E003 stays the only error.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006:
// one static_assert per root cause).
#include <trellis/static/validate.hpp>
#include <trellis/trellis.hpp>

namespace {

struct widget {};
struct gadget {};

}  // namespace

int main() {
  auto injector =
      trellis::make_injector(trellis::bind<widget>, trellis::bind<widget>.in(trellis::singleton));
  constexpr bool widget_allowed =
      trellis::detail::request_check<decltype(injector), widget&>::allowed;
  constexpr bool gadget_allowed =
      trellis::detail::request_check<decltype(injector), gadget>::allowed;
  return widget_allowed || gadget_allowed ? 1 : 0;
}
