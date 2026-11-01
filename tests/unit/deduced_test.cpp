// Tests for include/trellis/discovery/deduced.hpp: deduced constructor discovery (spike S1,
// ADR-0003) and its entry point discovered_ctor_t.
//
// - "Forms": each of the 35 parameter forms of S1's table (S1 §3.2) is discovered and resolved
//   to the request S1 recorded, at compile time (a context that serves exactly that request) and
//   at run time (a context that serves every request and records each one).
// - "Limits": every case of S1's limits.cpp (S1 §3.5), plus the aggregate checks T1.3 adds and the
//   bound.
// - "Entry point": a declaration wins over deduction, cv-qualifiers don't matter, and the results
//   agree with ctor_matches_v.
// - "Workarounds": the config.hpp workarounds: for GCC, TRELLIS_PROBE_DECLARES_TWINS and the
//   -Wconversion pragma pair, which this file needs to compile under -Werror on GCC; for MSVC,
//   TRELLIS_REPEAT_THROUGH_CLASS and TRELLIS_ARRAY_CHECK_WITH_BRACES.
//
// Most checks are static_asserts, so a wrong answer breaks the build; the TESTs run the
// constructions. tests/compile_fail/discovery/deduced_*.cpp have the types that must not compile.
#include <trellis/config.hpp>
#include <trellis/discovery/ctor_traits.hpp>
#include <trellis/discovery/deduced.hpp>
#include <trellis/meta/type_name.hpp>

#include <any>
#include <array>
#include <concepts>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

namespace dd {

// ---- the vocabulary of the forms ----------------------------------------------------------------

struct x {
  int value = 0;
};
struct iface {
  iface() = default;
  iface(const iface&) = delete;
  iface& operator=(const iface&) = delete;
  virtual ~iface() = default;
  [[nodiscard]] virtual int get() const = 0;
};
struct impl final : iface {
  [[nodiscard]] int get() const override { return 7; }
};
// A stand-in for a wrapper Trellis owns, such as lazy<T&>: no converting constructor template, so
// that it deduces exactly (ADR-0003 item 4).
struct lazy_tag {};
template <class T>
class lazy {
 public:
  explicit lazy(lazy_tag /*tag*/) {}
};

// consumer<P>: a class whose only constructor takes one P.
template <class P>
struct consumer {
  explicit consumer(P /*value*/) {}
};

// form<P, Request>: parameter type P, and the request S1 recorded for it (S1 §3.2).
template <class P, class Request>
struct form {
  using param = P;
  using request = Request;
};

// The 32 forms that deduce (S1 §3.2, rows 1-19, 21-32 and 35), in S1's order: the request is the
// parameter type, except that a by-value parameter loses its top-level const, as in the function
// type, and the three rvalue-reference forms are the request of their referred type (ADR-0003
// item 3).
using deducible_forms =
    std::tuple<form<x, x>,                                                  // 1
               form<const x, x>,                                            // 2
               form<x&, x&>,                                                // 3
               form<const x&, const x&>,                                    // 4
               form<x&&, x>,                                                // 5
               form<const x&&, x>,                                          // 6
               form<x*, x*>,                                                // 7
               form<const x*, const x*>,                                    // 8
               form<x* const, x*>,                                          // 9
               form<std::unique_ptr<x>, std::unique_ptr<x>>,                // 10
               form<std::unique_ptr<x>&&, std::unique_ptr<x>>,              // 11
               form<std::shared_ptr<x>, std::shared_ptr<x>>,                // 12
               form<const std::shared_ptr<x>&, const std::shared_ptr<x>&>,  // 13
               form<std::shared_ptr<const x>, std::shared_ptr<const x>>,    // 14
               form<std::weak_ptr<x>, std::weak_ptr<x>>,                    // 15
               form<std::function<int()>, std::function<int()>>,            // 16
               form<std::function<std::unique_ptr<iface>(int)>,             // 17
                    std::function<std::unique_ptr<iface>(int)>>,            //
               form<std::span<x>, std::span<x>>,                            // 18
               form<std::span<const x>, std::span<const x>>,                // 19
               form<int, int>,                                              // 21
               form<const int&, const int&>,                                // 22
               form<std::string, std::string>,                              // 23
               form<std::string_view, std::string_view>,                    // 24
               form<std::vector<x>, std::vector<x>>,                        // 25
               form<iface&, iface&>,                                        // 26
               form<const iface&, const iface&>,                            // 27
               form<iface*, iface*>,                                        // 28
               form<std::unique_ptr<iface>, std::unique_ptr<iface>>,        // 29
               form<std::shared_ptr<iface>, std::shared_ptr<iface>>,        // 30
               form<std::reference_wrapper<x>, std::reference_wrapper<x>>,  // 31
               form<lazy<x&>, lazy<x&>>,                                    // 32
               form<std::variant<x, int>, std::variant<x, int>>>;           // 35

// ---- contexts: what any_arg asks ----------------------------------------------------------------

// exact_ctx<R>: serves exactly the request R. Declared only: the compile-time checks never build.
template <class R>
struct exact_ctx {
  template <class Q>
  static constexpr bool can_resolve = std::is_same_v<Q, R>;
  template <class Q>
  Q resolve();
};

// everything_ctx: serves every request, like S1's tracing placeholder, which had no context.
struct everything_ctx {
  template <class Q>
  static constexpr bool can_resolve = true;
  template <class Q>
  Q resolve();
};

// A distinct address per type, to record a request exactly: references and cv-qualifiers included.
template <class T>
inline constexpr char type_marker = 0;

// The objects that the recording context serves by reference or pointer.
x& shared_x() {
  static x object{11};
  return object;
}
impl& shared_impl() {
  static impl object;
  return object;
}
const int& shared_int() {
  static const int object = 13;
  return object;
}
const std::shared_ptr<x>& shared_pointer() {
  static const std::shared_ptr<x> object = std::make_shared<x>(x{17});
  return object;
}
std::span<x> shared_span() {
  static std::array<x, 2> objects{};
  return objects;
}

// make<Q>(): a value for request Q.
template <class Q>
Q make() {
  using bare = std::remove_cvref_t<Q>;
  if constexpr (std::is_same_v<bare, x> && std::is_reference_v<Q>) {
    return shared_x();
  } else if constexpr (std::is_same_v<bare, iface> && std::is_reference_v<Q>) {
    return shared_impl();
  } else if constexpr (std::is_same_v<Q, const int&>) {
    return shared_int();
  } else if constexpr (std::is_same_v<Q, const std::shared_ptr<x>&>) {
    return shared_pointer();
  } else if constexpr (std::is_same_v<bare, x*> || std::is_same_v<bare, const x*>) {
    return &shared_x();
  } else if constexpr (std::is_same_v<bare, iface*>) {
    return &shared_impl();
  } else if constexpr (std::is_same_v<bare, std::unique_ptr<iface>>) {
    return std::make_unique<impl>();
  } else if constexpr (std::is_same_v<bare, std::shared_ptr<iface>>) {
    return std::make_shared<impl>();
  } else if constexpr (std::is_same_v<bare, std::unique_ptr<x>>) {
    return std::make_unique<x>();
  } else if constexpr (std::is_same_v<bare, std::shared_ptr<x>> ||
                       std::is_same_v<bare, std::shared_ptr<const x>>) {
    return std::make_shared<x>();
  } else if constexpr (std::is_same_v<bare, std::function<int()>>) {
    return [] { return 1; };
  } else if constexpr (std::is_same_v<bare, std::function<std::unique_ptr<iface>(int)>>) {
    return [](int /*seed*/) -> std::unique_ptr<iface> { return std::make_unique<impl>(); };
  } else if constexpr (std::is_same_v<bare, std::span<x>> ||
                       std::is_same_v<bare, std::span<const x>>) {
    return shared_span();
  } else if constexpr (std::is_same_v<bare, std::reference_wrapper<x>>) {
    return std::ref(shared_x());
  } else if constexpr (std::is_same_v<bare, lazy<x&>>) {
    return lazy<x&>(lazy_tag{});
  } else if constexpr (std::is_same_v<bare, int>) {
    return 42;
  } else {
    return bare{};
  }
}

// recording_ctx: serves every request and records each one, so that a test can compare what
// deduction asked for with what S1 recorded.
struct recording_ctx {
  std::vector<const char*> requests;  // type_marker addresses
  std::vector<std::string_view> names;

