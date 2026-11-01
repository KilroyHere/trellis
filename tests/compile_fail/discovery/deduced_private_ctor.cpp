// compile-fail: deduction meets a class whose only constructor is private
// expect-id: TRL-E002
// expect-regex: TRL-E002: type not constructible: deduction found no public, non-deleted, unambiguous constructor
// expect-regex: E002<app::hidden, trellis::(v1::)?detail::no_ctor_declared ?>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// std::is_constructible checks access, so a private constructor looks the same to deduction as a
// missing one (spike S1 §3.5). The entry point is used twice, and the last regex allows no second
// error line.
#include <trellis/discovery/deduced.hpp>

namespace app {

struct store {};
class hidden {
  explicit hidden(store /*data*/) {}
};

}  // namespace app

int main() {
  [[maybe_unused]] trellis::detail::discovered_ctor_t<app::hidden>* first = nullptr;
  [[maybe_unused]] trellis::detail::discovered_ctor_t<app::hidden>* second = nullptr;
}
