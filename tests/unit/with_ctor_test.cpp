// Tests for a constructor declared on the binding, .with_ctor<Args...>() (T1.15; plan §4 ADR-03,
// §6.1, §7.2, ADR-0003 item 1), through public names only, so that they run through the C++20
// module too: the step returns one binding type, in every order the DSL accepts; create<T>() calls
// the declared constructor for every lifetime and request form the binding allows, through an
// interface, below another class, for an aggregate, eagerly and through an override; the
// declaration wins over the class's member alias, a ctor_traits specialization and deduction, for
// its binding only; and testing::wireable answers for a declaration that lies without an error.
//
// How the declaration travels, from the binding to discovery and to the explain pass, is in
// with_ctor_engine_test.cpp. What must not compile is in tests/compile_fail/dsl/with_ctor_*.cpp,
// tests/compile_fail/create/*with_ctor*.cpp and tests/compile_fail/explain/*with_ctor*.cpp.
#include <trellis/testing/wiring.hpp>
#include <trellis/trellis.hpp>

#include <memory>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

namespace wc {

struct config {
  int port = 0;
};
// Not an aggregate, whose int member deduction would ask for: built with no arguments.
struct logger {
  logger() = default;
  int lines = 0;
};

// Two constructors: deduction finds the longer one, and .with_ctor<const config&>() declares the
// other. arity() says which one built the object.
class service {
 public:
  explicit service(const config& settings) : arity_(1), port_(settings.port) {}
  service(const config& settings, logger& log) : arity_(2), port_(settings.port), log_(&log) {}
  [[nodiscard]] int arity() const { return arity_; }
  [[nodiscard]] int port() const { return port_; }
  [[nodiscard]] const logger* log() const { return log_; }

 private:
  int arity_;
  int port_;
  const logger* log_ = nullptr;
};

// An interface and an implementation with the same two constructors.
class sink {
 public:
  sink() = default;
  sink(const sink&) = delete;
  sink& operator=(const sink&) = delete;
  virtual ~sink() = default;
  [[nodiscard]] virtual int arity() const = 0;
};
class file_sink final : public sink {
 public:
  explicit file_sink(const config& /*settings*/) : arity_(1) {}
  file_sink(const config& /*settings*/, logger& /*log*/) : arity_(2) {}
  [[nodiscard]] int arity() const override { return arity_; }

 private:
  int arity_;
};

// The same two constructors, the longer one declared by the class itself: with a member alias,
// and with a ctor_traits specialization (below the namespace).
class aliased {
 public:
  using trellis_ctor = trellis::ctor<const config&, logger&>;
  explicit aliased(const config& /*settings*/) : arity_(1) {}
  aliased(const config& /*settings*/, logger& /*log*/) : arity_(2) {}
  [[nodiscard]] int arity() const { return arity_; }

 private:
  int arity_;
};
class specialized {
 public:
  explicit specialized(const config& /*settings*/) : arity_(1) {}
  specialized(const config& /*settings*/, logger& /*log*/) : arity_(2) {}
  [[nodiscard]] int arity() const { return arity_; }

 private:
  int arity_;
};

// A member alias that lies: no constructor takes an int, so the class can't be built as it
// declares itself (TRL-E002), unless its binding declares a constructor that exists.
class misdeclared {
 public:
  using trellis_ctor = trellis::ctor<int>;
  explicit misdeclared(const config& settings) : port_(settings.port) {}
  [[nodiscard]] int port() const { return port_; }

 private:
  int port_;
};

// A default constructor next to a longer one: deduction finds the longer one, and
// .with_ctor<>() declares the default one.
class defaulted {
 public:
  defaulted() : arity_(0) {}
  explicit defaulted(const config& /*settings*/) : arity_(1) {}
  [[nodiscard]] int arity() const { return arity_; }

