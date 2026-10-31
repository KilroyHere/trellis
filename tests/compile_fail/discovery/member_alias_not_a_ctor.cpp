// compile-fail: a member alias trellis_ctor names a function signature instead of trellis::ctor<...>
// expect-regex: a member alias trellis_ctor must name trellis::ctor<Args\.\.\.>
// expect-regex: member_ctor<app::service>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// Without the check, an alias that names a class other than a ctor would be ignored without a
// word. Plan §7.8 gives this mistake no ID, so the message has none (an open question for the
// owner). The instantiation note names the class; the last regex allows no second error line.
#include <trellis/discovery/ctor_traits.hpp>

namespace app {

struct config {};
struct service {
  using trellis_ctor = void(const config&);
  explicit service(const config& /*settings*/) {}
};

}  // namespace app

int main() { [[maybe_unused]] trellis::detail::explicit_ctor_t<app::service>* unused = nullptr; }
