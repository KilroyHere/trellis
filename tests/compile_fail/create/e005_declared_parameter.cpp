// compile-fail: a declared constructor takes a unique object by reference, which nothing would keep alive
// expect-id: TRL-E005
// expect-regex: nothing would keep the object alive
// expect-regex: E005<.*timer, trellis::error::request<.*timer ?&>, trellis::(v1::)?unique_t>|error C2338:
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
//
// A class with a declared constructor (ctor_traits.hpp) resolves its declared parameter types;
// the failure pass checks each one and reports the request that fails (static/resolve.hpp,
// explain_node). The class is created twice: one error.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006).
// The regex on the error type also accepts MSVC's static_assert error, C2338, in its place: MSVC
// prints the literal but not the condition (ADR-0006; untested here, as MSVC runs only in CI).
#include <trellis/trellis.hpp>

namespace {

struct timer {};
struct scheduler {
  scheduler(int ticks, timer& clock) : ticks_(ticks), clock_(&clock) {}
  int ticks_;
  timer* clock_;
};

}  // namespace

template <>
struct trellis::ctor_traits<scheduler> : trellis::ctor<int, timer&> {};

int main() {
  auto injector = trellis::make_injector(trellis::bind<timer>, trellis::bind<int>.to_value(10));
  [[maybe_unused]] const scheduler first = injector.create<scheduler>();
  [[maybe_unused]] const scheduler second = injector.create<scheduler>();
}
