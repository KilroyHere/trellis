// compile-fail: deduction finds a constructor only above the default bound of 16 parameters
// expect-id: TRL-E010
// expect-regex: TRL-E010: constructor arity above max_ctor_arity
// expect-regex: E010<app::wide, 16(UL)?>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// The constructor takes 17 parameters; deduction probes 16 at most by default (ADR-0003 item 5),
// then the 16 arities above, only to tell this case from a missing constructor. The entry point is
// used twice, and the last regex allows no second error line.
#include <trellis/discovery/deduced.hpp>

namespace app {

struct part {};
struct wide {
  wide(part, part, part, part, part, part, part, part, part, part, part, part, part, part, part,
       part, part) {}
};

}  // namespace app

int main() {
  [[maybe_unused]] trellis::detail::discovered_ctor_t<app::wide>* first = nullptr;
  [[maybe_unused]] trellis::detail::discovered_ctor_t<app::wide>* second = nullptr;
}
