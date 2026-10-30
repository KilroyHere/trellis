// expect: TRL-E005
#include "../injector_stub.hpp"
struct logger {};
int main() {
  s1::injector<s1::singleton<logger>> inj;
  logger copy = inj.create<logger>();
  (void)copy;
}
