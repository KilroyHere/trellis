// expect: TRL-E010
#include "../injector_stub.hpp"
struct d {};
struct wide {
  wide(d, d, d, d, d, d, d, d, d, d, d, d, d, d, d, d, d) {}
};
int main() {
  s1::injector<> inj;
  auto w = inj.create<wide>();
}
