// compile-fail: create<const I>() by value, twice, for an abstract I bound to an implementation
// expect-id: TRL-E002
// expect-regex: a by-value request takes a copy of the bound object, or a new object of the bound class as the key
// expect-regex: E002<.*shape, trellis::error::request<const .*shape> ?>|error C2338:
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
//
// A by-value request of an abstract key can't be served, even with an implementation bound: the
// new square can't become a shape object (TRL-E002). create<const shape>() returns void rather
// than the abstract shape, which a function can't be declared to return, so the request's one
// error is the only one (static/injector.hpp, create_result_t; T1.5's review). Two requests: one
// error.
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
struct square final : shape {
  square() = default;
  [[nodiscard]] int sides() const override { return 4; }
};

}  // namespace

int main() {
  auto injector = trellis::make_injector(trellis::bind<shape>.to<square>());
  injector.create<const shape>();
  injector.create<const shape>();
}
