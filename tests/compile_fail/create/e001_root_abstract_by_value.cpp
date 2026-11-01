// compile-fail: create<I>() by value, twice, for an abstract I without a binding
// expect-id: TRL-E001
// expect-regex: no binding for an abstract type: bind it to an implementation
// expect-regex: E001<.*shape, trellis::error::request<.*shape> ?>|error C2338:
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
//
// A function can't be declared to return an abstract class, so create<shape>() doesn't name
// shape as its return type; it returns void, and the request's one error is the only one
// (static/injector.hpp, create_result_t; T1.5's review). Two requests: one error.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006).
// The regex on the error type also accepts MSVC's static_assert error, C2338, in its place: MSVC
// prints the literal but not the condition (ADR-0006; untested here, as MSVC runs only in CI).
#include <trellis/trellis.hpp>

namespace {

struct shape {
  shape() = default;
  shape(const shape&) = default;
  shape& operator=(const shape&) = default;
  shape(shape&&) = default;
  shape& operator=(shape&&) = default;
  virtual ~shape() = default;
  [[nodiscard]] virtual int sides() const = 0;
};

}  // namespace

int main() {
  auto injector = trellis::make_injector();
  injector.create<shape>();
  injector.create<shape>();
}
