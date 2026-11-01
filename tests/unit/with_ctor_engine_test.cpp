// How a binding's constructor declaration travels (T1.15; plan §5.3, §7.2, §7.8, ADR-0003 item 1,
// ADR-0006): .with_ctor<Args...>() keeps the binding's four parts, with the declaration where the
// implementation goes (dsl/bind.hpp); the static engine reads it back as discovery's BindingCtor,
// so the plan's node is the declared constructor (static/resolve.hpp), and the explain pass
// follows that constructor's parameters and raises discovery's report for a declaration that
// lies (diagnostics/explain.hpp). The singleton storage holds the class itself
// (static/storage.hpp).
//
// What users see is in with_ctor_test.cpp; what the compilers print is in tests/compile_fail/.
// These are static_asserts, each in a TEST so that a failure points at a name.
#include <trellis/diagnostics/errors.hpp>
#include <trellis/diagnostics/explain.hpp>
#include <trellis/discovery/ctor_traits.hpp>
#include <trellis/dsl/bind.hpp>
#include <trellis/static/resolve.hpp>
#include <trellis/static/storage.hpp>
#include <trellis/trellis.hpp>

#include <memory>
#include <type_traits>

#include <gtest/gtest.h>

namespace wce {

struct config {
  int port = 0;
};
struct logger {};
struct sink {
  sink() = default;
  sink(const sink&) = delete;
  sink& operator=(const sink&) = delete;
  virtual ~sink() = default;
  [[nodiscard]] virtual int lines() const = 0;
};
// Deduction finds the constructor whose parameters the tests' bindings serve; the declared one
// takes a sink, which has no binding there (TRL-E001).
struct audited {
  explicit audited(sink& out) : out_(&out) {}
  audited(const config& /*settings*/, logger& /*log*/) {}
  sink* out_ = nullptr;
};
struct service {
  explicit service(const config& settings) : port(settings.port) {}
  int port;
};
struct endpoint {
  int port;
  double timeout;
};
// Only declared, here: neither its binding's declaration nor deduction can look at it.
struct journal;

}  // namespace wce