  template <class Q>
  static constexpr bool can_resolve = true;
  template <class Q>
  Q resolve() {
    requests.push_back(&type_marker<Q>);
    names.push_back(trellis::type_name<Q>());
    return make<Q>();
  }
};

}  // namespace dd

namespace {

using trellis::ctor;
using trellis::detail::construct_deduced;
using trellis::detail::ctor_matches_v;
using trellis::detail::deduced_constructible_v;
using trellis::detail::deduction;
using trellis::detail::deduction_status;
using trellis::detail::discoverable_v;
using trellis::detail::discovered_ctor_t;
using trellis::detail::no_ctor_declared;
using trellis::detail::probe_arg;

// ---- Forms: compile time ------------------------------------------------------------------------

// For each deducible form: discovery finds the one parameter, and resolution asks for exactly the
// request S1 recorded. The second check proves it exact: a context that serves that request alone
// builds the consumer, and one that serves only the other operator's request doesn't, as the
// deleted twin of the operator that matches the form is then selected rather than the other one.
template <class Form>
consteval bool form_deduces() {
  using c = dd::consumer<typename Form::param>;
  using request = typename Form::request;
  // The request the other operator would make: T& for a value request, and the referred type
  // for a reference request.
  using other =
      std::conditional_t<std::is_reference_v<request>, std::remove_cvref_t<request>, request&>;
  return deduction<c>::status == deduction_status::deduced && deduction<c>::arity == 1 &&
         std::is_same_v<discovered_ctor_t<c>, ctor<probe_arg<c>>> &&
         deduced_constructible_v<c, 1, dd::exact_ctx<request>> &&
         !deduced_constructible_v<c, 1, dd::exact_ctx<other>>;
}
template <class... Forms>
consteval bool all_deduce(std::tuple<Forms...>* /*forms*/) {
  return (form_deduces<Forms>() && ...);
}
static_assert(all_deduce(static_cast<dd::deducible_forms*>(nullptr)));
static_assert(std::tuple_size_v<dd::deducible_forms> == 32);

// Each form on its own as well, so that a failure names the form.
static_assert(form_deduces<dd::form<dd::x, dd::x>>());
static_assert(form_deduces<dd::form<const dd::x, dd::x>>());
static_assert(form_deduces<dd::form<dd::x&, dd::x&>>());
static_assert(form_deduces<dd::form<const dd::x&, const dd::x&>>());
static_assert(form_deduces<dd::form<dd::x&&, dd::x>>());
static_assert(form_deduces<dd::form<const dd::x&&, dd::x>>());
static_assert(form_deduces<dd::form<std::unique_ptr<dd::x>&&, std::unique_ptr<dd::x>>>());
static_assert(
    form_deduces<dd::form<std::reference_wrapper<dd::x>, std::reference_wrapper<dd::x>>>());
static_assert(form_deduces<dd::form<const dd::iface&, const dd::iface&>>());

// The explicit-route forms: discovery finds no constructor, and with a context that serves every
// request, as S1's placeholder did, the construction is ambiguous (S1 §3.2 note c). On every
// compiler, gcc-13 included, since the twins replaced S1's operator T&&() (config.hpp).
template <class P>
consteval bool needs_explicit_route() {
  using c = dd::consumer<P>;
  return deduction<c>::status == deduction_status::not_constructible && !discoverable_v<c> &&
         !deduced_constructible_v<c, 1, dd::everything_ctx>;
}
static_assert(needs_explicit_route<std::optional<dd::x>>());
static_assert(needs_explicit_route<std::tuple<dd::x>>());
static_assert(needs_explicit_route<std::any>());
// std::any is ambiguous in every context. std::optional<X> and std::tuple<X> only when X itself is
// resolvable, which is the usual case: their converting constructor template needs X to be
// constructible from the placeholder, and with X unresolvable the deleted twins rule that out.
static_assert(!deduced_constructible_v<dd::consumer<std::any>, 1, dd::exact_ctx<std::any>>);
static_assert(deduced_constructible_v<dd::consumer<std::optional<dd::x>>, 1,
                                      dd::exact_ctx<std::optional<dd::x>>>);

}  // namespace

