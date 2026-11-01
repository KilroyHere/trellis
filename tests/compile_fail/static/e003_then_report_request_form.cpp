// compile-fail: an injector with a key bound twice, then a request checked against it
// expect-id: TRL-E003
// expect-regex: duplicate binding for a key
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// make_injector reports the repeated key. A request to that injector, through the entry point
// that create<T>() (T1.5) calls, must add nothing: the map, which can't hold a repeated key, is
// never looked up.
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
  trellis::detail::report_request_form<decltype(injector), widget&>();
}
