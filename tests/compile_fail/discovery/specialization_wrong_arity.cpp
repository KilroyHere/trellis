// compile-fail: a ctor_traits specialization declares more parameters than any constructor takes
// expect-id: TRL-E002
// expect-regex: TRL-E002: type not constructible
// expect-regex: E002<app::service,
// expect-regex: trellis::(v1::)?ctor<const app::config ?&, int> ?>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// The first parameter type is right, but the only constructor takes one parameter, not two. The
// last regex allows no second error line.
#include <trellis/discovery/ctor_traits.hpp>

namespace app {

struct config {};
struct service {
  explicit service(const config& /*settings*/) {}
};

}  // namespace app

template <>
struct trellis::ctor_traits<app::service> : trellis::ctor<const app::config&, int> {};

int main() { [[maybe_unused]] trellis::detail::explicit_ctor_t<app::service>* unused = nullptr; }