// ---- Forms: run time ----------------------------------------------------------------------------

namespace {

// Builds consumer<P> with construct_deduced, as the engine will, from a context that serves every
// request, and checks the one request it made.
template <class Form>
void expect_form_resolves() {
  using c = dd::consumer<typename Form::param>;
  dd::recording_ctx ctx;
  [[maybe_unused]] const c built = construct_deduced<c, 1>(ctx);
  ASSERT_EQ(ctx.requests.size(), 1U) << trellis::type_name<typename Form::param>();
  EXPECT_EQ(ctx.requests.front(), &dd::type_marker<typename Form::request>)
      << "parameter " << trellis::type_name<typename Form::param>() << ": requested "
      << ctx.names.front() << ", S1 recorded " << trellis::type_name<typename Form::request>();
}
template <class... Forms>
void expect_forms_resolve(std::tuple<Forms...>* /*forms*/) {
  (expect_form_resolves<Forms>(), ...);
}

}  // namespace

TEST(DeducedForms, EachFormResolvesToTheRequestS1Recorded) {
  expect_forms_resolve(static_cast<dd::deducible_forms*>(nullptr));
}

namespace dd {
// Classes that keep what they were given, to check that resolution delivers the served objects.
struct holds_references {
  holds_references(x& a, const x& b, const iface& c, iface* d, std::reference_wrapper<x> e)
      : mutable_x(&a), const_x(&b), service(&c), pointer(d), wrapped(&e.get()) {}
  x* mutable_x;
  const x* const_x;
  const iface* service;
  iface* pointer;
  x* wrapped;
};
struct holds_values {
  holds_values(int a, std::unique_ptr<iface> b, std::function<int()> c, std::string d)
      : number(a), owned(std::move(b)), callback(std::move(c)), text(std::move(d)) {}
  int number;
  std::unique_ptr<iface> owned;
  std::function<int()> callback;
  std::string text;
};
}  // namespace dd

TEST(DeducedForms, ResolutionDeliversTheServedObjects) {
  dd::recording_ctx ctx;
  static_assert(deduction<dd::holds_references>::arity == 5);
  const auto refs = construct_deduced<dd::holds_references, 5>(ctx);
  EXPECT_EQ(refs.mutable_x, &dd::shared_x());
  EXPECT_EQ(refs.const_x, &dd::shared_x());
  EXPECT_EQ(refs.service, &dd::shared_impl());
  EXPECT_EQ(refs.pointer, &dd::shared_impl());
  EXPECT_EQ(refs.wrapped, &dd::shared_x());

  static_assert(deduction<dd::holds_values>::arity == 4);
  const auto values = construct_deduced<dd::holds_values, 4>(ctx);
  EXPECT_EQ(values.number, 42);
  ASSERT_NE(values.owned, nullptr);
  EXPECT_EQ(values.owned->get(), 7);
  EXPECT_EQ(values.callback(), 1);
  EXPECT_EQ(values.text, "");
  // Each parameter is resolved once: 5 requests, then 4.
  EXPECT_EQ(ctx.requests.size(), 9U);
}

// ---- Limits -------------------------------------------------------------------------------------

