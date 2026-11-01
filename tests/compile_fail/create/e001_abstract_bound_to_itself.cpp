// compile-fail: an abstract class with a trellis_ctor alias, bound to itself with bind<I> alone, so nothing implements it
// expect-id: TRL-E001
// expect-regex: no binding for an abstract type: bind<I> alone binds it to itself
// expect-regex: E001<.*IStore, trellis::error::request<.*IStore ?&> ?>|error C2338:
// expect-regex: \A(?![\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: [\s\S]*(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: )
// expect-max-lines: 40
//
// Discovery reports an abstract class as TRL-E002, also through a constructor declaration
// (T1.2, T1.3), but the mistake is a missing implementation, which plan §7.8 calls E001. So
// create<T>() checks for an abstract class before it asks discovery, and reports TRL-E001 with
// or without a declaration (T1.5 decided T1.2's follow-up). The caller and cache's constructor
// both request IStore&: one error.
//
// The regex that starts with \A accepts only an output with exactly one error line (ADR-0006).
// The regex on the error type also accepts MSVC's static_assert error, C2338, in its place: MSVC
// prints the literal but not the condition (ADR-0006; untested here, as MSVC runs only in CI).
#include <trellis/trellis.hpp>

namespace {

struct IStore {
  using trellis_ctor = trellis::ctor<>;
  IStore() = default;
  IStore(const IStore&) = delete;
  IStore& operator=(const IStore&) = delete;
  virtual ~IStore() = default;
  virtual int size() const = 0;
};
struct cache {
  explicit cache(IStore& store) : store_(&store) {}
  IStore* store_;
};

}  // namespace

int main() {
  auto injector = trellis::make_injector(trellis::bind<IStore>.in(trellis::singleton));
  [[maybe_unused]] IStore& store = injector.create<IStore&>();
  [[maybe_unused]] const cache first = injector.create<cache>();
}
