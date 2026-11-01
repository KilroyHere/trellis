// compile-fail: a reference to a std::shared_ptr, which names the smart pointer as its key
// expect-id: TRL-E012
// expect-regex: a reference or pointer to a smart pointer asks for the smart pointer itself
// expect-regex: take it by value, such as std::shared_ptr<T>
// expect-regex: E012<std::shared_ptr<.*sensor.*request<const std::shared_ptr<|error C2338:
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// sensor is bound, but const std::shared_ptr<sensor>& asks for an object of type
// std::shared_ptr<sensor>, which nothing binds (ADR-0004's open question; T1.4 keeps rejecting
// it). The message points to taking the shared_ptr by value, not to binding the smart pointer type,
// which would give an empty pointer.
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
  auto injector = trellis::make_injector(trellis::bind<sensor>.in(trellis::singleton));
  trellis::detail::report_request_form<decltype(injector), const std::shared_ptr<sensor>&>();
}
