// expect: TRL-E004
#include "../injector_stub.hpp"
struct greedy {
  template <class U>
  greedy(U&&) {}
};
int main() {
  s1::injector<> inj;
  auto g = inj.create<greedy>();
}
