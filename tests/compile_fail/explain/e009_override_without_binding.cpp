// compile-fail: an override whose key no other binding binds, checked twice through the entry point that trellis::override<K> (T1.7) will call
// expect-id: TRL-E009
// expect-regex: TRL-E009: override without an existing binding
// expect-regex: did you mean trellis::bind\?
// expect-regex: trellis::error::E009<app::clock, ?trellis::(v1::)?binding<app::clock, ?app::fake_clock
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-regex-msvc: TRL-E009: override without an existing binding
// expect-regex-msvc: did you mean trellis::bind\?
// expect-regex-msvc: trellis::error::E009<app::clock,Override>[^\n]*\n *with\n *\[\n(?: +\w+=[^\n]*\n)*? +Override=main::override_binding(?:,|$)
// expect-regex-msvc: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
//
// An override replaces the binding of its key that the injector's other bindings provide (plan
// §6.10), and none of them binds clock: TRL-E009. trellis::override<K> arrives with T1.7, so the
// test drives the check through its entry point, trellis::detail::report_override_target, with a
// plain binding standing for the override. The check runs twice: one error.
//
// The regexes accept MSVC's spelling of template arguments, without a space after the comma; the
// regex that starts with \A accepts only an output with exactly one error line.
#include <trellis/trellis.hpp>

namespace app {

struct logger {};
struct clock {};
struct fake_clock : clock {};

}  // namespace app

int main() {
  using bindings = trellis::detail::type_list<decltype(trellis::bind<app::logger>)>;
  using override_binding = decltype(trellis::bind<app::clock>.to<app::fake_clock>());
  trellis::detail::report_override_target<bindings, override_binding>();
  trellis::detail::report_override_target<bindings, override_binding>();
}
