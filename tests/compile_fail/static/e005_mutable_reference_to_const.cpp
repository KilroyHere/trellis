// compile-fail: a const external object requested as a mutable reference
// expect-id: TRL-E005
// expect-regex: the bound object is const
// expect-regex: E005<.*sensor.*request<.*sensor ?&>.*external_t>|error C2338:
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// Constness carries through (plan §6.3): a const object bound with .to_ref() can be
// requested as const T& or const T*, not as T&.
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

const sensor the_sensor{};

}  // namespace

int main() {
  auto injector = trellis::make_injector(trellis::bind<sensor>.to_ref(the_sensor));
  trellis::detail::report_request_form<decltype(injector), sensor&>();
}
