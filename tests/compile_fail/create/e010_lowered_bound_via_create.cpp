// compile-fail: options{.max_ctor_arity = 2} and a class whose constructor takes 3 parameters, created twice
// expect-id: TRL-E010
// expect-regex: constructor arity above max_ctor_arity
// expect-regex: E010<.*triple, 2>|error C2338:
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
//
// create<T>() passes the injector's max_ctor_arity to discovery as its bound (plan §6.13,
// ADR-0003), so a constructor just above it is TRL-E010, which names the bound: raise it, or
// declare the constructor.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006).
// The regex on the error type also accepts MSVC's static_assert error, C2338, in its place: MSVC
// prints the literal but not the condition (ADR-0006; untested here, as MSVC runs only in CI).
#include <trellis/trellis.hpp>

namespace {

struct triple {
  triple(int first, int second, int third) : sum(first + second + third) {}
  int sum;
};

}  // namespace

int main() {
  auto injector =
      trellis::make_injector<trellis::options{.max_ctor_arity = 2}>(trellis::bind<int>.to_value(1));
  [[maybe_unused]] const triple first = injector.create<triple>();
  [[maybe_unused]] const triple second = injector.create<triple>();
}
