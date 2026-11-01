// compile-fail: decltype(override<K>) with a const key, as a type only
// expect-regex: bind<I>: I must be an object type without const, volatile or reference
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// Only the type of override<K> is named, and no override object is ever made: the key is still
// checked, because override<K> is an auto variable, so deducing its type instantiates its
// initializer, whose constructor makes the binding the override wraps (dsl/override.hpp), as for
// bind<I> (tests/compile_fail/dsl/bind_bad_key_decltype.cpp). With the type spelled out,
// decltype(override<K>) would never look at the initializer, and the key would go unchecked.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006:
// one static_assert per root cause).
#include <trellis/trellis.hpp>

namespace app {

struct sensor {
  sensor() = default;
};

}  // namespace app

int main() {
  using const_key = decltype(trellis::override<const app::sensor>);
  [[maybe_unused]] const const_key* none = nullptr;
}
