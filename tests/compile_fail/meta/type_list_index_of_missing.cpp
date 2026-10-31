// compile-fail: index_of_v of a type that the type_list doesn't contain
// expect-regex: index_of_v<T, List>: T is not one of the types in List
// expect-regex: absent
// expect-max-lines: 20
#include <trellis/meta/type_list.hpp>

namespace {

struct absent {};

}  // namespace

int main() {
  return static_cast<int>(
      trellis::detail::index_of_v<absent, trellis::detail::type_list<int, char>>);
}
