// compile-fail: at_t with an index past the end of the type_list
// expect-regex: at_t<I, List>: I is not less than the number of types in List
// expect-max-lines: 20
//
// One error: the index is clamped after the assertion, so the element lookup itself (a builtin,
// pack indexing or indexed bases, depending on the compiler) doesn't fail as well.
#include <trellis/meta/type_list.hpp>

int main() {
  [[maybe_unused]] trellis::detail::at_t<3, trellis::detail::type_list<int, char, long>>* unused =
      nullptr;
}
