// compile-fail: deduction is asked to construct an abstract class
// expect-id: TRL-E002
// expect-regex: TRL-E002: type not constructible: it is abstract
// expect-regex: E002<app::repository, trellis::(v1::)?detail::no_ctor_declared ?>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-regex-msvc: TRL-E002: type not constructible: it is abstract
// expect-regex-msvc: E002<T,trellis::v1::detail::no_ctor_declared>[^\n]*\n *with\n *\[\n(?: +\w+=[^\n]*\n)*? +T=app::repository(?:,|$)
// expect-regex-msvc: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// An abstract class is never constructible, so deduction checks for it before probing (spike S1
// §3.5). The injector will report TRL-E001 for an abstract type without a binding before it asks
// discovery (T1.6), so discovery sees one only as an implementation type. The entry point is used
// twice, and the last regex allows no second error line.
#include <trellis/discovery/deduced.hpp>

namespace app {

struct repository {
  repository() = default;
  repository(const repository&) = delete;
  repository& operator=(const repository&) = delete;
  virtual ~repository() = default;
  virtual void save() = 0;
};

}  // namespace app

int main() {
  [[maybe_unused]] trellis::detail::discovered_ctor_t<app::repository>* first = nullptr;
  [[maybe_unused]] trellis::detail::discovered_ctor_t<app::repository>* second = nullptr;
}
