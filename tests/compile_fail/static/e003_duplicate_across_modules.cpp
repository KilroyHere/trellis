// compile-fail: two modules bind the same key
// expect-id: TRL-E003
// expect-regex: duplicate binding for a key
// expect-regex: E003<.*logger, .*console_logger.*file_logger|error C2338:
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// make_injector flattens its modules into one binding map, where each key may appear once
// (plan §5.3, step 1). The error names the key and both bindings.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006:
// one static_assert per root cause). The regex on the error type also accepts MSVC's
// static_assert error, C2338, in its place: MSVC prints the literal but not the condition
// (ADR-0006; untested here, as MSVC runs only in CI).
#include <trellis/trellis.hpp>

namespace {

struct logger {
  logger() = default;
  logger(const logger&) = delete;
  logger& operator=(const logger&) = delete;
  virtual ~logger() = default;
  virtual void write() = 0;
};
struct console_logger final : logger {
  void write() override {}
};
struct file_logger final : logger {
  void write() override {}
};

auto console_module() {
  return trellis::make_module(trellis::bind<logger>.to<console_logger>().in(trellis::singleton));
}
auto file_module() { return trellis::make_module(trellis::bind<logger>.to<file_logger>()); }

}  // namespace

int main() {
  [[maybe_unused]] auto injector = trellis::make_injector(console_module(), file_module());
}
