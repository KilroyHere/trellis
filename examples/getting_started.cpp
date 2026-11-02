// The examples of docs/getting-started.md: one small application wired with Trellis, whose parts
// the guide shows section by section. examples/CMakeLists.txt builds it with the project's warning
// set as errors and runs it as the test example.getting_started, which also checks what the guide
// says each request returns. The guide's C++ blocks are excerpts of this file and of the other
// files they name, which the check tools.doc_snippets keeps in step: change the guide with them.
// Copyright (c) 2026 Aryan Patel
// SPDX-License-Identifier: BSL-1.0
#include <trellis/trellis.hpp>

#include <chrono>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace di = trellis;

// ---- the domain -------------------------------------------------------------------------------

struct Config {
  std::string db_url;
  std::string smtp_host;
};

struct ILogger {
  virtual ~ILogger() = default;
  virtual void log(std::string_view message) = 0;
};
struct IClock {
  virtual ~IClock() = default;
  [[nodiscard]] virtual long now() const = 0;
};
struct IRepository {
  virtual ~IRepository() = default;
  virtual void save(int order) = 0;
};
struct ITransport {
  virtual ~ITransport() = default;
  virtual void send(std::string_view text) = 0;
};

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
  SqlRepository(const Config& cfg, ILogger& log) : cfg_(cfg), log_(log) {}
  void save(int order) override {
    log_.log(cfg_.db_url + ": saved order " + std::to_string(order));
  }

 private:
  const Config& cfg_;
  ILogger& log_;
};

class SmtpTransport final : public ITransport {
 public:
  SmtpTransport(const Config& cfg, ILogger& log) : host_(cfg.smtp_host), log_(log) {}
  void send(std::string_view text) override { log_.log(host_ + " <- " + std::string(text)); }

 private:
  std::string host_;
  ILogger& log_;
};

// Two constructors: deduction would take the one with more parameters, so the class declares the
// one the injector calls.
class Mailer {
 public:
  using trellis_ctor = trellis::ctor<ILogger&>;

  explicit Mailer(ILogger& log) : log_(log) {}
  Mailer(ILogger& log, std::unique_ptr<ITransport> transport)
      : log_(log), transport_(std::move(transport)) {}

  void send(std::string_view text) {
    if (transport_) {
      transport_->send(text);
    } else {
      log_.log("not sent: " + std::string(text));
    }
  }
  [[nodiscard]] bool has_transport() const { return transport_ != nullptr; }

 private:
  ILogger& log_;
  std::unique_ptr<ITransport> transport_;
};

namespace vendor {  // a library's class, which knows nothing of Trellis

class EventBus {
 public:
  using handler = std::function<void(std::string_view)>;

  // Subscribes any number of handlers, of any types that convert to handler.
  template <class... Handlers>
  explicit EventBus(Handlers... handlers) : handlers_{handler(std::move(handlers))...} {}

  void publish(std::string_view event) const {
    for (const handler& on_event : handlers_) {
      on_event(event);
    }
  }

 private:
  std::vector<handler> handlers_;
};

}  // namespace vendor

// Deduction can't count the parameters of a constructor template that takes anything (TRL-E004),
// so a specialization of trellis::ctor_traits declares the constructor to call, outside the class:
// here, the one with no arguments.
template <>
struct trellis::ctor_traits<vendor::EventBus> : trellis::ctor<> {};

// An aggregate: each member is a request, in order, as a constructor's parameters are.
struct OrderDeps {
  std::shared_ptr<IRepository> repository;
  Mailer& mailer;
  vendor::EventBus& events;
};

class OrderService {
 public:
  OrderService(OrderDeps deps, ILogger& log, std::unique_ptr<IClock> clock)
      : deps_(std::move(deps)), log_(log), clock_(std::move(clock)) {}

  void place(int order) {
    log_.log("order " + std::to_string(order) + " at " + std::to_string(clock_->now()));
    deps_.repository->save(order);
    deps_.mailer.send("order " + std::to_string(order) + " placed");
    deps_.events.publish("order placed");
  }

 private:
  OrderDeps deps_;
  ILogger& log_;
  std::unique_ptr<IClock> clock_;
};

// A value that the injector copies into each consumer that takes it by value.
struct RetryPolicy {
  int attempts = 3;
};

// ---- the composition root ---------------------------------------------------------------------

// The production wiring, as a function that returns a module.
auto app_module(const Config& cfg) {
  return di::make_module(
      di::bind<Config>.to_ref(cfg),                                 // the caller's object
      di::bind<ILogger>.to<ConsoleLogger>().in(di::singleton),      // one per injector
      di::bind<IRepository>.to<SqlRepository>().in(di::singleton),  // one per injector
      di::bind<IClock>.to<SystemClock>(),                           // unique: one per request
      di::bind<Mailer>.in(di::singleton),                           // its constructor: the alias
      di::bind<vendor::EventBus>.in(di::singleton_inline));         // stored in the injector
}

