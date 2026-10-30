// Spike S1: what discovery reports for each known limit, and how each is detected.
// Every expectation is a static_assert, so a compiler that behaves differently
// fails the build. The program prints one LIMIT line per case for the write-up.
#include <concepts>
#include <cstdio>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>

#include "injector_stub.hpp"

namespace lim {

struct X {};
struct Y {};

struct plain0 {
  plain0() {}
};
struct plain3 {
  plain3(X, const X&, X*) {}
};
// Copy and move constructors are excluded by not_self, so this is arity 0.
struct copy_move_and_default {
  copy_move_and_default() {}
  copy_move_and_default(const copy_move_and_default&) {}
  copy_move_and_default(copy_move_and_default&&) {}
};
struct only_copy {  // no default constructor: nothing injectable remains
  only_copy(const only_copy&) {}
};
struct explicit_one {
  explicit explicit_one(X) {}
};
// Aggregates: probed with braces, one dependency per field.
struct agg {
  X a;
  const Y& b;
  std::unique_ptr<X> c;
};
struct agg_defaults {  // default member initializers are overridden: both fields injected
  int port = 80;
  X x;
};
struct agg_array {  // brace elision: the array's two elements count as two fields
  int a[2];
  X x;
};
struct agg_base : X {  // C++17 aggregate with a base: the base is the first "field"
  int y;
};
// Greedy constructor templates.
struct greedy {
  template <class U>
  greedy(U&&) {}
};
struct greedy_but_longer_exists {  // the template is irrelevant: the 2-parameter constructor is
                                   // chosen
  template <class U>
  greedy_but_longer_exists(U&&) {}
  greedy_but_longer_exists(X, Y) {}
};
struct constrained_template {  // the probe does not satisfy std::integral, so not greedy
  template <std::integral U>
  constrained_template(U) {}
  constrained_template(X) {}
};
struct deleted_template_guard {  // the common "no implicit conversions" idiom catches the probe
  deleted_template_guard(int) {}
  template <class T>
  deleted_template_guard(T) = delete;
};
// Variadic constructors.
struct variadic_template {
  template <class... Us>
  variadic_template(Us&&...) {}
};
struct c_variadic {
  c_variadic(...) {}
};
// Same-arity overloads.
struct same_arity {
  same_arity(X, Y) {}
  same_arity(Y, X) {}
};
struct same_arity_plus_default {  // undetectable: the ambiguous pair is skipped, arity 0 wins
  same_arity_plus_default() {}
  same_arity_plus_default(X, Y) {}
  same_arity_plus_default(Y, X) {}
};
// Defaulted parameters and shorter overloads: same signal (arity - 1 also viable).
struct defaulted_param {
  defaulted_param(X, int = 3) {}
};
struct shorter_overload {
  shorter_overload(X) {}
  shorter_overload(X, Y) {}
};
// Private and deleted constructors.
class private_ctor {
  private_ctor(X) {}
};
struct deleted_ctor {
  deleted_ctor(X) = delete;
};
class private_longer_public_shorter {  // the private constructor is invisible; arity 1
  private_longer_public_shorter(X, Y) {}

