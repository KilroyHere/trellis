// Tests for include/trellis/static/injector.hpp, static/binding_map.hpp, static/validate.hpp and
// dsl/options.hpp: make_injector, the flattened binding map, duplicate keys, options, and the
// request check against the request-form table.
//
// The request-check section runs every request form against a binding of every lifetime that
// exists in Phase 1, so each cell of the table (tests/unit/request_forms_test.cpp) is checked
// again end to end, together with .allow_copy() and constness. Compile errors are in
// tests/compile_fail/static/: TRL-E003, TRL-E005 and TRL-E012.
#include <trellis/dsl/request_forms.hpp>
#include <trellis/meta/type_map.hpp>
#include <trellis/static/binding_map.hpp>
#include <trellis/static/validate.hpp>
#include <trellis/trellis.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

namespace {

using trellis::bind;
using trellis::binding;
using trellis::make_injector;
using trellis::make_module;
using trellis::options;
using trellis::detail::binding_for_t;
using trellis::detail::form_verdict;
using trellis::detail::injector_traits;
using trellis::detail::request_check;
using trellis::detail::stored_binding;
using trellis::detail::type_list;
using trellis::detail::unbound;

struct repository {
  repository() = default;
  repository(const repository&) = delete;
  repository& operator=(const repository&) = delete;
  virtual ~repository() = default;
  virtual int save() = 0;
};
struct sql_repository final : repository {
  int save() override { return 1; }
};
struct config {
  int port = 0;
};
struct service {};
struct timer {};

// ---- options (plan §6.13) ---------------------------------------------------------------------

// The defaults, and designated initializers in declaration order.
static_assert(!options{}.strict && !options{}.eager && !options{}.cycle_check);
static_assert(options{}.threading == trellis::threading::safe);
static_assert(options{}.max_ctor_arity == 16);
constexpr options custom{.strict = true,
                         .eager = true,
                         .threading = trellis::threading::none,
                         .max_ctor_arity = 12,
                         .cycle_check = true};
static_assert(custom.strict && custom.eager && custom.cycle_check);
static_assert(custom.threading == trellis::threading::none && custom.max_ctor_arity == 12);
// A template argument: the options are part of the injector's type.
static_assert(injector_traits<decltype(make_injector<custom>())>::opts.max_ctor_arity == 12);
static_assert(!std::is_same_v<decltype(make_injector()), decltype(make_injector<custom>())>);
static_assert(
    std::is_same_v<decltype(make_injector<options{.eager = true}>()),
                   trellis::injector<options{false, true, trellis::threading::safe, 16, false}>>);

// ---- the injector's type: its arguments, not its bindings (ADR-0010) ---------------------------

using unique_timer = binding<timer, timer, trellis::unique_t>;
using singleton_service = binding<service, service, trellis::singleton_t>;
using singleton_repository = binding<repository, sql_repository, trellis::singleton_t>;

static_assert(std::is_same_v<decltype(make_injector(bind<timer>)),
                             trellis::injector<options{}, unique_timer>>);
// A named module keeps the type short, whatever it holds.
struct app_module : decltype(make_module(bind<timer>, bind<service>.in(trellis::singleton))){};
static_assert(std::is_same_v<decltype(make_injector(app_module{})),
                             trellis::injector<options{}, app_module>>);
static_assert(std::is_same_v<injector_traits<decltype(make_injector(app_module{}))>::bindings,
                             type_list<unique_timer, singleton_service>>);
// Lvalue and const arguments name the same injector type.
static_assert(std::is_same_v<decltype(make_injector(std::declval<const app_module&>())),
                             trellis::injector<options{}, app_module>>);

// An injector is neither copyable nor movable (plan §7.5).
using timer_injector = decltype(make_injector(bind<timer>));
static_assert(!std::is_copy_constructible_v<timer_injector> &&
              !std::is_move_constructible_v<timer_injector> &&
              !std::is_copy_assignable_v<timer_injector> &&
              !std::is_move_assignable_v<timer_injector>);

// ---- flattening and the binding map -----------------------------------------------------------

auto storage_module(const config& settings) {
  return make_module(bind<config>.to_ref(settings),
                     bind<repository>.to<sql_repository>().in(trellis::singleton));
}
auto logging_module() { return make_module(bind<service>.in(trellis::singleton)); }

using composed = decltype(make_injector(storage_module(std::declval<const config&>()),
                                        logging_module(), bind<timer>));
using composed_traits = injector_traits<composed>;
using external_config = binding<config, const config, trellis::external_t>;

// One flat list, in argument order (plan §5.3, step 1).
static_assert(
    std::is_same_v<composed_traits::bindings, type_list<external_config, singleton_repository,
                                                        singleton_service, unique_timer>>);
static_assert(!composed_traits::has_duplicate_keys);
// Each key finds its binding and where it is stored, by position; a key without a binding finds
// unbound<Key>.
static_assert(std::is_same_v<binding_for_t<composed_traits::map, config>,
                             stored_binding<0, external_config>>);
static_assert(std::is_same_v<binding_for_t<composed_traits::map, repository>,
                             stored_binding<1, singleton_repository>>);
static_assert(std::is_same_v<binding_for_t<composed_traits::map, service>,
                             stored_binding<2, singleton_service>>);
static_assert(
    std::is_same_v<binding_for_t<composed_traits::map, timer>, stored_binding<3, unique_timer>>);
static_assert(
    std::is_same_v<binding_for_t<composed_traits::map, sql_repository>, unbound<sql_repository>>);
// Keys are exact: the map holds config, not const config or config&.
static_assert(
    std::is_same_v<binding_for_t<composed_traits::map, const config>, unbound<const config>>);

// ---- duplicate keys (TRL-E003 is in tests/compile_fail/static/) --------------------------------

template <class... Parts>
inline constexpr bool duplicates =
    injector_traits<trellis::injector<options{}, Parts...>>::has_duplicate_keys;

static_assert(duplicates<unique_timer, unique_timer>);
static_assert(duplicates<unique_timer, binding<timer, timer, trellis::singleton_t>>);
static_assert(duplicates<decltype(make_module(bind<timer>)), decltype(make_module(bind<timer>))>);
static_assert(!duplicates<unique_timer, singleton_service>);
static_assert(!duplicates<>);

// When keys repeat, nothing builds or looks up the map: it is an empty one, and request_check
// answers keys_repeat for any request, bound or not, which reports nothing more than TRL-E003
// (tests/compile_fail/static/e003_then_*.cpp).
using repeating =
    trellis::injector<options{}, unique_timer, binding<timer, timer, trellis::singleton_t>>;
static_assert(std::is_same_v<injector_traits<repeating>::map, trellis::detail::type_map<>>);
static_assert(request_check<repeating, timer&>::verdict == form_verdict::keys_repeat);
static_assert(request_check<repeating, service>::verdict == form_verdict::keys_repeat);
static_assert(!request_check<repeating, timer>::allowed);

// Keys that differ only in & and &&, nested in a class template or a function type, are distinct
// (GCC 13 and 14 confused them before T1.1's fix; the map's lookups no longer deduce).
template <class T>
struct holder {};
using reference_keys =
    decltype(make_injector(bind<holder<int&>>.in(trellis::singleton), bind<holder<int&&>>,
                           bind<std::function<void(int&)>>.to_value(std::function<void(int&)>{}),
                           bind<std::function<void(int&&)>>.in(trellis::singleton)));
static_assert(!injector_traits<reference_keys>::has_duplicate_keys);
static_assert(
    std::is_same_v<binding_for_t<injector_traits<reference_keys>::map, holder<int&>>,
                   stored_binding<0, binding<holder<int&>, holder<int&>, trellis::singleton_t>>>);
static_assert(
    std::is_same_v<binding_for_t<injector_traits<reference_keys>::map, holder<int&&>>,
                   stored_binding<1, binding<holder<int&&>, holder<int&&>, trellis::unique_t>>>);
static_assert(duplicates<binding<holder<int&>, holder<int&>, trellis::unique_t>,
                         binding<holder<int&>, holder<int&>, trellis::singleton_t>>);
static_assert(!duplicates<binding<holder<int&>, holder<int&>, trellis::unique_t>,
                          binding<holder<int&&>, holder<int&&>, trellis::unique_t>>);

// first_duplicate names the first key bound twice and its first two bindings.
using duplicated = trellis::detail::first_duplicate<
    type_list<unique_timer, singleton_service, binding<timer, timer, trellis::singleton_t>,
              binding<service, service, trellis::unique_t>>>;
static_assert(std::is_same_v<duplicated::key, timer>);
static_assert(duplicated::first == 0 && duplicated::second == 2);
static_assert(std::is_same_v<duplicated::first_binding, unique_timer>);
static_assert(
    std::is_same_v<duplicated::second_binding, binding<timer, timer, trellis::singleton_t>>);

// ---- many bindings: no fold over them (Clang allows 256 operands per fold) ---------------------

template <std::size_t I>
struct key {};
template <std::size_t... Is>
auto many(std::index_sequence<Is...>) {
  return make_injector(bind<key<Is>>...);
}
using injector_300 = decltype(many(std::make_index_sequence<300>{}));
static_assert(!injector_traits<injector_300>::has_duplicate_keys);
static_assert(std::is_same_v<binding_for_t<injector_traits<injector_300>::map, key<299>>,
                             stored_binding<299, binding<key<299>, key<299>, trellis::unique_t>>>);
template <std::size_t... Is>
auto many_with_repeat(std::index_sequence<Is...>)
    -> trellis::injector<options{}, binding<key<Is>, key<Is>, trellis::unique_t>...,
                         binding<key<150>, key<150>, trellis::singleton_t>>;
using repeating_301 = decltype(many_with_repeat(std::make_index_sequence<300>{}));
static_assert(injector_traits<repeating_301>::has_duplicate_keys);
static_assert(trellis::detail::first_duplicate<injector_traits<repeating_301>::bindings>::first ==
              150);

// ---- the request check, end to end ------------------------------------------------------------

// One binding of each lifetime that Phase 1 has, and two unbound types; scoped bindings arrive in
// Phase 2, and their column is checked in request_forms_test.cpp.
struct unique_part {};
struct singleton_part {};
struct inline_part {};
struct copyable_part {};
struct external_part {};
struct const_external_part {};
struct shared_part {};
struct const_shared_part {};
struct value_part {};
struct unbound_part {};
struct unbound_interface {
  unbound_interface() = default;
  unbound_interface(const unbound_interface&) = delete;
  unbound_interface& operator=(const unbound_interface&) = delete;
  virtual ~unbound_interface() = default;
  virtual void run() = 0;
};

external_part an_external_part;
const const_external_part a_const_external_part{};

using all_lifetimes = decltype(make_injector(
    bind<unique_part>, bind<singleton_part>.in(trellis::singleton),
    bind<inline_part>.in(trellis::singleton_inline),
    bind<copyable_part>.in(trellis::singleton).allow_copy(),
    bind<external_part>.to_ref(an_external_part),
    bind<const_external_part>.to_ptr(&a_const_external_part),
    bind<shared_part>.to_shared(std::shared_ptr<shared_part>()),
    bind<const_shared_part>.to_shared(std::shared_ptr<const const_shared_part>()),
    bind<value_part>.to_value(value_part{})));

template <class R>
constexpr form_verdict verdict = request_check<all_lifetimes, R>::verdict;

// The forms of one key T, against the verdicts expected for its binding:
// T, T&, const T&, T*, const T*, unique_ptr<T>, shared_ptr<T>, weak_ptr<T>.
template <class T>
constexpr bool row(form_verdict value, form_verdict mutable_reference, form_verdict const_reference,
                   form_verdict mutable_pointer, form_verdict const_pointer, form_verdict unique,
                   form_verdict shared, form_verdict weak) {
  return verdict<T> == value && verdict<T&> == mutable_reference &&
         verdict<const T&> == const_reference && verdict<T*> == mutable_pointer &&
         verdict<const T*> == const_pointer && verdict<std::unique_ptr<T>> == unique &&
         verdict<std::shared_ptr<T>> == shared && verdict<std::weak_ptr<T>> == weak &&
         // T&& and const T&& are T (ADR-0003), and so are top-level cv qualifiers.
         verdict<T&&> == value && verdict<const T&&> == value && verdict<const T> == value;
}

constexpr form_verdict yes = form_verdict::allowed;
constexpr form_verdict copies = form_verdict::copies_hide_sharing;
constexpr form_verdict alive = form_verdict::nothing_keeps_it_alive;
constexpr form_verdict owner = form_verdict::ambiguous_ownership;
constexpr form_verdict twice = form_verdict::duplicates_ownership;
constexpr form_verdict heap = form_verdict::not_heap_owned;
constexpr form_verdict caller = form_verdict::caller_owns_it;
constexpr form_verdict constant = form_verdict::const_object;
constexpr form_verdict e012 = form_verdict::unbound_shared;

//                                 T       T&        const T&  T*        const T*  unique_ptr
//                                 shared_ptr  weak_ptr
static_assert(row<unique_part>(yes, alive, alive, owner, owner, yes, yes, alive));
static_assert(row<singleton_part>(copies, yes, yes, yes, yes, twice, yes, yes));
static_assert(row<inline_part>(copies, yes, yes, yes, yes, twice, heap, heap));
static_assert(row<copyable_part>(yes, yes, yes, yes, yes, twice, yes, yes));
static_assert(row<external_part>(copies, yes, yes, yes, yes, twice, caller, caller));
static_assert(row<const_external_part>(copies, constant, yes, constant, yes, twice, caller,
                                       caller));
static_assert(row<shared_part>(copies, yes, yes, yes, yes, twice, yes, yes));
static_assert(row<const_shared_part>(copies, constant, yes, constant, yes, twice, constant,
                                     constant));
static_assert(row<value_part>(yes, yes, yes, yes, yes, twice, heap, heap));
static_assert(row<unbound_part>(yes, e012, e012, e012, e012, yes, e012, e012));

// A const object can be requested through a pointer to const: shared_ptr<const T> and
// weak_ptr<const T> are allowed where shared_ptr<T> and weak_ptr<T> are refused.
static_assert(verdict<std::shared_ptr<const const_shared_part>> == yes);
static_assert(verdict<std::weak_ptr<const const_shared_part>> == yes);
static_assert(verdict<std::unique_ptr<const unique_part>> == yes);
static_assert(verdict<unique_part* const> == owner);

// An unbound abstract type has no implicit binding: nothing to check here (E001, T1.6).
static_assert(verdict<unbound_interface&> == form_verdict::no_binding);
static_assert(verdict<std::unique_ptr<unbound_interface>> == form_verdict::no_binding);
static_assert(!request_check<all_lifetimes, unbound_interface&>::allowed);

// A reference to a std::shared_ptr names the key std::shared_ptr<T>, which nothing binds here
// (ADR-0004's open question: T1.4 keeps the rejection).
static_assert(verdict<const std::shared_ptr<shared_part>&> == e012);

// report_request_form compiles to nothing for an allowed request, and for an unbound abstract
// type, which the explain pass reports (T1.6).
constexpr bool reports_nothing() {
  trellis::detail::report_request_form<all_lifetimes, singleton_part&>();
  trellis::detail::report_request_form<all_lifetimes, unique_part>();
  trellis::detail::report_request_form<all_lifetimes, unbound_interface&>();
  return true;
}
static_assert(reports_nothing());

TEST(Injector, HoldsTheFlattenedBindings) {
  const config settings{8080};
  auto injector = make_injector(storage_module(settings), logging_module(), bind<timer>);
  EXPECT_TRUE((std::is_same_v<decltype(injector), composed>));
}

TEST(Injector, TakesModulesByValueOrByReference) {
  const config settings{80};
  const auto storage = storage_module(settings);
  auto by_copy = make_injector(storage, bind<timer>);
  auto by_move = make_injector(storage_module(settings), bind<timer>);
  EXPECT_TRUE((std::is_same_v<decltype(by_copy), decltype(by_move)>));
}

TEST(Injector, AcceptsOptions) {
  auto injector = make_injector<options{.strict = true, .max_ctor_arity = 8}>(app_module{});
  EXPECT_EQ(injector_traits<decltype(injector)>::opts.max_ctor_arity, 8U);
  EXPECT_TRUE(injector_traits<decltype(injector)>::opts.strict);
}

TEST(Injector, ChecksRequestsAgainstTheTable) {
  EXPECT_EQ(verdict<unique_part&>, form_verdict::nothing_keeps_it_alive);
  EXPECT_EQ(verdict<singleton_part>, form_verdict::copies_hide_sharing);
  EXPECT_EQ(verdict<copyable_part>, form_verdict::allowed);
  EXPECT_EQ(verdict<const_external_part&>, form_verdict::const_object);
  EXPECT_EQ(verdict<unbound_part&>, form_verdict::unbound_shared);
  EXPECT_TRUE((request_check<all_lifetimes, std::shared_ptr<unique_part>>::allowed));
}

TEST(Injector, BuildsFromEveryKindOfBinding) {
  auto injector =
      make_injector(bind<unique_part>, bind<singleton_part>.in(trellis::singleton),
                    bind<inline_part>.in(trellis::singleton_inline),
                    bind<copyable_part>.in(trellis::singleton).allow_copy(),
                    bind<external_part>.to_ref(an_external_part),
                    bind<const_external_part>.to_ptr(&a_const_external_part),
                    bind<shared_part>.to_shared(std::make_shared<shared_part>()),
                    bind<const_shared_part>.to_shared(std::make_shared<const const_shared_part>()),
                    bind<value_part>.to_value(value_part{}));
  EXPECT_TRUE((std::is_same_v<decltype(injector), all_lifetimes>));
}

}  // namespace
