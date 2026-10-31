// compile-fail: has_duplicate_keys_v of a type that is not a type_map
// expect-regex: expected a trellis::detail::type_map<entry<K, V>\.\.\.>
// expect-max-lines: 20
#include <trellis/meta/type_map.hpp>

namespace {

struct not_a_map {};

}  // namespace

int main() { return trellis::detail::has_duplicate_keys_v<not_a_map> ? 1 : 0; }
