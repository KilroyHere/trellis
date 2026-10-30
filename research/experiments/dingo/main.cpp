// dingo (romanpauk/dingo, header only, C++17): runtime container with
// compile-time constructor detection.
//
// Build modes (same file):
//   default              scenario program
//   -DDI_MISSING_BINDING the IRepository registration is removed
//   -DDI_BENCH           micro-benchmark main
//
// dingo resolves only registered types, so Service and App are registered too
// (unique scope). The interface is fixed at registration via interfaces<>.
#include <dingo/container.h>
#include <dingo/storage/external.h>
#include <dingo/storage/shared.h>
#include <dingo/storage/unique.h>

#include "domain.hpp"

using namespace demo;

namespace {

void register_all(dingo::container<>& c, Config& config) {
  using namespace dingo;
  c.register_type<scope<external>, storage<Config*>>(&config);  // instance, by reference
  c.register_type<scope<shared>, storage<std::shared_ptr<CountingLogger>>,
                  interfaces<ILogger>>();
#ifndef DI_MISSING_BINDING
  c.register_type<scope<shared>, storage<std::shared_ptr<InMemoryRepository>>,
                  interfaces<IRepository>>();
#endif
  c.register_type<scope<unique>, storage<std::unique_ptr<SteadyClock>>, interfaces<IClock>>();
  c.register_type<scope<unique>, storage<std::unique_ptr<Service>>>();
  c.register_type<scope<unique>, storage<App>>();
}

}  // namespace

#if defined(DI_BENCH)
#include "bench.hpp"

int main(int argc, char** argv) {
  const bench::Options opt = bench::parse(argc, argv);
  Config config{"demo", 1000};
  {
    dingo::container<> c;
    register_all(c, config);
    App a = c.resolve<App>();
    App b = c.resolve<App>();
    if (check_wiring(a, b, config) != 0) return 2;
  }
  bench::run("dingo", "graph", opt, [&] {
    dingo::container<> c;
    register_all(c, config);
    App app = c.resolve<App>();
    bench::do_not_optimize(app);
  });
  dingo::container<> c;
  register_all(c, config);
  bench::run("dingo", "resolve", opt, [&] {
    App app = c.resolve<App>();
    bench::do_not_optimize(app);
  });
  return 0;
}
#else
int main() {
  Config config{"demo", 100};
  dingo::container<> c;
  register_all(c, config);
  App a = c.resolve<App>();
  App b = c.resolve<App>();
  return report("dingo", a, b, config);
}
#endif
