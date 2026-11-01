// The research scenario wired with Trellis: the variant that the targets of plan §3.2 measure
// against the hand-written wiring in manual.hpp (T1.9). It uses the public API only, as a user
// would: one binding per decision that manual_wiring makes by hand, and create<App>(). Service and
// App have no binding: they are concrete, so each request builds a new one. Every constructor is
// deduced; the domain classes in domain.hpp know nothing of Trellis.
#pragma once

#include <trellis/trellis.hpp>

#include <utility>

#include "domain.hpp"

namespace demo {

// The composition root: the logger and the repository are shared, one per injector; each request
// gets a new clock; the Config is the caller's, used and never owned.
inline auto make_trellis_injector(const Config& config) {
  return trellis::make_injector(
      trellis::bind<Config>.to_ref(config),
      trellis::bind<ILogger>.to<CountingLogger>().in(trellis::singleton),
      trellis::bind<IRepository>.to<InMemoryRepository>().in(trellis::singleton),
      trellis::bind<IClock>.to<SteadyClock>());
}

// The wiring class of the shape manual.hpp describes, for the scenario program (trellis.cpp) and
// the runtime benchmarks (benchmarks/runtime/scenario_bench.cpp): the injector, made with the
// wiring, and make_app() resolving the root. The injector is mutable because create<T>() isn't
// const (it builds singletons on first use) while the shape's make_app() is; create<T>() is
// thread-safe, as a mutable member of a const object must be (ADR-0012).
class trellis_wiring {
 public:
  explicit trellis_wiring(const Config& config) : injector_(make_trellis_injector(config)) {}

  [[nodiscard]] App make_app() const { return injector_.create<App>(); }

 private:
  mutable decltype(make_trellis_injector(std::declval<const Config&>())) injector_;
};

}  // namespace demo
