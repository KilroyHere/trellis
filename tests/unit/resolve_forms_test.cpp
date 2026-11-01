// Tests that the static engine's check and its failure pass agree with constructor discovery on
// every parameter form of spike S1's table (S1 §3.2; docs/internals/deduction.md, "Forms"), with
// consumer<P>, a class whose one constructor takes a P:
//
// - "NodesAgree": on an injector of singletons and on one of unique bindings, the check's node
//   for consumer<P> (static/resolve.hpp, node_v, with the __is_constructible builtin) gives the
//   verdict of discovery's deduced_constructible_v, and can_resolve<consumer<P>> is that verdict,
//   for the 32 forms that deduce; create<consumer<P>>() builds every consumer the check accepts.
//   Rows 20, 33 and 34 (std::optional<X>, std::tuple<X>, std::any) don't deduce: discovery finds
//   no constructor (TRL-E002), so the plan's node is never_node, with no deduced node to compare,
//   and the check rejects them.
// - "FailurePassAgrees": the failure pass's placeholders deduce what discovery's do. explain_arg
//   selects, for each form, the operator any_arg selects, with the request S1 recorded
//   (tests/unit/deduced_test.cpp checks any_arg against the same table), and the owner check,
//   which explain_build asks first, finds no owner of an incomplete class in any form, so the
//   failure pass builds each consumer from explain_arg. A placeholder whose operators drifted from
//   the probe's would fail here. (T1.6 replaced T1.5's failure pass, explain_node and owner_arg,
//   with the explain pass, diagnostics/explain.hpp; the checks are the same.)
#include <trellis/config.hpp>
#include <trellis/diagnostics/errors.hpp>
#include <trellis/diagnostics/explain.hpp>
#include <trellis/discovery/deduced.hpp>
#include <trellis/static/resolve.hpp>
#include <trellis/trellis.hpp>

#include <any>
#include <array>
#include <cstddef>
#include <functional>
#include <memory>
#include <new>  // IWYU pragma: keep
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

