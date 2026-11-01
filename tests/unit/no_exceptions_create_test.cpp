// injector::create<T>() without exceptions and without RTTI (NFR-11, T1.5). tests/unit/
// CMakeLists.txt builds this file with -fno-exceptions -fno-rtti, which GoogleTest can't take, so
// it is a plain program that returns 0 when every check passes. It builds singletons in both
// storages under both threading modes, lazily and eagerly, and unique objects in every owning
// form: the slots' code, whose retry after a throw is a destructor rather than a try and catch,
// and the holders and control blocks of std::shared_ptr singletons (static/storage.hpp).
#include <trellis/trellis.hpp>

#include <memory>
#include <utility>

static_assert(TRELLIS_HAS_EXCEPTIONS == 0);

namespace {

struct logger {
  logger() = default;
  int lines = 0;
};
struct repository {
  explicit repository(logger& log) : log_(&log) {}
  logger* log_;
};
struct clock_part {
  clock_part() = default;
  int ticks = 1;
};
struct service {
  service(std::shared_ptr<repository> store, logger& log, std::unique_ptr<clock_part> time)
      : store_(std::move(store)), log_(&log), time_(std::move(time)) {}
  std::shared_ptr<repository> store_;
  logger* log_;
  std::unique_ptr<clock_part> time_;
};

template <trellis::options Opts>
bool builds_the_graph() {
  auto injector = trellis::make_injector<Opts>(trellis::bind<logger>.in(trellis::singleton_inline),
                                               trellis::bind<repository>.in(trellis::singleton),
                                               trellis::bind<clock_part>);
  const service built = injector.template create<service>();
  logger& log = injector.template create<logger&>();
  const std::shared_ptr<clock_part> shared_clock =
      injector.template create<std::shared_ptr<clock_part>>();
  return built.log_ == &log && built.store_->log_ == &log && built.time_ != nullptr &&
         built.store_ == injector.template create<std::shared_ptr<repository>>() &&
         shared_clock != nullptr && &injector.template create<repository&>() == built.store_.get();
}

}  // namespace

int main() {
  const bool lazy_safe = builds_the_graph<trellis::options{}>();
  const bool lazy_none =
      builds_the_graph<trellis::options{.threading = trellis::threading::none}>();
  const bool eager = builds_the_graph<trellis::options{.eager = true}>();
  return lazy_safe && lazy_none && eager ? 0 : 1;
}
