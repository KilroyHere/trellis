// compile-fail: deduction meets an aggregate with an array member
// expect-id: TRL-E004
// expect-regex: TRL-E004: ambiguous constructor: an aggregate with an array member
// expect-regex: E004<app::ports>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// GCC and Clang would count the array's two elements as two fields and MSVC the array as one
// (spike S1's MSVC addendum, T1.13), so deduction refuses the aggregate on every compiler, with the
// same error. The entry point is used twice, and the last regex allows no second error line.
#include <trellis/discovery/deduced.hpp>

namespace app {

struct endpoint {};
struct ports {
  int numbers[2];  // NOLINT(modernize-avoid-c-arrays): the case under test
  endpoint where;
};

}  // namespace app

int main() {
  [[maybe_unused]] trellis::detail::discovered_ctor_t<app::ports>* first = nullptr;
  [[maybe_unused]] trellis::detail::discovered_ctor_t<app::ports>* second = nullptr;
}
