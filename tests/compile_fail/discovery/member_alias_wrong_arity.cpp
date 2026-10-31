// compile-fail: a member alias trellis_ctor declares no parameters, but every constructor takes one
// expect-id: TRL-E002
// expect-regex: TRL-E002: type not constructible
// expect-regex: E002<app::service,
// expect-regex: trellis::(v1::)?ctor<> ?>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// ctor<> declares the default constructor, which the class doesn't have. The last regex allows no
// second error line.
#include <trellis/discovery/ctor_traits.hpp>

namespace app {

struct config {};
struct service {
  using trellis_ctor = trellis::ctor<>;
  explicit service(const config& /*settings*/) {}
};

}  // namespace app

int main() { [[maybe_unused]] trellis::detail::explicit_ctor_t<app::service>* unused = nullptr; }
