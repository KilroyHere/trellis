// The hello world of plan §6.1, which README.md shows: plain domain classes that know nothing of
// Trellis, and one composition root that wires them. examples/CMakeLists.txt builds it and runs it
// as the test example.hello_world. Deleting the bind<Service> line is one compile error,
// TRL-E012 (tests/compile_fail/create/e012_hello_world_without_service.cpp).
//
// README.md shows the two parts between the "README.md: begin" and "README.md: end" lines, and
// the check tools.readme_example keeps them in step, line for line. Trellis is included only
// before the composition root: the domain above it compiles without it.
// Copyright (c) 2026 Aryan Patel
// SPDX-License-Identifier: BSL-1.0
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

// The domain: interfaces, their implementations and the classes that use them. Constructors are
// discovered without macros. The interfaces keep plan §6.1's one-line layout.
// README.md: begin
// clang-format off
struct ILogger     { virtual ~ILogger() = default;     virtual void log(std::string_view) = 0; };
struct IRepository { virtual ~IRepository() = default; virtual void save(int) = 0; };
// NOLINTNEXTLINE(modernize-use-nodiscard): plan §6.1's line, as README.md shows it
struct IClock      { virtual ~IClock() = default;      virtual long now() const = 0; };
struct Config      { std::string db_url; };
// clang-format on

class ConsoleLogger final : public ILogger {
 public:
  void log(std::string_view message) override {
    std::printf("%.*s\n", static_cast<int>(message.size()), message.data());
  }
};

class SystemClock final : public IClock {
 public:
  [[nodiscard]] long now() const override {
    const auto since_epoch = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<long>(std::chrono::duration_cast<std::chrono::seconds>(since_epoch).count());
  }
};

class SqlRepository final : public IRepository {
 public:
  SqlRepository(const Config& cfg, ILogger& log);
  void save(int value) override;

 private:
  const Config& cfg_;
  ILogger& log_;
};

class Service {
 public:
  Service(std::shared_ptr<IRepository> repo, ILogger& log, std::unique_ptr<IClock> clock);
  void handle(int value);

 private:
  std::shared_ptr<IRepository> repo_;
  ILogger& log_;
  std::unique_ptr<IClock> clock_;
};

class App {
 public:
  explicit App(Service& service);
  void run();

 private:
  Service& service_;
};
// README.md: end

// The definitions, out of line so that the declarations above read as README.md shows them.
SqlRepository::SqlRepository(const Config& cfg, ILogger& log) : cfg_(cfg), log_(log) {}
void SqlRepository::save(int value) { log_.log(cfg_.db_url + ": saved " + std::to_string(value)); }

Service::Service(std::shared_ptr<IRepository> repo, ILogger& log, std::unique_ptr<IClock> clock)
    : repo_(std::move(repo)), log_(log), clock_(std::move(clock)) {}
void Service::handle(int value) {
  log_.log(clock_->now() > 0 ? "handling a request" : "handling a request before 1970");
  repo_->save(value);
}

App::App(Service& service) : service_(service) {}
void App::run() { service_.handle(42); }

Config load_config() { return Config{"postgres://localhost/app"}; }

// The composition root: which implementation goes with each interface, and how long each object
// lives.
// README.md: begin
#include <trellis/trellis.hpp>

namespace di = trellis;

int main() {
  const Config cfg = load_config();
  auto injector = di::make_injector(
      di::bind<ILogger>.to<ConsoleLogger>().in(di::singleton),  // one per injector
      di::bind<IRepository>.to<SqlRepository>().in(di::singleton),
      di::bind<IClock>.to<SystemClock>(),   // unique: a new one per request
      di::bind<Service>.in(di::singleton),  // App takes Service&, so it needs an owner
      di::bind<Config>.to_ref(cfg));        // external: used, never owned
  injector.create<App>().run();
}
// README.md: end
