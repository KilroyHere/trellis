// compile-fail: a constructor takes std::shared_ptr of an abstract type that has no binding
// expect-id: TRL-E001
// expect-regex: no binding for an abstract type
// expect-regex: E001<.*IRepository, trellis::error::request<std::shared_ptr<.*IRepository> ?> ?>|error C2338:
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
//
// The missing binding of plan §7.8's example: Service needs an IRepository, which is abstract
// and has no binding, so nothing can implement the request (ADR-0004). create<T>() reports
// TRL-E001 for the request one hop down (T1.6 adds the path). Two roots need Service: one error.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006).
// The regex on the error type also accepts MSVC's static_assert error, C2338, in its place: MSVC
// prints the literal but not the condition (ADR-0006; untested here, as MSVC runs only in CI).
#include <trellis/trellis.hpp>

#include <memory>
#include <utility>

namespace {

struct IRepository {
  IRepository() = default;
  IRepository(const IRepository&) = delete;
  IRepository& operator=(const IRepository&) = delete;
  virtual ~IRepository() = default;
  virtual void save(int value) = 0;
};
struct Service {
  explicit Service(std::shared_ptr<IRepository> repository) : repository_(std::move(repository)) {}
  std::shared_ptr<IRepository> repository_;
};
struct App {
  explicit App(Service& service) : service_(&service) {}
  Service* service_;
};

}  // namespace

int main() {
  auto injector = trellis::make_injector(trellis::bind<Service>.in(trellis::singleton));
  [[maybe_unused]] const App app = injector.create<App>();
  [[maybe_unused]] Service& service = injector.create<Service&>();
}
