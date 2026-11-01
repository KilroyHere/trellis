// compile-fail: deduction is asked about a class that is declared but not defined
// expect-id: TRL-E002
// expect-regex: TRL-E002: type not constructible: it is incomplete here
// expect-regex: E002<app::pending, trellis::(v1::)?detail::no_ctor_declared ?>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-regex-msvc: TRL-E002: type not constructible: it is incomplete here
// expect-regex-msvc: E002<T,trellis::v1::detail::no_ctor_declared>[^\n]*\n *with\n *\[\n(?: +\w+=[^\n]*\n)*? +T=app::pending(?:,|$)
// expect-regex-msvc: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// Every probe and type trait that deduction uses needs the class's definition, and without the
// completeness check an incomplete class gave dozens of errors from the standard library. It is
// one TRL-E002 instead, which asks for the definition where Trellis discovers the constructor. The
// entry point is used twice, once with const, and asked once without asserting; the last regex
// allows no second error line.
#include <trellis/discovery/deduced.hpp>

namespace app {

struct pending;  // declared, never defined

}  // namespace app

int main() {
  [[maybe_unused]] trellis::detail::discovered_ctor_t<app::pending>* first = nullptr;
  [[maybe_unused]] trellis::detail::discovered_ctor_t<const app::pending>* second = nullptr;
  [[maybe_unused]] constexpr bool asked = trellis::detail::discoverable_v<app::pending>;
}
