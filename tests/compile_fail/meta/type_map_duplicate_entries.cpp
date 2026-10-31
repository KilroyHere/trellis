// compile-fail: building a type_map that lists the same entry twice
// expect-regex: type_map: a key appears in more than one entry
// expect-max-lines: 20
//
// Two identical entries would be the same base class twice, which C++ forbids with an error of
// its own. The map reports the repeated key instead, once, and doesn't derive from its entries.
#include <trellis/meta/type_map.hpp>

namespace {

struct key {};
using map =
    trellis::detail::type_map<trellis::detail::entry<key, int>, trellis::detail::entry<key, int>>;

}  // namespace

int main() { [[maybe_unused]] map instance; }
