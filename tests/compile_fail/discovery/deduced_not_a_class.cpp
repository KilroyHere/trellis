// compile-fail: deduction is asked to construct a type that isn't a class
// expect-id: TRL-E002
// expect-regex: TRL-E002: type not constructible: deduction needs a class type
// expect-regex: E002<int, trellis::(v1::)?detail::no_ctor_declared ?>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-regex-msvc: TRL-E002: type not constructible: deduction needs a class type
// expect-regex-msvc: E002<T,trellis::v1::detail::no_ctor_declared>[^\n]*\n *with\n *\[\n(?: +\w+=[^\n]*\n)*? +T=int(?:,|$)
// expect-regex-msvc: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// A fundamental type, a pointer, an enumeration or a union has no constructor to deduce: the
// injector needs a value for it (spike S1 §3.5). The entry point is used twice, and the last regex
// allows no second error line.
#include <trellis/discovery/deduced.hpp>

int main() {
  [[maybe_unused]] trellis::detail::discovered_ctor_t<int>* first = nullptr;
  [[maybe_unused]] trellis::detail::discovered_ctor_t<const int>* second = nullptr;
}
