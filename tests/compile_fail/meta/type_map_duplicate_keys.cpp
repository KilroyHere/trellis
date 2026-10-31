// compile-fail: building a type_map whose entries give one key two values
// expect-regex: type_map: a key appears in more than one entry
// expect-max-lines: 20
//
// has_duplicate_keys_v<Map> is true for this map, and asking it is fine (tests/unit); building the
// map is the error, reported once.
#include <trellis/meta/type_map.hpp>

namespace {

struct key {};
using map =
    trellis::detail::type_map<trellis::detail::entry<key, int>, trellis::detail::entry<key, long>>;

}  // namespace

int main() { [[maybe_unused]] map instance; }
