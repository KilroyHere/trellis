// compile-fail: deduction meets an aggregate member that a placeholder can't initialize
// expect-id: TRL-E004
// expect-regex: TRL-E004: ambiguous constructor: deduction can't initialize every member
// expect-regex: E004<app::settings>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-regex-msvc: TRL-E004: ambiguous constructor: deduction can't initialize every member
// expect-regex-msvc: E004<T>[^\n]*\n *with\n *\[\n(?: +\w+=[^\n]*\n)*? +T=app::settings(?:,|$)
// expect-regex-msvc: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// std::optional's converting constructor template accepts the placeholder itself, which is
// ambiguous (spike S1 §3.2 note c), so the placeholders could only initialize host; port and
// retries would be value-initialized without a word. Deduction refuses the aggregate instead. The
// entry point is used twice, and the last regex allows no second error line.
#include <trellis/discovery/deduced.hpp>

#include <optional>
#include <string>

namespace app {

struct settings {
  std::string host;
  std::optional<int> port;
  int retries;
};

}  // namespace app

int main() {
  [[maybe_unused]] trellis::detail::discovered_ctor_t<app::settings>* first = nullptr;
  [[maybe_unused]] trellis::detail::discovered_ctor_t<app::settings>* second = nullptr;
}