 private:
  int arity_;
};

// An aggregate with an array member, which deduction refuses (TRL-E004, ADR-0003 item 5). A
// declaration builds it with braces, as discovery builds an aggregate, which elide the array's.
struct pair_of_ints {
  int values[2];  // NOLINT(modernize-avoid-c-arrays): the case under test
};

// A declared singleton below a deduced class, by reference and by value.
class front {
 public:
  explicit front(service& part) : part_(&part) {}
  [[nodiscard]] const service& part() const { return *part_; }

 private:
  service* part_;
};
class holder {
 public:
  explicit holder(service part) : part_(part) {}
  [[nodiscard]] const service& part() const { return part_; }

 private:
  service part_;
};

// Counts its constructions, to show that an eager injector builds it in make_injector.
class counted {
 public:
  explicit counted(const config& /*settings*/) : arity_(1) { ++built(); }
  counted(const config& /*settings*/, logger& /*log*/) : arity_(2) { ++built(); }
  [[nodiscard]] int arity() const { return arity_; }
  static int& built() {
    static int count = 0;
    return count;
  }

 private:
  int arity_;
};

}  // namespace wc

template <>
struct trellis::ctor_traits<wc::specialized> : trellis::ctor<const wc::config&, wc::logger&> {};

namespace {

using trellis::bind;
using trellis::make_injector;
using wc::config;
using wc::logger;
using wc::service;

// ---- the binding types: one per step (plan §8.1) ----------------------------------------------

// .with_ctor<Args...>() combines with .in() and .allow_copy() in any order, and with .to<C>()
// before or after it: every order gives the same binding type, and nothing accumulates.
using declared_sink =
    decltype(bind<wc::sink>.to<wc::file_sink>().with_ctor<const config&>().in(trellis::singleton));
static_assert(
    std::is_same_v<
        declared_sink,
        decltype(bind<wc::sink>.to<wc::file_sink>().in(trellis::singleton).with_ctor<const config&>())>);
static_assert(
    std::is_same_v<declared_sink,
                   decltype(bind<wc::sink>.with_ctor<const config&>().to<wc::file_sink>().in(
                       trellis::singleton))>);
static_assert(
    std::is_same_v<
        declared_sink,
        decltype(bind<wc::sink>.in(trellis::singleton).with_ctor<const config&>().to<wc::file_sink>())>);
static_assert(
    std::is_same_v<
        decltype(bind<wc::sink>.to<wc::file_sink>().with_ctor<const config&>().allow_copy().in(
            trellis::singleton)),
        decltype(bind<wc::sink>.allow_copy().to<wc::file_sink>().in(trellis::singleton).with_ctor<const config&>())>);
// A declaration is part of the type: other parameters, another type. The binding stays empty.
static_assert(!std::is_same_v<decltype(bind<service>.with_ctor<const config&>()),
                              decltype(bind<service>.with_ctor<config>())>);
static_assert(!std::is_same_v<decltype(bind<service>.with_ctor<const config&>()),
                              std::remove_cvref_t<decltype(bind<service>)>>);
static_assert(std::is_empty_v<declared_sink>);

// ---- create<T>(): the declared constructor, for every lifetime and request form ---------------

TEST(WithCtor, UniqueBindingCallsTheDeclaredConstructorInEveryForm) {
  auto injector =
      make_injector(bind<config>.to_value(config{8080}), bind<logger>.in(trellis::singleton),
                    bind<service>.with_ctor<const config&>());
  EXPECT_EQ(injector.create<service>().arity(), 1);
  EXPECT_EQ(injector.create<service>().port(), 8080);
  EXPECT_EQ(injector.create<const service>().arity(), 1);
  EXPECT_EQ(injector.create<service&&>().arity(), 1);
  EXPECT_EQ(injector.create<std::unique_ptr<service>>()->arity(), 1);
  EXPECT_EQ(injector.create<std::unique_ptr<const service>>()->arity(), 1);
  EXPECT_EQ(injector.create<std::shared_ptr<service>>()->arity(), 1);
  EXPECT_EQ(injector.create<std::shared_ptr<const service>>()->arity(), 1);
}

TEST(WithCtor, SingletonCallsTheDeclaredConstructorOnceForEveryForm) {
  auto injector =
      make_injector(bind<config>.to_value(config{1}), bind<logger>.in(trellis::singleton),
                    bind<service>.with_ctor<const config&>().in(trellis::singleton).allow_copy());
  service& object = injector.create<service&>();
  EXPECT_EQ(object.arity(), 1);
  EXPECT_EQ(&injector.create<const service&>(), &object);
  EXPECT_EQ(injector.create<service*>(), &object);
  EXPECT_EQ(injector.create<const service*>(), &object);
  EXPECT_EQ(injector.create<std::shared_ptr<service>>().get(), &object);
  EXPECT_EQ(injector.create<std::shared_ptr<const service>>().get(), &object);
  EXPECT_EQ(injector.create<std::weak_ptr<service>>().lock().get(), &object);
  const service copy = injector.create<service>();  // .allow_copy()
  EXPECT_EQ(copy.arity(), 1);
  EXPECT_NE(&copy, &object);
}

TEST(WithCtor, SingletonInlineCallsTheDeclaredConstructorOnceForEveryForm) {
  auto injector = make_injector(
      bind<config>.to_value(config{2}), bind<logger>.in(trellis::singleton),
      bind<service>.in(trellis::singleton_inline).allow_copy().with_ctor<const config&>());
  service& object = injector.create<service&>();
  EXPECT_EQ(object.arity(), 1);
  EXPECT_EQ(object.port(), 2);
  EXPECT_EQ(&injector.create<const service&>(), &object);
  EXPECT_EQ(injector.create<service*>(), &object);
  EXPECT_EQ(injector.create<const service*>(), &object);
  EXPECT_EQ(injector.create<service>().arity(), 1);  // .allow_copy()
}

TEST(WithCtor, InterfaceGetsItsImplementationsDeclaredConstructor) {
  auto unique = make_injector(bind<config>.to_value(config{}), bind<logger>.in(trellis::singleton),
                              bind<wc::sink>.to<wc::file_sink>().with_ctor<const config&>());
  EXPECT_EQ(unique.create<std::unique_ptr<wc::sink>>()->arity(), 1);
  EXPECT_EQ(unique.create<std::shared_ptr<wc::sink>>()->arity(), 1);

  auto shared = make_injector(
      bind<config>.to_value(config{}), bind<logger>.in(trellis::singleton),
      bind<wc::sink>.with_ctor<const config&>().to<wc::file_sink>().in(trellis::singleton));
  wc::sink& object = shared.create<wc::sink&>();
  EXPECT_EQ(object.arity(), 1);
  EXPECT_EQ(shared.create<const wc::sink*>(), &object);
  EXPECT_EQ(shared.create<std::shared_ptr<wc::sink>>().get(), &object);
  EXPECT_EQ(shared.create<std::weak_ptr<wc::sink>>().lock().get(), &object);

  auto inline_storage = make_injector(
      bind<config>.to_value(config{}), bind<logger>.in(trellis::singleton),
      bind<wc::sink>.to<wc::file_sink>().in(trellis::singleton_inline).with_ctor<const config&>());
  EXPECT_EQ(inline_storage.create<wc::sink&>().arity(), 1);
}

TEST(WithCtor, DeclaredParametersAreRequestsServedByTheBindings) {
  const config settings{9};
  auto injector = make_injector(bind<config>.to_ref(settings), bind<logger>.in(trellis::singleton),
                                bind<service>.with_ctor<const config&, logger&>());
  const service built = injector.create<service>();
  EXPECT_EQ(built.arity(), 2);
  EXPECT_EQ(built.port(), 9);
  EXPECT_EQ(built.log(), &injector.create<logger&>());
}

TEST(WithCtor, DeclaredClassBelowADeducedOne) {
  auto injector =
      make_injector(bind<config>.to_value(config{3}), bind<logger>.in(trellis::singleton),
                    bind<service>.with_ctor<const config&>().in(trellis::singleton));
  const wc::front by_reference = injector.create<wc::front>();
  EXPECT_EQ(&by_reference.part(), &injector.create<service&>());
  EXPECT_EQ(by_reference.part().arity(), 1);

  auto unique = make_injector(bind<config>.to_value(config{3}), bind<logger>.in(trellis::singleton),
                              bind<service>.with_ctor<const config&>());
  EXPECT_EQ(unique.create<wc::holder>().part().arity(), 1);
}

TEST(WithCtor, AggregateIsBuiltWithBraces) {
  auto injector =
      make_injector(bind<int>.to_value(7), bind<wc::pair_of_ints>.with_ctor<int, int>());
  const wc::pair_of_ints pair = injector.create<wc::pair_of_ints>();
  EXPECT_EQ(pair.values[0], 7);
  EXPECT_EQ(pair.values[1], 7);
  // Deduction refuses the array member; the declaration is the way to build it.
  static_assert(!trellis::testing::wireable<decltype(make_injector(bind<int>.to_value(7))),
                                            wc::pair_of_ints>);
}

TEST(WithCtor, EagerInjectorBuildsTheSingletonWithTheDeclaredConstructor) {
  wc::counted::built() = 0;
  auto injector = make_injector<trellis::options{.eager = true}>(
      bind<config>.to_value(config{}), bind<logger>.in(trellis::singleton),
      bind<wc::counted>.with_ctor<const config&>().in(trellis::singleton));
  EXPECT_EQ(wc::counted::built(), 1);
  EXPECT_EQ(injector.create<wc::counted&>().arity(), 1);
  EXPECT_EQ(wc::counted::built(), 1);
}

// A named module carries a declared binding as it carries any other (ADR-0010), also beside other
// arguments of make_injector.
auto make_service_module(const config& settings) {
  return trellis::make_module(bind<config>.to_value(settings),
                              bind<service>.with_ctor<const config&>().in(trellis::singleton));
}
struct service_module : decltype(make_service_module(std::declval<const config&>())){};

TEST(WithCtor, NamedModuleCarriesTheDeclaration) {
  auto alone = make_injector(service_module{make_service_module(config{5})});
  EXPECT_EQ(alone.create<service&>().arity(), 1);
  EXPECT_EQ(alone.create<service&>().port(), 5);
  auto beside = make_injector(service_module{make_service_module(config{6})},
                              bind<logger>.in(trellis::singleton));
  EXPECT_EQ(beside.create<service&>().arity(), 1);
  EXPECT_EQ(beside.create<wc::front>().part().port(), 6);
}

// ---- the declaration wins, for its binding only -----------------------------------------------

TEST(WithCtor, WinsOverTheMemberAlias) {
  auto declared =
      make_injector(bind<config>.to_value(config{}), bind<logger>.in(trellis::singleton),
                    bind<wc::aliased>.with_ctor<const config&>());
  EXPECT_EQ(declared.create<wc::aliased>().arity(), 1);
  auto by_alias =
      make_injector(bind<config>.to_value(config{}), bind<logger>.in(trellis::singleton));
  EXPECT_EQ(by_alias.create<wc::aliased>().arity(), 2);
}

TEST(WithCtor, WinsOverACtorTraitsSpecialization) {
  auto declared =
      make_injector(bind<config>.to_value(config{}), bind<logger>.in(trellis::singleton),
                    bind<wc::specialized>.with_ctor<const config&>());
  EXPECT_EQ(declared.create<wc::specialized>().arity(), 1);
  auto by_specialization =
      make_injector(bind<config>.to_value(config{}), bind<logger>.in(trellis::singleton));
  EXPECT_EQ(by_specialization.create<wc::specialized>().arity(), 2);
}

TEST(WithCtor, WinsOverDeduction) {
  auto declared =
      make_injector(bind<config>.to_value(config{}), bind<logger>.in(trellis::singleton),
                    bind<service>.with_ctor<const config&>(), bind<wc::defaulted>.with_ctor<>());
  EXPECT_EQ(declared.create<service>().arity(), 1);
  EXPECT_EQ(declared.create<wc::defaulted>().arity(), 0);
  auto deduced =
      make_injector(bind<config>.to_value(config{}), bind<logger>.in(trellis::singleton));
  EXPECT_EQ(deduced.create<service>().arity(), 2);
  EXPECT_EQ(deduced.create<wc::defaulted>().arity(), 1);
}

TEST(WithCtor, ReplacesATypeDeclarationThatLies) {
  using lying = decltype(make_injector(bind<config>.to_value(config{})));
  static_assert(!trellis::testing::wireable<lying, wc::misdeclared>);
  auto injector = make_injector(bind<config>.to_value(config{4}),
                                bind<wc::misdeclared>.with_ctor<const config&>());
  EXPECT_EQ(injector.create<wc::misdeclared>().port(), 4);
}

TEST(WithCtor, ConcernsOneBindingOfOneInjector) {
  // The class's own requests, through the implicit binding of an unbound class, keep its own
  // discovery: the declaration belongs to the binding of the interface.
  auto injector =
      make_injector(bind<config>.to_value(config{}), bind<logger>.in(trellis::singleton),
                    bind<wc::sink>.to<wc::file_sink>().with_ctor<const config&>());
  EXPECT_EQ(injector.create<std::unique_ptr<wc::sink>>()->arity(), 1);
  EXPECT_EQ(injector.create<wc::file_sink>().arity(), 2);
  // Another injector's binding of the same class declares nothing.
  auto other = make_injector(bind<config>.to_value(config{}), bind<logger>.in(trellis::singleton),
                             bind<wc::sink>.to<wc::file_sink>());
  EXPECT_EQ(other.create<std::unique_ptr<wc::sink>>()->arity(), 2);
}

// ---- overrides and testing::wireable ----------------------------------------------------------

TEST(WithCtor, OverrideDeclaresTheConstructorOfItsBinding) {
  auto injector =
      make_injector(bind<config>.to_value(config{}), bind<logger>.in(trellis::singleton),
                    bind<service>.in(trellis::singleton),
                    trellis::override<service>.with_ctor<const config&>().in(trellis::singleton));
  EXPECT_EQ(injector.create<service&>().arity(), 1);
  // An override without a declaration replaces a binding with one, and its declaration with it.
  auto undeclared =
      make_injector(bind<config>.to_value(config{}), bind<logger>.in(trellis::singleton),
                    bind<service>.with_ctor<const config&>(), trellis::override<service>);
  EXPECT_EQ(undeclared.create<service>().arity(), 2);
  auto interface =
      make_injector(bind<config>.to_value(config{}), bind<logger>.in(trellis::singleton),
                    bind<wc::sink>.to<wc::file_sink>(),
                    trellis::override<wc::sink>.to<wc::file_sink>().with_ctor<const config&>());
  EXPECT_EQ(interface.create<std::unique_ptr<wc::sink>>()->arity(), 1);
}

TEST(WithCtor, WireableAnswersForADeclarationThatLies) {
  using lying =
      decltype(make_injector(bind<config>.to_value(config{}), bind<service>.with_ctor<int>()));
  static_assert(!trellis::testing::wireable<lying, service>);
  static_assert(!trellis::testing::wireable<lying, wc::holder>);
  using right = decltype(make_injector(bind<config>.to_value(config{}),
                                       bind<service>.with_ctor<const config&>()));
  static_assert(trellis::testing::wireable<right, service>);
  static_assert(trellis::testing::wireable<right, wc::holder>);
  // An eager injector can't be made with a singleton whose declaration lies.
  using eager = decltype(make_injector<trellis::options{.eager = true}>(
      bind<config>.to_value(config{}), bind<service>.with_ctor<int>().in(trellis::singleton)));
  static_assert(!trellis::testing::wireable<eager, config>);
}

}  // namespace