namespace lim {

struct x {};
struct y {};

struct plain0 {
  plain0() = default;
};
struct plain3 {
  plain3(x /*a*/, const x& /*b*/, x* /*c*/) {}
};
// Copy and move constructors are excluded by not_self, so this has arity 0.
struct copy_move_and_default {
  copy_move_and_default() = default;
  copy_move_and_default(const copy_move_and_default&) = default;
  copy_move_and_default(copy_move_and_default&&) noexcept = default;
  copy_move_and_default& operator=(const copy_move_and_default&) = default;
  copy_move_and_default& operator=(copy_move_and_default&&) = default;
  ~copy_move_and_default() = default;
};
// Nothing injectable remains.
struct only_copy {
  only_copy(const only_copy&) = default;
};
struct explicit_one {
  explicit explicit_one(x /*a*/) {}
};
// Aggregates: probed with braces, one request per field.
struct agg {
  x a;
  const y& b;
  std::unique_ptr<x> c;
};
// A default member initializer is overridden: both fields are injected.
struct agg_defaults {
  int port = 80;
  x b;
};
// A base class is the first field.
struct agg_base : x {
  int b;
};
// The array is inside a member: the member is one request, and its own type has the array.
struct with_array {
  std::array<int, 2> a;  // std::array is an aggregate with an array member
};
struct agg_nested {
  with_array a;
  x b;
};
// A member that is an aggregate with a field: one request, as any class member.
struct port_number {
  int value;
};
struct agg_of_aggregate {
  int host;
  port_number port;
};
struct agg_rvalue_member {
  x&& a;
};
struct agg_empty {};
// Array members: refused on every compiler, however it counts them (TRL-E004).
struct agg_array {
  int a[2];  // NOLINT(modernize-avoid-c-arrays): the case under test
  x b;
};
struct agg_array_last {
  x a;
  int b[2];  // NOLINT(modernize-avoid-c-arrays)
};
struct agg_array_of_one {
  int a[1];  // NOLINT(modernize-avoid-c-arrays)
  x b;
};
struct agg_class_array {
  x a[2];  // NOLINT(modernize-avoid-c-arrays)
};
struct agg_matrix {
  int a[2][2];  // NOLINT(modernize-avoid-c-arrays)
};
// A member the probe can't initialize: refused, rather than value-initializing the members after
// it (TRL-E004).
struct agg_optional_member {
  std::string host;
  std::optional<int> port;
  int retries;
};
struct agg_any_member {
  std::any a;
};
struct agg_tuple_last {
  x a;
  std::tuple<x> b;
};
// Unions aren't classes: not deduced.
union agg_union {
  int a;
  x b;
};
// Greedy constructor templates (TRL-E004), and the cases that aren't.
// NOLINTBEGIN(bugprone-forwarding-reference-overload): constructors that take anything are the case
// under test.
struct greedy {
  template <class U>
  explicit greedy(U&& /*any*/) {}
};
struct greedy_but_longer_exists {  // the longer non-template constructor is chosen
  template <class U>
  explicit greedy_but_longer_exists(U&& /*any*/) {}
  greedy_but_longer_exists(x /*a*/, y /*b*/) {}
};
// NOLINTEND(bugprone-forwarding-reference-overload)
struct constrained_template {  // the probe doesn't satisfy std::integral: not greedy
  template <std::integral U>
  explicit constrained_template(U /*number*/) {}
  explicit constrained_template(x /*a*/) {}
};
struct deleted_template_guard {  // the "no implicit conversions" idiom catches the probe
  explicit deleted_template_guard(int /*number*/) {}
  template <class T>
  explicit deleted_template_guard(T) = delete;
};
// A constructor template whose constraint the probe satisfies and the inert argument doesn't:
// deduced, the residual limit of ADR-0003 item 7. The placeholder itself is the argument.
struct probe_satisfies_constraint {
  template <class U>
    requires std::is_convertible_v<U, int>
  explicit probe_satisfies_constraint(U /*number*/) {}
};
// Variadic constructors (TRL-E004).
struct variadic_template {
  template <class... Us>
  explicit variadic_template(Us&&... /*any*/) {}
};
struct c_variadic {
  explicit c_variadic(...) {}
};
// Same-arity overloads: ambiguous, so not constructible at that arity (TRL-E002).
struct same_arity {
  same_arity(x /*a*/, y /*b*/) {}
  same_arity(y /*a*/, x /*b*/) {}
};
// ...and undetectable next to a shorter constructor: the shorter one is chosen (ADR-0003 item 7).
struct same_arity_plus_default {
  same_arity_plus_default() = default;
  same_arity_plus_default(x /*a*/, y /*b*/) {}
  same_arity_plus_default(y /*a*/, x /*b*/) {}
};
// A defaulted parameter is injected; a shorter overload looks the same to deduction.
struct defaulted_param {
  explicit defaulted_param(x /*a*/, int b = 3) : number(b) {}
  int number;
};
struct shorter_overload {
  explicit shorter_overload(x /*a*/) {}
  shorter_overload(x /*a*/, y /*b*/) {}
};
// Private and deleted constructors look the same as none (TRL-E002).
class private_ctor {
  explicit private_ctor(x /*a*/) {}
};
struct deleted_ctor {
  explicit deleted_ctor(x) = delete;
};
class private_longer_public_shorter {  // the private constructor is invisible: arity 1
  private_longer_public_shorter(x /*a*/, y /*b*/) {}

