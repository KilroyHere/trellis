// compile-fail: a singleton requested by value, which would copy the shared object
// expect-id: TRL-E005
// expect-regex: copies hide sharing
// expect-regex: allow_copy
// expect-regex: E005<.*sensor.*request<.*sensor>.*singleton_t>|error C2338:
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// The request-form table (plan §6.3) rejects T of a singleton: a copy would hide the sharing,
// unless the binding opts in with .allow_copy().
//
// There is no create<T>() yet (T1.5), so the test drives the check through its entry point,
// trellis::detail::report_request_form, which create<T>()'s failure branch calls.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006:
// one static_assert per root cause). The regex on the error type also accepts MSVC's
// static_assert error, C2338, in its place: MSVC prints the literal but not the condition
// (ADR-0006; untested here, as MSVC runs only in CI).
#include <trellis/static/validate.hpp>
#include <trellis/trellis.hpp>

namespace {

struct sensor {};

}  // namespace

int main() {
  auto injector = trellis::make_injector(trellis::bind<sensor>.in(trellis::singleton));
  trellis::detail::report_request_form<decltype(injector), sensor>();
}
