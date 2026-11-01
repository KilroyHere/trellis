// compile-fail: an unbound concrete type requested as std::shared_ptr
// expect-id: TRL-E012
// expect-regex: unbound concrete type requested by reference, pointer or shared_ptr
// expect-regex: E012<.*sensor.*request<std::shared_ptr<.*sensor|error C2338:
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// ADR-0004 rejects std::shared_ptr<T> of an unbound type, although a bound unique object
// may be requested so: an unbound type is only ever built for owning requests.
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

#include <memory>

namespace {

struct sensor {};

}  // namespace

int main() {
  auto injector = trellis::make_injector();
  trellis::detail::report_request_form<decltype(injector), std::shared_ptr<sensor>>();
}
