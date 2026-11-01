// compile-fail: a defaulted parameter is injected, so a context that can't serve it can't build the class
// expect-regex: (use of deleted function|invokes a deleted function)
// expect-regex: (T = int|to 'int')
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-regex-msvc: error C2280: 'trellis::v1::detail::any_arg<app::pool,Ctx>::operator T\(void\)<int>': attempting to reference a deleted function
// expect-regex-msvc: error C2280: [^\n]*[^\n]*\n *with\n *\[\n(?: +\w+=[^\n]*\n)*? +T=int(?:,|$)
// expect-regex-msvc: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 30
//
// Deduction counts the defaulted int, so the arity is 2 and the int is requested, never left to
// its default (ADR-0003 item 7); the deleted twin of operator T() names the request. Declare the
// constructor without it, ctor<store>, to keep the default. The engine will check this before it
// builds (T1.5), and its explain pass will report the parameter with an ID (T1.6); building
// directly gives the compiler's error.
#include <trellis/discovery/deduced.hpp>

#include <type_traits>

namespace app {

struct store {};
struct pool {
  explicit pool(store /*data*/, int count = 4) : size(count) {}
  int size;
};
// Serves classes by value only, as for unique bindings: no int.
struct by_value_context {
  template <class R>
  static constexpr bool can_resolve = std::is_class_v<R>;
  template <class R>
  R resolve() {
    return R{};
  }
};

}  // namespace app

int main() {
  app::by_value_context context;
  static_assert(trellis::detail::deduction<app::pool>::arity == 2);
  [[maybe_unused]] const app::pool built =
      trellis::detail::construct_deduced<app::pool, 2>(context);
}
