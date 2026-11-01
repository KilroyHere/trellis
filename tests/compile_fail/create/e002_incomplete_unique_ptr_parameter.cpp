// compile-fail: std::unique_ptr to a forward-declared type, at the root, as a constructor parameter and as an aggregate member
// expect-id: TRL-E002
// expect-regex: type not constructible: it is incomplete here
// expect-regex: E002<.*database, trellis::(v1::)?detail::no_ctor_declared ?>|error C2338:
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
//
// database is only declared, so a std::unique_ptr<database> request can't be served: discovery
// reports the incomplete class, TRL-E002. Nothing else may fail: no std::unique_ptr<database> can
// be destroyed here, as std::default_delete asserts that database is complete. So create<>()
// returns void rather than the std::unique_ptr (static/injector.hpp, owns_incomplete), and the
// failure pass doesn't build user or holder from placeholders, which would instantiate that
// destructor, but names their requests unevaluated (static/resolve.hpp, owner_arg; T1.5's
// review). holder is a singleton, requested by reference, as a holder by value would be destroyed
// by the caller, with the same assertion. Three uses of the request: one error.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006).
// The regex on the error type also accepts MSVC's static_assert error, C2338, in its place: MSVC
// prints the literal but not the condition (ADR-0006; untested here, as MSVC runs only in CI).
#include <trellis/trellis.hpp>

#include <memory>

namespace {

struct database;
struct user {
  explicit user(std::unique_ptr<database> owned);
};
struct holder {
  std::unique_ptr<database> owned;
};

}  // namespace

int main() {
  auto injector = trellis::make_injector(trellis::bind<holder>.in(trellis::singleton));
  injector.create<std::unique_ptr<database>>();
  [[maybe_unused]] const user first = injector.create<user>();
  [[maybe_unused]] holder& second = injector.create<holder&>();
}
