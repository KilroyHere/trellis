// Hypodermic (ybainier/Hypodermic, header only, needs Boost headers:
// signals2, algorithm/string, range). Autofac-style runtime container.
//
// Build modes (same file):
//   default              scenario program
//   -DDI_MISSING_BINDING the IRepository registration is removed
//   -DDI_BENCH           micro-benchmark main
//
// Deviation (documented in the README): Hypodermic auto-wires only
// constructor arguments of type std::shared_ptr<T> (plus vector/function
// variants of it) and always hands out std::shared_ptr<T>. It cannot inject
// std::unique_ptr<IClock>, std::unique_ptr<Service> or const Config&, so App
// is registered with an instance factory that builds Service and the clock
// itself and asks the container only for ILogger, IRepository and Config.
#include <Hypodermic/Hypodermic.h>

#include "domain.hpp"

using namespace demo;

namespace {

std::shared_ptr<Hypodermic::Container> make_container(Config& config) {
  Hypodermic::ContainerBuilder builder;
  // instance, by reference: a non-owning (aliasing) shared_ptr to config
  builder.registerInstance(std::shared_ptr<Config>(std::shared_ptr<Config>(), &config));
  builder.registerType<CountingLogger>().as<ILogger>().singleInstance();
#ifndef DI_MISSING_BINDING
  builder.registerType<InMemoryRepository>().as<IRepository>().singleInstance();
#endif
  builder.registerInstanceFactory([](Hypodermic::ComponentContext& ctx) {
    auto logger = ctx.resolve<ILogger>();
    return std::make_shared<App>(
        std::make_unique<Service>(logger, ctx.resolve<IRepository>(),
                                  std::make_unique<SteadyClock>(), *ctx.resolve<Config>()),
        logger);
  });
  return builder.build();
}

}  // namespace

#if defined(DI_BENCH)
#include "bench.hpp"

int main(int argc, char** argv) {
  const bench::Options opt = bench::parse(argc, argv);
  Config config{"demo", 1000};
  {
    auto container = make_container(config);
    auto a = container->resolve<App>();
    auto b = container->resolve<App>();
    if (check_wiring(*a, *b, config) != 0) return 2;
  }
  bench::run("hypodermic", "graph", opt, [&] {
    auto container = make_container(config);
    auto app = container->resolve<App>();
    bench::do_not_optimize(*app);
  });
  auto container = make_container(config);
  bench::run("hypodermic", "resolve", opt, [&] {
    auto app = container->resolve<App>();
    bench::do_not_optimize(*app);
  });
  return 0;
}
#else
int main() {
  Config config{"demo", 100};
  auto container = make_container(config);
  std::shared_ptr<App> a = container->resolve<App>();
  std::shared_ptr<App> b = container->resolve<App>();
  return report("hypodermic", *a, *b, config);
}
#endif
