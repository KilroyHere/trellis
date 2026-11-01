// compile-fail: an unbound concrete type requested by reference, which nothing would own
// expect-id: TRL-E012
// expect-regex: unbound concrete type requested by reference
// expect-regex: E012<.*sensor.*request<const .*sensor ?&>|error C2338:
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// An unbound concrete type is implicitly unique (ADR-0004), so a const T& of it would refer
// to an object that nothing keeps alive.
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
  auto injector = trellis::make_injector();
  trellis::detail::report_request_form<decltype(injector), const sensor&>();
}
