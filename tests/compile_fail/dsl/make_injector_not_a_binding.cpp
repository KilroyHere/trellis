// compile-fail: make_injector with an argument that is neither a binding nor a module
// expect-regex: make_module and make_injector take bindings
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// Such an argument contributes no bindings, so this one static_assert is the only error.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006:
// one static_assert per root cause).
#include <trellis/trellis.hpp>

int main() { [[maybe_unused]] auto injector = trellis::make_injector(42); }
