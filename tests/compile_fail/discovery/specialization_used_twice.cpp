// compile-fail: a lying ctor_traits specialization used by two consumers is still one error
// expect-id: TRL-E002
// expect-regex: TRL-E002: type not constructible
// expect-regex: E002<app::service,
// expect-regex: trellis::(v1::)?ctor<int> ?>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// The T1.2 review's probe. Two class templates read the same declaration, as two nodes of the
// static engine that need the same class will. When the check was a class template, Clang marked
// its specialization invalid after the failed assertion and reported "no type named 'type'" once
// more for the second consumer. The last regex allows no second error line.
#include <trellis/discovery/ctor_traits.hpp>

namespace app {

struct config {};
struct service {
  explicit service(const config& /*settings*/) {}
};

}  // namespace app

template <>
struct trellis::ctor_traits<app::service> : trellis::ctor<int> {};

template <class T>
struct consumer_a {
  using declared = trellis::detail::explicit_ctor_t<T>;
};
template <class T>
struct consumer_b {
  using declared = trellis::detail::explicit_ctor_t<T>;
};

int main() {
  [[maybe_unused]] consumer_a<app::service>::declared* a = nullptr;
  [[maybe_unused]] consumer_b<app::service>::declared* b = nullptr;
}
