// compile-fail: a by-value parameter two classes below create<T>() takes a copy of an .allow_copy() singleton whose class can't be copied; created twice
// expect-id: TRL-E002
// expect-regex: TRL-E002: type not constructible: a by-value request takes a copy of the bound object
// expect-regex: trellis::error::E002<app::pool, ?trellis::error::request<app::pool> ?>
// expect-regex: trellis::error::path<app::server, ?app::worker>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
//
// .allow_copy() lets a singleton be requested by value, as a copy (plan §6.3), and pool can't be
// copied: TRL-E002 for the request pool, which worker makes, two classes below create<server>().
// The explain pass names the path, path<server, worker>, in the note of explain_cause. The
// root is created twice: one error.
//
// The regexes accept MSVC's spelling of template arguments, without a space after the comma; the
// regex that starts with \A accepts only an output with exactly one error line.
#include <trellis/trellis.hpp>

namespace app {

struct pool {
  pool() = default;
  pool(const pool&) = delete;
  pool& operator=(const pool&) = delete;
};
struct worker {
  explicit worker(pool /*connections*/) {}
};
struct server {
  explicit server(worker& main) : main_(&main) {}
  worker* main_;
};

}  // namespace app

int main() {
  auto injector =
      trellis::make_injector(trellis::bind<app::pool>.in(trellis::singleton).allow_copy(),
                             trellis::bind<app::worker>.in(trellis::singleton));
  [[maybe_unused]] const app::server first = injector.create<app::server>();
  [[maybe_unused]] const app::server second = injector.create<app::server>();
}
