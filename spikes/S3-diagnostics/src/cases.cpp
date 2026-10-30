// Spike S3 (throwaway): the wiring mistakes that run.sh compiles.
//
//   -DCASE_E001  Store needs std::shared_ptr<IRepository>; IRepository has no binding
//   -DCASE_E005  Store needs IRepository&; IRepository is bound with the unique lifetime
//   -DCASE_E012  Store needs Clock&; Clock is concrete and has no binding
//   -DCASE_OK    everything is bound (sanity check; the program runs)
//   -DDEPTH_ROOT requests the failing form directly: create<edge>()
//   otherwise    create<App>(), and the failing edge is three hops down:
//                App -> Service -> Store -> edge
//   -DPRINT_EXPLAIN  no create(): print what the explain pass found (C++20 is enough)
#include <cstdio>
#include <memory>

#include "mock.hpp"

struct IRepository {
  virtual ~IRepository() = default;
  virtual int get() const = 0;
};
struct SqlRepository final : IRepository {
  int get() const override { return 42; }
};
struct ILogger {
  virtual ~ILogger() = default;
  virtual void log(const char* text) = 0;
};
struct ConsoleLogger final : ILogger {
  void log(const char* text) override { std::puts(text); }
};
struct Clock {
  int now() const { return 7; }
};

#if defined(CASE_E001) || defined(CASE_OK)
using edge = std::shared_ptr<IRepository>;
#elif defined(CASE_E005)
using edge = IRepository&;
#elif defined(CASE_E012)
using edge = Clock&;
#else
#error "define one of CASE_E001, CASE_E005, CASE_E012, CASE_OK"
#endif

struct Store {
  using inject = trellis::ctor<ILogger&, edge>;
  Store(ILogger& log, edge e) : logger(&log), value(get(e)) {}
  static int get(const std::shared_ptr<IRepository>& r) { return r->get(); }
  static int get(const IRepository& r) { return r.get(); }
  static int get(const Clock& c) { return c.now(); }
  ILogger* logger;
  int value;
};
struct Service {
  using inject = trellis::ctor<std::unique_ptr<Clock>, Store>;
  Service(std::unique_ptr<Clock> c, Store s) : clock(std::move(c)), store(s) {}
  std::unique_ptr<Clock> clock;
  Store store;
};
struct App {
  using inject = trellis::ctor<ILogger&, Service>;
  App(ILogger& log, Service s) : logger(&log), service(std::move(s)) {}
  int run() {
    logger->log(service.store.value == 42 ? "ok: repository=42" : "unexpected");
    return service.store.value == 42 ? 0 : 1;
  }
  ILogger* logger;
  Service service;
};

struct app_module {
  using bindings = trellis::bindings<trellis::binding<ILogger, ConsoleLogger, trellis::singleton>
#if defined(CASE_E005)
                                     ,
                                     trellis::binding<IRepository, SqlRepository, trellis::unique>
#elif defined(CASE_OK)
                                     ,
                                     trellis::binding<IRepository, SqlRepository, trellis::singleton>
#endif
                                     >;
};

#if defined(DEPTH_ROOT)
using root = edge;
#else
using root = App;
#endif

int main() {
#if defined(PRINT_EXPLAIN)
  using X = trellis::explain<app_module::bindings, root>;
  std::printf("error type: %.*s\n", static_cast<int>(s3::type_name<X::type>().size()),
              s3::type_name<X::type>().data());
  std::printf("explain pass: %zu hops, %zu memoized reads\n", X::hops, X::reads);
  std::printf("message:\n%.*s\n", static_cast<int>(trellis::message<X>.size()),
              trellis::message<X>.data());
  return 0;
#else
  trellis::injector<app_module> injector;
#if defined(DEPTH_ROOT)
  [[maybe_unused]] edge e = injector.create<edge>();
  return 0;
#else
  App app = injector.create<App>();
  return app.run();
#endif
#endif
}
