// compile-fail: plan §6.1's hello world without its bind<Service> line: App takes Service&, which nothing owns
// expect-id: TRL-E012
// expect-regex: unbound concrete type requested by reference
// expect-regex: E012<Service, trellis::error::request<Service ?&>|error C2338:
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
//
// examples/hello_world.cpp with the line `di::bind<Service>.in(di::singleton),` deleted, and
// nothing else changed: keep the two in step. Plan §6.1 promises that this is error E012 (Service
// is requested by reference but nothing owns it), not a silent new object. Service is concrete,
// so without a binding it is implicitly unique (ADR-0004), and a reference to a unique object
// would dangle; create<App>() reports the request it finds one hop down, exactly once.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006:
// one static_assert per root cause). The regex on the error type also accepts MSVC's
// static_assert error, C2338, in its place: MSVC prints the literal but not the condition
// (ADR-0006; untested here, as MSVC runs only in CI).
#include <trellis/trellis.hpp>

#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

// The domain: interfaces, their implementations and the classes that use them. Constructors are
// discovered without macros. The interfaces keep plan §6.1's one-line layout.
// clang-format off
struct ILogger     { virtual ~ILogger() = default;     virtual void log(std::string_view) = 0; };
struct IRepository { virtual ~IRepository() = default; virtual void save(int) = 0; };
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
namespace di = trellis;

int main() {
  const Config cfg = load_config();
  auto injector = di::make_injector(
      di::bind<ILogger>.to<ConsoleLogger>().in(di::singleton),  // one per injector
      di::bind<IRepository>.to<SqlRepository>().in(di::singleton),
      di::bind<IClock>.to<SystemClock>(),  // unique: a new one per request
      di::bind<Config>.to_ref(cfg));       // external: used, never owned
  injector.create<App>().run();
}
