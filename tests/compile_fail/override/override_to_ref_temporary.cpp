// compile-fail: override<K>.to_ref() of a temporary, which would dangle
// expect-regex: to_ref
// expect-regex: delete
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// As bind<I>.to_ref() (tests/compile_fail/dsl/bind_to_ref_temporary.cpp), the override would
// hold the address of an object destroyed at the end of the statement, so its overload for
// rvalues is deleted.
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
  [[maybe_unused]] auto replacement = trellis::override<app::settings>.to_ref(app::settings{});
}