 public:
  explicit private_longer_public_shorter(x /*a*/) {}
};
struct abstract_iface {
  abstract_iface() = default;
  abstract_iface(const abstract_iface&) = delete;
  abstract_iface& operator=(const abstract_iface&) = delete;
  virtual ~abstract_iface() = default;
  virtual void run() = 0;
};
// Above the bound (TRL-E010), and within a raised one.
struct seventeen {
  seventeen(x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x) {}
};
struct five {
  five(x /*a*/, x /*b*/, x /*c*/, x /*d*/, x /*e*/) {}
};
// An initializer_list constructor can't capture the placeholders, which go in parentheses.
struct init_list_and_two {
  explicit init_list_and_two(std::initializer_list<int> /*numbers*/) {}
  init_list_and_two(x /*a*/, y /*b*/) {}
};
struct optional_param {
  explicit optional_param(std::optional<x> /*a*/) {}
};
struct rvalue_param {
  explicit rvalue_param(x&& /*a*/) {}
};
// The arity is fixed by discovery, so an unresolvable parameter is an error, never a silent
// switch to a shorter constructor.
struct needs_ref_to_unique {
  needs_ref_to_unique() = default;
  explicit needs_ref_to_unique(const x& /*a*/) {}
};

template <class T, std::size_t MaxArity = 16>
constexpr deduction_status status_of = deduction<T, MaxArity>::status;
template <class T, std::size_t MaxArity = 16>
constexpr std::size_t arity_of = deduction<T, MaxArity>::arity;

constexpr auto deduced = deduction_status::deduced;

static_assert(status_of<plain0> == deduced && arity_of<plain0> == 0);
static_assert(status_of<plain3> == deduced && arity_of<plain3> == 3);
static_assert(status_of<copy_move_and_default> == deduced && arity_of<copy_move_and_default> == 0);
static_assert(status_of<only_copy> == deduction_status::not_constructible);
static_assert(status_of<explicit_one> == deduced && arity_of<explicit_one> == 1);
static_assert(status_of<agg> == deduced && arity_of<agg> == 3);
static_assert(status_of<agg_defaults> == deduced && arity_of<agg_defaults> == 2);
static_assert(status_of<agg_base> == deduced && arity_of<agg_base> == 2);
static_assert(status_of<agg_nested> == deduced && arity_of<agg_nested> == 2);
static_assert(status_of<with_array> == deduced && arity_of<with_array> == 1);
static_assert(status_of<agg_of_aggregate> == deduced && arity_of<agg_of_aggregate> == 2);
static_assert(status_of<std::array<int, 2>> == deduction_status::array_member);
static_assert(status_of<agg_rvalue_member> == deduced && arity_of<agg_rvalue_member> == 1);
static_assert(status_of<agg_empty> == deduced && arity_of<agg_empty> == 0);
static_assert(status_of<agg_array> == deduction_status::array_member);
static_assert(status_of<agg_array_last> == deduction_status::array_member);
static_assert(status_of<agg_array_of_one> == deduction_status::array_member);
static_assert(status_of<agg_class_array> == deduction_status::array_member);
static_assert(status_of<agg_matrix> == deduction_status::array_member);
static_assert(status_of<agg_optional_member> == deduction_status::undeducible_member &&
              arity_of<agg_optional_member> == 1);
static_assert(status_of<agg_any_member> == deduction_status::undeducible_member &&
              arity_of<agg_any_member> == 0);
static_assert(status_of<agg_tuple_last> == deduction_status::undeducible_member &&
              arity_of<agg_tuple_last> == 1);
static_assert(status_of<agg_union> == deduction_status::not_a_class);
static_assert(status_of<greedy> == deduction_status::greedy && arity_of<greedy> == 1);
static_assert(status_of<greedy_but_longer_exists> == deduced &&
              arity_of<greedy_but_longer_exists> == 2);
static_assert(status_of<constrained_template> == deduced && arity_of<constrained_template> == 1);
static_assert(status_of<deleted_template_guard> == deduction_status::not_constructible);
static_assert(status_of<probe_satisfies_constraint> == deduced &&
              arity_of<probe_satisfies_constraint> == 1);
static_assert(status_of<variadic_template> == deduction_status::greedy &&
              arity_of<variadic_template> == 16);
static_assert(status_of<c_variadic> == deduction_status::greedy && arity_of<c_variadic> == 16);
static_assert(status_of<same_arity> == deduction_status::not_constructible);
static_assert(status_of<same_arity_plus_default> == deduced &&
              arity_of<same_arity_plus_default> == 0);
static_assert(status_of<defaulted_param> == deduced && arity_of<defaulted_param> == 2);
static_assert(status_of<shorter_overload> == deduced && arity_of<shorter_overload> == 2);
static_assert(status_of<private_ctor> == deduction_status::not_constructible);
static_assert(status_of<deleted_ctor> == deduction_status::not_constructible);
static_assert(status_of<private_longer_public_shorter> == deduced &&
              arity_of<private_longer_public_shorter> == 1);
static_assert(status_of<abstract_iface> == deduction_status::abstract);
static_assert(status_of<seventeen> == deduction_status::above_bound);
static_assert(status_of<int> == deduction_status::not_a_class);
static_assert(status_of<x*> == deduction_status::not_a_class);
static_assert(status_of<x&> == deduction_status::not_a_class);
static_assert(status_of<void> == deduction_status::not_a_class);
static_assert(status_of<init_list_and_two> == deduced && arity_of<init_list_and_two> == 2);
static_assert(status_of<optional_param> == deduction_status::not_constructible);
static_assert(status_of<rvalue_param> == deduced && arity_of<rvalue_param> == 1);
static_assert(status_of<needs_ref_to_unique> == deduced && arity_of<needs_ref_to_unique> == 1);

// The bound: a parameter count above it is TRL-E010 when it is within 16 of the bound; a raised
// bound finds it; a variadic constructor takes the bound itself.
static_assert(status_of<five, 4> == deduction_status::above_bound);
static_assert(status_of<five, 5> == deduced && arity_of<five, 5> == 5);
static_assert(status_of<seventeen, 17> == deduced && arity_of<seventeen, 17> == 17);
static_assert(status_of<seventeen, 32> == deduced && arity_of<seventeen, 32> == 17);
static_assert(status_of<plain3, 32> == deduced && arity_of<plain3, 32> == 3);
static_assert(status_of<plain3, 0> == deduction_status::above_bound);
static_assert(status_of<plain0, 0> == deduced && arity_of<plain0, 0> == 0);
static_assert(status_of<variadic_template, 4> == deduction_status::greedy &&
              arity_of<variadic_template, 4> == 4);
static_assert(status_of<variadic_template, 32> == deduction_status::greedy &&
              arity_of<variadic_template, 32> == 32);

// Resolution at the discovered arity (X and Y unbound: a context that serves the requests below).
// const x& and x* of a unique x aren't served, so plain3 doesn't resolve, and needs_ref_to_unique
// doesn't fall back to its default constructor.
struct values_only {  // serves by-value requests of classes, as a unique binding would
  template <class Q>
  static constexpr bool can_resolve = std::is_class_v<Q>;
  template <class Q>
  Q resolve() {
    return Q{};
  }
};
static_assert(!deduced_constructible_v<plain3, 3, values_only>);
static_assert(!deduced_constructible_v<needs_ref_to_unique, 1, values_only>);
static_assert(deduced_constructible_v<explicit_one, 1, values_only>);
static_assert(deduced_constructible_v<greedy_but_longer_exists, 2, values_only>);
static_assert(deduced_constructible_v<rvalue_param, 1, values_only>);  // x&& is the request x
static_assert(deduced_constructible_v<shorter_overload, 2, values_only>);
// The defaulted parameter is requested, never defaulted: int isn't served here.
static_assert(!deduced_constructible_v<defaulted_param, 2, values_only>);
// A singleton x, served only by reference, can't be requested by value or as x&&.
struct references_only {
  template <class Q>
  static constexpr bool can_resolve = std::is_lvalue_reference_v<Q>;
  template <class Q>
  Q resolve();
};
static_assert(!deduced_constructible_v<rvalue_param, 1, references_only>);
static_assert(!deduced_constructible_v<explicit_one, 1, references_only>);
// A context whose can_resolve<R> doesn't exist for reference requests, rather than being false:
// constrained, or SFINAE'd with enable_if. A const x& must still meet the deleted twin, not fall
// back to operator T() and bind a temporary (ADR-0003 item 1); the twins' constraint uses the
// concept can_resolve, which is false where the variable template fails to substitute.
struct sfinae_ctx {
  template <class Q>
    requires(!std::is_reference_v<Q>)
  static constexpr bool can_resolve = true;
  template <class Q>
  Q resolve();
};
struct enable_if_ctx {
  template <class Q, class = std::enable_if_t<!std::is_reference_v<Q>>>
  static constexpr bool can_resolve = true;
  template <class Q>
  Q resolve();
};
struct holds_cref {
  explicit holds_cref(const x& /*a*/) {}
};
static_assert(!deduced_constructible_v<holds_cref, 1, sfinae_ctx>);
static_assert(!deduced_constructible_v<holds_cref, 1, enable_if_ctx>);
static_assert(!deduced_constructible_v<dd::consumer<x&>, 1, sfinae_ctx>);
static_assert(deduced_constructible_v<explicit_one, 1, sfinae_ctx>);
static_assert(deduced_constructible_v<explicit_one, 1, enable_if_ctx>);
// Aggregates resolve field by field, with braces.
struct agg_ctx {
  template <class Q>
  static constexpr bool can_resolve =
      std::is_same_v<Q, x> || std::is_same_v<Q, const y&> || std::is_same_v<Q, std::unique_ptr<x>>;
  template <class Q>
  Q resolve();
};
static_assert(deduced_constructible_v<agg, 3, agg_ctx>);
static_assert(!deduced_constructible_v<agg, 3, values_only>);  // const y& isn't served there

}  // namespace lim

