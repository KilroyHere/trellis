// compile-fail: a constructor takes std::shared_ptr of a singleton_inline, which no shared_ptr owns
// expect-id: TRL-E005
// expect-regex: not heap-owned
// expect-regex: E005<.*bus, trellis::error::request<std::shared_ptr<.*bus> ?>, trellis::(v1::)?singleton_inline_t>|error C2338:
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
// expect-max-lines-msvc: 102
//
// singleton_inline stores the object inside the injector, so no std::shared_ptr owns it, and a
// shared_ptr request is TRL-E005 (ADR-0005 item 3, plan §6.3). The request is a deduced
// parameter one hop down; two creations report it once.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006).
// The regex on the error type also accepts MSVC's static_assert error, C2338, in its place: MSVC
// prints the literal but not the condition (ADR-0006; untested here, as MSVC runs only in CI).
#include <trellis/trellis.hpp>

#include <memory>
#include <utility>

namespace {

struct bus {};
struct station {
  explicit station(std::shared_ptr<bus> line) : line_(std::move(line)) {}
  std::shared_ptr<bus> line_;
};

}  // namespace

int main() {
  auto injector = trellis::make_injector(trellis::bind<bus>.in(trellis::singleton_inline));
  [[maybe_unused]] const station first = injector.create<station>();
  [[maybe_unused]] const station second = injector.create<station>();
}
