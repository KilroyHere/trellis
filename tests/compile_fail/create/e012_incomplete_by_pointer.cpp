// compile-fail: a forward-declared, unbound type taken by pointer, by two constructor parameters
// expect-id: TRL-E012
// expect-regex: unbound concrete type requested by reference, pointer or shared_ptr
// expect-regex: E012<.*database, trellis::error::request<.*database ?\*> ?>|error C2338:
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
// expect-max-lines-msvc: 102
//
// database is only declared, as in a header that holds a pointer to it. The check asks whether an
// unbound key is abstract only once it knows the key is complete (static/validate.hpp,
// complete_abstract), so the request goes on to the table, which rejects a pointer to an unbound
// type: TRL-E012, and no error from std::is_abstract on an incomplete class (T1.5's review). Two
// parameters make the same request: one error.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006).
// The regex on the error type also accepts MSVC's static_assert error, C2338, in its place: MSVC
// prints the literal but not the condition (ADR-0006; untested here, as MSVC runs only in CI).
#include <trellis/trellis.hpp>

namespace {

struct database;
struct user {
  user(database* primary, database* replica) : primary_(primary), replica_(replica) {}
  database* primary_;
  database* replica_;
};

}  // namespace

int main() {
  auto injector = trellis::make_injector();
  [[maybe_unused]] const user first = injector.create<user>();
}
