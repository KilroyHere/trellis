// compile-fail: an abstract type with no binding as the parameter of a .with_ctor<Args...>() binding, below create<T>(); two roots fail on it
// expect-id: TRL-E001
// expect-regex: TRL-E001: no binding for an abstract type: bind it to an implementation
// expect-regex: trellis::error::E001<app::repository, ?trellis::error::request<std::shared_ptr<app::repository> ?> ?>
// expect-regex: trellis::error::path<app::front, ?app::store>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-regex-msvc: TRL-E001: no binding for an abstract type: bind it to an implementation
// expect-regex-msvc: trellis::error::E001<app::repository,trellis::error::request<R>>[^\n]*\n *with\n *\[\n(?: +\w+=[^\n]*\n)*? +R=std::shared_ptr<app::repository>(?:,|$)
// expect-regex-msvc: trellis::error::path<app::back,app::store>
// expect-regex-msvc: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
// expect-max-lines-msvc: 102
//
// store's binding declares the constructor that takes std::shared_ptr<repository>, whose abstract
// key has no binding; deduction would have found store(int, int), which the binding of int serves.
// The explain pass follows the binding's declaration as it follows a type's own (T1.6): from
// create<front>() through front's deduced parameter store& to store's declared parameter 1. The
// note of explain_cause names the path, path<front, store>, which runs through store although
// store, a declared class, prints no note of its own. back fails on the same request, which is
// reported once (one error per root cause, ADR-0006).
//
// The regexes accept MSVC's spelling of template arguments, without a space after the comma; the
// regex that starts with \A accepts only an output with exactly one error line.
#include <trellis/trellis.hpp>

#include <memory>
#include <utility>

namespace app {

struct repository {
  repository() = default;
  repository(const repository&) = delete;
  repository& operator=(const repository&) = delete;
  virtual ~repository() = default;
  virtual int load() const = 0;
};
struct store {
  explicit store(std::shared_ptr<repository> source) : source_(std::move(source)) {}
  store(int limit, int offset) : limit_(limit + offset) {}
  std::shared_ptr<repository> source_;
  int limit_ = 0;
};
struct front {
  explicit front(store& data) : data_(&data) {}
  store* data_;
};
struct back {
  explicit back(store& data) : data_(&data) {}
  store* data_;
};

}  // namespace app

int main() {
  auto injector = trellis::make_injector(
      trellis::bind<int>.to_value(3),
      trellis::bind<app::store>.with_ctor<std::shared_ptr<app::repository>>().in(
          trellis::singleton));
  [[maybe_unused]] const app::front first = injector.create<app::front>();
  [[maybe_unused]] const app::back second = injector.create<app::back>();
}
