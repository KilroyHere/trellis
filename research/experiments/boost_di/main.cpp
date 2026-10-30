// Boost.DI (boost-ext/di, header only, C++14).
//
// Build modes (same file):
//   default              scenario program
//   -DDI_MISSING_BINDING the IRepository -> InMemoryRepository binding is removed
//   -DDI_BENCH           micro-benchmark main
//   -DDI_BOOST_SHARED_EXT use the per-injector `extension::shared` scope instead
//                        of the core `di::singleton` scope (see note below)
//
// Note: in Boost.DI 1.3.x the core `di::singleton` scope keeps its object in a
// function-local static, i.e. one instance per process, not per injector. The
// optional extension scope `di::extension::shared` stores the object inside
// the injector, which matches "one instance per container" in the other
// programs. The scenario uses the core scope (the documented default).
#include <boost/di.hpp>
#if defined(DI_BOOST_SHARED_EXT)
#include <boost/di/extension/scopes/shared.hpp>
#endif

#include "domain.hpp"

namespace di = boost::di;
using namespace demo;

namespace {

#if defined(DI_BOOST_SHARED_EXT)
constexpr auto& kShared = di::extension::shared;
#else
constexpr auto& kShared = di::singleton;
#endif

auto make_injector(Config& config) {
  return di::make_injector(
      di::bind<Config>.to(config),  // instance, bound by reference
      di::bind<ILogger>.to<CountingLogger>().in(kShared),
#ifndef DI_MISSING_BINDING
      di::bind<IRepository>.to<InMemoryRepository>().in(kShared),
#endif
      di::bind<IClock>.to<SteadyClock>().in(di::unique));
}

}  // namespace

#if defined(DI_BENCH)
#include "bench.hpp"

int main(int argc, char** argv) {
  const bench::Options opt = bench::parse(argc, argv);
  Config config{"demo", 1000};
#if defined(DI_BOOST_SHARED_EXT)
  const char* name = "boost_di_shared_ext";
#else
  const char* name = "boost_di";
#endif
  {
    auto injector = make_injector(config);
    App a = injector.create<App>();
    App b = injector.create<App>();
    if (check_wiring(a, b, config) != 0) return 2;
  }
  bench::run(name, "graph", opt, [&] {
    auto injector = make_injector(config);
    App app = injector.create<App>();
    bench::do_not_optimize(app);
  });
  auto injector = make_injector(config);
  bench::run(name, "resolve", opt, [&] {
    App app = injector.create<App>();
    bench::do_not_optimize(app);
  });
  return 0;
}
#else
int main() {
  Config config{"demo", 100};
  auto injector = make_injector(config);
  App a = injector.create<App>();
  App b = injector.create<App>();
  return report("boost_di", a, b, config);
}
#endif
