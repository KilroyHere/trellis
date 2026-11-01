// compile-fail: create_all, twice, on an injector with a singleton that needs an abstract type nothing binds
// expect-id: TRL-E001
// expect-regex: no binding for an abstract type
// expect-regex: E001<.*store, trellis::error::request<.*store ?&>|error C2338:
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
// expect-max-lines-msvc: 125
//
// trellis::testing::create_all requests each singleton as create<Key&>() does (testing/wiring.hpp),
// so a singleton that can't be built is that request's one error, here the cache's store, and the
// singleton after it, which can be built, adds nothing. The requests are instantiated once per
// injector type, so the second call adds no error either.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006).
// The regex on the error type also accepts MSVC's static_assert error, C2338, in its place: MSVC
// prints the literal but not the condition (ADR-0006; untested here, as MSVC runs only in CI).
#include <trellis/testing/wiring.hpp>
#include <trellis/trellis.hpp>

namespace app {

struct store {
  store() = default;
  store(const store&) = delete;
  store& operator=(const store&) = delete;
  virtual ~store() = default;
  virtual void put(int value) = 0;
};
struct cache {
  explicit cache(store& backing) : backing_(&backing) {}
  store* backing_;
};
struct mailer {
  mailer() = default;
};

}  // namespace app

int main() {
  auto injector = trellis::make_injector(trellis::bind<app::cache>.in(trellis::singleton),
                                         trellis::bind<app::mailer>.in(trellis::singleton));
  trellis::testing::create_all(injector);
  trellis::testing::create_all(injector);
}
