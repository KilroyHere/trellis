// compile-fail: a type that is only declared, bound with .to_ptr(p).allow_copy() and requested by value, twice; one error
// expect-id: TRL-E002
// expect-regex: TRL-E002: type not constructible: a by-value request takes a copy of the bound object
// expect-regex: trellis::error::E002<app::opaque, ?trellis::error::request<app::opaque> ?>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
//
// A binding of an object whose type is only declared serves its pointer and reference requests
// (tests/unit/create_test.cpp, IncompleteTypesBoundByPointerOrReference). A by-value request
// takes a copy, which an incomplete type can't give: TRL-E002 for the copy, once. Before T1.6's
// review, the plan asked std::is_constructible of the incomplete type for every request of the
// binding, by value or not, which is a compiler error of its own (T1.7's finding).
//
// The regexes accept MSVC's spelling of template arguments, without a space after the comma; the
// regex that starts with \A accepts only an output with exactly one error line.
#include <trellis/trellis.hpp>

namespace app {

struct opaque;  // only declared, as a host application's handle type would be
opaque* host_object();

}  // namespace app

int main() {
  auto injector =
      trellis::make_injector(trellis::bind<app::opaque>.to_ptr(app::host_object()).allow_copy());
  injector.create<app::opaque>();
  injector.create<app::opaque>();
}
