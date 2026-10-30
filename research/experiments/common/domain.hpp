// Domain model shared by every program in this experiment (manual baseline
// and all DI frameworks). The classes are plain C++: no framework macros, no
// framework base classes. Each framework program wires exactly these types.
//
// Scenario
//   ILogger     -> CountingLogger      shared (one instance per container)
//   IRepository -> InMemoryRepository  shared (one instance per container)
//   IClock      -> SteadyClock         unique (fresh instance per resolution)
//   Config                             instance bound by reference
//   Service(shared_ptr<ILogger>, shared_ptr<IRepository>,
//           unique_ptr<IClock>, const Config&)              unique
//   App(unique_ptr<Service>, shared_ptr<ILogger>)           resolved root
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace demo {

struct Config {
  std::string app_name;
  int max_items;
};

class ILogger {
 public:
  virtual ~ILogger() = default;
  virtual void log(const char* message) = 0;
  virtual std::size_t lines() const = 0;
};

class IRepository {
 public:
  virtual ~IRepository() = default;
  virtual void add(int value) = 0;
  virtual std::size_t size() const = 0;
};

class IClock {
 public:
  virtual ~IClock() = default;
  virtual std::int64_t now() const = 0;
};

class CountingLogger final : public ILogger {
 public:
  CountingLogger() = default;
  void log(const char* /*message*/) override { ++lines_; }
  std::size_t lines() const override { return lines_; }

 private:
  std::size_t lines_ = 0;
};

class InMemoryRepository final : public IRepository {
 public:
  InMemoryRepository() = default;
  void add(int value) override { items_.push_back(value); }
  std::size_t size() const override { return items_.size(); }

 private:
  std::vector<int> items_;
};

class SteadyClock final : public IClock {
 public:
  SteadyClock() = default;
  std::int64_t now() const override {
    return static_cast<std::int64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
  }
};

class Service {
 public:
  Service(std::shared_ptr<ILogger> logger, std::shared_ptr<IRepository> repository,
          std::unique_ptr<IClock> clock, const Config& config)
      : logger_(std::move(logger)),
        repository_(std::move(repository)),
        clock_(std::move(clock)),
        config_(&config) {}

  bool handle(int value) {
    if (static_cast<int>(repository_->size()) >= config_->max_items) {
      logger_->log("repository full");
      return false;
    }
    repository_->add(value);
    logger_->log("stored");
    return clock_->now() != 0;
  }

  ILogger& logger() const { return *logger_; }
  IRepository& repository() const { return *repository_; }
  IClock& clock() const { return *clock_; }
  const Config& config() const { return *config_; }

 private:
  std::shared_ptr<ILogger> logger_;
  std::shared_ptr<IRepository> repository_;
  std::unique_ptr<IClock> clock_;
  const Config* config_;
};

class App {
 public:
  App(std::unique_ptr<Service> service, std::shared_ptr<ILogger> logger)
      : service_(std::move(service)), logger_(std::move(logger)) {}

  int run() {
    int handled = 0;
    for (int i = 0; i < 3; ++i) handled += service_->handle(i) ? 1 : 0;
    logger_->log("done");
    return handled;
  }

  Service& service() const { return *service_; }
  ILogger& logger() const { return *logger_; }

 private:
  std::unique_ptr<Service> service_;
  std::shared_ptr<ILogger> logger_;
};

// Bit mask of violated wiring expectations for two App objects resolved from
// the same container (0 == every expectation holds).
inline int check_wiring(const App& a, const App& b, const Config& config) {
  int errors = 0;
  if (&a.logger() != &a.service().logger()) errors |= 1;    // shared inside one graph
  if (&a.logger() != &b.logger()) errors |= 2;              // shared across resolutions
  if (&a.service().repository() != &b.service().repository()) errors |= 4;
  if (&a.service() == &b.service()) errors |= 8;            // unique per resolution
  if (&a.service().clock() == &b.service().clock()) errors |= 16;
  if (&a.service().config() != &config) errors |= 32;       // instance bound by reference
  return errors;
}

// Prints one line describing the run; returns the process exit code.
inline int report(const char* framework, App& a, App& b, const Config& config) {
  const int errors = check_wiring(a, b, config);
  const int handled = a.run() + b.run();
  std::printf("%s: wiring_errors=%d handled=%d log_lines=%zu repo_size=%zu\n", framework,
              errors, handled, a.logger().lines(), a.service().repository().size());
  return (errors == 0 && handled == 6) ? 0 : 1;
}

}  // namespace demo
