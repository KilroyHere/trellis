// compile-fail: a missing binding fourteen classes below create<T>(), through deduced and declared classes; NFR-07's 40 lines still hold
// expect-id: TRL-E012
// expect-regex: TRL-E012: unbound concrete type requested by reference, pointer or shared_ptr
// expect-regex: trellis::error::E012<app::sensor, ?trellis::error::request<const app::sensor ?&> ?>
// expect-regex: trellis::error::path<app::c0, ?app::c1, ?app::c2, ?app::c3, ?app::c4, ?app::c5, ?app::c6, ?app::c7, ?app::c8, ?app::c9, ?app::c10, ?app::c11, ?app::c12, ?app::c13>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-regex-msvc: TRL-E012: unbound concrete type requested by reference, pointer or shared_ptr
// expect-regex-msvc: trellis::error::E012<app::sensor,trellis::error::request<R>>[^\n]*\n *with\n *\[\n(?: +\w+=[^\n]*\n)*? +R=const app::sensor &(?:,|$)
// expect-regex-msvc: trellis::error::path<app::c0, ?app::c1, ?app::c2, ?app::c3, ?app::c4, ?app::c5, ?app::c6, ?app::c7, ?app::c8, ?app::c9, ?app::c10, ?app::c11, ?app::c12, ?app::c13>
// expect-regex-msvc: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
// expect-max-lines-msvc: 380
//
// Each deduced class on the failing path costs the error one instantiation note, from one line of
// explain.hpp, and a declared class none (docs/internals/explain-pass.md): Clang prints that
// line's source once for a run of such notes, and GCC folds the run into one "recursively required
// from" line, so the output doesn't grow with the path. The note of explain_cause, the step that
// reports, names the whole path, path<c0, ..., c13>. c10 and c12 declare their constructors; the
// others are deduced. The root is created twice: one error.
//
// The regexes accept MSVC's spelling of template arguments, without a space after the comma; the
// regex that starts with \A accepts only an output with exactly one error line.
#include <trellis/trellis.hpp>

namespace app {

struct sensor {};
struct c13 {
  explicit c13(const sensor& input) : input_(&input) {}
  const sensor* input_;
};
struct c12 {
  using trellis_ctor = trellis::ctor<c13>;
  explicit c12(c13 next) : next_(next) {}
  c13 next_;
};
struct c11 {
  explicit c11(c12 next) : next_(next) {}
  c12 next_;
};
struct c10 {
  using trellis_ctor = trellis::ctor<c11>;
  explicit c10(c11 next) : next_(next) {}
  c11 next_;
};
struct c9 {
  explicit c9(c10 next) : next_(next) {}
  c10 next_;
};
struct c8 {
  explicit c8(c9 next) : next_(next) {}
  c9 next_;
};
struct c7 {
  explicit c7(c8 next) : next_(next) {}
  c8 next_;
};
struct c6 {
  explicit c6(c7 next) : next_(next) {}
  c7 next_;
};
struct c5 {
  explicit c5(c6 next) : next_(next) {}
  c6 next_;
};
struct c4 {
  explicit c4(c5 next) : next_(next) {}
  c5 next_;
};
struct c3 {
  explicit c3(c4 next) : next_(next) {}
  c4 next_;
};
struct c2 {
  explicit c2(c3 next) : next_(next) {}
  c3 next_;
};
struct c1 {
  explicit c1(c2 next) : next_(next) {}
  c2 next_;
};
struct c0 {
  explicit c0(c1 next) : next_(next) {}
  c1 next_;
};

}  // namespace app

int main() {
  auto injector = trellis::make_injector();
  [[maybe_unused]] const app::c0 first = injector.create<app::c0>();
  [[maybe_unused]] const app::c0 second = injector.create<app::c0>();
}
