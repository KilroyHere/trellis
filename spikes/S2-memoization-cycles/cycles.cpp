// Spike S2: dependency cycles in deduced mode. Every case must fail to compile;
// the spike records what each compiler says (s2.py cycles).
//
//   -DCASE=1  A -> B -> A
//   -DCASE=2  A -> B -> C -> D -> E -> A
//   -DCASE=3  A -> A (A takes A*)
//   -DCASE=4  App -> X -> A -> B -> A (a cycle below the root)
//   -DSTUB=1  spike S1's injector_stub.hpp, as written
//   -DSTUB=2  this spike's stub.hpp, with -DS2_STRATEGY=0, 1 or 2
//
// All classes are singletons requested by reference, the usual shape of a real
// cycle (two services that know each other).
#include <cstdio>

#if STUB == 1
#include "injector_stub.hpp"
#else
#include "stub.hpp"
#endif

struct A;
struct B;
struct C;
struct D;
struct E;
struct X;

#if CASE == 1
struct A {
  explicit A(B&) {}
};
struct B {
  explicit B(A&) {}
};
using root = A;
#if STUB == 1
using injector_t = s1::injector<s1::singleton<A>, s1::singleton<B>>;
#else
struct app : s2::map<s2::singleton<A>, s2::singleton<B>> {};
#endif
#elif CASE == 2
struct A {
  explicit A(B&) {}
};
struct B {
  explicit B(C&) {}
};
struct C {
  explicit C(D&) {}
};
struct D {
  explicit D(E&) {}
};
struct E {
  explicit E(A&) {}
};
using root = A;
#if STUB == 1
using injector_t = s1::injector<s1::singleton<A>, s1::singleton<B>, s1::singleton<C>,
                                s1::singleton<D>, s1::singleton<E>>;
#else
struct app : s2::map<s2::singleton<A>, s2::singleton<B>, s2::singleton<C>, s2::singleton<D>,
                     s2::singleton<E>> {};
#endif
#elif CASE == 3
struct A {
  explicit A(A*) {}
};
using root = A;
#if STUB == 1
using injector_t = s1::injector<s1::singleton<A>>;
#else
struct app : s2::map<s2::singleton<A>> {};
#endif
#elif CASE == 4
struct A {
  explicit A(B&) {}
};
struct B {
  explicit B(A&) {}
};
struct X {
  explicit X(A&) {}
};
struct App {
  explicit App(X&) {}
};
using root = App;
#if STUB == 1
using injector_t =
    s1::injector<s1::singleton<App>, s1::singleton<X>, s1::singleton<A>, s1::singleton<B>>;
#else
struct app : s2::map<s2::singleton<App>, s2::singleton<X>, s2::singleton<A>, s2::singleton<B>> {};
#endif
#else
#error "CASE must be 1 to 4"
#endif

#if STUB != 1
using injector_t = s2::injector<app>;
#endif

int main() {
  injector_t inj;
  root& r = inj.create<root&>();
  std::printf("%p\n", static_cast<void*>(&r));
  return 0;
}
