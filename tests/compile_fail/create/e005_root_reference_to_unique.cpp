// compile-fail: create<T&>() of a unique binding, twice; nothing would keep the object alive
// expect-id: TRL-E005
// expect-regex: nothing would keep the object alive
// expect-regex: E005<.*sensor, trellis::error::request<.*sensor ?&>, trellis::(v1::)?unique_t>|error C2338:
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 30
// expect-max-lines-msvc: 37
//
// The request-form table (plan §6.3) rejects T& of a unique binding, also when the caller of
// create<T>() makes the request: create<T>() reports TRL-E005 from the table's entry point
// (static/validate.hpp, report_request_form), once however often it is called.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006).
// The regex on the error type also accepts MSVC's static_assert error, C2338, in its place: MSVC
// prints the literal but not the condition (ADR-0006; untested here, as MSVC runs only in CI).
#include <trellis/trellis.hpp>

namespace {

struct sensor {};

}  // namespace

int main() {
  auto injector = trellis::make_injector(trellis::bind<sensor>);
  [[maybe_unused]] sensor& first = injector.create<sensor&>();
  [[maybe_unused]] sensor& second = injector.create<sensor&>();
}
