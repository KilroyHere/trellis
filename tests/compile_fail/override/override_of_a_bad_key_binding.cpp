// compile-fail: a binding of a const key in the wiring, and an override of the key without const
// expect-regex: bind<I>: I must be an object type without const, volatile or reference
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// bind<const K> is bind<I>'s key error. For an override's target, such a binding counts as a
// binding of K (dsl/override.hpp, target_keys_of), so override<K> finds it and make_injector adds
// no TRL-E009 to that one error. The override replaces nothing, as no request can reach a binding
// of const K; the binding stays, as it would without the override.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006:
// one static_assert per root cause).
#include <trellis/trellis.hpp>

namespace app {

struct settings {
  int port = 80;
};

}  // namespace app

int main() {
  const app::settings production{};
  app::settings test{};
  [[maybe_unused]] auto injector =
      trellis::make_injector(trellis::bind<const app::settings>.to_ref(production),
                             trellis::override<app::settings>.to_ref(test));
}
