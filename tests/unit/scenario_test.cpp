// The research scenario (research/experiments/common/domain.hpp) wired with Trellis, against the
// same scenario wired by hand (research/experiments/manual/main.cpp), object by object (T1.5).
//
// The domain classes are plain C++ that knows nothing of Trellis: their constructors are
// discovered by deduction. tests/unit/CMakeLists.txt builds this file as its own program, under
// the project's warnings as errors, with test names prefixed scenario., so that
// `ctest -R scenario` selects it; the asan-ubsan preset runs it under AddressSanitizer and
// UndefinedBehaviorSanitizer.
#include <trellis/trellis.hpp>

#include <cstdio>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "domain.hpp"

namespace {

using namespace demo;

// The hand-written composition root, as in research/experiments/manual/main.cpp: the shared
// objects are made with it, and every make_app() makes a new Service and a new clock.
struct manual_wiring {
  explicit manual_wiring(const Config& settings)
      : config(&settings),
        logger(std::make_shared<CountingLogger>()),
        repository(std::make_shared<InMemoryRepository>()) {}

  [[nodiscard]] App make_app() const {
    return {std::make_unique<Service>(logger, repository, std::make_unique<SteadyClock>(), *config),
            logger};
  }

  const Config* config;
  std::shared_ptr<ILogger> logger;
  std::shared_ptr<IRepository> repository;
};

// The same composition root with Trellis: one binding per decision the hand-written one makes.
// Service and App have no binding: they are concrete, so each request gets a new one.
auto trellis_wiring(const Config& settings) {
  return trellis::make_injector(
      trellis::bind<Config>.to_ref(settings),
      trellis::bind<ILogger>.to<CountingLogger>().in(trellis::singleton),
      trellis::bind<IRepository>.to<InMemoryRepository>().in(trellis::singleton),
      trellis::bind<IClock>.to<SteadyClock>());
}

// The implementation types behind each interface, as the wiring chose them.
template <class Impl, class Interface>
bool is_a(const Interface& object) {
  return dynamic_cast<const Impl*>(&object) != nullptr;
}

TEST(Scenario, MatchesTheHandWiredGraphObjectByObject) {
  const Config config{"demo", 100};
  const manual_wiring manual(config);
  auto injector = trellis_wiring(config);

  App manual_a = manual.make_app();
  App manual_b = manual.make_app();
  App wired_a = injector.create<App>();
  App wired_b = injector.create<App>();

  // The scenario's own expectations (research/experiments/common/domain.hpp, check_wiring): 0
  // means every sharing and identity expectation holds.
  EXPECT_EQ(check_wiring(manual_a, manual_b, config), 0);
  EXPECT_EQ(check_wiring(wired_a, wired_b, config), 0);

  // Each object, in both graphs: the same implementation types...
  for (const App* app : {&manual_a, &wired_a}) {
    EXPECT_TRUE(is_a<CountingLogger>(app->logger()));
    EXPECT_TRUE(is_a<InMemoryRepository>(app->service().repository()));
    EXPECT_TRUE(is_a<SteadyClock>(app->service().clock()));
  }
  // ... the same sharing within one graph and across two: one logger and one repository per
  // composition root, a new Service and a new clock per App, and the caller's Config itself.
  EXPECT_EQ(&wired_a.logger(), &wired_a.service().logger());
  EXPECT_EQ(&wired_a.logger(), &wired_b.logger());
  EXPECT_EQ(&wired_a.service().repository(), &wired_b.service().repository());
  EXPECT_NE(&wired_a.service(), &wired_b.service());
  EXPECT_NE(&wired_a.service().clock(), &wired_b.service().clock());
  EXPECT_EQ(&wired_a.service().config(), &config);
  EXPECT_EQ(&wired_b.service().config(), &config);
  // ... and the injector's singletons are the ones the graph holds.
  EXPECT_EQ(&injector.create<ILogger&>(), &wired_a.logger());
  EXPECT_EQ(&injector.create<IRepository&>(), &wired_a.service().repository());
  EXPECT_EQ(injector.create<std::shared_ptr<ILogger>>().get(), &wired_a.logger());
  // The two composition roots share nothing.
  EXPECT_NE(&manual_a.logger(), &wired_a.logger());
  EXPECT_NE(&manual_a.service().repository(), &wired_a.service().repository());

  // Running both graphs gives the same results: three items handled per App, the log lines and
  // the repository size of the scenario's expected output line.
  const int manual_handled = manual_a.run() + manual_b.run();
  const int wired_handled = wired_a.run() + wired_b.run();
  EXPECT_EQ(wired_handled, manual_handled);
  EXPECT_EQ(wired_handled, 6);
  EXPECT_EQ(wired_a.logger().lines(), manual_a.logger().lines());
  EXPECT_EQ(wired_a.logger().lines(), 8U);
  EXPECT_EQ(wired_a.service().repository().size(), manual_a.service().repository().size());
  EXPECT_EQ(wired_a.service().repository().size(), 6U);
}

TEST(Scenario, ReportsLikeTheResearchPrograms) {
  // The scenario program's own report, as benchmarks/compile/run.py expects it from every
  // wiring: "<name>: wiring_errors=0 handled=6 log_lines=8 repo_size=6", exit status 0.
  const Config config{"demo", 100};
  auto injector = trellis_wiring(config);
  App a = injector.create<App>();
  App b = injector.create<App>();
  ::testing::internal::CaptureStdout();
  const int status = report("trellis", a, b, config);
  const std::string line = ::testing::internal::GetCapturedStdout();
  EXPECT_EQ(status, 0);
  EXPECT_EQ(line, "trellis: wiring_errors=0 handled=6 log_lines=8 repo_size=6\n");
}

TEST(Scenario, ConfigLimitsBehaveTheSameInBothWirings) {
  // A repository limit of 2: the third item is refused, in both wirings alike.
  const Config config{"tight", 2};
  const manual_wiring manual(config);
  auto injector = trellis_wiring(config);
  App manual_app = manual.make_app();
  App wired_app = injector.create<App>();
  EXPECT_EQ(wired_app.run(), manual_app.run());
  EXPECT_EQ(wired_app.logger().lines(), manual_app.logger().lines());
  EXPECT_EQ(wired_app.service().repository().size(), 2U);
}

TEST(Scenario, SingletonsDieWithTheirLastOwner) {
  // The App holds the logger by shared_ptr, so it outlives the injector, as it outlives the
  // hand-written root (ADR-0005).
  const Config config{"demo", 100};
  std::unique_ptr<App> app;
  {
    auto injector = trellis_wiring(config);
    app = injector.create<std::unique_ptr<App>>();
  }
  ASSERT_TRUE(app);
  EXPECT_EQ(app->run(), 3);
  EXPECT_EQ(app->logger().lines(), 4U);
}

}  // namespace
