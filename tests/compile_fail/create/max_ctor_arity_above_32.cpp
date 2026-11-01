// compile-fail: options{.max_ctor_arity = 33}, above the largest bound deduced discovery supports; reported once, by make_injector
// expect-regex: options::max_ctor_arity is at most 32
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 20
//
// make_injector checks the bound once (plan §6.13: at most 32), and the engine then deduces with
// the bound clamped to 32, so the deduced classes that create<T>() discovers don't report it
// again (static/injector.hpp, effective_max_ctor_arity). Plan §7.8 has no ID for a misconfigured
// option, so the message has none, like discovery's own check of the bound (T1.3; "Waiting for
// owner" in docs/PROGRESS.md asks about IDs for such misuse).
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006).
#include <trellis/trellis.hpp>

namespace {

struct widget {
  widget() = default;
};
struct gadget {
  explicit gadget(widget /*part*/) {}
};

}  // namespace

int main() {
  auto injector = trellis::make_injector<trellis::options{.max_ctor_arity = 33}>();
  [[maybe_unused]] const gadget built = injector.create<gadget>();
}