namespace {

// ---- the forms --------------------------------------------------------------------------------

struct x {
  x() = default;
  int value = 7;
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
// A stand-in for a wrapper Trellis owns, such as lazy<T&>, as in deduced_test.cpp.
struct lazy_tag {};
template <class T>
class lazy {
 public:
  explicit lazy(lazy_tag /*tag*/) {}
};

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
template <class... Forms>
struct form_list {};

// The 32 forms that deduce, as in deduced_test.cpp (rows 1-19, 21-32 and 35).
using deducible_forms = form_list<form<x, x>,                                                  // 1
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

// Rows 20, 33 and 34: their converting constructor template accepts the placeholder itself.
template <class... Ps>
struct param_list {};
using undeducible_params = param_list<std::optional<x>, std::tuple<x>, std::any>;

// ---- NodesAgree -------------------------------------------------------------------------------

auto singleton_injector() {
  return trellis::make_injector(trellis::bind<x>.in(trellis::singleton),
                                trellis::bind<iface>.to<impl>().in(trellis::singleton),
                                trellis::bind<int>.to_value(1),
                                trellis::bind<std::string>.to_value(std::string("s")));
}
auto unique_injector() {
  return trellis::make_injector(trellis::bind<x>, trellis::bind<iface>.to<impl>(),
                                trellis::bind<int>.to_value(1),
                                trellis::bind<std::string>.to_value(std::string("s")));
}

template <class Injector, class P>
constexpr bool node_agrees() {
  using ctx = trellis::detail::context<Injector, trellis::unique_t>;
  using deduction = trellis::detail::deduction<consumer<P>>;
  static_assert(deduction::status == trellis::detail::deduction_status::deduced);
  static_assert(deduction::arity == 1);
  using node = trellis::detail::deduced_node<consumer<P>, ctx, std::index_sequence<0>, false>;
  constexpr bool verdict = trellis::detail::node_v<node>;
  return verdict == trellis::detail::deduced_constructible_v<consumer<P>, 1, ctx> &&
         verdict == ctx::template can_resolve<consumer<P>>;
}
template <class Injector, class... Forms>
constexpr bool nodes_agree(form_list<Forms...> /*forms*/) {
  return (node_agrees<Injector, typename Forms::param>() && ...);
}

template <class Injector, class P>
constexpr bool rejected_undeducible() {
  using ctx = trellis::detail::context<Injector, trellis::unique_t>;
  return !trellis::detail::discoverable_v<consumer<P>, trellis::detail::no_ctor_declared,
                                          trellis::detail::default_max_ctor_arity> &&
         !ctx::template can_resolve<consumer<P>>;
}
template <class Injector, class... Ps>
constexpr bool undeducible_rejected(param_list<Ps...> /*params*/) {
  return (rejected_undeducible<Injector, Ps>() && ...);
}

using singletons = decltype(singleton_injector());
using uniques = decltype(unique_injector());
static_assert(nodes_agree<singletons>(deducible_forms{}));
static_assert(nodes_agree<uniques>(deducible_forms{}));
static_assert(undeducible_rejected<singletons>(undeducible_params{}));
static_assert(undeducible_rejected<uniques>(undeducible_params{}));

// Builds consumer<P> for every form the check accepts, and counts them: the creation code compiles
// and runs wherever the check says it may.
template <class Injector, class... Forms>
std::size_t build_accepted(Injector& injector, form_list<Forms...> /*forms*/) {
  using ctx = trellis::detail::context<Injector, trellis::unique_t>;
  std::size_t built = 0;
  const auto build_one = [&]<class P>(std::type_identity<P> /*param*/) {
    if constexpr (ctx::template can_resolve<consumer<P>>) {
      [[maybe_unused]] const consumer<P> object = injector.template create<consumer<P>>();
      ++built;
    }
  };
  (build_one(std::type_identity<typename Forms::param>{}), ...);
  return built;
}

template <class Injector, class... Forms>
constexpr std::size_t accepted(form_list<Forms...> /*forms*/) {
  using ctx = trellis::detail::context<Injector, trellis::unique_t>;
  return (std::size_t{ctx::template can_resolve<consumer<typename Forms::param>>} + ...);
}

TEST(NodesAgree, EveryFormTheCheckAcceptsIsBuilt) {
  auto shared = singleton_injector();
  auto fresh = unique_injector();
  EXPECT_EQ(build_accepted(shared, deducible_forms{}), accepted<singletons>(deducible_forms{}));
  EXPECT_EQ(build_accepted(fresh, deducible_forms{}), accepted<uniques>(deducible_forms{}));
  // Neither injector accepts everything or nothing: each serves forms the other rejects.
  EXPECT_GT(accepted<singletons>(deducible_forms{}), 0U);
  EXPECT_LT(accepted<singletons>(deducible_forms{}), 32U);
  EXPECT_GT(accepted<uniques>(deducible_forms{}), 0U);
  EXPECT_LT(accepted<uniques>(deducible_forms{}), 32U);
}

// ---- FailurePassAgrees ------------------------------------------------------------------------

// expect_ctx<Request>: a context whose check accepts only Request and fails the compilation for
// any other: explain_arg's selected operator asks Ctx::can_resolve<T> in its body (T1.6's explain
// pass, diagnostics/explain.hpp), so building consumer<P> from it asks exactly the request it
// deduced. The answer is yes, so the body continues to nothing. Before it, the body asks the
// context's injector, an empty one here, for the stop below the request, never can_resolve, and
// overload resolution evaluates nothing of the candidates, so only the selected operator reaches
// the check.
template <class T, class Request>
consteval bool deduced_as() {
  static_assert(std::is_same_v<T, Request>, "explain_arg deduced another request than any_arg");
  return true;
}
template <class Request>
struct expect_ctx {
  using injector_type = decltype(trellis::make_injector());
  template <class T>
  static constexpr bool can_resolve = deduced_as<T, Request>();
};

// The explain pass's build of consumer<P> in expect_ctx<Request>, as explain_build builds a
// deduced class: the owner check must find no parameter that owns an incomplete class, or the
// pass would build consumer<P> only unevaluated, and the construction is instantiated by taking
// the function's address (it is never called: the operators end in unreachable()).
template <class Form>
constexpr bool owner_check_passes =
    !trellis::detail::has_owner<trellis::detail::deduced_node<consumer<typename Form::param>,
                                                              expect_ctx<typename Form::request>,
                                                              std::index_sequence<0>, false>>;
template <class... Forms>
constexpr bool owner_checks_pass(form_list<Forms...> /*forms*/) {
  return (owner_check_passes<Forms> && ...);
}
static_assert(owner_checks_pass(deducible_forms{}));

// The one parameter the owner check finds: a std::unique_ptr to a class that is incomplete here,
// whose destructor building the class would instantiate (compile_fail/create/
// e002_incomplete_unique_ptr_parameter.cpp shows the one error it gives instead).
struct never_defined;
static_assert(
    !owner_check_passes<form<std::unique_ptr<never_defined>, std::unique_ptr<never_defined>>>);

TRELLIS_PLACEHOLDER_CONVERSIONS_BEGIN

template <class Form>
void explain_build_of() {
  using built = consumer<typename Form::param>;
  // The last argument is the lookahead's sequence, which only a stop that builds a class reads.
  using arg =
      trellis::detail::explain_arg<trellis::error::path<built>, 1,
                                   expect_ctx<typename Form::request>, std::index_sequence<>>;
  alignas(built) std::array<std::byte, sizeof(built)> storage{};
  void* const where = storage.data();
  ::new (where) built(arg{});
}

TRELLIS_PLACEHOLDER_CONVERSIONS_END

template <class... Forms>
std::size_t explained(form_list<Forms...> /*forms*/) {
  const std::array<void (*)(), sizeof...(Forms)> passes{&explain_build_of<Forms>...};
  std::size_t count = 0;
  for (void (*pass)() : passes) {
    count += pass != nullptr ? 1U : 0U;
  }
  return count;
}

TEST(FailurePassAgrees, ExplainArgDeducesWhatAnyArgDeduces) {
  EXPECT_EQ(explained(deducible_forms{}), 32U);
}

}  // namespace
