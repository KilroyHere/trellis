// compile-fail: two injectors of different types bind the same key twice in the same way; one error
// expect-id: TRL-E003
// expect-regex: TRL-E003: duplicate binding for a key
// expect-regex: trellis::error::E003<app::logger, ?trellis::(v1::)?binding<app::logger, ?app::console_logger
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
//
// make_injector reports a key bound twice before anything uses the binding map (plan §5.3 step
// 1). Both injectors repeat the same two bindings of logger, so their reports are the same:
// raise() is instantiated once per report, and the second make_injector adds no error, although
// its injector type differs (it also binds a clock). Creating from the first afterwards adds
// nothing either: after TRL-E003 nothing resolves, and nothing more is reported.
//
// The regexes accept MSVC's spelling of template arguments, without a space after the comma; the
// regex that starts with \A accepts only an output with exactly one error line.
#include <trellis/trellis.hpp>

namespace app {

struct logger {
  virtual ~logger() = default;
  virtual void write(int line) = 0;
};
struct console_logger : logger {
  void write(int /*line*/) override {}
};
struct file_logger : logger {
  void write(int /*line*/) override {}
};
struct clock {};

}  // namespace app

int main() {
  auto first = trellis::make_injector(trellis::bind<app::logger>.to<app::console_logger>(),
                                      trellis::bind<app::logger>.to<app::file_logger>());
  [[maybe_unused]] auto second = trellis::make_injector(
      trellis::bind<app::logger>.to<app::console_logger>(),
      trellis::bind<app::logger>.to<app::file_logger>(), trellis::bind<app::clock>);
  [[maybe_unused]] app::clock now = first.create<app::clock>();
}
