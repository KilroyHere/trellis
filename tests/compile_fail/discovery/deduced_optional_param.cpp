// compile-fail: deduction meets a parameter of type std::optional<T>, which needs the explicit route
// expect-id: TRL-E002
// expect-regex: TRL-E002: type not constructible: deduction found no public, non-deleted, unambiguous constructor
// expect-regex: E002<app::client, trellis::(v1::)?detail::no_ctor_declared ?>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// std::optional's converting constructor template accepts the placeholder itself, which competes
// with the placeholder's conversion operator, ambiguously, on every compiler (spike S1 §3.2 note
// c), GCC 13 included since T1.3's probe workaround. std::tuple and std::any parameters behave the
// same. The entry point is used twice, and the last regex allows no second error line.
#include <trellis/discovery/deduced.hpp>

#include <optional>

namespace app {

struct proxy {};
struct client {
  explicit client(std::optional<proxy> /*via*/) {}
};

}  // namespace app

int main() {
  [[maybe_unused]] trellis::detail::discovered_ctor_t<app::client>* first = nullptr;
  [[maybe_unused]] trellis::detail::discovered_ctor_t<app::client>* second = nullptr;
}