namespace lim {
// Serves x by value, and int as `number`. (A local class can't have member templates.)
struct int_and_x_ctx {
  int number = 0;
  template <class Q>
  static constexpr bool can_resolve = std::is_same_v<Q, x> || std::is_same_v<Q, int>;
  template <class Q>
  Q resolve() {
    if constexpr (std::is_same_v<Q, int>) {
      return number;
    } else {
      return Q{};
    }
  }
};
}  // namespace lim

TEST(DeducedLimits, DefaultedParameterIsInjected) {
  // The constructor gets the served 42, not its default 3.
  lim::int_and_x_ctx served{42};
  const auto built = construct_deduced<lim::defaulted_param, 2>(served);
  EXPECT_EQ(built.number, 42);
}

TEST(DeducedLimits, AggregateFieldsAreInjected) {
  // The default member initializer, 80, is overridden by the injected value.
  lim::int_and_x_ctx served{8080};
  const auto built = construct_deduced<lim::agg_defaults, 2>(served);
  EXPECT_EQ(built.port, 8080);
}

// ---- Entry point --------------------------------------------------------------------------------

namespace entry {

struct config {};
// Deduced: two parameters.
struct deduced_two {
  deduced_two(const config& /*settings*/, std::unique_ptr<config> /*owned*/) {}
};
// A member alias picks the one-parameter constructor; deduction would pick the other one.
struct declared_by_alias {
  using trellis_ctor = trellis::ctor<const config&>;
  explicit declared_by_alias(const config& /*settings*/) {}
  declared_by_alias(config /*a*/, config /*b*/) {}
};
// A specialization makes a greedy class usable: the fix that TRL-E004 names.
struct greedy_declared {
  template <class U>
  explicit greedy_declared(U&& /*any*/) {}  // NOLINT(bugprone-forwarding-reference-overload)
};
// A declaration that lies is TRL-E002, from the explicit check, even where deduction would work.
struct lying {
  explicit lying(const config& /*settings*/) {}
};
// The aggregates that deduction refuses with TRL-E004, declared as their error page says: one
// parameter per array element, and every field. Declarations are checked with braces, as the
// engine builds aggregates, so brace elision fills the array on every compiler.
struct ports_declared {
  int numbers[2];  // NOLINT(modernize-avoid-c-arrays): the case under test
  config where;
};
struct settings_declared {
  std::string host;
  std::optional<int> port;
  int retries;
};

}  // namespace entry

template <>
struct trellis::ctor_traits<entry::greedy_declared> : trellis::ctor<int> {};
template <>
struct trellis::ctor_traits<entry::ports_declared> : trellis::ctor<int, int, entry::config> {};
template <>
struct trellis::ctor_traits<entry::settings_declared>
    : trellis::ctor<std::string, std::optional<int>, int> {};

