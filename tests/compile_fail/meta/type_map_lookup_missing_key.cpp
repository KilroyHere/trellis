// compile-fail: lookup_t of a key that no entry of the type_map has
// expect-regex: lookup_t<K, Map>: K is not a key of Map
// expect-regex: absent
// expect-max-lines: 20
//
// The toolkit is internal (trellis::detail), so its misuse reports through a plain static_assert
// message, not a catalog ID (plan §7.8 has none for it). The message names lookup_t; the
// instantiation note names the key.
#include <trellis/meta/type_map.hpp>

namespace {

struct present {};
struct absent {};
using map = trellis::detail::type_map<trellis::detail::entry<present, int>>;

}  // namespace

int main() { [[maybe_unused]] trellis::detail::lookup_t<absent, map>* unused = nullptr; }
