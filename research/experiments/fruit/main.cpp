// Google Fruit (compiled library + headers, C++11).
//
// Build modes (same file):
//   default              scenario program
//   -DDI_MISSING_BINDING the IRepository -> InMemoryRepository binding is removed
//   -DDI_BENCH           micro-benchmark main
//
// Deviations forced by Fruit's model (documented in the README):
//  * Every object Fruit constructs is owned by the injector (one per injector).
//    Per-resolution objects are expressed as factories,
//    std::function<std::unique_ptr<T>()>, which is how Fruit spells "unique".
//  * Fruit cannot inject std::unique_ptr<T> constructor parameters, so Service
//    and App (whose constructors take unique_ptr) are registered with
//    registerFactory() and a small lambda that calls the factory it receives.
//  * The domain classes stay macro-free (no INJECT), so the leaf types use
//    registerConstructor<>() instead.
//  * shared_ptr<ILogger> handed out by Fruit does not own the object; the
//    injector must outlive the App objects.
#include <fruit/fruit.h>

#include <functional>

#include "domain.hpp"

using namespace demo;

namespace {

using ClockFactory = std::function<std::unique_ptr<IClock>()>;
using ServiceFactory = std::function<std::unique_ptr<Service>()>;
using AppFactory = std::function<std::unique_ptr<App>()>;

fruit::Component<AppFactory> getAppComponent(const Config* config) {
  return fruit::createComponent()
      .bindInstance(*config)  // instance, bound by reference
      .registerConstructor<CountingLogger()>()
      .bind<ILogger, CountingLogger>()
#ifndef DI_MISSING_BINDING
      .registerConstructor<InMemoryRepository()>()
      .bind<IRepository, InMemoryRepository>()
#endif
      .registerFactory<std::unique_ptr<SteadyClock>()>(
          []() { return std::unique_ptr<SteadyClock>(new SteadyClock()); })
      .bind<IClock, SteadyClock>()  // also binds ClockFactory
      .registerFactory<std::unique_ptr<Service>(std::shared_ptr<ILogger>,
                                                std::shared_ptr<IRepository>, ClockFactory,
                                                const Config&)>(
          [](std::shared_ptr<ILogger> logger, std::shared_ptr<IRepository> repository,
             ClockFactory clock, const Config& cfg) {
            return std::unique_ptr<Service>(
                new Service(std::move(logger), std::move(repository), clock(), cfg));
          })
      .registerFactory<std::unique_ptr<App>(ServiceFactory, std::shared_ptr<ILogger>)>(
          [](ServiceFactory service, std::shared_ptr<ILogger> logger) {
            return std::unique_ptr<App>(new App(service(), std::move(logger)));
          });
}

#if defined(DI_BENCH)
fruit::Component<> getEmptyComponent() { return fruit::createComponent(); }
#endif

}  // namespace

#if defined(DI_BENCH)
#include "bench.hpp"

int main(int argc, char** argv) {
  const bench::Options opt = bench::parse(argc, argv);
  Config config{"demo", 1000};
  {
    fruit::Injector<AppFactory> injector(getAppComponent, &config);
    AppFactory factory = injector.get<AppFactory>();
    auto a = factory();
    auto b = factory();
    if (check_wiring(*a, *b, config) != 0) return 2;
  }
  bench::run("fruit", "graph", opt, [&] {
    fruit::Injector<AppFactory> injector(getAppComponent, &config);
    auto app = injector.get<AppFactory>()();
    bench::do_not_optimize(*app);
  });
  // Fruit's recommended pattern for creating many injectors: normalize the
  // component once, then build cheap per-request injectors from it.
  const fruit::NormalizedComponent<AppFactory> normalized(getAppComponent, &config);
  bench::run("fruit", "graph_normalized", opt, [&] {
    fruit::Injector<AppFactory> injector(normalized, getEmptyComponent);
    auto app = injector.get<AppFactory>()();
    bench::do_not_optimize(*app);
  });
  fruit::Injector<AppFactory> injector(getAppComponent, &config);
  AppFactory factory = injector.get<AppFactory>();
  bench::run("fruit", "resolve", opt, [&] {
    auto app = factory();
    bench::do_not_optimize(*app);
  });
  return 0;
}
#else
int main() {
  Config config{"demo", 100};
  fruit::Injector<AppFactory> injector(getAppComponent, &config);
  AppFactory factory = injector.get<AppFactory>();
  std::unique_ptr<App> a = factory();
  std::unique_ptr<App> b = factory();
  return report("fruit", *a, *b, config);
}
#endif
