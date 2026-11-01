// Tests for the testing helpers (T1.7; plan §6.10): the gMock example of plan §6.10, with a
// StrictMock repository that trellis::override<IRepository> puts in the production wiring;
// isolation, each injector owning its singletons, so tests can't leak state into each other;
// trellis::testing::wireable, both ways, against the errors create<T>() itself reports; and
// trellis::testing::create_all.
//
// Overrides themselves are tested in override_test.cpp. tests/compile_fail/testing/ checks that
// create<T>() fails, with one error, where wireable is false.
#include <trellis/testing/wiring.hpp>
#include <trellis/trellis.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace di = trellis;

// ---- the application of plan §6.1, as a test sees it ------------------------------------------

namespace shop {

struct Config {
  std::string db_url;
};

struct ILogger {
  ILogger() = default;
  ILogger(const ILogger&) = delete;
  ILogger& operator=(const ILogger&) = delete;
  virtual ~ILogger() = default;
  virtual void log(std::string_view message) = 0;
};
struct IRepository {
  IRepository() = default;
  IRepository(const IRepository&) = delete;
  IRepository& operator=(const IRepository&) = delete;
  virtual ~IRepository() = default;
  virtual void save(int value) = 0;
};

// Keeps what it logs, so that a test can read it.
class MemoryLogger final : public ILogger {
 public:
  void log(std::string_view message) override { lines.emplace_back(message); }
  std::vector<std::string> lines;
};

class SqlRepository final : public IRepository {
 public:
  SqlRepository(const Config& cfg, ILogger& log) : cfg_(&cfg), log_(&log) {}
  void save(int value) override { log_->log(cfg_->db_url + ": saved " + std::to_string(value)); }

 private:
  const Config* cfg_;
  ILogger* log_;
};

class Service {
 public:
  Service(IRepository& repo, ILogger& log) : repo_(&repo), log_(&log) {}
  void handle(int value) {
    log_->log("handling");
    repo_->save(value);
  }

 private:
  IRepository* repo_;
  ILogger* log_;
};

// A consumer that shares the repository, as plan §6.1's Service does.
class Auditor {
 public:
  explicit Auditor(std::shared_ptr<IRepository> repo) : repo_(std::move(repo)) {}
  void audit(int value) { repo_->save(value); }

 private:
  std::shared_ptr<IRepository> repo_;
};

// The production wiring. The configuration is a value binding, so a test can pass a temporary.
inline auto app_module(Config cfg) {
  return di::make_module(di::bind<Config>.to_value(std::move(cfg)),
                         di::bind<ILogger>.to<MemoryLogger>().in(di::singleton),
                         di::bind<IRepository>.to<SqlRepository>().in(di::singleton));
}

inline Config test_config() { return Config{"sqlite::memory:"}; }

class MockRepository : public IRepository {
 public:
  MOCK_METHOD(void, save, (int value), (override));
};

}  // namespace shop

