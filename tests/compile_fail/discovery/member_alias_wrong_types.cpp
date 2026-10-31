// compile-fail: a member alias trellis_ctor declares parameter types that no constructor takes
// expect-id: TRL-E002
// expect-regex: TRL-E002: type not constructible
// expect-regex: E002<app::service,
// expect-regex: trellis::(v1::)?ctor<app::logger ?&> ?>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// The alias says logger& where the only constructor takes a const config&. The last regex allows
// no second error line.
#include <trellis/discovery/ctor_traits.hpp>

namespace app {

struct config {};
struct logger {};
struct service {
  using trellis_ctor = trellis::ctor<logger&>;
  explicit service(const config& /*settings*/) {}
};

}  // namespace app

int main() { [[maybe_unused]] trellis::detail::explicit_ctor_t<app::service>* unused = nullptr; }
