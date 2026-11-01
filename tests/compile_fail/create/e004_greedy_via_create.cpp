// compile-fail: a class with a constructor template that accepts anything, needed one hop down, created twice
// expect-id: TRL-E004
// expect-regex: ambiguous constructor
// expect-regex: E004<.*greedy>|error C2338:
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
//
// Deduction can't tell what a constructor template that accepts any argument needs (ADR-0003),
// so discovery reports TRL-E004, from create<T>() as from its own entry point, once. (Taken by
// value, such a class would make its consumer's own constructor ambiguous, TRL-E002, as a
// std::optional parameter does; through std::unique_ptr, it is the class itself that fails.)
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006).
// The regex on the error type also accepts MSVC's static_assert error, C2338, in its place: MSVC
// prints the literal but not the condition (ADR-0006; untested here, as MSVC runs only in CI).
#include <trellis/trellis.hpp>

#include <memory>
#include <utility>

namespace {

struct greedy {
  template <class U>
  explicit greedy(U&& /*anything*/) {}
};
struct consumer {
  explicit consumer(std::unique_ptr<greedy> part) : part_(std::move(part)) {}
  std::unique_ptr<greedy> part_;
};

}  // namespace

int main() {
  auto injector = trellis::make_injector();
  [[maybe_unused]] const consumer first = injector.create<consumer>();
  [[maybe_unused]] const consumer second = injector.create<consumer>();
}