 public:
  private_longer_public_shorter(X) {}
};
// Abstract types.
struct abstract_iface {
  virtual ~abstract_iface() = default;
  virtual void f() = 0;
};
// Above the limit.
struct seventeen {
  seventeen(X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X) {}
};
// initializer_list constructors cannot capture the placeholders (parentheses).
struct init_list_and_two {
  init_list_and_two(std::initializer_list<int>) {}
  init_list_and_two(X, Y) {}
};
// Parameter types with their own greedy converting constructor.
struct optional_param {
  optional_param(std::optional<X>) {}
};
// An rvalue-reference parameter. gcc-13 discovers it only thanks to the extra
// probe operator (S1_GCC13_RREF_PROBE), and resolves it because, for a unique X,
// operator T&() is the deleted twin, which gcc-13 then no longer mis-selects.
struct rvalue_param {
  rvalue_param(X&&) {}
};
// The arity is fixed by discovery, so an unresolvable parameter is an error,
// never a silent switch to a shorter constructor.
struct needs_ref_to_unique {
  needs_ref_to_unique() {}
  needs_ref_to_unique(const X&) {}
};

using s1::ctor_status;
template <class T>
constexpr ctor_status st = s1::ctor_info<T>::status;
template <class T>
constexpr std::size_t ar = s1::ctor_info<T>::arity;
template <class T>
constexpr bool sv = s1::ctor_info<T>::shorter_viable;
constexpr bool gcc13 = S1_GCC13_RREF_PROBE != 0;
constexpr std::size_t npos = s1::npos;

static_assert(st<plain0> == ctor_status::ok && ar<plain0> == 0);
static_assert(st<plain3> == ctor_status::ok && ar<plain3> == 3 && !sv<plain3>);
static_assert(st<copy_move_and_default> == ctor_status::ok && ar<copy_move_and_default> == 0);
static_assert(st<only_copy> == ctor_status::not_constructible);
static_assert(st<explicit_one> == ctor_status::ok && ar<explicit_one> == 1);
static_assert(st<agg> == ctor_status::ok && ar<agg> == 3 && s1::ctor_info<agg>::braces);
static_assert(st<agg_defaults> == ctor_status::ok && ar<agg_defaults> == 2);
static_assert(st<agg_array> == ctor_status::ok && ar<agg_array> == 3);
static_assert(st<agg_base> == ctor_status::ok && ar<agg_base> == 2);
static_assert(st<greedy> == ctor_status::greedy && ar<greedy> == 1);
static_assert(st<greedy_but_longer_exists> == ctor_status::ok && ar<greedy_but_longer_exists> == 2);
static_assert(st<constrained_template> == ctor_status::ok && ar<constrained_template> == 1);
static_assert(st<deleted_template_guard> == ctor_status::not_constructible);
static_assert(st<variadic_template> == ctor_status::greedy &&
              ar<variadic_template> == s1::max_ctor_arity);
static_assert(st<c_variadic> == ctor_status::greedy && ar<c_variadic> == s1::max_ctor_arity);
static_assert(st<same_arity> == ctor_status::not_constructible);
static_assert(st<same_arity_plus_default> == ctor_status::ok && ar<same_arity_plus_default> == 0);
static_assert(st<defaulted_param> == ctor_status::ok && ar<defaulted_param> == 2 &&
              sv<defaulted_param>);
static_assert(st<shorter_overload> == ctor_status::ok && ar<shorter_overload> == 2 &&
              sv<shorter_overload>);
static_assert(st<private_ctor> == ctor_status::not_constructible);
static_assert(st<deleted_ctor> == ctor_status::not_constructible);
static_assert(st<private_longer_public_shorter> == ctor_status::ok &&
              ar<private_longer_public_shorter> == 1);
static_assert(st<abstract_iface> == ctor_status::abstract);
static_assert(st<seventeen> == ctor_status::over_limit);
static_assert(st<int> == ctor_status::not_class);
static_assert(st<init_list_and_two> == ctor_status::ok && ar<init_list_and_two> == 2);
// gcc-13's extra probe operator also accepts optional<X> (see forms.cpp, set min_rr).
static_assert(st<optional_param> == (gcc13 ? ctor_status::ok : ctor_status::not_constructible));
static_assert(st<rvalue_param> == ctor_status::ok && ar<rvalue_param> == 1);
static_assert(st<needs_ref_to_unique> == ctor_status::ok && ar<needs_ref_to_unique> == 1);

// Resolution against the stub injector (X and Y are unbound, so unique).
using inj = s1::injector<>;
static_assert(inj::can_resolve<plain3> == false);  // const X& and X* of a unique X: E005
static_assert(inj::can_resolve<explicit_one> && inj::can_resolve<greedy_but_longer_exists>);
static_assert(inj::can_resolve<rvalue_param>, "X&& of a unique X resolves (as a new X)");
// X&& of a singleton X is refused on every compiler (it would be a moved-from copy).
static_assert(!s1::injector<s1::singleton<X>>::can_resolve<rvalue_param>);
static_assert(!inj::can_resolve<optional_param>, "optional<T> is ambiguous with the placeholder");
static_assert(!inj::can_resolve<needs_ref_to_unique>,
              "no silent fallback to the default constructor");
static_assert(inj::can_resolve<shorter_overload> && inj::can_resolve<defaulted_param> == false);

const char* name(ctor_status s) {
  switch (s) {
    case ctor_status::ok:
      return "ok";
    case ctor_status::not_class:
      return "not_class";
    case ctor_status::abstract:
      return "abstract (E001)";
    case ctor_status::not_constructible:
      return "not_constructible (E002)";
    case ctor_status::greedy:
      return "greedy (E004)";
    case ctor_status::over_limit:
      return "over_limit (E010)";
  }
  return "?";
}
template <class T>
void row(const char* label) {
  const auto a = ar<T>;
  std::printf("LIMIT|%s|%s|%s|%s\n", label, name(st<T>),
              a == npos ? "-" : std::to_string(a).c_str(), sv<T> ? "yes" : "no");
}

}  // namespace lim

int main() {
  using namespace lim;
  row<plain0>("plain, 0 parameters");
  row<plain3>("plain, 3 parameters");
  row<copy_move_and_default>("copy + move + default constructor");
  row<only_copy>("only a copy constructor");
  row<explicit_one>("explicit constructor");
  row<agg>("aggregate {X, const Y&, unique_ptr<X>}");
  row<agg_defaults>("aggregate with default member initializer");
  row<agg_array>("aggregate {int[2], X}");
  row<agg_base>("aggregate with a base class");
  row<greedy>("template<class U> C(U&&)");
  row<greedy_but_longer_exists>("greedy template + C(X, Y)");
  row<constrained_template>("template<integral U> C(U) + C(X)");
  row<deleted_template_guard>("C(int) + template<class T> C(T) = delete");
  row<variadic_template>("template<class... U> C(U&&...)");
  row<c_variadic>("C(...)");
  row<same_arity>("C(X, Y) + C(Y, X)");
  row<same_arity_plus_default>("C() + C(X, Y) + C(Y, X)");
  row<defaulted_param>("C(X, int = 3)");
  row<shorter_overload>("C(X) + C(X, Y)");
  row<private_ctor>("private C(X)");
  row<deleted_ctor>("C(X) = delete");
  row<private_longer_public_shorter>("private C(X, Y) + public C(X)");
  row<abstract_iface>("abstract class");
  row<seventeen>("17 parameters (max_ctor_arity 16)");
  row<int>("int");
  row<init_list_and_two>("C(initializer_list<int>) + C(X, Y)");
  row<optional_param>("C(optional<X>)");
  row<rvalue_param>("C(X&&)");
  row<needs_ref_to_unique>("C() + C(const X&)");
  inj injector;
  rvalue_param built = injector.create<rvalue_param>();  // instantiates the conversion for real
  (void)built;
  std::printf("OK: limits (gcc13 probe workaround %s)\n", gcc13 ? "on" : "off");
  return 0;
}
