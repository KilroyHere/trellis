// compile-fail: deduction meets a class whose only constructor is deleted
// expect-id: TRL-E002
// expect-regex: TRL-E002: type not constructible: deduction found no public, non-deleted, unambiguous constructor
// expect-regex: E002<app::retired, trellis::(v1::)?detail::no_ctor_declared ?>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-regex-msvc: TRL-E002: type not constructible: deduction found no public, non-deleted, unambiguous constructor
// expect-regex-msvc: E002<T,trellis::v1::detail::no_ctor_declared>[^\n]*\n *with\n *\[\n(?: +\w+=[^\n]*\n)*? +T=app::retired(?:,|$)
// expect-regex-msvc: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// A deleted constructor takes part in overload resolution, and choosing it makes the construction
// ill-formed, so it looks the same to deduction as a missing one (spike S1 §3.5). The entry point
// is used twice, and the last regex allows no second error line.
#include <trellis/discovery/deduced.hpp>

namespace app {

struct store {};
struct retired {
  explicit retired(store) = delete;
};

}  // namespace app

int main() {
  [[maybe_unused]] trellis::detail::discovered_ctor_t<app::retired>* first = nullptr;
  [[maybe_unused]] trellis::detail::discovered_ctor_t<app::retired>* second = nullptr;
}
