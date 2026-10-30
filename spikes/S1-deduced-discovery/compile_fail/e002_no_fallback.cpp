// expect: TRL-E002
// The const dep& parameter of a unique dep is not resolvable. The default
// constructor is not used instead, and no temporary is bound.
#include "../injector_stub.hpp"
struct dep {};
struct consumer {
  consumer() {}
  explicit consumer(const dep& d) : held(&d) {}
  const dep* held = nullptr;
};
int main() {
  s1::injector<> inj;
  auto c = inj.create<consumer>();
}