namespace {

using entry::config;

// Deduction gives one placeholder per parameter, the declared form the parameter types.
static_assert(std::is_same_v<discovered_ctor_t<entry::deduced_two>,
                             ctor<probe_arg<entry::deduced_two>, probe_arg<entry::deduced_two>>>);
static_assert(std::is_same_v<discovered_ctor_t<lim::plain0>, ctor<>>);
static_assert(std::is_same_v<discovered_ctor_t<entry::declared_by_alias>, ctor<const config&>>);
static_assert(std::is_same_v<discovered_ctor_t<entry::greedy_declared>, ctor<int>>);
static_assert(std::is_same_v<discovered_ctor_t<entry::ports_declared>, ctor<int, int, config>>);
static_assert(std::is_same_v<discovered_ctor_t<entry::settings_declared>,
                             ctor<std::string, std::optional<int>, int>>);
static_assert(deduction<entry::ports_declared>::status == deduction_status::array_member);
static_assert(deduction<entry::settings_declared>::status == deduction_status::undeducible_member);
// A binding's declaration wins over everything, deduction included.
static_assert(
    std::is_same_v<discovered_ctor_t<entry::deduced_two, ctor<config, std::unique_ptr<config>>>,
                   ctor<config, std::unique_ptr<config>>>);
static_assert(std::is_same_v<discovered_ctor_t<entry::declared_by_alias, ctor<config, config>>,
                             ctor<config, config>>);
// cv-qualifiers don't matter.
static_assert(std::is_same_v<discovered_ctor_t<const entry::deduced_two>,
                             discovered_ctor_t<entry::deduced_two>>);
static_assert(
    std::is_same_v<discovered_ctor_t<volatile entry::declared_by_alias>, ctor<const config&>>);
// The bound applies to deduction only.
static_assert(std::is_same_v<discovered_ctor_t<lim::seventeen, no_ctor_declared, 17>,
                             discovered_ctor_t<lim::seventeen, no_ctor_declared, 20>>);
static_assert(discovered_ctor_t<lim::seventeen, no_ctor_declared, 17>::args::size == 17);

// discoverable_v asks the same question without asserting.
static_assert(discoverable_v<entry::deduced_two>);
static_assert(discoverable_v<const entry::deduced_two>);
static_assert(discoverable_v<entry::declared_by_alias>);
static_assert(discoverable_v<entry::greedy_declared>);
static_assert(discoverable_v<entry::ports_declared>);
static_assert(discoverable_v<entry::settings_declared>);
static_assert(!discoverable_v<entry::lying, ctor<int>>);
static_assert(discoverable_v<entry::lying, ctor<const config&>>);
static_assert(!discoverable_v<lim::greedy>);
static_assert(!discoverable_v<lim::variadic_template>);
static_assert(!discoverable_v<lim::agg_array>);
static_assert(!discoverable_v<lim::agg_optional_member>);
static_assert(!discoverable_v<lim::seventeen>);
static_assert(discoverable_v<lim::seventeen, no_ctor_declared, 17>);
static_assert(!discoverable_v<lim::private_ctor>);
static_assert(!discoverable_v<lim::abstract_iface>);
static_assert(!discoverable_v<int>);

// The deduced declaration agrees with T1.2's ctor_matches_v, which checks a declaration by the same
// construction rule: the probes are exactly what discovery found T constructible from.
static_assert(ctor_matches_v<entry::deduced_two, discovered_ctor_t<entry::deduced_two>>);
static_assert(ctor_matches_v<lim::plain3, discovered_ctor_t<lim::plain3>>);
static_assert(ctor_matches_v<lim::agg, discovered_ctor_t<lim::agg>>);
static_assert(ctor_matches_v<lim::agg_defaults, discovered_ctor_t<lim::agg_defaults>>);
static_assert(ctor_matches_v<lim::plain0, discovered_ctor_t<lim::plain0>>);

}  // namespace

TEST(DeducedEntryPoint, DeclarationWinsOverDeduction) {
  EXPECT_TRUE((std::is_same_v<discovered_ctor_t<entry::declared_by_alias>, ctor<const config&>>));
  EXPECT_TRUE((std::is_same_v<discovered_ctor_t<entry::greedy_declared>, ctor<int>>));
  EXPECT_EQ(discovered_ctor_t<entry::deduced_two>::args::size, 2U);
}

// ---- cv-qualified and incomplete types ----------------------------------------------------------

namespace cvq {

struct dep {};
struct takes_dep {
  explicit takes_dep(dep /*a*/) {}
};
// Serves dep by value, and nothing else: in particular not takes_dep, which a cv-qualified
// consumer would ask for through its own move constructor if not_self compared cv-qualified types.
struct dep_only_ctx {
  std::vector<const char*> requests;
  template <class Q>
  static constexpr bool can_resolve = std::is_same_v<Q, dep>;
  template <class Q>
  Q resolve() {
    requests.push_back(&dd::type_marker<Q>);
    return Q{};
  }
};
// Declared and never defined.
struct never_defined;

}  // namespace cvq

namespace {

// Every entry point ignores cv-qualifiers, so const and volatile bring back no copy or move
// constructor: deduction, the placeholders, the resolvable check and the discovery entry points.
template <class T>
consteval bool same_deduction() {
  using plain = deduction<T>;
  return deduction<const T>::status == plain::status && deduction<const T>::arity == plain::arity &&
         deduction<volatile T>::status == plain::status &&
         deduction<const volatile T>::arity == plain::arity;
}
static_assert(same_deduction<lim::only_copy>());
static_assert(same_deduction<lim::plain3>());
static_assert(same_deduction<cvq::takes_dep>());
static_assert(deduction<const lim::only_copy>::status == deduction_status::not_constructible);
static_assert(!trellis::detail::not_self<const cvq::takes_dep, cvq::takes_dep>);
static_assert(!trellis::detail::not_self<volatile cvq::takes_dep, const cvq::takes_dep&>);
static_assert(!std::is_constructible_v<lim::only_copy, probe_arg<const lim::only_copy>>);
static_assert(deduced_constructible_v<cvq::takes_dep, 1, cvq::dep_only_ctx>);
static_assert(deduced_constructible_v<const cvq::takes_dep, 1, cvq::dep_only_ctx>);
static_assert(deduced_constructible_v<volatile cvq::takes_dep, 1, cvq::dep_only_ctx>);
static_assert(!deduced_constructible_v<const lim::only_copy, 1, dd::everything_ctx>);
static_assert(!deduced_constructible_v<volatile lim::only_copy, 1, dd::everything_ctx>);
static_assert(std::is_same_v<discovered_ctor_t<const volatile cvq::takes_dep>,
                             discovered_ctor_t<cvq::takes_dep>>);
static_assert(discoverable_v<volatile cvq::takes_dep>);
static_assert(!discoverable_v<const volatile lim::only_copy>);
static_assert(std::is_same_v<decltype(construct_deduced<const cvq::takes_dep, 1>(
                                 std::declval<cvq::dep_only_ctx&>())),
                             cvq::takes_dep>);

// An incomplete type stops deduction before any probe or type trait that needs its definition.
static_assert(deduction<cvq::never_defined>::status == deduction_status::incomplete);
static_assert(!discoverable_v<cvq::never_defined>);

// deduced_constructible_v writes out the construction rule rather than calling builds_from, to save
// a level of depth per hop; the two must agree.
template <class T, class Ctx, std::size_t... Is>
consteval bool rule_agrees(std::index_sequence<Is...> /*arity*/) {
  return deduced_constructible_v<T, sizeof...(Is), Ctx> ==
         trellis::detail::builds_from<
             T, trellis::detail::repeat_t<Is, trellis::detail::any_arg<T, Ctx>>...>();
}
template <class T, std::size_t Arity, class Ctx>
constexpr bool agrees = rule_agrees<T, Ctx>(std::make_index_sequence<Arity>{});
static_assert(agrees<dd::consumer<dd::x>, 1, dd::everything_ctx>);
static_assert(agrees<dd::consumer<const dd::x&>, 1, lim::values_only>);
static_assert(agrees<lim::plain3, 3, lim::values_only>);
static_assert(agrees<lim::init_list_and_two, 2, lim::values_only>);
static_assert(agrees<lim::only_copy, 1, dd::everything_ctx>);
static_assert(agrees<lim::agg, 3, lim::agg_ctx>);
static_assert(agrees<lim::agg, 3, lim::values_only>);
static_assert(agrees<lim::agg_defaults, 2, lim::int_and_x_ctx>);
static_assert(agrees<lim::agg_rvalue_member, 1, lim::values_only>);
static_assert(agrees<lim::agg_empty, 0, lim::values_only>);

}  // namespace

