// compile-fail: a unique object requested by reference, which nothing would keep alive
// expect-id: TRL-E005
// expect-regex: nothing would keep the object alive
// expect-regex: E005<.*sensor.*request<.*sensor ?&>.*unique_t>|error C2338:
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// The request-form table (plan §6.3) rejects T& of a unique binding: the consumer would hold
// a reference to an object that nothing keeps alive.
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
  auto injector = trellis::make_injector(trellis::bind<sensor>);
  trellis::detail::report_request_form<decltype(injector), sensor&>();
}