namespace {

namespace d = trellis::detail;
namespace e = trellis::error;
using trellis::bind;
using trellis::binding;
using trellis::ctor;
using trellis::make_injector;
using wce::config;

template <class B>
using plain = std::remove_cvref_t<B>;
template <class Injector, class R>
using node_t = typename d::plan<Injector, R>::node;
template <class Injector>
using root_t = d::context<Injector, trellis::unique_t>;

TEST(WithCtorEngine, BindingCarriesTheDeclarationWhereTheImplementationGoes) {
  // binding<Key, Impl, Lifetime, AllowCopy> keeps its four parts: a fifth would change how GCC
  // and Clang print every binding, declared or not. The class and the declaration read back.
  using declared = d::declared_impl<wce::service, ctor<const config&>>;
  static_assert(std::is_same_v<plain<decltype(bind<wce::service>.with_ctor<const config&>())>,
                               binding<wce::service, declared, trellis::unique_t>>);
  static_assert(std::is_same_v<d::impl_class_t<declared>, wce::service>);
  static_assert(std::is_same_v<d::impl_ctor_t<declared>, ctor<const config&>>);
  // Any other Impl is the class, or the object a binding holds, and declares nothing.
  static_assert(std::is_same_v<d::impl_class_t<wce::service>, wce::service>);
  static_assert(std::is_same_v<d::impl_ctor_t<wce::service>, d::no_ctor_declared>);
  static_assert(std::is_same_v<d::impl_class_t<const config>, const config>);
  // .to<C>() after .with_ctor<Args...>() keeps the declaration, for C.
  static_assert(
      std::is_same_v<
          decltype(bind<wce::sink>.with_ctor<config&&>().to<wce::sink>()),
          binding<wce::sink, d::declared_impl<wce::sink, ctor<config&&>>, trellis::unique_t>>);
  // The parameter types stay as declared, references and X&& included.
  static_assert(std::is_same_v<
                d::impl_ctor_t<d::binding_traits<
                    decltype(bind<wce::service>.with_ctor<config&&, const wce::logger&>())>::impl>,
                ctor<config&&, const wce::logger&>>);
}

TEST(WithCtorEngine, ServedBindingGivesTheClassAndTheDeclaration) {
  using injector_type =
      decltype(make_injector(bind<config>.to_value(config{}),
                             bind<wce::service>.with_ctor<const config&>().in(trellis::singleton)));
  using served = typename d::plan<injector_type, wce::service&>::served;
  static_assert(std::is_same_v<served::impl, wce::service>);
  static_assert(std::is_same_v<served::binding_ctor, ctor<const config&>>);
  static_assert(served::owned);
  // An unbound class and a binding without a declaration leave the decision to the class.
  static_assert(std::is_same_v<typename d::plan<injector_type, config>::served::binding_ctor,
                               d::no_ctor_declared>);
  static_assert(std::is_same_v<typename d::plan<injector_type, wce::logger>::served::binding_ctor,
                               d::no_ctor_declared>);
}

TEST(WithCtorEngine, NodeIsTheDeclaredConstructor) {
  using injector_type = decltype(make_injector(bind<config>.to_value(config{}),
                                               bind<wce::logger>.in(trellis::singleton),
                                               bind<wce::audited>.with_ctor<wce::sink&>()));
  static_assert(
      std::is_same_v<node_t<injector_type, wce::audited>,
                     d::declared_node<wce::audited, root_t<injector_type>, false, wce::sink&>>);
  static_assert(!root_t<injector_type>::can_resolve<wce::audited>);
  // Without the declaration, deduction finds the constructor whose parameters resolve.
  using deduced_injector = decltype(make_injector(bind<config>.to_value(config{}),
                                                  bind<wce::logger>.in(trellis::singleton)));
  static_assert(root_t<deduced_injector>::can_resolve<wce::audited>);
}

TEST(WithCtorEngine, ExplainPassFollowsTheDeclaredConstructor) {
  // The pass reads the declared parameters' checks: audited's parameter 1, sink&, fails, and
  // the stop is that request, with audited on the path, as for a type's own declaration.
  using injector_type = decltype(make_injector(bind<config>.to_value(config{}),
                                               bind<wce::logger>.in(trellis::singleton),
                                               bind<wce::audited>.with_ctor<wce::sink&>()));
  static_assert(d::failing_parameter<node_t<injector_type, wce::audited>> == 1);
  using stop = d::explain_stop<e::path<>, 0, wce::audited, root_t<injector_type>>;
  static_assert(stop::reports);
  static_assert(std::is_same_v<stop::path, e::path<wce::audited>>);
  static_assert(stop::parameter == 1);
  static_assert(std::is_same_v<stop::request, wce::sink&>);
  using report = d::own_report<injector_type, wce::sink&>;
  static_assert(std::is_same_v<report::error, e::E001<wce::sink, e::request<wce::sink&>>>);
}

TEST(WithCtorEngine, DeclarationThatLiesRaisesDiscoverysReport) {
  // The report names the binding's declaration, which discovery's own check raises for it
  // (discovery_report), unique to the class and the declaration: no injector in its scope.
  using lying = decltype(make_injector(bind<wce::service>.with_ctor<int>()));
  static_assert(d::failure_of<node_t<lying, wce::service>> == d::failure::discovery);
  using report = d::own_report<lying, wce::service>;
  static_assert(std::is_same_v<report::error, e::E002<wce::service, ctor<int>>>);
  static_assert(report::message == d::error_message::declaration);
  static_assert(std::is_void_v<report::scope>);
  static_assert(
      std::is_same_v<report::error, d::discovery_report<wce::service, ctor<int>,
                                                        d::default_max_ctor_arity>::error>);
  // An aggregate's declaration is checked with braces, which reject narrowing.
  using narrowing = decltype(make_injector(bind<wce::endpoint>.with_ctor<double, double>()));
  using aggregate_report = d::own_report<narrowing, wce::endpoint>;
  static_assert(
      std::is_same_v<aggregate_report::error, e::E002<wce::endpoint, ctor<double, double>>>);
  static_assert(aggregate_report::message == d::error_message::declaration_aggregate);
}

TEST(WithCtorEngine, DeclarationOfAnIncompleteClassIsReportedAsIncomplete) {
  // The declaration can't be checked, nor the class asked whether it is an aggregate: the report
  // is deduction's for an incomplete class, one error type for the class however it is declared.
  using declared = decltype(make_injector(bind<wce::journal>.with_ctor<int>()));
  static_assert(d::failure_of<node_t<declared, std::unique_ptr<wce::journal>>> ==
                d::failure::discovery);
  using report = d::own_report<declared, std::unique_ptr<wce::journal>>;
  static_assert(std::is_same_v<report::error, e::E002<wce::journal, d::no_ctor_declared>>);
  static_assert(report::message == d::error_message::incomplete);
  using deduced = decltype(make_injector());
  static_assert(
      std::is_same_v<d::own_report<deduced, std::unique_ptr<wce::journal>>::error, report::error>);
}

TEST(WithCtorEngine, SingletonSlotHoldsTheClass) {
  using declared_binding =
      plain<decltype(bind<wce::service>.with_ctor<const config&>().in(trellis::singleton))>;
  using inline_binding =
      plain<decltype(bind<wce::service>.with_ctor<const config&>().in(trellis::singleton_inline))>;
  static_assert(d::owns_object<declared_binding> && d::owns_object<inline_binding>);
  using shared_cell = decltype(d::slot_holder<0, declared_binding, trellis::threading::safe>::cell);
  using inline_cell = decltype(d::slot_holder<0, inline_binding, trellis::threading::safe>::cell);
  static_assert(
      std::is_same_v<shared_cell,
                     d::object_slot<d::shared_storage<wce::service>, trellis::threading::safe>>);
  static_assert(
      std::is_same_v<inline_cell,
                     d::object_slot<d::inline_storage<wce::service>, trellis::threading::safe>>);
}

}  // namespace
