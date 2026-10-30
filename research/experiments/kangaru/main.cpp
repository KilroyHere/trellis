// Kangaru 4.x (gracicot/kangaru, header only, C++11; needs the CMake-generated
// kangaru/detail/config.hpp).
//
// Build modes (same file):
//   default              scenario program
//   -DDI_MISSING_BINDING IRepositoryService loses its default implementation
//                        (kgr::defaults_to<InMemoryRepositoryService>)
//   -DDI_BENCH           micro-benchmark main
//
// Kangaru is non-intrusive but explicit: every type gets a "service
// definition" struct listing its dependencies (kgr::dependency<...>).
// Deviation: Kangaru can only override (bind to an interface) *single*
// services, so the per-resolution clock is defined on its concrete type
// (unique_service<SteadyClock>); its unique_ptr<SteadyClock> converts to the
// unique_ptr<IClock> constructor parameter.
#include <kangaru/kangaru.hpp>

#include "domain.hpp"

using namespace demo;

namespace {

struct ConfigService : kgr::extern_service<Config> {};  // instance, by reference

struct CountingLoggerService;
struct ILoggerService : kgr::abstract_shared_service<ILogger>,
                        kgr::defaults_to<CountingLoggerService> {};
struct CountingLoggerService : kgr::shared_service<CountingLogger>,
                               kgr::overrides<ILoggerService> {};

struct InMemoryRepositoryService;
#ifndef DI_MISSING_BINDING
struct IRepositoryService : kgr::abstract_shared_service<IRepository>,
                            kgr::defaults_to<InMemoryRepositoryService> {};
#else
struct IRepositoryService : kgr::abstract_shared_service<IRepository> {};
#endif
struct InMemoryRepositoryService : kgr::shared_service<InMemoryRepository>,
                                   kgr::overrides<IRepositoryService> {};

struct SteadyClockService : kgr::unique_service<SteadyClock> {};

struct ServiceService
    : kgr::unique_service<Service, kgr::dependency<ILoggerService, IRepositoryService,
                                                   SteadyClockService, ConfigService>> {};

struct AppService : kgr::service<App, kgr::dependency<ServiceService, ILoggerService>> {};

}  // namespace

#if defined(DI_BENCH)
#include "bench.hpp"

int main(int argc, char** argv) {
  const bench::Options opt = bench::parse(argc, argv);
  Config config{"demo", 1000};
  {
    kgr::container container;
    container.emplace<ConfigService>(config);
    App a = container.service<AppService>();
    App b = container.service<AppService>();
    if (check_wiring(a, b, config) != 0) return 2;
  }
  bench::run("kangaru", "graph", opt, [&] {
    kgr::container container;
    container.emplace<ConfigService>(config);
    App app = container.service<AppService>();
    bench::do_not_optimize(app);
  });
  kgr::container container;
  container.emplace<ConfigService>(config);
  bench::run("kangaru", "resolve", opt, [&] {
    App app = container.service<AppService>();
    bench::do_not_optimize(app);
  });
  return 0;
}
#else
int main() {
  Config config{"demo", 100};
  kgr::container container;
  container.emplace<ConfigService>(config);
  App a = container.service<AppService>();
  App b = container.service<AppService>();
  return report("kangaru", a, b, config);
}
#endif
