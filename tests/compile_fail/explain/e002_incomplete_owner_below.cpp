// compile-fail: a class two classes below create<T>() takes std::unique_ptr of a forward-declared type by value, after another failing parameter; one error, which names the incomplete class
// expect-id: TRL-E002
// expect-regex: TRL-E002: type not constructible: it is incomplete here
// expect-regex: trellis::error::E002<app::database, ?trellis::(v1::)?detail::no_ctor_declared ?>
// expect-regex: trellis::error::path<app::server, ?app::store>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
//
// database is only declared, so store's parameter std::unique_ptr<database> can't be served, and
// store can't be built from placeholders either: the parameter's destructor asserts that database
// is complete. The explain pass reports that parameter, rather than store's first failing one,
// clock& (unbound, TRL-E012), and names its request in an unevaluated construction, so that the
// one error is discovery's TRL-E002 for database, with the path from create<server>(),
// path<server, store>. The root is created twice.
//
// The regexes accept MSVC's spelling of template arguments, without a space after the comma; the
// regex that starts with \A accepts only an output with exactly one error line.
#include <trellis/trellis.hpp>

#include <memory>

namespace app {

struct database;
struct clock {};
struct store {
  store(clock& time, std::unique_ptr<database> db);
};
struct server {
  explicit server(store& data) : data_(&data) {}
  store* data_;
};

}  // namespace app

int main() {
  auto injector = trellis::make_injector(trellis::bind<app::store>.in(trellis::singleton));
  [[maybe_unused]] const app::server first = injector.create<app::server>();
  [[maybe_unused]] const app::server second = injector.create<app::server>();
}
