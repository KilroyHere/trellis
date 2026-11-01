// compile-fail: an abstract type with no binding two classes below create<T>(), through a declared singleton; two roots fail on it
// expect-id: TRL-E001
// expect-regex: TRL-E001: no binding for an abstract type: bind it to an implementation
// expect-regex: trellis::error::E001<app::repository, ?trellis::error::request<std::shared_ptr<app::repository> ?> ?>
// expect-regex: trellis::error::path<app::front, ?app::store>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
//
// The explain pass (T1.6) follows the first failing request from create<front>() down: front's
// deduced parameter store&, then store's declared parameter std::shared_ptr<repository>, whose
// abstract key has no binding. The note of explain_cause names the path, path<front, store>,
// which runs through store although store, a declared class, prints no note of its own. back
// fails on the same request, which is reported once (one error per root cause, ADR-0006).
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
  using trellis_ctor = trellis::ctor<std::shared_ptr<repository>>;
  explicit store(std::shared_ptr<repository> source) : source_(std::move(source)) {}
  std::shared_ptr<repository> source_;
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
  auto injector = trellis::make_injector(trellis::bind<app::store>.in(trellis::singleton));
  [[maybe_unused]] const app::front first = injector.create<app::front>();
  [[maybe_unused]] const app::back second = injector.create<app::back>();
}
