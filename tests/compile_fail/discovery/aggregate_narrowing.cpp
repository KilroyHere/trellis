// compile-fail: a declaration for an aggregate narrows a double into its int member
// expect-id: TRL-E002
// expect-regex: TRL-E002: type not constructible: .*brace-initialize the aggregate
// expect-regex: E002<app::endpoint,
// expect-regex: trellis::(v1::)?ctor<double, double> ?>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// The static engine builds an aggregate with braces (ADR-0003), and the check follows suit, so
// the narrowing double -> int is rejected here, although C++20 accepts it with parentheses. The
// message for an aggregate says so. The last regex allows no second error line.
#include <trellis/discovery/ctor_traits.hpp>

namespace app {

struct endpoint {
  int port;
  double timeout;
};

}  // namespace app

template <>
struct trellis::ctor_traits<app::endpoint> : trellis::ctor<double, double> {};

int main() { [[maybe_unused]] trellis::detail::explicit_ctor_t<app::endpoint>* unused = nullptr; }
