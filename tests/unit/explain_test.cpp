// Tests for the explain pass and the reports (T1.6; plan §7.8, ADR-0006 items 3 and 5): the first
// failing parameter of deduced and declared classes, a by-value owner of an incomplete class,
// where the pass stops and the path it builds, the lookahead that gives each step the next class's
// arity, the stop at a dependency cycle, the report each failure raises, the messages of the
// request-form table and of deduction, the error types' arguments that raise() spells out,
// TRL-E009's entry point, and eager mode's check in make_injector.
//
// What the compilers print is in tests/compile_fail/explain/ and its goldens; these tests check the
// type-level parts that the printing rests on. They are static_asserts, each in a TEST so that a
// failure points at a name.
#include <trellis/diagnostics/errors.hpp>
#include <trellis/diagnostics/explain.hpp>
#include <trellis/dsl/request_forms.hpp>
#include <trellis/meta/type_list.hpp>
#include <trellis/static/resolve.hpp>
#include <trellis/static/validate.hpp>
#include <trellis/trellis.hpp>

#include <memory>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

namespace tx {

struct sensor {};
struct repository {
  virtual ~repository() = default;
  [[nodiscard]] virtual int load() const = 0;
};
// Deduced, one parameter, which fails without a binding of sensor.
struct reader {
  explicit reader(sensor& input) : input_(&input) {}
  sensor* input_;
};
// Deduced, the second of three parameters fails, as an aggregate and as a class.
struct gauge {
  int scale;
  sensor& input;
  int offset;
};
struct meter {
  meter(int scale, sensor& input, int offset) : scale_(scale + offset), input_(&input) {}
  int scale_;
  sensor* input_;
};
// Declared, the second parameter fails without a binding of repository.
struct store {
  using trellis_ctor = trellis::ctor<int, std::shared_ptr<repository>>;
  store(int limit, std::shared_ptr<repository> source)
      : limit_(limit), source_(std::move(source)) {}
  int limit_;
  std::shared_ptr<repository> source_;
};
// Deduced, needs the declared store, which fails below it.
struct front {
  explicit front(store& data) : data_(&data) {}
  store* data_;
};
// Deduced, takes the deduced meter by value, whose second parameter fails.
struct panel {
  explicit panel(meter part) : part_(part) {}
  meter part_;
};
// Two singletons on a cycle, ring_a(ring_b&) and ring_b(ring_a&), and two whose second asks for
// std::unique_ptr of the first, a request the table rejects (TRL-E005): no cycle. Only declared:
// the tests ask which classes the requests' nodes build, never the checks, which a cycle leaves
// unreadable.
struct ring_b;
struct ring_a {
  explicit ring_a(ring_b& next);
};
struct ring_b {
  explicit ring_b(ring_a& next);
};
struct back_b;
struct back_a {
  explicit back_a(back_b& next);
};
struct back_b {
  explicit back_b(std::unique_ptr<back_a> owned);
};
// A class whose constructor deduction can't find.
class sealed {
  sealed() = default;
};
struct holder {
  explicit holder(sealed part) : part_(part) {}
  sealed part_;
};
struct plain {};
// A base class whose destructor isn't virtual, and a class derived from it.
struct base {
  int value = 0;
};
struct derived : base {
  derived() = default;
};
// Deduced, both parameters fail: the second owns a class that is only declared, by value. Only
// declared, as it is only ever probed.
struct missing;
struct owner {
  owner(sensor& input, std::unique_ptr<missing> part);
};

}  // namespace tx

