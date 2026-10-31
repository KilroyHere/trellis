// The meta toolkit without exceptions and without RTTI (NFR-11). tests/unit/CMakeLists.txt builds
// this file with -fno-exceptions -fno-rtti, which GoogleTest can't take, so it is a plain program
// that returns 0 when every check passes. It compiles every meta header and runs the code that
// depends on TRELLIS_HAS_EXCEPTIONS: the assignment of result<T>'s fallback.
#include <trellis/meta/type_list.hpp>
#include <trellis/meta/type_map.hpp>
#include <trellis/trellis.hpp>

#include <string>
#include <type_traits>

static_assert(TRELLIS_HAS_EXCEPTIONS == 0);
static_assert(trellis::detail::contains_v<int, trellis::detail::type_list<char, int>>);
static_assert(std::is_same_v<trellis::detail::lookup_t<
                                 int, trellis::detail::type_map<trellis::detail::entry<int, char>>>,
                             char>);
static_assert(trellis::type_name<int>() == "int");
static_assert(trellis::fixed_string{"ok"}.size() == 2);

int main() {
  trellis::result<std::string> target = trellis::make_unexpected(trellis::failure{"kept"});
  const trellis::result<std::string> source = std::string("value");
  const bool failure_first = !target.has_value() && target.error().message() == "kept";
  target = source;  // a value over a failure, through the path without try and catch
  const bool value_after = target.has_value() && *target == "value";
  target = trellis::make_unexpected(trellis::failure{"again"});
  const bool failure_after = !target.has_value() && target.error().message() == "again";
  return failure_first && value_after && failure_after ? 0 : 1;
}
