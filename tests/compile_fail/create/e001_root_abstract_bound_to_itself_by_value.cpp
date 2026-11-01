// compile-fail: create<I&&>(), twice, for an abstract I bound to itself with bind<I> alone
// expect-id: TRL-E001
// expect-regex: no binding for an abstract type: bind<I> alone binds it to itself
// expect-regex: E001<.*shape, trellis::error::request<.*shape ?&&> ?>|error C2338:
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
//
// I&& is the by-value request I (ADR-0003 item 3), whose result would be an abstract shape, which
// a function can't be declared to return; create<shape&&>() returns void instead, so the
// request's one error is the only one (static/injector.hpp, create_result_t; T1.5's review). Two
// requests: one error.
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
  auto injector = trellis::make_injector(trellis::bind<shape>);
  injector.create<shape&&>();
  injector.create<shape&&>();
}
