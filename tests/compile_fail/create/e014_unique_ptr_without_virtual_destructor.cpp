// compile-fail: bind<base>.to<derived>(), where base's destructor isn't virtual, requested as std::unique_ptr<base> at the root and by a constructor parameter
// expect-id: TRL-E014
// expect-regex: owning request would delete the implementation through a base class whose destructor isn't virtual
// expect-regex: E014<.*base, trellis::error::request<std::unique_ptr<.*base.*> ?>, .*derived ?>|error C2338:
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
//
// A std::unique_ptr<base> holding a new derived would delete it through base, whose destructor
// isn't virtual: undefined behaviour ([expr.delete]/3), which no compiler warns about here. So the
// request is TRL-E014 (provisional ID, T1.5's review), at the root and as a parameter alike; the
// same request in both places is one error. std::shared_ptr<base>, a singleton and a by-value
// request stay allowed (tests/unit/create_test.cpp, "NonVirtualBase").
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006).
// The regex on the error type also accepts MSVC's static_assert error, C2338, in its place: MSVC
// prints the literal but not the condition (ADR-0006; untested here, as MSVC runs only in CI).
#include <trellis/trellis.hpp>

#include <memory>
#include <utility>

namespace {

struct base {
  base() = default;
  int id = 1;
};
struct derived : base {
  derived() = default;
  int payload = 2;
};
struct owner {
  explicit owner(std::unique_ptr<base> owned) : owned_(std::move(owned)) {}
  std::unique_ptr<base> owned_;
};

}  // namespace

int main() {
  auto injector = trellis::make_injector(trellis::bind<base>.to<derived>());
  [[maybe_unused]] const std::unique_ptr<base> direct = injector.create<std::unique_ptr<base>>();
  [[maybe_unused]] const owner holder = injector.create<owner>();
}
