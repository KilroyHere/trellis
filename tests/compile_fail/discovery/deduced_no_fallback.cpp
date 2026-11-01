// compile-fail: the discovered constructor has a parameter the context can't serve; no shorter constructor is used instead
// expect-regex: (use of deleted function|invokes a deleted function)
// expect-regex: (T = const app::store|to 'const (app::)?store')
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-regex-msvc: error C2280: 'trellis::v1::detail::any_arg<app::consumer,Ctx>::operator T &\(void\) const<const app::store>': attempting to reference a deleted function
// expect-regex-msvc: error C2280: [^\n]*[^\n]*\n *with\n *\[\n(?: +\w+=[^\n]*\n)*? +T=const app::store(?:,|$)
// expect-regex-msvc: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 30
//
// Discovery fixes the arity at 1 before any resolution (ADR-0003 item 2), so a const store& that
// the context won't serve, as a unique store would be refused (plan §6.3), makes the construction
// fail on the deleted twin of operator T&(), which names the request; the default constructor is
// never chosen instead. The engine will check this before it builds (T1.5), and its explain pass
// will report the parameter with an ID (T1.6); building directly gives the compiler's error.
#include <trellis/discovery/deduced.hpp>

#include <type_traits>

namespace app {

struct store {};
struct consumer {
  consumer() = default;
  explicit consumer(const store& /*data*/) {}
};
// Serves classes by value only, as for unique bindings.
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
  constexpr auto arity = trellis::detail::deduction<app::consumer>::arity;
  [[maybe_unused]] const app::consumer built =
      trellis::detail::construct_deduced<app::consumer, arity>(context);
}
