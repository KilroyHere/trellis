// expect: TRL-E002
#include "../injector_stub.hpp"
struct dep {};
class hidden {
  hidden(dep) {}
};
int main() {
  s1::injector<> inj;
  auto h = inj.create<hidden>();
}
