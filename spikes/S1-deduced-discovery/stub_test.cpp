// Spike S1: the injector stub resolves a small graph, and the deleted-twin
// operators turn form/lifetime mismatches into errors instead of silent
// fallbacks. Exit code 0 means every check passed.
#include <cstdio>
#include <memory>
#include <type_traits>

#include "injector_stub.hpp"

namespace graph {

int config_built = 0, logger_built = 0, clock_live = 0;

struct Config {  // not an aggregate (user-declared constructor): built with Config()
  Config() { ++config_built; }
  int port = 8080;
};
struct Logger {  // singleton, requested as Logger&, const Logger& and Logger*
  explicit Logger(const Config& c) : port(c.port) { ++logger_built; }
  Logger(const Logger&) = default;  // copyable on purpose: by-value requests must still be rejected
  int port;
  int lines = 0;
};
struct Clock {  // unique
  Clock() { ++clock_live; }
  Clock(const Clock&) { ++clock_live; }
  ~Clock() { --clock_live; }
  int now() const { return 42; }
};
struct Repo {
  Repo(Logger& l, std::unique_ptr<Clock> c) : log(l), clock(std::move(c)) { ++log.lines; }
  Logger& log;
  std::unique_ptr<Clock> clock;
};
struct Service {
  Service(Repo r, const Config& c, Logger* l, Clock k) : repo(std::move(r)), cfg(c), log(l), t(k.now()) {}
  Repo repo;
  const Config& cfg;
  Logger* log;
  int t;
};
struct Settings {  // aggregate with a reference member and an owning member
  const Logger& log;
  std::shared_ptr<Clock> clock;
};
struct App {
  App(Service s, std::shared_ptr<Repo> r, const Logger& l, Settings t)
      : svc(std::move(s)), repo(std::move(r)), log(l), settings(std::move(t)) {}
  Service svc;
  std::shared_ptr<Repo> repo;
  const Logger& log;
  Settings settings;
};

// Mismatches that a constraint-only design would silently "resolve".
struct HoldsClockRef {  // const Clock& of a unique Clock: nothing keeps it alive
  explicit HoldsClockRef(const Clock& k) : c(k) {}
  const Clock& c;
};
struct CopiesLogger {  // a singleton by value: would be a hidden copy
  explicit CopiesLogger(Logger g) : l(g) {}
  Logger l;
};

using inj_t = s1::injector<s1::singleton<Config>, s1::singleton<Logger>>;

// The naive placeholder: operators constrained on can_resolve, no deleted twins.
template <class Consumer, class Ctx>
struct naive_arg {
  Ctx& ctx;
  template <class T>
    requires s1::not_self<Consumer, T> && s1::can_resolve<Ctx, T>
  operator T() {
    return ctx.template resolve<T>();
  }
  template <class T>
    requires s1::not_self<Consumer, T> && s1::can_resolve<Ctx, T&>
  operator T&() const {
    return ctx.template resolve<T&>();
  }
};

}  // namespace graph

using namespace graph;

// Discovery found the arities.
static_assert(s1::ctor_info<App>::arity == 4 && s1::ctor_info<Service>::arity == 4);
static_assert(s1::ctor_info<Repo>::arity == 2 && s1::ctor_info<Logger>::arity == 1);
static_assert(s1::ctor_info<Config>::arity == 0 && !s1::ctor_info<Config>::braces);
static_assert(s1::ctor_info<Settings>::arity == 2 && s1::ctor_info<Settings>::braces);
static_assert(s1::ctor_info<Clock>::arity == 0);

// The graph resolves.
static_assert(inj_t::can_resolve<App>);
static_assert(inj_t::can_resolve<Logger&> && inj_t::can_resolve<const Logger&> &&
              inj_t::can_resolve<Logger*>);
static_assert(inj_t::can_resolve<std::unique_ptr<Clock>> && inj_t::can_resolve<Clock>);

// Form/lifetime mismatches are errors, not fallbacks.
static_assert(!inj_t::can_resolve<const Clock&> && !inj_t::can_resolve<Logger>);
static_assert(!inj_t::can_resolve<HoldsClockRef>,
              "const T& of a unique T must not bind a temporary");
static_assert(!inj_t::can_resolve<CopiesLogger>, "a singleton by value must not be copied");
// ...whereas the naive placeholder accepts both, by falling back to the other operator.
static_assert(std::is_constructible_v<HoldsClockRef, naive_arg<HoldsClockRef, inj_t>>);
static_assert(std::is_constructible_v<CopiesLogger, naive_arg<CopiesLogger, inj_t>>);

int failures = 0;
void check(bool ok, const char* what) {
  std::printf("%s %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++failures;
}

int main() {
  {
    inj_t inj;
    App app = inj.create<App>();
    Logger& log = inj.create<Logger&>();
    check(logger_built == 1 && config_built == 1, "one Logger and one Config for the whole graph");
    check(&app.log == &log && app.svc.log == &log && &app.svc.repo.log == &log &&
              &app.repo->log == &log && &app.settings.log == &log,
          "every Logger&, const Logger&, Logger* and aggregate member refers to the singleton");
    check(&app.svc.cfg == &inj.create<const Config&>() && log.port == 8080,
          "Config singleton shared");
    check(app.svc.repo.clock != nullptr && app.repo->clock != nullptr &&
              app.svc.repo.clock.get() != app.repo->clock.get(),
          "unique_ptr<Clock> requests get distinct objects");
    check(app.settings.clock != nullptr && app.settings.clock.use_count() == 1,
          "shared_ptr<Clock> sole owner");
    check(app.svc.t == 42, "Clock by value delivered");
    check(log.lines == 2, "two Repo objects were built (one by value, one in a shared_ptr)");
    check(clock_live == 3, "three Clock objects alive: two in Repo unique_ptrs, one in Settings");
  }
  check(clock_live == 0, "everything destroyed with the injector and the app");
  {
    // The hazard the twins prevent: the naive placeholder binds a temporary.
    inj_t inj;
    HoldsClockRef h(naive_arg<HoldsClockRef, inj_t>{inj});
    check(clock_live == 0, "naive placeholder: HoldsClockRef::c refers to a destroyed temporary");
    (void)h;
  }
  std::printf("%s\n", failures == 0 ? "OK: injector stub" : "FAIL: injector stub");
  return failures == 0 ? 0 : 1;
}
