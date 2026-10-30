// compile-fail: sample: a static_assert reports a Trellis-style error ID and names the type
// expect-id: TRL-E000
// expect-regex: static assertion failed
// expect-regex: widget
// expect-max-lines: 20
//
// The runner's sample, not a library diagnostic. TRL-E000 is a placeholder ID that only tooling
// samples use; the library's error IDs start at TRL-E001 (plan §7.8).
#include <trellis/trellis.hpp>

namespace sample {

template <class T>
inline constexpr bool always_false = false;

template <class T>
void require_binding() {
  static_assert(always_false<T>, "trellis [TRL-E000]: sample diagnostic: no binding for T");
}

struct widget {};

}  // namespace sample

int main() { sample::require_binding<sample::widget>(); }
