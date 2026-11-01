// Tests for injector::create<T>() and the injector's storage (T1.5; plan §5.3, §6.3, §7.5, §7.7,
// ADR-0005, ADR-0012): every request form the table allows, against every lifetime of Phase 1,
// with identity checks for sharing; deduced, declared and aggregate classes; singleton storage
// (std::shared_ptr and inline) and its sizes; teardown in reverse order of construction; retry
// after a throwing constructor; eager mode and its strong guarantee; strict mode;
// threading::none; and the check's node against discovery's deduced_constructible_v.
//
// Compile errors are in tests/compile_fail/create/, concurrency in threading_test.cpp, and the
// research scenario in scenario_test.cpp.
#include <trellis/discovery/deduced.hpp>
#include <trellis/meta/type_list.hpp>
#include <trellis/static/resolve.hpp>
#include <trellis/static/storage.hpp>
#include <trellis/trellis.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "opaque_host.hpp"

namespace {

using trellis::bind;
using trellis::make_injector;
using trellis::options;

// ---- a log of constructions and destructions --------------------------------------------------

std::vector<std::string>& events() {
  static std::vector<std::string> log;
  return log;
}

// Records "+name" when constructed and "-name" when destroyed. Neither copyable nor movable, like
// many services, so every test that builds one shows that creation needs no copy or move.
template <class Self>
struct recorded {
  recorded() { events().push_back("+" + std::string(Self::name)); }
  recorded(const recorded&) = delete;
  recorded(recorded&&) = delete;
  recorded& operator=(const recorded&) = delete;
  recorded& operator=(recorded&&) = delete;
  ~recorded() { events().push_back("-" + std::string(Self::name)); }
};

// The constructions in the log, in order, and the destructions, in order.
std::vector<std::string> marked(char mark) {
  std::vector<std::string> names;
  for (const std::string& event : events()) {
    if (event.front() == mark) {
      names.push_back(event.substr(1));
    }
  }
  return names;
}

std::vector<std::string> reversed(std::vector<std::string> names) {
  return {names.rbegin(), names.rend()};
}

class Create : public ::testing::Test {
 protected:
  void SetUp() override { events().clear(); }
};

// ---- request forms against lifetimes (plan §6.3) ----------------------------------------------

// One class per binding. The defaulted constructor keeps it from being an aggregate, whose int
// field deduction would inject (ADR-0003 item 5), so it is built with no arguments.
template <int Tag>
struct part {
  part() = default;
  int value = 0;
};
using unique_part = part<0>;
using singleton_part = part<1>;
using inline_part = part<2>;
using copyable_part = part<3>;
using external_part = part<4>;
using shared_part = part<5>;
using value_part = part<6>;
using unbound_part = part<7>;

TEST_F(Create, UniqueGivesANewObjectPerRequest) {
  auto injector = make_injector(bind<unique_part>);
  unique_part first = injector.create<unique_part>();
  unique_part second = injector.create<unique_part>();
  EXPECT_NE(&first, &second);
  std::unique_ptr<unique_part> owned = injector.create<std::unique_ptr<unique_part>>();
  std::unique_ptr<const unique_part> owned_const =
      injector.create<std::unique_ptr<const unique_part>>();
  std::shared_ptr<unique_part> shared = injector.create<std::shared_ptr<unique_part>>();
  std::shared_ptr<unique_part> other = injector.create<std::shared_ptr<unique_part>>();
  ASSERT_TRUE(owned && owned_const && shared && other);
  EXPECT_NE(owned.get(), injector.create<std::unique_ptr<unique_part>>().get());
  EXPECT_NE(shared.get(), other.get());  // the consumer is the sole owner
  EXPECT_EQ(shared.use_count(), 1);
  // T&& and const T are the request T (ADR-0003).
  EXPECT_TRUE((std::is_same_v<decltype(injector.create<unique_part&&>()), unique_part>));
  EXPECT_TRUE((std::is_same_v<decltype(injector.create<const unique_part>()), unique_part>));
}

TEST_F(Create, UnboundConcreteTypesAreImplicitlyUnique) {
  auto injector = make_injector();
  unbound_part first = injector.create<unbound_part>();
  std::unique_ptr<unbound_part> second = injector.create<std::unique_ptr<unbound_part>>();
  ASSERT_TRUE(second);
  EXPECT_NE(&first, second.get());
}

TEST_F(Create, SingletonGivesTheSameObjectInEverySharingForm) {
  auto injector = make_injector(bind<singleton_part>.in(trellis::singleton));
  singleton_part& object = injector.create<singleton_part&>();
  EXPECT_EQ(&injector.create<const singleton_part&>(), &object);
  EXPECT_EQ(injector.create<singleton_part*>(), &object);
  EXPECT_EQ(injector.create<const singleton_part*>(), &object);
  const std::shared_ptr<singleton_part> shared = injector.create<std::shared_ptr<singleton_part>>();
  EXPECT_EQ(shared.get(), &object);
  EXPECT_EQ(injector.create<std::shared_ptr<const singleton_part>>().get(), &object);
  EXPECT_EQ(shared.use_count(), 2);  // the injector's and this one: real shared ownership
  const std::weak_ptr<singleton_part> weak = injector.create<std::weak_ptr<singleton_part>>();
  EXPECT_EQ(weak.lock().get(), &object);
}

TEST_F(Create, SingletonInlineLivesInsideTheInjector) {
  auto injector = make_injector(bind<inline_part>.in(trellis::singleton_inline));
  inline_part& object = injector.create<inline_part&>();
  EXPECT_EQ(&injector.create<const inline_part&>(), &object);
  EXPECT_EQ(injector.create<inline_part*>(), &object);
  EXPECT_EQ(injector.create<const inline_part*>(), &object);
  const auto* begin = reinterpret_cast<const std::byte*>(&injector);
  const auto* address = reinterpret_cast<const std::byte*>(&object);
  EXPECT_TRUE(address >= begin && address < begin + sizeof(injector));  // no heap allocation
}

TEST_F(Create, AllowCopyMakesCopiesOfSharedObjects) {
  external_part external_object;
  external_object.value = 3;
  auto injector = make_injector(bind<copyable_part>.in(trellis::singleton).allow_copy(),
                                bind<external_part>.to_ref(external_object).allow_copy(),
                                bind<inline_part>.in(trellis::singleton_inline).allow_copy());
  copyable_part& shared = injector.create<copyable_part&>();
  shared.value = 5;
  copyable_part copy = injector.create<copyable_part>();
  EXPECT_NE(&copy, &shared);
  EXPECT_EQ(copy.value, 5);
  external_object.value = 7;
  const external_part external_copy = injector.create<external_part>();
  EXPECT_EQ(external_copy.value, 7);
  EXPECT_NE(&external_copy, &external_object);
  injector.create<inline_part&>().value = 9;
  EXPECT_EQ(injector.create<inline_part>().value, 9);
}

TEST_F(Create, ExternalObjectsAreTheCallers) {
  external_part object;
  const value_part const_object{};
  auto injector =
      make_injector(bind<external_part>.to_ref(object), bind<value_part>.to_ptr(&const_object));
  EXPECT_EQ(&injector.create<external_part&>(), &object);
  EXPECT_EQ(&injector.create<const external_part&>(), &object);
  EXPECT_EQ(injector.create<external_part*>(), &object);
  EXPECT_EQ(injector.create<const external_part*>(), &object);
  // Constness carries through: a const object is served through references to const only.
  EXPECT_EQ(&injector.create<const value_part&>(), &const_object);
  EXPECT_EQ(injector.create<const value_part*>(), &const_object);
}

TEST_F(Create, IncompleteTypesBoundByPointerOrReference) {
  // opaque_host::handle is only declared in this file. A binding of an object of it serves the
  // requests that need no copy: the plan asks whether the key can be copied for a by-value
  // request only (its by-value request is TRL-E002, compile_fail/create/
  // e002_copy_of_incomplete_object.cpp). Before T1.6's review it asked for every request, which
  // made each one a compiler error (T1.7's finding).
  opaque_host::handle& object = opaque_host::instance();
  opaque_host::handle* const address = std::addressof(object);
  auto by_pointer = make_injector(bind<opaque_host::handle>.to_ptr(address));
  EXPECT_EQ(by_pointer.create<opaque_host::handle*>(), address);
  EXPECT_EQ(by_pointer.create<const opaque_host::handle*>(), address);
  EXPECT_EQ(std::addressof(by_pointer.create<opaque_host::handle&>()), address);
  EXPECT_EQ(opaque_host::id(by_pointer.create<const opaque_host::handle&>()), 42);
  auto by_reference = make_injector(bind<opaque_host::handle>.to_ref(object));
  EXPECT_EQ(std::addressof(by_reference.create<opaque_host::handle&>()), address);
  EXPECT_EQ(by_reference.create<opaque_host::handle*>(), address);
}

TEST_F(Create, SharedExternalObjectsAreSharedWithTheCaller) {
  const auto given = std::make_shared<shared_part>();
  const auto given_const = std::make_shared<const value_part>();
  auto injector =
      make_injector(bind<shared_part>.to_shared(given), bind<value_part>.to_shared(given_const));
  EXPECT_EQ(&injector.create<shared_part&>(), given.get());
  EXPECT_EQ(injector.create<shared_part*>(), given.get());
  EXPECT_EQ(injector.create<std::shared_ptr<shared_part>>(), given);
  EXPECT_EQ(injector.create<std::weak_ptr<shared_part>>().lock(), given);
  EXPECT_EQ(given.use_count(), 2);  // this one and the binding's
  EXPECT_EQ(&injector.create<const value_part&>(), given_const.get());
  EXPECT_EQ(injector.create<std::shared_ptr<const value_part>>(), given_const);
}

TEST_F(Create, ValueBindingsHoldTheInjectorsCopy) {
  value_part original;
  original.value = 4;
  auto injector = make_injector(bind<value_part>.to_value(original), bind<int>.to_value(42));
  value_part& stored = injector.create<value_part&>();
  EXPECT_NE(&stored, &original);  // the binding holds a copy
  EXPECT_EQ(stored.value, 4);
  EXPECT_EQ(&injector.create<value_part&>(), &stored);
  EXPECT_EQ(injector.create<const value_part*>(), &stored);
  stored.value = 6;
  EXPECT_EQ(injector.create<value_part>().value, 6);  // a copy of the injector's object
  EXPECT_EQ(injector.create<int>(), 42);
  EXPECT_EQ(&injector.create<const int&>(), &injector.create<int&>());
}

// ---- interfaces and implementations -----------------------------------------------------------

struct shape {
  shape() = default;
  shape(const shape&) = delete;
  shape& operator=(const shape&) = delete;
  virtual ~shape() = default;
  [[nodiscard]] virtual int sides() const = 0;
};
struct square final : shape {
  [[nodiscard]] int sides() const override { return 4; }
};
struct triangle final : shape {
  [[nodiscard]] int sides() const override { return 3; }
};
struct polygon {
  polygon() = default;
  polygon(const polygon&) = default;
  polygon& operator=(const polygon&) = default;
  virtual ~polygon() = default;
  [[nodiscard]] virtual int corners() const { return 0; }
};
struct pentagon final : polygon {
  [[nodiscard]] int corners() const override { return 5; }
};

TEST_F(Create, InterfacesGetTheirImplementations) {
  auto injector = make_injector(bind<shape>.to<square>().in(trellis::singleton),
                                bind<triangle>.in(trellis::singleton_inline));
  EXPECT_EQ(injector.create<shape&>().sides(), 4);
  EXPECT_EQ(injector.create<std::shared_ptr<shape>>()->sides(), 4);
  EXPECT_EQ(injector.create<triangle&>().sides(), 3);

  auto fresh = make_injector(bind<shape>.to<triangle>());
  const std::unique_ptr<shape> first = fresh.create<std::unique_ptr<shape>>();
  const std::shared_ptr<shape> second = fresh.create<std::shared_ptr<shape>>();
  EXPECT_EQ(first->sides(), 3);
  EXPECT_EQ(second->sides(), 3);
  EXPECT_NE(first.get(), second.get());
}

TEST_F(Create, ABaseRequestedByValueIsACopyOfANewImplementation) {
  // As a copy of a derived object as its base always does, this slices: the consumer asked for a
  // polygon, not for a pointer to one (as for .to_value(), dsl/bind.hpp).
  auto injector = make_injector(bind<polygon>.to<pentagon>());
  const polygon sliced = injector.create<polygon>();
  EXPECT_EQ(sliced.corners(), 0);
  EXPECT_EQ(injector.create<std::unique_ptr<polygon>>()->corners(), 5);
}

// A base class whose destructor isn't virtual. std::unique_ptr<plain_base> holding a new
// plain_derived would delete it through plain_base, which is undefined behaviour, so that request
// is TRL-E014 (compile_fail/create/e014_unique_ptr_without_virtual_destructor.cpp). Every other
// form destroys the plain_derived itself: a std::shared_ptr's control block deletes the class it
// was made for, a slot destroys its own class, and a by-value request is a sliced copy.
struct plain_base {
  plain_base() = default;
  plain_base(const plain_base&) = default;
  plain_base& operator=(const plain_base&) = default;
  plain_base(plain_base&&) = default;
  plain_base& operator=(plain_base&&) = default;
  ~plain_base() = default;  // not virtual
  int id = 1;
};
struct plain_derived : plain_base, recorded<plain_derived> {
  static constexpr const char* name = "plain_derived";
  plain_derived() = default;
};
using plain_injector = decltype(make_injector(bind<plain_base>.to<plain_derived>()));
using plain_context = trellis::detail::context<plain_injector, trellis::unique_t>;
static_assert(!plain_context::can_resolve<std::unique_ptr<plain_base>>);    // TRL-E014
static_assert(plain_context::can_resolve<std::unique_ptr<plain_derived>>);  // its own class
static_assert(plain_context::can_resolve<std::shared_ptr<plain_base>>);
static_assert(plain_context::can_resolve<plain_base>);

TEST_F(Create, NonVirtualBaseFormsDestroyTheDerivedClass) {
  auto injector = make_injector(bind<plain_base>.to<plain_derived>());
  {
    const std::shared_ptr<plain_base> shared = injector.create<std::shared_ptr<plain_base>>();
    EXPECT_EQ(shared->id, 1);
    EXPECT_TRUE(marked('-').empty());
  }
  EXPECT_EQ(marked('-'), std::vector<std::string>{"plain_derived"});  // by the control block
  {
    const plain_base sliced = injector.create<plain_base>();  // the new object, sliced
    EXPECT_EQ(sliced.id, 1);
  }
  EXPECT_EQ(marked('-').size(), 2U);
  {
    auto shared = make_injector(bind<plain_base>.to<plain_derived>().in(trellis::singleton));
    plain_base& object = shared.create<plain_base&>();
    EXPECT_EQ(shared.create<std::shared_ptr<plain_base>>().get(), &object);
    auto in_place =
        make_injector(bind<plain_base>.to<plain_derived>().in(trellis::singleton_inline));
    EXPECT_EQ(&in_place.create<plain_base&>(), &in_place.create<plain_base&>());
  }
  EXPECT_EQ(marked('+').size(), 4U);
  EXPECT_EQ(marked('-').size(), 4U);  // each object as a plain_derived, once
}

// A singleton shared through a base at an offset (static/storage.hpp, shared_storage::share): the
// slot keeps a std::shared_ptr<void> to the class, and each request converts the class's address to
// its own element type, so a base that isn't first, or is virtual, gets its own subobject, owned
// with the slot's control block.
struct first_base {
  first_base() = default;
  first_base(const first_base&) = delete;
  first_base& operator=(const first_base&) = delete;
  first_base(first_base&&) = delete;
  first_base& operator=(first_base&&) = delete;
  virtual ~first_base() = default;
  int first = 1;
};
struct second_base {
  second_base() = default;
  second_base(const second_base&) = delete;
  second_base& operator=(const second_base&) = delete;
  second_base(second_base&&) = delete;
  second_base& operator=(second_base&&) = delete;
  virtual ~second_base() = default;
  [[nodiscard]] virtual int id() const { return 0; }
  int second = 2;
};
struct two_bases final : first_base, second_base, recorded<two_bases> {
  static constexpr const char* name = "two_bases";
  two_bases() = default;
  [[nodiscard]] int id() const override { return 42; }
};

TEST_F(Create, ASingletonIsSharedThroughABaseThatIsntFirst) {
  std::shared_ptr<const second_base> held;
  std::weak_ptr<second_base> observed;
  {
    auto injector = make_injector(bind<second_base>.to<two_bases>().in(trellis::singleton));
    second_base& object = injector.create<second_base&>();
    ASSERT_NE(static_cast<void*>(&object), dynamic_cast<void*>(&object));  // at an offset
    const std::shared_ptr<second_base> shared = injector.create<std::shared_ptr<second_base>>();
    held = injector.create<std::shared_ptr<const second_base>>();
    observed = injector.create<std::weak_ptr<second_base>>();
    EXPECT_EQ(shared.get(), &object);
    EXPECT_EQ(held.get(), &object);
    EXPECT_EQ(observed.lock().get(), &object);
    EXPECT_EQ(shared->id(), 42);
    EXPECT_FALSE(shared.owner_before(observed) || observed.owner_before(shared));
    EXPECT_EQ(shared.use_count(), 3);  // the slot's, shared and held
  }
  EXPECT_TRUE(marked('-').empty());  // teardown dropped the injector's reference only
  EXPECT_EQ(held->second, 2);
  EXPECT_EQ(observed.lock().get(), held.get());
  held.reset();
  EXPECT_TRUE(observed.expired());
  EXPECT_EQ(marked('-'), std::vector<std::string>{"two_bases"});  // as itself, once
}

struct shared_root {
  shared_root() = default;
  shared_root(const shared_root&) = delete;
  shared_root& operator=(const shared_root&) = delete;
  shared_root(shared_root&&) = delete;
  shared_root& operator=(shared_root&&) = delete;
  virtual ~shared_root() = default;
  int root = 3;
};
struct left_branch : virtual shared_root {
  int left = 4;
};
struct right_branch : virtual shared_root {
  int right = 5;
};
struct diamond final : left_branch, right_branch, recorded<diamond> {
  static constexpr const char* name = "diamond";
  diamond() = default;
};

TEST_F(Create, ASingletonIsSharedThroughAVirtualBase) {
  std::shared_ptr<shared_root> held;
  std::weak_ptr<const shared_root> observed;
  {
    auto injector = make_injector(bind<shared_root>.to<diamond>().in(trellis::singleton));
    shared_root& object = injector.create<shared_root&>();
    held = injector.create<std::shared_ptr<shared_root>>();
    observed = injector.create<std::weak_ptr<const shared_root>>();
    EXPECT_EQ(held.get(), &object);
    EXPECT_EQ(observed.lock().get(), &object);
    EXPECT_NE(dynamic_cast<diamond*>(held.get()), nullptr);
    EXPECT_EQ(held->root, 3);
    EXPECT_FALSE(held.owner_before(observed) || observed.owner_before(held));
  }
  EXPECT_TRUE(marked('-').empty());
  EXPECT_FALSE(observed.expired());
  held.reset();
  EXPECT_TRUE(observed.expired());
  EXPECT_EQ(marked('-'), std::vector<std::string>{"diamond"});
  // A unique binding requested as std::shared_ptr takes the same conversion.
  auto fresh = make_injector(bind<shared_root>.to<diamond>());
  const std::shared_ptr<shared_root> made = fresh.create<std::shared_ptr<shared_root>>();
  EXPECT_NE(dynamic_cast<diamond*>(made.get()), nullptr);
  EXPECT_EQ(made->root, 3);
  EXPECT_EQ(made.use_count(), 1);
}

// ---- how classes are built --------------------------------------------------------------------

struct clock_part {
  clock_part() = default;
  int ticks = 1;
};
struct settings {
  std::string name;
  int port = 0;
};

// A deduced class with a parameter in every form the table allows for its bindings.
struct consumer {
  consumer(clock_part by_value, std::unique_ptr<clock_part> owned, singleton_part& shared,
           const settings& config, std::shared_ptr<singleton_part> counted,
           std::weak_ptr<singleton_part> observed, singleton_part* pointer, int number)
      : ticks(by_value.ticks),
        owned_(std::move(owned)),
        shared_(&shared),
        config_(&config),
        counted_(std::move(counted)),
        observed_(std::move(observed)),
        pointer_(pointer),
        number_(number) {}
  int ticks;
  std::unique_ptr<clock_part> owned_;
  singleton_part* shared_;
  const settings* config_;
  std::shared_ptr<singleton_part> counted_;
  std::weak_ptr<singleton_part> observed_;
  singleton_part* pointer_;
  int number_;
};

TEST_F(Create, DeducedParametersAreResolvedInEveryForm) {
  const settings config{"db", 5432};
  auto injector = make_injector(bind<singleton_part>.in(trellis::singleton),
                                bind<settings>.to_ref(config), bind<int>.to_value(7));
  const consumer built = injector.create<consumer>();
  singleton_part& shared = injector.create<singleton_part&>();
  EXPECT_EQ(built.ticks, 1);
  ASSERT_TRUE(built.owned_);
  EXPECT_EQ(built.shared_, &shared);
  EXPECT_EQ(built.config_, &config);
  EXPECT_EQ(built.counted_.get(), &shared);
  EXPECT_EQ(built.observed_.lock().get(), &shared);
  EXPECT_EQ(built.pointer_, &shared);
  EXPECT_EQ(built.number_, 7);
}

// Declared constructors: a ctor_traits specialization, which chooses among overloads, and a
// member alias.
struct overloaded {
  overloaded(const settings& config, int number) : port(config.port + number) {}
  overloaded(const settings& config, double /*unused*/) : port(config.port) {}
  int port;
};
struct aliased {
  using trellis_ctor = trellis::ctor<const settings&, std::unique_ptr<clock_part>>;
  aliased(const settings& config, std::unique_ptr<clock_part> owned)
      : port(config.port), ticks(owned->ticks) {}
  int port;
  int ticks;
};
// An aggregate: deduced with braces, one placeholder per field, and built the same way.
struct aggregate {
  const settings& config;
  clock_part clock;
  int number;
};
// An aggregate with a declaration, built with braces too (ADR-0003).
struct declared_aggregate {
  int first;
  int second;
};

}  // namespace

