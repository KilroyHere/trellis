// expect: TRL-E005
#include "../injector_stub.hpp"
struct dep {};
int main() {
  s1::injector<> inj;
  const dep& d = inj.create<const dep&>();
  (void)d;
}
