// compile-fail: a class whose only constructor is private, needed by two classes that are created; discovery reports it once
// expect-id: TRL-E002
// expect-regex: deduction found no public, non-deleted, unambiguous constructor
// expect-regex: E002<.*vault, trellis::(v1::)?detail::no_ctor_declared>|error C2338:
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
//
// create<T>() leaves a class whose constructor can't be found to discovery's own assertion
// (discovery/deduced.hpp, discover), which reports once per class and bound however often the
// class is needed. Two consumers need a vault: one error.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006).
// The regex on the error type also accepts MSVC's static_assert error, C2338, in its place: MSVC
// prints the literal but not the condition (ADR-0006; untested here, as MSVC runs only in CI).
#include <trellis/trellis.hpp>

#include <memory>
#include <utility>

namespace {

class vault {
  vault() = default;
};
struct teller {
  explicit teller(std::unique_ptr<vault> safe) : safe_(std::move(safe)) {}
  std::unique_ptr<vault> safe_;
};
struct banker {
  explicit banker(std::shared_ptr<vault> safe) : safe_(std::move(safe)) {}
  std::shared_ptr<vault> safe_;
};

}  // namespace

int main() {
  auto injector = trellis::make_injector(trellis::bind<vault>);
  [[maybe_unused]] const teller first = injector.create<teller>();
  [[maybe_unused]] const banker second = injector.create<banker>();
}
