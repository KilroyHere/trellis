// expect: TRL-E001
#include "../injector_stub.hpp"
struct iface {
  virtual ~iface() = default;
  virtual void f() = 0;
};
int main() {
  s1::injector<> inj;
  auto p = inj.create<std::unique_ptr<iface>>();
}
