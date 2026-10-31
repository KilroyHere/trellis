// compile-fail: a binding's .with_ctor<Args...>() declares parameter types that no constructor takes
// expect-id: TRL-E002
// expect-regex: TRL-E002: type not constructible
// expect-regex: E002<app::service,
// expect-regex: trellis::(v1::)?ctor<int> ?>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// The class's own member alias is right, but the binding's declaration wins over it, and that one
// says int. The binding DSL comes with T1.4; until then the declaration that a binding's
// .with_ctor<int>() makes is passed to explicit_ctor_t directly, as the DSL will pass it. The
// last regex allows no second error line.
#include <trellis/discovery/ctor_traits.hpp>

namespace app {

struct config {};
struct service {
  using trellis_ctor = trellis::ctor<const config&>;
  explicit service(const config& /*settings*/) {}
};

}  // namespace app

int main() {
  [[maybe_unused]] trellis::detail::explicit_ctor_t<app::service, trellis::ctor<int>>* unused =
      nullptr;
}
