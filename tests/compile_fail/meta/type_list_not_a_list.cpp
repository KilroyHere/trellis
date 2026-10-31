// compile-fail: contains_v of something that is not a type_list
// expect-regex: expected a trellis::detail::type_list<\.\.\.>
// expect-max-lines: 20
#include <trellis/meta/type_list.hpp>

namespace {

template <class... Ts>
struct some_other_list {};

}  // namespace

int main() { return trellis::detail::contains_v<int, some_other_list<int>> ? 1 : 0; }