namespace {

// ---- the gMock example of plan §6.10 ----------------------------------------------------------

TEST(GMockExample, SavesThroughRepository) {
  testing::StrictMock<shop::MockRepository> repo;
  auto injector = di::make_injector(shop::app_module(shop::test_config()),
                                    di::override<shop::IRepository>.to_ref(repo));
  EXPECT_CALL(repo, save(42));
  injector.create<shop::Service>().handle(42);
}

// A consumer that takes a std::shared_ptr<IRepository> needs the override to share the mock:
// .to_ref() gives no std::shared_ptr (plan §6.3), which create<T>() would report as TRL-E005.
TEST(GMockExample, SharesTheMockWithAConsumerThatTakesASharedPtr) {
  const auto repo = std::make_shared<testing::StrictMock<shop::MockRepository>>();
  auto injector = di::make_injector(shop::app_module(shop::test_config()),
                                    di::override<shop::IRepository>.to_shared(repo));
  EXPECT_CALL(*repo, save(7));
  EXPECT_CALL(*repo, save(42));
  injector.create<shop::Auditor>().audit(7);
  injector.create<shop::Service>().handle(42);
}

TEST(GMockExample, TheProductionWiringUsesTheRealRepository) {
  auto injector = di::make_injector(shop::app_module(shop::test_config()));
  injector.create<shop::Service>().handle(42);
  auto& logger = static_cast<shop::MemoryLogger&>(injector.create<shop::ILogger&>());
  EXPECT_EQ(logger.lines, (std::vector<std::string>{"handling", "sqlite::memory:: saved 42"}));
}

// ---- isolation --------------------------------------------------------------------------------

// Counts the objects alive, so that a test sees which injector destroys what.
int& alive() {
  static int count = 0;
  return count;
}
struct counter {
  counter() { ++alive(); }
  counter(const counter&) = delete;
  counter& operator=(const counter&) = delete;
  ~counter() { --alive(); }
  int value = 0;
};

auto counting_module() { return di::make_module(di::bind<counter>.in(di::singleton)); }

TEST(Isolation, EachInjectorOwnsItsSingletons) {
  alive() = 0;
  auto first = di::make_injector(counting_module());
  {
    auto second = di::make_injector(counting_module());
    counter& mine = first.create<counter&>();
    counter& theirs = second.create<counter&>();
    EXPECT_NE(&mine, &theirs);
    mine.value = 7;
    EXPECT_EQ(second.create<counter&>().value, 0);
    EXPECT_EQ(alive(), 2);
  }
  // The second injector took its singleton with it; the first keeps its own, state included.
  EXPECT_EQ(alive(), 1);
  EXPECT_EQ(first.create<counter&>().value, 7);
}

TEST(Isolation, AnOverrideStaysInItsInjector) {
  testing::NiceMock<shop::MockRepository> repo;
  auto test = di::make_injector(shop::app_module(shop::test_config()),
                                di::override<shop::IRepository>.to_ref(repo));
  auto production = di::make_injector(shop::app_module(shop::test_config()));
  EXPECT_EQ(&test.create<shop::IRepository&>(), &repo);
  EXPECT_NE(&production.create<shop::IRepository&>(), &repo);
  EXPECT_NE(&test.create<shop::ILogger&>(), &production.create<shop::ILogger&>());
}

// ---- wireable ---------------------------------------------------------------------------------

using production_injector = decltype(di::make_injector(shop::app_module(shop::test_config())));

// True where create<T>() compiles, in every request form the bindings allow, and for the type of
// a function's result, references and cv qualifiers ignored.
static_assert(di::testing::wireable<production_injector, shop::Service>);
static_assert(di::testing::wireable<production_injector, shop::Auditor>);
static_assert(di::testing::wireable<production_injector, shop::IRepository&>);
static_assert(di::testing::wireable<production_injector, std::shared_ptr<shop::IRepository>>);
static_assert(di::testing::wireable<production_injector, const shop::Config&>);
static_assert(di::testing::wireable<production_injector&, shop::Service>);
static_assert(di::testing::wireable<const production_injector&&, shop::Service>);

struct widget {
  widget() = default;
};
struct shape {
  shape() = default;
  shape(const shape&) = delete;
  shape& operator=(const shape&) = delete;
  virtual ~shape() = default;
  [[nodiscard]] virtual int sides() const = 0;
};
struct square final : shape {
  [[nodiscard]] int sides() const override { return 4; }
};
struct plain_base {};  // no virtual destructor
struct plain_derived final : plain_base {
  plain_derived() = default;  // not an aggregate, whose base would be a field (ADR-0003 item 5)
};
class hidden {
  hidden() = default;
};
// A constructor that takes anything is the case under test (TRL-E004).
// NOLINTBEGIN(bugprone-forwarding-reference-overload)
struct greedy {
  template <class T>
  explicit greedy(T&& /*anything*/) {}
};
// NOLINTEND(bugprone-forwarding-reference-overload)
struct three {
  three(widget /*a*/, widget /*b*/, widget /*c*/) {}
};
struct needs_widget_ref {
  explicit needs_widget_ref(widget& used) : used_(&used) {}
  widget* used_;
};
struct deep {
  explicit deep(needs_widget_ref /*inner*/) {}
};

using empty_injector = decltype(di::make_injector());
using unique_widget = decltype(di::make_injector(di::bind<widget>));

// The errors of create<T>(), one case each: wireable is false, and never an error itself.
static_assert(di::testing::wireable<unique_widget, widget>);
static_assert(!di::testing::wireable<unique_widget, widget&>);  // TRL-E005
static_assert(di::testing::wireable<empty_injector, widget>);
static_assert(!di::testing::wireable<empty_injector, widget&>);                  // TRL-E012
static_assert(!di::testing::wireable<empty_injector, std::shared_ptr<widget>>);  // TRL-E012
static_assert(!di::testing::wireable<empty_injector, std::unique_ptr<shape>>);   // TRL-E001
static_assert(!di::testing::wireable<decltype(di::make_injector(di::bind<shape>)),
                                     std::unique_ptr<shape>>);  // TRL-E001, bound to itself
static_assert(!di::testing::wireable<empty_injector, hidden>);  // TRL-E002
static_assert(!di::testing::wireable<empty_injector, greedy>);  // TRL-E004
static_assert(
    !di::testing::wireable<decltype(di::make_injector<di::options{.max_ctor_arity = 2}>()),
                           three>);  // TRL-E010
static_assert(di::testing::wireable<empty_injector, three>);
static_assert(!di::testing::wireable<decltype(di::make_injector<di::options{.strict = true}>()),
                                     widget>);  // TRL-E013
static_assert(
    !di::testing::wireable<decltype(di::make_injector(di::bind<plain_base>.to<plain_derived>())),
                           std::unique_ptr<plain_base>>);  // TRL-E014
static_assert(
    di::testing::wireable<decltype(di::make_injector(di::bind<plain_base>.to<plain_derived>())),
                          std::shared_ptr<plain_base>>);
// A failure deep in the graph makes the root unwireable, and binding what it lacks fixes it.
static_assert(!di::testing::wireable<empty_injector, deep>);
static_assert(
    di::testing::wireable<decltype(di::make_injector(di::bind<widget>.in(di::singleton))), deep>);
// Constness carries through a const external object.
static_assert(!di::testing::wireable<
              decltype(di::make_injector(di::bind<widget>.to_ref(std::declval<const widget&>()))),
              widget&>);  // TRL-E005
static_assert(di::testing::wireable<
              decltype(di::make_injector(di::bind<widget>.to_ref(std::declval<const widget&>()))),
              const widget&>);
// An injector make_injector rejects builds nothing: decltype names its type without the error.
static_assert(
    !di::testing::wireable<decltype(di::make_injector(di::bind<widget>, di::bind<widget>)),
                           widget>);  // TRL-E003
static_assert(!di::testing::wireable<
              decltype(di::make_injector(di::bind<widget>,
                                         di::override<square>.to_ref(std::declval<square&>()))),
              widget>);  // TRL-E009
// Overrides count: the mock serves a reference, not a std::shared_ptr (TRL-E005).
using mocked_injector = decltype(di::make_injector(
    shop::app_module(shop::test_config()),
    di::override<shop::IRepository>.to_ref(std::declval<shop::MockRepository&>())));
static_assert(di::testing::wireable<mocked_injector, shop::Service>);
static_assert(!di::testing::wireable<mocked_injector, shop::Auditor>);
// Not an injector, or not a request.
static_assert(!di::testing::wireable<int, widget>);
static_assert(!di::testing::wireable<empty_injector, void>);
// Eager injectors are checked the same way.
static_assert(di::testing::wireable<decltype(di::make_injector<di::options{.eager = true}>(
                                        di::bind<widget>.in(di::singleton))),
                                    widget&>);

// Injectors that make_injector rejects for what their arguments are, with no error from wireable:
// an argument that is no binding, a bad option, a binding whose key is no key (its type named
// directly, as no such binding object can be made), and an eager injector with a singleton that
// can't be built, while the same injector, not eager, builds what it can.
static_assert(!di::testing::wireable<decltype(di::make_injector(42)), widget>);
static_assert(!di::testing::wireable<
              decltype(di::make_injector<di::options{.max_ctor_arity = 40}>()), widget>);
static_assert(di::testing::wireable<
              decltype(di::make_injector<di::options{.max_ctor_arity = 32}>()), widget>);
static_assert(!di::testing::wireable<
              di::injector<di::options{}, di::binding<const widget, const widget, di::unique_t>>,
              widget>);
static_assert(!di::testing::wireable<
              di::injector<di::options{}, di::binding<widget, widget, di::unique_t>,
                           di::override_binding<di::binding<widget&, widget&, di::unique_t>>>,
              widget>);
static_assert(!di::testing::wireable<decltype(di::make_injector<di::options{.eager = true}>(
                                         di::bind<needs_widget_ref>.in(di::singleton))),
                                     widget>);
static_assert(di::testing::wireable<
              decltype(di::make_injector(di::bind<needs_widget_ref>.in(di::singleton))), widget>);

// Incomplete types: a class that is only declared, with no binding, can't be built, and nothing
// would own it for a reference, a pointer or a std::shared_ptr; a consumer that needs one can't
// be built either. None of it is an error of wireable's own.
struct declared_only;
struct takes_declared {
  explicit takes_declared(declared_only* given) : given_(given) {}
  declared_only* given_;
};
static_assert(!di::testing::wireable<empty_injector, declared_only>);                   // TRL-E002
static_assert(!di::testing::wireable<empty_injector, declared_only&>);                  // TRL-E012
static_assert(!di::testing::wireable<empty_injector, declared_only*>);                  // TRL-E012
static_assert(!di::testing::wireable<empty_injector, std::shared_ptr<declared_only>>);  // TRL-E012
static_assert(!di::testing::wireable<empty_injector, std::unique_ptr<declared_only>>);  // TRL-E002
static_assert(!di::testing::wireable<empty_injector, takes_declared>);                  // TRL-E012

// What wireable accepts, create<T>() builds.
TEST(Wireable, WhatItAcceptsCreateBuilds) {
  auto injector = di::make_injector(shop::app_module(shop::test_config()));
  static_assert(di::testing::wireable<decltype(injector), shop::Service>);
  injector.create<shop::Service>().handle(1);
  auto widgets = di::make_injector(di::bind<widget>.in(di::singleton));
  static_assert(di::testing::wireable<decltype(widgets), deep>);
  [[maybe_unused]] const deep built = widgets.create<deep>();
  auto empty = di::make_injector();
  static_assert(di::testing::wireable<decltype(empty), three>);
  [[maybe_unused]] const three made = empty.create<three>();
}

// ---- create_all -------------------------------------------------------------------------------

std::vector<std::string>& events() {
  static std::vector<std::string> log;
  return log;
}
template <class Self>
struct recorded {
  recorded() { events().push_back("+" + std::string(Self::name)); }
  recorded(const recorded&) = delete;
  recorded& operator=(const recorded&) = delete;
  ~recorded() { events().push_back("-" + std::string(Self::name)); }
};

// Each declares a constructor, so that it isn't an aggregate, whose base would be a field that
// deduction requests (ADR-0003 item 5).
struct database : recorded<database> {
  static constexpr const char* name = "database";
  database() = default;
};
struct cache : recorded<cache> {
  static constexpr const char* name = "cache";
  explicit cache(database& /*source*/) {}
};
struct mailer : recorded<mailer> {
  static constexpr const char* name = "mailer";
  mailer() = default;
};
struct request : recorded<request> {
  static constexpr const char* name = "request";
  request() = default;
};
struct fake_database : database {
  fake_database() = default;
};

// Throws while fail() says so, as a constructor that reaches a server might.
bool& fail() {
  static bool failing = false;
  return failing;
}
struct flaky : recorded<flaky> {
  static constexpr const char* name = "flaky";
  flaky() {
    if (fail()) {
      throw std::runtime_error("flaky is down");
    }
  }
};

TEST(CreateAll, BuildsEverySingletonInBindingOrderDependenciesFirst) {
  events().clear();
  {
    auto injector = di::make_injector(di::bind<cache>.in(di::singleton), di::bind<request>,
                                      di::bind<mailer>.in(di::singleton_inline),
                                      di::bind<database>.in(di::singleton));
    EXPECT_TRUE(events().empty());
    di::testing::create_all(injector);
    // cache needs the database, so the database comes first; request is unique, so nothing
    // builds it until it is requested.
    EXPECT_EQ(events(), (std::vector<std::string>{"+database", "+cache", "+mailer"}));
    di::testing::create_all(injector);  // every singleton is built once
    EXPECT_EQ(events().size(), 3U);
    { const std::unique_ptr<request> asked = injector.create<std::unique_ptr<request>>(); }
  }
  EXPECT_EQ(events(), (std::vector<std::string>{"+database", "+cache", "+mailer", "+request",
                                                "-request", "-mailer", "-cache", "-database"}));
}

TEST(CreateAll, FailsFastWithTheConstructorsException) {
  events().clear();
  fail() = true;
  {
    auto injector =
        di::make_injector(di::bind<database>.in(di::singleton), di::bind<flaky>.in(di::singleton),
                          di::bind<mailer>.in(di::singleton));
    EXPECT_THROW(di::testing::create_all(injector), std::runtime_error);
    // The database was built before, and stays; flaky's base was built and destroyed when its
    // constructor threw; the mailer, after the failure, wasn't built.
    EXPECT_EQ(events(), (std::vector<std::string>{"+database", "+flaky", "-flaky"}));
    fail() = false;
    di::testing::create_all(injector);  // the failed singleton is built on the next request
    EXPECT_EQ(events(),
              (std::vector<std::string>{"+database", "+flaky", "-flaky", "+flaky", "+mailer"}));
  }
  EXPECT_EQ(events().back(), "-database");
}

TEST(CreateAll, BuildsTheOverridesInTheirPlace) {
  events().clear();
  {
    auto injector = di::make_injector(
        di::make_module(di::bind<mailer>.in(di::singleton), di::bind<cache>.in(di::singleton),
                        di::bind<database>.in(di::singleton)),
        di::override<database>.to<fake_database>().in(di::singleton_inline));
    di::testing::create_all(injector);
    EXPECT_EQ(events(), (std::vector<std::string>{"+mailer", "+database", "+cache"}));
  }
}

TEST(CreateAll, BuildsNothingMoreForAnEagerInjector) {
  events().clear();
  auto injector = di::make_injector<di::options{.eager = true}>(
      di::bind<database>.in(di::singleton), di::bind<cache>.in(di::singleton));
  EXPECT_EQ(events(), (std::vector<std::string>{"+database", "+cache"}));
  di::testing::create_all(injector);
  EXPECT_EQ(events(), (std::vector<std::string>{"+database", "+cache"}));
}

}  // namespace
