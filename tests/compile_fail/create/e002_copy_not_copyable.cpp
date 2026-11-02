// compile-fail: .allow_copy() on a singleton whose class can't be copied, requested by value
// expect-id: TRL-E002
// expect-regex: a by-value request takes a copy of the bound object
// expect-regex: E002<.*pool, trellis::error::request<.*pool> ?>|error C2338:
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
// expect-fail-msvc: CWG 2327: cl can't initialize a by-value parameter of a non-movable class from a conversion function's prvalue, so deduction of the consumer fails
//
// .allow_copy() lets a consumer take a copy of a shared object (plan §6.3), so the key must be
// constructible from the object. pool can't be copied: create<T>() reports TRL-E002, type not
// constructible, rather than the copy constructor's error deep in the creation code. Two
// consumers need the copy: one error.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006).
// The regex on the error type also accepts MSVC's static_assert error, C2338, in its place: MSVC
// prints the literal but not the condition (ADR-0006; untested here, as MSVC runs only in CI).
#include <trellis/trellis.hpp>

namespace {

struct pool {
  pool() = default;
  pool(const pool&) = delete;
  pool& operator=(const pool&) = delete;
  ~pool() = default;
  int size = 4;
};
struct worker {
  explicit worker(pool threads) : size(threads.size) {}
  int size;
};
struct runner {
  explicit runner(pool threads) : size(threads.size) {}
  int size;
};

}  // namespace

int main() {
  auto injector = trellis::make_injector(trellis::bind<pool>.in(trellis::singleton).allow_copy());
  [[maybe_unused]] const worker first = injector.create<worker>();
  [[maybe_unused]] const runner second = injector.create<runner>();
}
