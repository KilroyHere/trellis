// compile-fail: a ctor_traits specialization declares parameter types that no constructor takes
// expect-id: TRL-E002
// expect-regex: TRL-E002: type not constructible
// expect-regex: E002<app::service,
// expect-regex: trellis::(v1::)?ctor<int> ?>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// The specialization says int where the only constructor takes a const config&, so the
// declaration lies (plan §7.2). The last regex allows no second error line: the assertion is the
// one error, and the declaration still names a type afterwards, so nothing else fails. Until the
// injector exists (T1.5), the check is reached through explicit_ctor_t, the engine's way into
// explicit discovery.
#include <trellis/discovery/ctor_traits.hpp>

namespace app {

struct config {};
struct service {
  explicit service(const config& /*settings*/) {}
};

}  // namespace app

template <>
struct trellis::ctor_traits<app::service> : trellis::ctor<int> {};

int main() { [[maybe_unused]] trellis::detail::explicit_ctor_t<app::service>* unused = nullptr; }