TEST(DeducedCv, ConstConsumerRequestsItsParameterNotItself) {
  cvq::dep_only_ctx served;
  [[maybe_unused]] const cvq::takes_dep a = construct_deduced<const cvq::takes_dep, 1>(served);
  [[maybe_unused]] const cvq::takes_dep b = construct_deduced<volatile cvq::takes_dep, 1>(served);
  ASSERT_EQ(served.requests.size(), 2U);
  EXPECT_EQ(served.requests[0], &dd::type_marker<cvq::dep>);
  EXPECT_EQ(served.requests[1], &dd::type_marker<cvq::dep>);
}

// ---- Workarounds --------------------------------------------------------------------------------

namespace work {

// The probe as S1 wrote it, without the twins.
template <class Consumer>
struct plain_probe {
  template <class T>
    requires trellis::detail::not_self<Consumer, T>
  operator T();
  template <class T>
    requires trellis::detail::not_self<Consumer, T>
  operator T&() const;
};
template <class P>
constexpr bool plain_probe_discovers =
    std::is_constructible_v<dd::consumer<P>, plain_probe<dd::consumer<P>>>;

}  // namespace work

namespace {

// TRELLIS_PROBE_DECLARES_TWINS is on exactly where the plain probe misses the rvalue-reference
// forms: GCC before 14. If a GCC 13 release fixes the bug, this fails, and the workaround can go.
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 14
static_assert(TRELLIS_PROBE_DECLARES_TWINS == 1);
#else
static_assert(TRELLIS_PROBE_DECLARES_TWINS == 0);
#endif
static_assert(work::plain_probe_discovers<dd::x&&> == (TRELLIS_PROBE_DECLARES_TWINS == 0));
static_assert(work::plain_probe_discovers<const dd::x&&> == (TRELLIS_PROBE_DECLARES_TWINS == 0));
static_assert(work::plain_probe_discovers<std::unique_ptr<dd::x>&&> ==
              (TRELLIS_PROBE_DECLARES_TWINS == 0));
static_assert(work::plain_probe_discovers<std::reference_wrapper<dd::x>> ==
              (TRELLIS_PROBE_DECLARES_TWINS == 0));
// The probe Trellis uses discovers them on every compiler (also checked under "Forms").
static_assert(deduction<dd::consumer<dd::x&&>>::status == deduction_status::deduced);
static_assert(deduction<dd::consumer<std::reference_wrapper<dd::x>>>::status ==
              deduction_status::deduced);

// The two MSVC workarounds are on exactly on MSVC (not clang-cl).
#if defined(_MSC_VER) && !defined(__clang__)
static_assert(TRELLIS_REPEAT_THROUGH_CLASS == 1);
static_assert(TRELLIS_ARRAY_CHECK_WITH_BRACES == 1);
#else
static_assert(TRELLIS_REPEAT_THROUGH_CLASS == 0);
static_assert(TRELLIS_ARRAY_CHECK_WITH_BRACES == 0);
#endif
// TRELLIS_REPEAT_THROUGH_CLASS: cl expands repeat_t<Is, A>... over a single index into no pack,
// so a deduced constructor with one parameter failed to compile (C3546). Every form under "Forms"
// has one parameter; here is one more, through the entry point.
static_assert(
    std::is_same_v<discovered_ctor_t<lim::explicit_one>, ctor<probe_arg<lim::explicit_one>>>);
// TRELLIS_ARRAY_CHECK_WITH_BRACES: cl's parentheses refuse a member of aggregate class type, which
// the parenthesized array check would take for an array member. If a cl release fixes that, this
// fails, and the workaround can go. The aggregates under "Limits" test the check itself.
using nested_probe = trellis::detail::no_array_arg<lim::agg_nested>;
static_assert(std::is_constructible_v<lim::agg_nested, nested_probe, nested_probe> ==
              (TRELLIS_ARRAY_CHECK_WITH_BRACES == 0));

}  // namespace

// The -Wconversion pair: this file builds consumers of by-value parameters through
// construct_deduced under the project warnings and -Werror, which GCC would refuse without
// TRELLIS_PLACEHOLDER_CONVERSIONS_BEGIN and _END (EachFormResolvesToTheRequestS1Recorded and
// ResolutionDeliversTheServedObjects). Here, the one construction that shows it most directly.
namespace work {
struct takes_int {
  explicit takes_int(int a) : number(a) {}
  int number;
};
}  // namespace work

TEST(DeducedWorkarounds, ByValueConversionsBuildWithoutWarnings) {
  lim::int_and_x_ctx served{5};
  const auto built = construct_deduced<work::takes_int, 1>(served);
  EXPECT_EQ(built.number, 5);
}
