// The research scenario wired by hand (research/experiments/manual/main.cpp): the baseline that the
// targets of plan §3.2 compare every other wiring with.
//
// A wiring of the scenario is a class that plays the injector: it is constructed from the external
// Config, which it keeps by reference, and make_app() resolves the root. The runtime benchmarks
// (benchmarks/runtime/scenario_bench.cpp) measure any class of this shape.
#pragma once

#include <memory>

#include "domain.hpp"

namespace demo {

// The shared objects (logger and repository) are created once, with the wiring; every make_app()
// creates a new Service and a new clock.
class manual_wiring {
 public:
  explicit manual_wiring(const Config& config)
      : config_(&config),
        logger_(std::make_shared<CountingLogger>()),
        repository_(std::make_shared<InMemoryRepository>()) {}

  [[nodiscard]] App make_app() const {
    return {
        std::make_unique<Service>(logger_, repository_, std::make_unique<SteadyClock>(), *config_),
        logger_};
  }

 private:
  const Config* config_;
  std::shared_ptr<ILogger> logger_;
  std::shared_ptr<IRepository> repository_;
};

}  // namespace demo