namespace {

namespace d = trellis::detail;
namespace e = trellis::error;

using empty_injector = decltype(trellis::make_injector());
using scaled_injector = decltype(trellis::make_injector(trellis::bind<int>.to_value(3)));
using store_injector = decltype(trellis::make_injector(
    trellis::bind<int>.to_value(3), trellis::bind<tx::store>.in(trellis::singleton)));
using strict_injector = decltype(trellis::make_injector<trellis::options{.strict = true}>());
using base_injector = decltype(trellis::make_injector(trellis::bind<tx::base>.to<tx::derived>()));
using ring_injector =
    decltype(trellis::make_injector(trellis::bind<tx::ring_a>.in(trellis::singleton),
                                    trellis::bind<tx::ring_b>.in(trellis::singleton)));
using back_injector =
    decltype(trellis::make_injector(trellis::bind<tx::back_a>.in(trellis::singleton),
                                    trellis::bind<tx::back_b>.in(trellis::singleton)));

template <class Injector, class R>
using node_t = typename d::plan<Injector, R>::node;
template <class Injector>
using root_t = d::context<Injector, trellis::unique_t>;

TEST(Explain, FailingParameterIsTheFirstThatCantBeServed) {
  static_assert(d::failing_parameter<node_t<empty_injector, tx::reader>> == 1);
  static_assert(d::failing_parameter<node_t<scaled_injector, tx::meter>> == 2);
  static_assert(d::failing_parameter<node_t<scaled_injector, tx::gauge>> == 2);
  static_assert(d::failing_parameter<node_t<store_injector, tx::store&>> == 2);
  // Without a binding of int, by value, the first parameter fails first.
  static_assert(d::failing_parameter<node_t<empty_injector, tx::meter>> == 1);
}

TEST(Explain, RootFailingOnItsOwnStopsAtTheRoot) {
  using stop = d::explain_stop<e::path<>, 0, tx::sensor&, root_t<empty_injector>>;
  static_assert(stop::reports);
  static_assert(std::is_same_v<stop::path, e::path<>>);
  static_assert(stop::parameter == 0);
  static_assert(
      std::is_same_v<stop::type, d::explain_cause<e::path<>, 0, tx::sensor&, empty_injector>>);
  static_assert(std::is_same_v<d::explain_root<empty_injector, tx::sensor&>::type, stop::type>);
}

TEST(Explain, DeducedClassIsBuiltWithItsFailingParameter) {
  using stop = d::explain_stop<e::path<>, 0, tx::reader, root_t<empty_injector>>;
  static_assert(!stop::reports);
  static_assert(std::is_same_v<stop::path, e::path<tx::reader>>);
  static_assert(stop::parameter == 1);
  static_assert(std::is_same_v<stop::type,
                               d::explain_build<e::path<tx::reader>, 1, empty_injector,
                                                trellis::unique_t, std::index_sequence<0>, false>>);
}

TEST(Explain, DeclaredClassesAreFollowedWithoutAStepOfTheirOwn) {
  // front's parameter store& is a singleton with a declared constructor, whose second parameter
  // fails: the stop is that request, with store on the path, as front's operator reports it.
  using stop = d::explain_stop<e::path<tx::front>, 1, tx::store&, root_t<store_injector>>;
  static_assert(stop::reports);
  static_assert(std::is_same_v<stop::path, e::path<tx::front, tx::store>>);
  static_assert(stop::parameter == 2);
  static_assert(std::is_same_v<stop::request, std::shared_ptr<tx::repository>>);
}

TEST(Explain, LookaheadGivesTheArityOfTheNextClass) {
  // panel's parameter 1 is the deduced meter, which takes three parameters: the explain_arg at
  // that position carries std::make_index_sequence<3>, so that its operator builds meter itself.
  static_assert(std::is_same_v<d::next_indices_t<e::path<tx::panel>, 1, root_t<scaled_injector>,
                                                 std::index_sequence<0>, false>,
                               std::make_index_sequence<3>>);
  // reader's parameter 1, sensor&, reports: nothing to build.
  static_assert(std::is_same_v<d::next_indices_t<e::path<tx::reader>, 1, root_t<empty_injector>,
                                                 std::index_sequence<0>, false>,
                               std::index_sequence<>>);
  // front's parameter 1 leads through the declared store to a report: nothing to build.
  static_assert(std::is_same_v<d::next_indices_t<e::path<tx::front>, 1, root_t<store_injector>,
                                                 std::index_sequence<0>, false>,
                               std::index_sequence<>>);
}

TEST(Explain, ClassOnThePathStopsThePass) {
  // A request whose class is already on the path: a dependency cycle, which the normal pass has
  // reported. The pass stops there and reports nothing.
  static_assert(d::on_path<tx::reader, e::path<tx::front, tx::reader>>);
  static_assert(!d::on_path<tx::reader, e::path<>>);
  using again = d::explain_stop<e::path<tx::reader>, 1, tx::reader, root_t<empty_injector>>;
  static_assert(again::cycle);
  static_assert(again::reports);
  static_assert(std::is_same_v<again::type, d::explain_nothing>);
  // A class whose constructor requests a class on the path stops it too, before its parameters'
  // checks are read: with store on the path, front, which requests store&, is on the cycle.
  static_assert(d::cycles_back<tx::front, store_injector, e::path<tx::store, tx::front>,
                               std::index_sequence<0>, false>);
  using back = d::explain_stop<e::path<tx::store>, 1, tx::front, root_t<store_injector>>;
  static_assert(back::cycle);
  // Without store on the path, front is explained as usual.
  static_assert(!d::explain_stop<e::path<>, 0, tx::front, root_t<store_injector>>::cycle);
}

TEST(Explain, OnlyARequestThatBuildsAClassOnThePathIsACycle) {
  // A real cycle: ring_b's ring_a& builds ring_a, a class node on the path, so the normal pass's
  // check recursed into it and the pass stops.
  static_assert(std::is_same_v<d::request_class_t<ring_injector, tx::ring_a&>, tx::ring_a>);
  static_assert(d::cycles_back<tx::ring_b, ring_injector, e::path<tx::ring_a, tx::ring_b>,
                               std::index_sequence<0>, false>);
  // A request back to the path that its own plan rejects builds nothing, so it is no cycle: the
  // check never recursed, and the pass goes on to report it (TRL-E005; T1.6 review).
  static_assert(std::is_void_v<d::request_class_t<back_injector, std::unique_ptr<tx::back_a>>>);
  static_assert(!d::cycles_back<tx::back_b, back_injector, e::path<tx::back_a, tx::back_b>,
                                std::index_sequence<0>, false>);
  // So is a request that a binding's object serves.
  static_assert(std::is_void_v<d::request_class_t<scaled_injector, int>>);
}

TEST(Explain, RequestThatCanBeServedGivesAHarmlessStop) {
  // Overload resolution may look at such a request through an operator's default template
  // arguments; the stop must be well-formed, and the pass never runs it.
  using stop = d::explain_stop<e::path<>, 0, tx::plain, root_t<empty_injector>>;
  static_assert(stop::reports);
  static_assert(std::is_same_v<stop::path, e::path<>>);
}

TEST(Explain, OwnReportsNameTheErrorTypeAndScope) {
  using unbound = d::own_report<empty_injector, tx::sensor&>;
  static_assert(std::is_same_v<unbound::error, e::E012<tx::sensor, e::request<tx::sensor&>>>);
  static_assert(unbound::message == d::error_message::unbound_shared);
  static_assert(std::is_same_v<unbound::scope, empty_injector>);

  using abstract = d::own_report<empty_injector, std::shared_ptr<tx::repository>>;
  static_assert(
      std::is_same_v<abstract::error,
                     e::E001<tx::repository, e::request<std::shared_ptr<tx::repository>>>>);
  static_assert(abstract::message == d::error_message::no_binding);

  // Discovery's report is discovery's own, unique to the class: no injector in its scope.
  using sealed = d::own_report<empty_injector, tx::sealed>;
  static_assert(std::is_same_v<sealed::error, e::E002<tx::sealed, d::no_ctor_declared>>);
  static_assert(sealed::message == d::error_message::not_constructible);
  static_assert(std::is_void_v<sealed::scope>);
  using via_holder = d::explain_stop<e::path<tx::holder>, 1, tx::sealed, root_t<empty_injector>>;
  static_assert(via_holder::reports);

  // T1.5's strict mode and unsafe delete, through the pass too.
  using strict = d::own_report<strict_injector, tx::plain>;
  static_assert(std::is_same_v<strict::error, e::E013<tx::plain, e::request<tx::plain>>>);
  static_assert(strict::message == d::error_message::strict_unbound);
  static_assert(std::is_same_v<strict::scope, strict_injector>);
  using unsafe = d::own_report<base_injector, std::unique_ptr<tx::base>>;
  static_assert(
      std::is_same_v<unsafe::error,
                     e::E014<tx::base, e::request<std::unique_ptr<tx::base>>, tx::derived>>);
  static_assert(unsafe::message == d::error_message::unsafe_delete);
  static_assert(std::is_same_v<unsafe::scope, base_injector>);
}

TEST(Explain, OwnerOfAnIncompleteClassIsReportedFirst) {
  // The class can't be built with that parameter even from placeholders, so the pass stops at it
  // rather than at the first failing parameter, and reports discovery's TRL-E002 for the class.
  using node = node_t<empty_injector, tx::owner>;
  static_assert(d::failing_parameter<node> == 1);
  static_assert(d::has_owner<node>);
  static_assert(d::owner_parameter<node> == 2);
  using stop = d::explain_stop<e::path<>, 0, tx::owner, root_t<empty_injector>>;
  static_assert(!stop::reports);
  static_assert(stop::parameter == 2);
  static_assert(
      std::is_same_v<stop::type,
                     d::explain_build<e::path<tx::owner>, 2, empty_injector, trellis::unique_t,
                                      std::index_sequence<0, 1>, false>>);
  using report = d::own_report<empty_injector, std::unique_ptr<tx::missing>>;
  static_assert(std::is_same_v<report::error, e::E002<tx::missing, d::no_ctor_declared>>);
  static_assert(report::message == d::error_message::incomplete);
  // A class without such a parameter has none.
  static_assert(!d::has_owner<node_t<empty_injector, tx::reader>>);
  static_assert(d::owner_parameter<node_t<empty_injector, tx::reader>> == 0);
}

TEST(Explain, RequestFormMessagesFollowTheTable) {
  using v = d::form_verdict;
  using m = d::error_message;
  static_assert(d::request_form_message(v::copies_hide_sharing, false) == m::copies_hide_sharing);
  static_assert(d::request_form_message(v::nothing_keeps_it_alive, false) ==
                m::nothing_keeps_it_alive);
  static_assert(d::request_form_message(v::ambiguous_ownership, false) == m::ambiguous_ownership);
  static_assert(d::request_form_message(v::duplicates_ownership, false) == m::duplicates_ownership);
  static_assert(d::request_form_message(v::not_heap_owned, false) == m::not_heap_owned);
  static_assert(d::request_form_message(v::caller_owns_it, false) == m::caller_owns_it);
  static_assert(d::request_form_message(v::const_object, false) == m::const_object);
  static_assert(d::request_form_message(v::unbound_shared, false) == m::unbound_shared);
  static_assert(d::request_form_message(v::unbound_shared, true) == m::smart_pointer_key);
  static_assert(d::request_form_message(v::allowed, false) == m::none);
  static_assert(d::request_form_message(v::no_binding, false) == m::none);
  static_assert(d::request_form_message(v::keys_repeat, false) == m::none);
}

TEST(Explain, DeductionMessagesFollowTheStatus) {
  using s = d::deduction_status;
  using m = d::error_message;
  static_assert(d::deduction_message(s::deduced) == m::none);
  static_assert(d::deduction_message(s::greedy) == m::greedy);
  static_assert(d::deduction_message(s::array_member) == m::array_member);
  static_assert(d::deduction_message(s::undeducible_member) == m::undeducible_member);
  static_assert(d::deduction_message(s::above_bound) == m::above_bound);
  static_assert(d::deduction_message(s::abstract) == m::abstract);
  static_assert(d::deduction_message(s::not_a_class) == m::not_a_class);
  static_assert(d::deduction_message(s::incomplete) == m::incomplete);
  static_assert(d::deduction_message(s::not_constructible) == m::not_constructible);
}

TEST(Explain, ErrorArgumentsAreReadBack) {
  using one = d::error_args<e::E004<tx::plain>>;
  static_assert(std::is_same_v<one::first, tx::plain>);
  using two = d::error_args<e::E012<tx::sensor, e::request<tx::sensor&>>>;
  static_assert(std::is_same_v<two::second, e::request<tx::sensor&>>);
  using three = d::error_args<e::E005<tx::sensor, e::request<tx::sensor*>, trellis::unique_t>>;
  static_assert(std::is_same_v<three::third, trellis::unique_t>);
  using bound = d::error_args<e::E010<tx::plain, 16>>;
  static_assert(std::is_same_v<bound::first, tx::plain>);
  static_assert(bound::bound == 16);
}

TEST(Explain, OverrideTargetIsFoundAmongTheBindings) {
  using bindings = d::type_list<decltype(trellis::bind<tx::sensor>), decltype(trellis::bind<int>)>;
  static_assert(d::override_target<bindings, decltype(trellis::bind<int>.to_value(1))>::found);
  static_assert(!d::override_target<bindings, decltype(trellis::bind<tx::plain>)>::found);
  // Nothing is raised when the key is bound: the entry point is a constant expression then.
  static_assert((d::report_override_target<bindings, decltype(trellis::bind<tx::sensor>)>(), true));
}

TEST(Explain, EagerModeChecksEverySingletonInMakeInjector) {
  using failing = decltype(trellis::make_injector<trellis::options{.eager = true}>(
      trellis::bind<tx::plain>.in(trellis::singleton),
      trellis::bind<tx::reader>.in(trellis::singleton)));
  static_assert(d::eager_fails<true, failing>);
  static_assert(std::is_same_v<d::eager_request_t<failing>, tx::reader&>);
  // An injector that isn't eager looks at no singleton: its singletons fail only when requested.
  static_assert(!d::eager_fails<false, failing>);
  using building = decltype(trellis::make_injector<trellis::options{.eager = true}>(
      trellis::bind<tx::plain>.in(trellis::singleton)));
  static_assert(!d::eager_fails<true, building>);
}

}  // namespace
