// compile-fail: options{.eager = true} and a singleton whose missing binding is eight classes down; make_injector reports it once, within NFR-07's 40 lines
// expect-id: TRL-E012
// expect-regex: TRL-E012: unbound concrete type requested by reference, pointer or shared_ptr
// expect-regex: trellis::error::E012<app::sensor, ?trellis::error::request<app::sensor ?&> ?>
// expect-regex: trellis::error::path<app::hub, ?app::d1, ?app::d2, ?app::d3, ?app::d4, ?app::d5, ?app::d6, ?app::d7>
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-regex-msvc: TRL-E012: unbound concrete type requested by reference, pointer or shared_ptr
// expect-regex-msvc: trellis::error::E012<app::sensor,trellis::error::request<R>>[^\n]*\n *with\n *\[\n(?: +\w+=[^\n]*\n)*? +R=app::sensor &(?:,|$)
// expect-regex-msvc: trellis::error::path<app::hub, ?app::d1, ?app::d2, ?app::d3, ?app::d4, ?app::d5, ?app::d6, ?app::d7>
// expect-regex-msvc: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
// expect-max-lines-msvc: 302
//
// An eager injector builds its singletons in make_injector, and hub can't be built. make_injector
// runs the explain pass for it before the injector exists (T1.6), so that make_injector is the
// only frame outside the explain pass, as create<T>() is for a request, and the notes of the
// deduced classes on the path fold as in deep_chain_through_declared.cpp. The injector is made
// twice: one error.
//
// The regexes accept MSVC's spelling of template arguments, without a space after the comma; the
// regex that starts with \A accepts only an output with exactly one error line.
#include <trellis/trellis.hpp>

namespace app {

struct sensor {};
struct d7 {
  explicit d7(sensor& input) : input_(&input) {}
  sensor* input_;
};
struct d6 {
  explicit d6(d7 next) : next_(next) {}
  d7 next_;
};
struct d5 {
  explicit d5(d6 next) : next_(next) {}
  d6 next_;
};
struct d4 {
  explicit d4(d5 next) : next_(next) {}
  d5 next_;
};
struct d3 {
  explicit d3(d4 next) : next_(next) {}
  d4 next_;
};
struct d2 {
  explicit d2(d3 next) : next_(next) {}
  d3 next_;
};
struct d1 {
  explicit d1(d2 next) : next_(next) {}
  d2 next_;
};
struct hub {
  explicit hub(d1 next) : next_(next) {}
  d1 next_;
};

constexpr trellis::options eager{.eager = true};

}  // namespace app

int main() {
  const auto hub = trellis::bind<app::hub>.in(trellis::singleton);
  [[maybe_unused]] auto first = trellis::make_injector<app::eager>(hub);
  [[maybe_unused]] auto second = trellis::make_injector<app::eager>(hub);
}
