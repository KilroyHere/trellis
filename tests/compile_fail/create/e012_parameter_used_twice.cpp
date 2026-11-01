// compile-fail: two classes take an unbound type by reference, and each is created twice; the request is reported once
// expect-id: TRL-E012
// expect-regex: unbound concrete type requested by reference
// expect-regex: E012<.*sensor, trellis::error::request<.*sensor ?&>|error C2338:
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
// expect-max-lines-msvc: 102
//
// The failure pass is instantiated once per request (static/resolve.hpp, explain), so a cause
// that several requests lead to is one error: here two consumers, each created twice, need the
// same sensor& (ADR-0006: one static_assert per root cause). Its assertion is in a function body,
// which Clang doesn't mark invalid, so the later uses add nothing.
//
// The regex that starts with \A accepts only an output with exactly one error line. The regex on
// the error type also accepts MSVC's static_assert error, C2338, in its place: MSVC prints the
// literal but not the condition (ADR-0006; untested here, as MSVC runs only in CI).
#include <trellis/trellis.hpp>

namespace {

struct sensor {};
struct reader {
  explicit reader(sensor& input) : input_(&input) {}
  sensor* input_;
};
struct writer {
  explicit writer(sensor& output) : output_(&output) {}
  sensor* output_;
};

}  // namespace

int main() {
  auto injector = trellis::make_injector();
  [[maybe_unused]] reader first = injector.create<reader>();
  [[maybe_unused]] reader second = injector.create<reader>();
  [[maybe_unused]] writer third = injector.create<writer>();
  [[maybe_unused]] writer fourth = injector.create<writer>();
}
