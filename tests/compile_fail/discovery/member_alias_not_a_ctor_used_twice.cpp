// compile-fail: a member alias trellis_ctor that names no ctor, used by two consumers, is one error
// expect-regex: a member alias trellis_ctor must name trellis::ctor<Args\.\.\.>
// expect-regex: member_ctor<app::service>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 25
//
// The check stays a class template, member_ctor, because the primary ctor_traits<T> reads its
// type once per class, however often the class is used; Clang would report every later read of
// a failed specialization's member as an error of its own. Two class templates read the
// declaration, checked and unchecked, and main a third time. The message has no ID: plan §7.8
// gives this mistake none (an open question for the owner). The last regex allows no second
// error line.
#include <trellis/discovery/ctor_traits.hpp>

namespace app {

struct config {};
struct service {
  using trellis_ctor = void(const config&);
  explicit service(const config& /*settings*/) {}
};

}  // namespace app

template <class T>
struct consumer_a {
  using declared = trellis::detail::explicit_ctor_t<T>;
};
template <class T>
struct consumer_b {
  using declared = trellis::detail::declared_ctor_t<T>;
};

int main() {
  [[maybe_unused]] consumer_a<app::service>::declared* a = nullptr;
  [[maybe_unused]] consumer_b<app::service>::declared* b = nullptr;
  [[maybe_unused]] trellis::detail::explicit_ctor_t<app::service>* c = nullptr;
}
