// compile-fail: the same binding passed twice, once directly and once inside a module
// expect-id: TRL-E003
// expect-regex: duplicate binding for a key
// expect-regex: E003<.*widget,|error C2338:
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// Two identical bindings are still two bindings of one key. The injector stores bindings by
// position, so the repeated type itself is no problem; the repeated key is the error.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006:
// one static_assert per root cause). The regex on the error type also accepts MSVC's
// static_assert error, C2338, in its place: MSVC prints the literal but not the condition
// (ADR-0006; untested here, as MSVC runs only in CI).
#include <trellis/trellis.hpp>

namespace {

struct widget {};
struct gadget {};

}  // namespace

int main() {
  [[maybe_unused]] auto injector = trellis::make_injector(
      trellis::bind<widget>, trellis::make_module(trellis::bind<gadget>, trellis::bind<widget>));
}
