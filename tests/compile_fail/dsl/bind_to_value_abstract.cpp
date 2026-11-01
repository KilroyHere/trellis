// compile-fail: .to_value() of an object reached through a reference to an abstract key
// expect-regex: bind<I>\.to_value\(v\): the binding can't store a copy of v
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// The binding stores a copy with the argument's type, logger here, which is abstract: the
// object's own type is lost behind the reference.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006:
// one static_assert per root cause).
#include <trellis/trellis.hpp>

namespace {

struct logger {
  logger() = default;
  logger(const logger&) = default;
  logger& operator=(const logger&) = default;
  virtual ~logger() = default;
  virtual void write() = 0;
};
struct console_logger final : logger {
  void write() override {}
};

}  // namespace

int main() {
  console_logger console;
  logger& base = console;
  [[maybe_unused]] auto binding = trellis::bind<logger>.to_value(base);
}