// The injector, with an option: eager builds every singleton in make_injector.
auto make_app_injector(const Config& cfg) {
  return di::make_injector<di::options{.eager = true}>(app_module(cfg));
}

// The injector can't be moved, so it is made where it lives: a member initialized from
// make_injector, or a heap object that new auto(...) initializes.
using app_injector = decltype(make_app_injector(std::declval<const Config&>()));

class Application {
 public:
  explicit Application(const Config& cfg) : injector_(make_app_injector(cfg)) {}
  void run() { injector_.create<OrderService>().place(42); }

 private:
  app_injector injector_;
};

std::unique_ptr<app_injector> make_heap_injector(const Config& cfg) {
  return std::unique_ptr<app_injector>(new auto(make_app_injector(cfg)));
}

// ---- the guide's sections, run ----------------------------------------------------------------

// Counts the checks that fail, so that the test fails with them.
int failures = 0;
void check(bool condition, const char* what) {
  if (!condition) {
    std::printf("FAILED: %s\n", what);
    ++failures;
  }
}

void declared_on_the_binding(const Config& cfg) {
  // The binding's declaration wins over the class's own, for this binding only.
  auto injector = di::make_injector(
      di::bind<Config>.to_ref(cfg), di::bind<ILogger>.to<ConsoleLogger>().in(di::singleton),
      di::bind<ITransport>.to<SmtpTransport>(),
      di::bind<Mailer>.with_ctor<ILogger&, std::unique_ptr<ITransport>>().in(di::singleton));
  injector.create<Mailer&>().send("sent through SMTP");

  check(injector.create<Mailer&>().has_transport(), "with_ctor calls the declared constructor");
  check(!make_app_injector(cfg).create<Mailer&>().has_transport(),
        "elsewhere the member alias declares the constructor");
}

void holding_bindings(const Config& cfg) {
  auto bus = std::make_shared<vendor::EventBus>();
  auto injector = di::make_injector(
      di::bind<Config>.to_ref(cfg).allow_copy(),       // a Config by value is a copy of cfg
      di::bind<RetryPolicy>.to_value(RetryPolicy{5}),  // the injector owns a copy
      di::bind<vendor::EventBus>.to_shared(bus));      // the caller and the injector share it
  const Config copy = injector.create<Config>();
  const RetryPolicy policy = injector.create<RetryPolicy>();
  const std::shared_ptr<vendor::EventBus> shared =
      injector.create<std::shared_ptr<vendor::EventBus>>();

  check(copy.db_url == cfg.db_url, "allow_copy gives a copy of cfg");
  check(&injector.create<const Config&>() == &cfg, "a reference still reaches cfg itself");
  check(policy.attempts == 5, "to_value gives a copy of the value");
  check(shared == bus, "to_shared shares the caller's object");
}

void request_forms(const Config& cfg) {
  auto injector = di::make_injector(app_module(cfg));
  OrderService service = injector.create<OrderService>();          // a new object
  ILogger& logger = injector.create<ILogger&>();                   // the singleton
  const ILogger* same_logger = injector.create<const ILogger*>();  // the same singleton
  const Config& config = injector.create<const Config&>();         // the caller's object
  std::unique_ptr<IClock> clock = injector.create<std::unique_ptr<IClock>>();  // a new clock
  std::shared_ptr<IRepository> repository = injector.create<std::shared_ptr<IRepository>>();
  std::weak_ptr<IRepository> weak = injector.create<std::weak_ptr<IRepository>>();
  vendor::EventBus& events = injector.create<vendor::EventBus&>();

  service.place(7);
  check(same_logger == &logger, "T& and const T* of a singleton are one object");
  check(&config == &cfg, "to_ref serves the caller's object");
  check(clock != nullptr && clock.get() != injector.create<std::unique_ptr<IClock>>().get(),
        "each unique_ptr request gets a new object");
  check(weak.lock() == repository, "weak_ptr and shared_ptr of a singleton share it");
  check(&events == injector.create<vendor::EventBus*>(), "singleton_inline is one object");
}

void where_the_injector_lives(const Config& cfg) {
  Application application(cfg);
  application.run();
  const std::unique_ptr<app_injector> heap = make_heap_injector(cfg);
  heap->create<OrderService>().place(43);
}

int main() {
  const Config cfg{"postgres://localhost/shop", "smtp.localhost"};
  declared_on_the_binding(cfg);
  holding_bindings(cfg);
  request_forms(cfg);
  where_the_injector_lives(cfg);
  return failures == 0 ? 0 : 1;
}
