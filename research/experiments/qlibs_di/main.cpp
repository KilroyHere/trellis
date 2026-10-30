// qlibs/di (Kris Jusiak's C++20 successor to Boost.DI, single header "di").
//
// Build modes (same file):
//   default              scenario program
//   -DDI_MISSING_BINDING the IRepository binding (overload) is removed
//   -DDI_BENCH           micro-benchmark main
//
// qlibs/di is a minimal building block, not a container: di::make<T>(f)
// deduces T's constructor parameters and asks the overload set `f` for each
// one, choosing the most constrained matching lambda. There are no built-in
// scopes, so "shared" objects live in a per-container Scope struct captured
// by reference (the library's README shows a function-local static instead,
// which would give one instance per process). The std::unique_ptr bindings
// are spelled out because the library's generic unique_ptr path returns a
// std::shared_ptr (see ctor_traits<std::unique_ptr<T>> in the header).
#include <di>

#include "domain.hpp"

using namespace demo;

namespace {

struct Scope {
  std::shared_ptr<ILogger> logger;
  std::shared_ptr<IRepository> repository;
};

auto make_injector(Scope& scope, const Config& config) {
  return di::overload{
      [&config](di::is<const Config&> auto) -> const Config& { return config; },  // instance
      [&scope](di::is<std::shared_ptr<ILogger>> auto) {
        if (!scope.logger) scope.logger = std::make_shared<CountingLogger>();
        return scope.logger;
      },
#ifndef DI_MISSING_BINDING
      [&scope](di::is<std::shared_ptr<IRepository>> auto) {
        if (!scope.repository) scope.repository = std::make_shared<InMemoryRepository>();
        return scope.repository;
      },
#endif
      [](di::is<std::unique_ptr<IClock>> auto) -> std::unique_ptr<IClock> {
        return std::make_unique<SteadyClock>();
      },
      [](di::is<std::unique_ptr<Service>> auto t) {
        return std::make_unique<Service>(di::make<Service>(t));
      },
      [](auto t) -> decltype(auto) { return di::make(t); },  // everything else
  };
}

}  // namespace

#if defined(DI_BENCH)
#include "bench.hpp"

int main(int argc, char** argv) {
  const bench::Options opt = bench::parse(argc, argv);
  Config config{"demo", 1000};
  {
    Scope scope;
    auto injector = make_injector(scope, config);
    App a = di::make<App>(injector);
    App b = di::make<App>(injector);
    if (check_wiring(a, b, config) != 0) return 2;
  }
  bench::run("qlibs_di", "graph", opt, [&] {
    Scope scope;
    auto injector = make_injector(scope, config);
    App app = di::make<App>(injector);
    bench::do_not_optimize(app);
  });
  Scope scope;
  auto injector = make_injector(scope, config);
  bench::run("qlibs_di", "resolve", opt, [&] {
    App app = di::make<App>(injector);
    bench::do_not_optimize(app);
  });
  return 0;
}
#else
int main() {
  Config config{"demo", 100};
  Scope scope;
  auto injector = make_injector(scope, config);
  App a = di::make<App>(injector);
  App b = di::make<App>(injector);
  return report("qlibs_di", a, b, config);
}
#endif
