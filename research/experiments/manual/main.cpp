// Manual (hand-written) dependency injection baseline.
//
// Build modes (same file):
//   default              scenario program (compile time / size / warnings)
//   -DDI_MISSING_BINDING the IRepository "binding" (its construction) is removed
//   -DDI_BENCH           micro-benchmark main
#include "domain.hpp"

using namespace demo;

namespace {

// The whole composition root: shared objects are created once and passed by
// shared_ptr; per-resolution objects are created on every call.
struct Wiring {
  explicit Wiring(const Config& config)
      : config(config),
        logger(std::make_shared<CountingLogger>())
#ifndef DI_MISSING_BINDING
        ,
        repository(std::make_shared<InMemoryRepository>())
#endif
  {
  }

  App make_app() const {
    return App(std::make_unique<Service>(logger, repository, std::make_unique<SteadyClock>(),
                                         config),
               logger);
  }

  const Config& config;
  std::shared_ptr<ILogger> logger;
#ifndef DI_MISSING_BINDING
  std::shared_ptr<IRepository> repository;
#endif
};

}  // namespace

#if defined(DI_BENCH)
#include "bench.hpp"

int main(int argc, char** argv) {
  const bench::Options opt = bench::parse(argc, argv);
  Config config{"demo", 1000};
  {
    Wiring w(config);
    App a = w.make_app();
    App b = w.make_app();
    if (check_wiring(a, b, config) != 0) return 2;
  }
  bench::run("manual", "graph", opt, [&] {
    Wiring w(config);
    App app = w.make_app();
    bench::do_not_optimize(app);
  });
  Wiring w(config);
  bench::run("manual", "resolve", opt, [&] {
    App app = w.make_app();
    bench::do_not_optimize(app);
  });
  return 0;
}
#else
int main() {
  Config config{"demo", 100};
  Wiring wiring(config);
  App a = wiring.make_app();
  App b = wiring.make_app();
  return report("manual", a, b, config);
}
#endif
