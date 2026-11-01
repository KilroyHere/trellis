// A reference member that a binding's .with_ctor<int>() declaration binds to a temporary: the int
// request is a new value, and the aggregate, allocated for a std::unique_ptr, keeps a reference to
// it after the full-expression that builds the aggregate. Clang reports it (-Wdangling-field, on
// by default) at Trellis's creation code, static/declared_aggregate.hpp, which switches off only
// -Wmissing-field-initializers there (TRELLIS_PARTIAL_AGGREGATE_INIT_BEGIN, config.hpp), so the
// warning must still reach the user. tests/warnings/CMakeLists.txt compiles this file with Clang
// and the project warnings and expects that warning, located in that header.
#include <trellis/trellis.hpp>

#include <memory>

namespace app {

struct holder {
  const int& ref;
};

}  // namespace app

int main() {
  auto injector = trellis::make_injector(trellis::bind<int>.to_value(7),
                                         trellis::bind<app::holder>.with_ctor<int>());
  [[maybe_unused]] const auto built = injector.create<std::unique_ptr<app::holder>>();
}
