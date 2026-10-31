// compile-fail: a lying member alias trellis_ctor used by two consumers is still one error
// expect-id: TRL-E002
// expect-regex: TRL-E002: type not constructible
// expect-regex: E002<app::service,
// expect-regex: trellis::(v1::)?ctor<app::logger ?&> ?>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// Two class templates read the same declaration, as two nodes of the static engine that need the
// same class will, and a third use comes from main. The last regex allows no second error line.
#include <trellis/discovery/ctor_traits.hpp>

namespace app {

struct config {};
struct logger {};
struct service {
  using trellis_ctor = trellis::ctor<logger&>;
  explicit service(const config& /*settings*/) {}
};

}  // namespace app

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
  [[maybe_unused]] trellis::detail::explicit_ctor_t<app::service>* c = nullptr;
}
