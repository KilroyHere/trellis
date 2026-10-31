// compile-fail: a lying .with_ctor<Args...>() declaration used by two consumers is still one error
// expect-id: TRL-E002
// expect-regex: TRL-E002: type not constructible
// expect-regex: E002<app::service,
// expect-regex: trellis::(v1::)?ctor<int> ?>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// The binding's declaration, passed as the binding DSL (T1.4) will pass it, read by two class
// templates and once more from main; the class's own member alias is right but loses to the
// binding. The last regex allows no second error line.
#include <trellis/discovery/ctor_traits.hpp>

namespace app {

struct config {};
struct service {
  using trellis_ctor = trellis::ctor<const config&>;
  explicit service(const config& /*settings*/) {}
};
using binding_declaration = trellis::ctor<int>;

}  // namespace app

template <class T>
struct consumer_a {
  using declared = trellis::detail::explicit_ctor_t<T, app::binding_declaration>;
};
template <class T>
struct consumer_b {
  using declared = trellis::detail::explicit_ctor_t<T, app::binding_declaration>;
};

int main() {
  [[maybe_unused]] consumer_a<app::service>::declared* a = nullptr;
  [[maybe_unused]] consumer_b<app::service>::declared* b = nullptr;
  [[maybe_unused]] trellis::detail::explicit_ctor_t<app::service, app::binding_declaration>* c =
      nullptr;
}