template <>
struct trellis::ctor_traits<overloaded> : trellis::ctor<const settings&, int> {};
template <>
struct trellis::ctor_traits<declared_aggregate> : trellis::ctor<int, int> {};

namespace {

TEST_F(Create, DeclaredConstructorsAndAggregatesAreBuiltAsDiscovered) {
  const settings config{"db", 100};
  auto injector = make_injector(bind<settings>.to_ref(config), bind<int>.to_value(5),
                                bind<overloaded>.in(trellis::singleton));
  EXPECT_EQ(injector.create<const overloaded&>().port, 105);
  EXPECT_EQ(injector.create<overloaded&>().port, 105);
  const aliased by_alias = injector.create<aliased>();
  EXPECT_EQ(by_alias.port, 100);
  EXPECT_EQ(by_alias.ticks, 1);
  const aggregate fields = injector.create<aggregate>();
  EXPECT_EQ(&fields.config, &config);
  EXPECT_EQ(fields.number, 5);
  const declared_aggregate pair = injector.create<declared_aggregate>();
  EXPECT_EQ(pair.first, 5);
  EXPECT_EQ(pair.second, 5);
  EXPECT_EQ(injector.create<std::unique_ptr<aggregate>>()->number, 5);
}

// A bound below the default still finds constructors within it (TRL-E010 is in compile_fail/).
struct two_parameters {
  two_parameters(int first, int second) : sum(first + second) {}
  int sum;
};

TEST_F(Create, MaxCtorArityIsTheInjectorsBound) {
  auto injector = make_injector<options{.max_ctor_arity = 2}>(bind<int>.to_value(3));
  EXPECT_EQ(injector.create<two_parameters>().sum, 6);
}

// ---- storage and teardown (ADR-0005) ----------------------------------------------------------

struct leaf : recorded<leaf> {
  static constexpr const char* name = "leaf";
  leaf() = default;
};
struct middle : recorded<middle> {
  static constexpr const char* name = "middle";
  explicit middle(leaf& dependency) : leaf_(&dependency) {}
  leaf* leaf_;
};
struct helper : recorded<helper> {
  static constexpr const char* name = "helper";
  helper() = default;
};
struct top : recorded<top> {
  static constexpr const char* name = "top";
  top(middle& first, std::unique_ptr<helper> owned, leaf& second)
      : middle_(&first), helper_(std::move(owned)), leaf_(&second) {}
  middle* middle_;
  std::unique_ptr<helper> helper_;
  leaf* leaf_;
};
struct alone : recorded<alone> {
  static constexpr const char* name = "alone";
  alone() = default;
};

TEST_F(Create, TeardownReversesTheOrderOfConstruction) {
  {
    auto injector = make_injector(
        bind<leaf>.in(trellis::singleton), bind<middle>.in(trellis::singleton_inline),
        bind<top>.in(trellis::singleton), bind<helper>, bind<alone>.in(trellis::singleton_inline));
    EXPECT_TRUE(events().empty());  // lazy: nothing is built before it is requested
    static_cast<void>(injector.create<alone&>());
    top& object = injector.create<top&>();
    EXPECT_EQ(object.middle_->leaf_, object.leaf_);
    EXPECT_EQ(&injector.create<leaf&>(), object.leaf_);
    // Dependencies come first, whatever the order of argument evaluation.
    const std::vector<std::string> built = marked('+');
    ASSERT_EQ(built.size(), 5U);
    EXPECT_EQ(built.front(), "alone");
    EXPECT_EQ(built.back(), "top");
    EXPECT_TRUE(marked('-').empty());
  }
  // The singletons are destroyed in exactly the reverse order of construction; the unique helper
  // belongs to top and goes with it.
  std::vector<std::string> singletons;
  for (const std::string& name : marked('+')) {
    if (name != "helper") {
      singletons.push_back(name);
    }
  }
  std::vector<std::string> destroyed;
  for (const std::string& name : marked('-')) {
    if (name != "helper") {
      destroyed.push_back(name);
    }
  }
  EXPECT_EQ(destroyed, reversed(singletons));
  EXPECT_EQ(events().size(), 10U);
}

TEST_F(Create, SingletonsArePerInjector) {
  auto first = make_injector(bind<leaf>.in(trellis::singleton));
  auto second = make_injector(bind<leaf>.in(trellis::singleton));
  EXPECT_NE(&first.create<leaf&>(), &second.create<leaf&>());
  EXPECT_EQ(marked('+').size(), 2U);
}

TEST_F(Create, ASharedPtrOutlivesTheInjector) {
  std::shared_ptr<leaf> held;
  std::weak_ptr<leaf> observed;
  {
    auto injector = make_injector(bind<leaf>.in(trellis::singleton));
    held = injector.create<std::shared_ptr<leaf>>();
    observed = injector.create<std::weak_ptr<leaf>>();
  }
  EXPECT_TRUE(marked('-').empty());  // teardown dropped the injector's reference only
  EXPECT_FALSE(observed.expired());
  held.reset();
  EXPECT_TRUE(observed.expired());
  EXPECT_EQ(marked('-'), std::vector<std::string>{"leaf"});
}

// Over-aligned classes (static/storage.hpp): a std::shared_ptr slot and a unique std::shared_ptr
// request get the object from the holder in std::make_shared's control block, an inline slot
// from the injector's own bytes, and std::unique_ptr from new, each at the class's alignment. The
// class is as large as its alignment, so that it needs no padding of its own.
struct alignas(128) aligned_part {
  aligned_part() = default;
  std::array<unsigned char, 128> bytes{};
};
bool aligned_to_128(const void* address) {
  return reinterpret_cast<std::uintptr_t>(address) % 128 == 0;
}

TEST_F(Create, OverAlignedClassesKeepTheirAlignment) {
  auto shared = make_injector(bind<aligned_part>.in(trellis::singleton));
  EXPECT_TRUE(aligned_to_128(&shared.create<aligned_part&>()));
  EXPECT_TRUE(aligned_to_128(shared.create<std::shared_ptr<aligned_part>>().get()));
  auto in_place = make_injector(bind<aligned_part>.in(trellis::singleton_inline));
  EXPECT_TRUE(aligned_to_128(&in_place.create<aligned_part&>()));
  auto fresh = make_injector(bind<aligned_part>);
  EXPECT_TRUE(aligned_to_128(fresh.create<std::unique_ptr<aligned_part>>().get()));
  EXPECT_TRUE(aligned_to_128(fresh.create<std::shared_ptr<aligned_part>>().get()));
}

// std::enable_shared_from_this (static/storage.hpp, construct_with): the std::shared_ptr is made
// with the object by std::allocate_shared, rather than adopting it, so shared_from_this() shares
// its control block, for a singleton reached through an interface and for a unique std::shared_ptr
// request alike.
struct self_source {
  self_source() = default;
  self_source(const self_source&) = delete;
  self_source& operator=(const self_source&) = delete;
  virtual ~self_source() = default;
  [[nodiscard]] virtual std::shared_ptr<self_source> self() = 0;
};
struct self_sharing final : self_source, std::enable_shared_from_this<self_sharing> {
  [[nodiscard]] std::shared_ptr<self_source> self() override { return shared_from_this(); }
};

TEST_F(Create, EnableSharedFromThisSharesTheControlBlock) {
  auto shared = make_injector(bind<self_source>.to<self_sharing>().in(trellis::singleton));
  const std::shared_ptr<self_source> held = shared.create<std::shared_ptr<self_source>>();
  const std::shared_ptr<self_source> again = shared.create<self_source&>().self();
  EXPECT_EQ(again.get(), held.get());
  EXPECT_FALSE(again.owner_before(held) || held.owner_before(again));  // one control block
  EXPECT_EQ(held.use_count(), 3);                                      // the slot, held and again

  auto fresh = make_injector(bind<self_source>.to<self_sharing>());
  const std::shared_ptr<self_source> owned = fresh.create<std::shared_ptr<self_source>>();
  const std::shared_ptr<self_source> self = owned->self();
  EXPECT_EQ(self.get(), owned.get());
  EXPECT_EQ(owned.use_count(), 2);
  EXPECT_NE(fresh.create<std::shared_ptr<self_source>>()->self().get(), owned.get());
}

TEST_F(Create, UnrequestedSingletonsAreNeverBuilt) {
  {
    auto injector =
        make_injector(bind<leaf>.in(trellis::singleton), bind<middle>.in(trellis::singleton));
  }
  EXPECT_TRUE(events().empty());
}

// Spike S4's layout (ADR-0005 items 2 to 4), on 64-bit targets.
struct one_byte {
  char value;
};
struct eight_bytes {
  std::uint64_t value;
};
struct sixty_four_bytes {
  std::array<char, 64> value;
};
template <class Storage, trellis::threading Mode>
using slot_t = trellis::detail::object_slot<Storage, Mode>;
using trellis::threading;
using trellis::detail::inline_storage;
using trellis::detail::shared_storage;
static_assert(sizeof(slot_t<inline_storage<one_byte>, threading::safe>) == 2);
static_assert(sizeof(slot_t<inline_storage<eight_bytes>, threading::safe>) == 16);
static_assert(sizeof(slot_t<inline_storage<sixty_four_bytes>, threading::safe>) == 65);
static_assert(sizeof(slot_t<inline_storage<eight_bytes>, threading::none>) == 16);
static_assert(sizeof(void*) != 8 ||
              sizeof(slot_t<shared_storage<eight_bytes>, threading::safe>) == 24);
static_assert(sizeof(void*) != 8 || sizeof(trellis::detail::teardown_stack<100, true>) == 1608);
// Bindings the injector doesn't own take no space in its slots (GCC and Clang).
using unowned_slots = trellis::detail::storage_for<
    threading::safe, trellis::detail::type_list<trellis::binding<int, int, trellis::unique_t>,
                                                trellis::binding<long, long, trellis::unique_t>>>;
static_assert(unowned_slots::owned == 0);
static_assert(std::is_empty_v<unowned_slots::slots>);
// One byte, not one per binding: TRELLIS_EMPTY_BASES gives MSVC the layout GCC and Clang have, and
// TRELLIS_NO_UNIQUE_ADDRESS does the same for a module of empty bindings (config.hpp).
static_assert(sizeof(unowned_slots::slots) == 1);
using empty_module =
    decltype(trellis::make_module(bind<part<20>>, bind<part<21>>.in(trellis::singleton),
                                  bind<part<22>>.in(trellis::singleton_inline)));
static_assert(sizeof(empty_module) == 1);

// A singleton's std::shared_ptr shares one control block type with the classes of its size, unless
// its class shares from this, which takes std::allocate_shared<T> (static/storage.hpp,
// make_shared_object, T1.14): shares_from_this holds exactly where std::shared_ptr sets up
// enable_shared_from_this, for an unambiguous and accessible base ([util.smartptr.shared.const]/1).
struct shares_publicly : std::enable_shared_from_this<shares_publicly> {};
struct shares_through_a_base : shares_publicly {};
struct shares_privately : private std::enable_shared_from_this<shares_privately> {};
struct shares_ambiguously : shares_publicly, std::enable_shared_from_this<shares_ambiguously> {};
using trellis::detail::shares_from_this;
static_assert(shares_from_this<shares_publicly>);
static_assert(shares_from_this<shares_through_a_base>);
static_assert(shares_from_this<self_sharing>);
static_assert(!shares_from_this<shares_privately>);
static_assert(!shares_from_this<shares_ambiguously>);
static_assert(!shares_from_this<eight_bytes>);
// The slots hold nothing that needs destroying but through the teardown stack.
static_assert(
    std::is_trivially_destructible_v<slot_t<shared_storage<self_sharing>, threading::safe>>);
static_assert(
    std::is_trivially_destructible_v<slot_t<inline_storage<std::string>, threading::none>>);

// The holder of a singleton's object (make_shared_object): its bytes, then the function pointer,
// with nothing padded, which MSVC would report as C4324 for an over-aligned class, at the size the
// pointer-first layout had: Size and the pointer rounded up to 8 bytes, or Size plus Align beyond
// that. A class's size is a multiple of its alignment, so the holder's arguments are too.
template <std::size_t Size, std::size_t Align>
using holder_t = trellis::detail::shared_holder<Size, Align>;
template <std::size_t Size, std::size_t Align>
constexpr bool unpadded = sizeof(holder_t<Size, Align>) == sizeof(holder_t<Size, Align>::bytes) +
                                                               sizeof(trellis::detail::destroy_fn);
static_assert(unpadded<1, 1> && unpadded<12, 4> && unpadded<16, 8> && unpadded<64, 64> &&
              unpadded<128, 128> && unpadded<256, 128>);
static_assert(sizeof(void*) != 8 ||
              (sizeof(holder_t<1, 1>) == 16 && sizeof(holder_t<12, 4>) == 24 &&
               sizeof(holder_t<16, 8>) == 24 && sizeof(holder_t<64, 64>) == 128 &&
               sizeof(holder_t<128, 64>) == 192 && sizeof(holder_t<128, 128>) == 256));
static_assert(alignof(holder_t<128, 128>) == 128 && alignof(holder_t<12, 4>) == alignof(void*));

// ---- exceptions (plan §7.7, ADR-0005 item 5, ADR-0012 item 6) ---------------------------------

// Like recorded, but asks Self::fails() first, and throws instead of recording when it says so,
// so that a construction that failed leaves no "+" in the log.
template <class Self>
struct may_throw {
  may_throw() {
    if (Self::fails()) {
      throw std::runtime_error(std::string(Self::name) + " failed");
    }
    events().push_back("+" + std::string(Self::name));
  }
  may_throw(const may_throw&) = delete;
  may_throw(may_throw&&) = delete;
  may_throw& operator=(const may_throw&) = delete;
  may_throw& operator=(may_throw&&) = delete;
  ~may_throw() { events().push_back("-" + std::string(Self::name)); }
};

int g_failures = 0;

struct h_node : recorded<h_node> {
  static constexpr const char* name = "h";
  h_node() = default;
};
struct f_node : recorded<f_node> {
  static constexpr const char* name = "f";
  f_node() = default;
};
struct u_node : recorded<u_node> {
  static constexpr const char* name = "u";
  u_node() = default;
};
// Throws while g_failures is positive, counting down.
struct g_node : may_throw<g_node> {
  static constexpr const char* name = "g";
  g_node() = default;
  static bool fails() {
    if (g_failures > 0) {
      --g_failures;
      return true;
    }
    return false;
  }
};
// An aggregate, so that its fields are created left to right (braces): f, then a unique u, then g.
struct e_node {
  f_node& f;
  u_node u;
  g_node& g;
};

TEST_F(Create, AThrowingConstructorLeavesTheSlotRetryable) {
  {
    auto injector = make_injector(
        bind<h_node>.in(trellis::singleton), bind<f_node>.in(trellis::singleton_inline),
        bind<g_node>.in(trellis::singleton), bind<e_node>.in(trellis::singleton_inline));
    static_cast<void>(injector.create<h_node&>());
    g_failures = 1;
    EXPECT_THROW(static_cast<void>(injector.create<e_node&>()), std::runtime_error);
    // The basic guarantee: f, built during the failed attempt, stays built; the unique u built for
    // it is destroyed; g and e aren't built.
    EXPECT_EQ(events(), (std::vector<std::string>{"+h", "+f", "+u", "-u"}));
    e_node& built = injector.create<e_node&>();  // the retry
    EXPECT_EQ(&built.f, &injector.create<f_node&>());
    EXPECT_EQ(marked('+'), (std::vector<std::string>{"h", "f", "u", "u", "g"}));
  }
  // e (whose u goes with it), g, f, h: the reverse of the order they were built in.
  EXPECT_EQ(marked('-'), (std::vector<std::string>{"u", "u", "g", "f", "h"}));
}

TEST_F(Create, AThrowingUniqueConstructorDestroysWhatTheRequestBuilt) {
  auto injector =
      make_injector(bind<f_node>.in(trellis::singleton), bind<g_node>.in(trellis::singleton));
  g_failures = 1;
  EXPECT_THROW(static_cast<void>(injector.create<std::unique_ptr<e_node>>()), std::runtime_error);
  EXPECT_EQ(events(), (std::vector<std::string>{"+f", "+u", "-u"}));
  EXPECT_NE(injector.create<std::unique_ptr<e_node>>(), nullptr);
}

// ---- eager mode (plan §7.5, ADR-0012 item 1) --------------------------------------------------

int g_throw_at = -1;

// The I-th of five singletons; throws when I is g_throw_at.
template <int I>
struct numbered : may_throw<numbered<I>> {
  static constexpr const char* name = I == 0   ? "0"
                                      : I == 1 ? "1"
                                      : I == 2 ? "2"
                                      : I == 3 ? "3"
                                               : "4";
  numbered() = default;
  static bool fails() { return I == g_throw_at; }
};

TEST_F(Create, EagerModeBuildsEverySingletonInMakeInjector) {
  {
    auto injector = make_injector<options{.eager = true}>(bind<middle>.in(trellis::singleton),
                                                          bind<leaf>.in(trellis::singleton_inline),
                                                          bind<helper>);
    EXPECT_EQ(marked('+'), (std::vector<std::string>{"leaf", "middle"}));  // dependencies first
    EXPECT_EQ(injector.create<middle&>().leaf_, &injector.create<leaf&>());
    EXPECT_EQ(marked('+').size(), 2U);
  }
  EXPECT_EQ(marked('-'), (std::vector<std::string>{"middle", "leaf"}));
}

TEST_F(Create, EagerModeGivesTheStrongGuarantee) {
  g_throw_at = 4;
  EXPECT_THROW(
      static_cast<void>(make_injector<options{.eager = true}>(
          bind<numbered<0>>.in(trellis::singleton), bind<numbered<1>>.in(trellis::singleton_inline),
          bind<numbered<2>>.in(trellis::singleton), bind<numbered<3>>.in(trellis::singleton),
          bind<numbered<4>>.in(trellis::singleton))),
      std::runtime_error);
  g_throw_at = -1;
  EXPECT_EQ(marked('+'), (std::vector<std::string>{"0", "1", "2", "3"}));
  EXPECT_EQ(marked('-'), (std::vector<std::string>{"3", "2", "1", "0"}));
}

// ---- strict mode and threading::none ----------------------------------------------------------

TEST_F(Create, StrictModeBuildsBoundTypes) {
  // Every type created needs a binding; unbound ones are TRL-E013 (compile_fail/create/).
  auto injector =
      make_injector<options{.strict = true}>(bind<leaf>.in(trellis::singleton), bind<middle>);
  EXPECT_EQ(injector.create<middle>().leaf_, &injector.create<leaf&>());
}

// Strict mode rejects exactly the requests that the implicit unique binding would serve, the
// owning ones (TRL-E013); a reference or std::shared_ptr to an unbound type is TRL-E012, and an
// abstract one TRL-E001, as without it.
using strict_injector = decltype(make_injector<options{.strict = true}>());
template <class R>
constexpr trellis::detail::failure strict_failure =
    trellis::detail::failure_of<typename trellis::detail::plan<strict_injector, R>::node>;
using trellis::detail::failure;
static_assert(strict_failure<unbound_part> == failure::strict_unbound);
static_assert(strict_failure<std::unique_ptr<unbound_part>> == failure::strict_unbound);
static_assert(strict_failure<unbound_part&> == failure::request_form);
static_assert(strict_failure<std::shared_ptr<unbound_part>> == failure::request_form);
static_assert(strict_failure<shape&> == failure::no_binding);

TEST_F(Create, ThreadingNoneUsesPlainSlots) {
  {
    auto injector = make_injector<options{.threading = trellis::threading::none}>(
        bind<leaf>.in(trellis::singleton), bind<middle>.in(trellis::singleton_inline));
    EXPECT_EQ(injector.create<middle&>().leaf_, &injector.create<leaf&>());
    EXPECT_EQ(&injector.create<middle&>(), &injector.create<middle&>());
    g_failures = 1;
    auto retrying = make_injector<options{.threading = trellis::threading::none}>(
        bind<g_node>.in(trellis::singleton));
    EXPECT_THROW(static_cast<void>(retrying.create<g_node&>()), std::runtime_error);
    EXPECT_EQ(&retrying.create<g_node&>(), &retrying.create<g_node&>());
  }
  EXPECT_EQ(marked('-'), (std::vector<std::string>{"g", "middle", "leaf"}));
}

// ---- the check's node agrees with discovery ---------------------------------------------------

// static/resolve.hpp evaluates a deduced class with node_v, discovery/deduced.hpp's
// deduced_constructible_v written with the __is_constructible builtin. They must agree.
struct needs_shared {
  explicit needs_shared(singleton_part& shared) : shared_(&shared) {}
  singleton_part* shared_;
};
struct needs_unique_by_reference {
  explicit needs_unique_by_reference(unique_part& part) : part_(&part) {}
  unique_part* part_;
};
struct holds_shared {
  singleton_part& shared;
  std::unique_ptr<unique_part> owned;
};
struct holds_unique_by_reference {
  unique_part& part;
};
using checked_injector =
    decltype(make_injector(bind<singleton_part>.in(trellis::singleton), bind<unique_part>));
using checked_context = trellis::detail::context<checked_injector, trellis::unique_t>;
template <class T, bool Aggregate = std::is_aggregate_v<T>>
constexpr bool nodes_agree() {
  constexpr std::size_t arity = trellis::detail::deduction<T>::arity;
  using node =
      trellis::detail::deduced_node<T, checked_context, std::make_index_sequence<arity>, Aggregate>;
  return trellis::detail::node_v<node> ==
         trellis::detail::deduced_constructible_v<T, arity, checked_context>;
}
static_assert(nodes_agree<needs_shared>() && nodes_agree<needs_unique_by_reference>());
static_assert(nodes_agree<consumer>() && nodes_agree<aggregate>() && nodes_agree<unique_part>());
static_assert(nodes_agree<holds_shared>() && nodes_agree<holds_unique_by_reference>());
static_assert(checked_context::can_resolve<needs_shared>);
static_assert(checked_context::can_resolve<holds_shared>);
static_assert(!checked_context::can_resolve<needs_unique_by_reference>);
static_assert(!checked_context::can_resolve<holds_unique_by_reference>);
static_assert(!checked_context::can_resolve<unique_part&>);

// can_resolve<R> evaluates the node of a deduced class in partial specializations of its own, and
// any other node through node_v (static/resolve.hpp). The two must agree: for aggregates and other
// classes, for nodes that hold and nodes that don't, and for requests whose node isn't deduced.
// deduced_kind shows which nodes the specializations evaluate: 1 for a class built with
// parentheses, 2 for an aggregate built with braces, 0 for any other node.
template <class Node>
constexpr int deduced_kind = 0;
template <class Impl, class Ctx, class Indices, bool Aggregate>
constexpr int deduced_kind<trellis::detail::deduced_node<Impl, Ctx, Indices, Aggregate>> =
    Aggregate ? 2 : 1;
template <class R>
using checked_node_t = typename trellis::detail::plan<checked_injector, R>::node;
template <class R>
constexpr bool check_agrees() {
  return checked_context::can_resolve<R> == trellis::detail::node_v<checked_node_t<R>>;
}
static_assert(deduced_kind<checked_node_t<needs_shared>> == 1);
static_assert(deduced_kind<checked_node_t<needs_unique_by_reference>> == 1);
static_assert(deduced_kind<checked_node_t<holds_shared>> == 2);
static_assert(deduced_kind<checked_node_t<holds_unique_by_reference>> == 2);
static_assert(deduced_kind<checked_node_t<singleton_part&>> == 1);
static_assert(deduced_kind<checked_node_t<unique_part&>> == 0);
static_assert(check_agrees<needs_shared>() && check_agrees<needs_unique_by_reference>());
static_assert(check_agrees<holds_shared>() && check_agrees<holds_unique_by_reference>());
static_assert(check_agrees<consumer>() && check_agrees<aggregate>() && check_agrees<unique_part>());
static_assert(check_agrees<std::unique_ptr<holds_shared>>() && check_agrees<unique_part&>());
static_assert(check_agrees<singleton_part&>() && check_agrees<std::shared_ptr<needs_shared>>());

}  // namespace
