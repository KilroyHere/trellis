// Tests for overrides, trellis::override<K> (T1.7; plan §6.10, ADR-0010): every form of the chain
// and the types it makes; replacing a binding wherever make_injector's other arguments hold it,
// passed directly, in a module or in a named module, in its place and with no TRL-E003; a lifetime
// that changes with the override; overrides copied from lvalues and moved from rvalues; eager
// mode; the production injector left alone; many bindings; and the switch that makes an injector
// whose overrides don't apply resolve nothing more.
//
// Compile errors are in tests/compile_fail/override/; the testing helpers, isolation and the gMock
// example are in testing_test.cpp.
#include <trellis/dsl/override.hpp>
#include <trellis/meta/type_list.hpp>
#include <trellis/static/injector_traits.hpp>
#include <trellis/trellis.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace {

using trellis::bind;
using trellis::binding;
using trellis::make_injector;
using trellis::make_module;
using trellis::options;
using trellis::override_binding;
namespace detail = trellis::detail;

// ---- the domain -------------------------------------------------------------------------------

struct ticker {
  ticker() = default;
  ticker(const ticker&) = delete;
  ticker& operator=(const ticker&) = delete;
  virtual ~ticker() = default;
  [[nodiscard]] virtual int now() const = 0;
};
struct system_ticker final : ticker {
  [[nodiscard]] int now() const override { return 1; }
};
struct fixed_ticker final : ticker {
  [[nodiscard]] int now() const override { return 42; }
};

struct store {
  store() = default;
  store(const store&) = delete;
  store& operator=(const store&) = delete;
  virtual ~store() = default;
  [[nodiscard]] virtual std::string name() const = 0;
};
struct sql_store final : store {
  [[nodiscard]] std::string name() const override { return "sql"; }
};
struct memory_store final : store {
  [[nodiscard]] std::string name() const override { return "memory"; }
};

struct settings {
  std::string url;
};

// A consumer of all three: a store by reference, a ticker of its own, and the settings.
class service {
 public:
  service(store& records, std::unique_ptr<ticker> time, const settings& config)
      : records_(&records), time_(std::move(time)), config_(&config) {}

  [[nodiscard]] std::string describe() const {
    return records_->name() + "@" + config_->url + ":" + std::to_string(time_->now());
  }

 private:
  store* records_;
  std::unique_ptr<ticker> time_;
  const settings* config_;
};

// The production wiring, as a test would get it from the application.
auto app_module(settings config) {
  return make_module(bind<settings>.to_value(std::move(config)),
                     bind<store>.to<sql_store>().in(trellis::singleton),
                     bind<ticker>.to<system_ticker>());
}

// ---- the types the chain makes ----------------------------------------------------------------

// override<K> is the override of K by itself, unique, as bind<K> is the binding; each step wraps
// the binding that the same step of bind<K> makes.
static_assert(std::is_same_v<decltype(trellis::override<store>),
                             const override_binding<binding<store, store, trellis::unique_t>>>);
static_assert(std::is_same_v<decltype(trellis::override<store>.to<memory_store>()),
                             override_binding<binding<store, memory_store, trellis::unique_t>>>);
static_assert(
    std::is_same_v<decltype(trellis::override<store>.to<memory_store>().in(trellis::singleton)),
                   override_binding<binding<store, memory_store, trellis::singleton_t>>>);
static_assert(std::is_same_v<decltype(trellis::override<store>.in(trellis::singleton_inline)),
                             override_binding<binding<store, store, trellis::singleton_inline_t>>>);
static_assert(
    std::is_same_v<decltype(trellis::override<settings>.allow_copy()),
                   override_binding<binding<settings, settings, trellis::unique_t, true>>>);
static_assert(
    std::is_same_v<decltype(trellis::override<settings>.to_ref(std::declval<settings&>())),
                   override_binding<binding<settings, settings, trellis::external_t>>>);
static_assert(
    std::is_same_v<decltype(trellis::override<settings>.to_ref(std::declval<const settings&>())),
                   override_binding<binding<settings, const settings, trellis::external_t>>>);
static_assert(
    std::is_same_v<decltype(trellis::override<store>.to_ptr(std::declval<memory_store*>())),
                   override_binding<binding<store, store, trellis::external_t>>>);
static_assert(std::is_same_v<decltype(trellis::override<store>.to_shared(
                                 std::declval<std::shared_ptr<memory_store>>())),
                             override_binding<binding<store, store, trellis::shared_external_t>>>);
static_assert(std::is_same_v<decltype(trellis::override<settings>.to_value(settings{})),
                             override_binding<binding<settings, settings, trellis::value_t>>>);
static_assert(
    std::is_same_v<decltype(trellis::override<settings>.to_value(settings{}).allow_copy()),
                   override_binding<binding<settings, settings, trellis::value_t, true>>>);
static_assert(std::is_same_v<
              decltype(trellis::override<settings>.to_ref(std::declval<settings&>()).allow_copy()),
              override_binding<binding<settings, settings, trellis::external_t, true>>>);

// An override is not a binding: make_module rejects it, and make_injector applies it.
static_assert(!detail::binding_traits<decltype(trellis::override<store>)>::is_binding);
static_assert(detail::override_part<override_binding<binding<store, store, trellis::unique_t>>>);
static_assert(!detail::override_part<binding<store, store, trellis::unique_t>>);
// ---- where the override goes ------------------------------------------------------------------

using production_module = decltype(app_module(settings{}));
using memory_override =
    override_binding<binding<store, memory_store, trellis::singleton_t>>;  // override<store>...

template <class... Parts>
using bindings_of =
    typename detail::injector_traits<trellis::injector<options{}, Parts...>>::bindings;

// In place: the override's binding takes the position of the binding it replaces, and the others
// keep theirs, whether the binding comes from a module, a named module or the arguments.
static_assert(std::is_same_v<bindings_of<production_module, memory_override>,
                             detail::type_list<binding<settings, settings, trellis::value_t>,
                                               binding<store, memory_store, trellis::singleton_t>,
                                               binding<ticker, system_ticker, trellis::unique_t>>>);
struct named_production : production_module {};
static_assert(std::is_same_v<bindings_of<memory_override, named_production>,
                             bindings_of<production_module, memory_override>>);
static_assert(
    std::is_same_v<bindings_of<binding<ticker, system_ticker, trellis::unique_t>, memory_override,
                               binding<store, sql_store, trellis::singleton_t>>,
                   detail::type_list<binding<ticker, system_ticker, trellis::unique_t>,
                                     binding<store, memory_store, trellis::singleton_t>>>);
// Without overrides, the list is make_module's, from the same flatten<...>.
static_assert(std::is_same_v<bindings_of<production_module>,
                             typename detail::flatten<production_module>::bindings>);

// The switch that makes an injector resolve nothing more once make_injector has reported its
// overrides: two of one key (TRL-E003), or one with no binding to replace (TRL-E009). The
// bindings are then the ones written, unreplaced.
using second_override = override_binding<binding<store, sql_store, trellis::unique_t>>;
using ticker_override = override_binding<binding<ticker, fixed_ticker, trellis::unique_t>>;
static_assert(detail::overrides_apply<production_module, memory_override, ticker_override>);
static_assert(!detail::overrides_apply<production_module, memory_override, second_override>);
static_assert(
    detail::override_layout<production_module, memory_override, second_override>::keys_repeat);
static_assert(
    !detail::overrides_apply<binding<ticker, system_ticker, trellis::unique_t>, memory_override>);
static_assert(detail::overrides_apply<binding<ticker, system_ticker, trellis::unique_t>>);
static_assert(
    detail::injector_traits<trellis::injector<options{}, production_module, memory_override,
                                              second_override>>::has_duplicate_keys);
static_assert(std::is_same_v<bindings_of<production_module, memory_override, second_override>,
                             typename detail::flatten<production_module>::bindings>);
static_assert(detail::injector_traits<
              trellis::injector<options{}, binding<ticker, system_ticker, trellis::unique_t>,
                                memory_override>>::has_duplicate_keys);
static_assert(!detail::injector_traits<trellis::injector<options{}, production_module,
                                                         memory_override>>::has_duplicate_keys);
// An override of a const key, which bind<I>'s check reports, has nothing to replace, and gets no
// TRL-E009 besides: it counts as applying, and make_injector reports only the others.
using const_override = override_binding<binding<const store, const store, trellis::unique_t>>;
static_assert(detail::overrides_apply<production_module, const_override>);
static_assert(std::is_same_v<
              typename detail::override_layout<binding<settings, settings, trellis::value_t>,
                                               const_override, ticker_override>::missing_bindings,
              detail::type_list<binding<ticker, fixed_ticker, trellis::unique_t>>>);
// A binding of a const key, which bind<I>'s check reports, counts as a binding of the key for an
// override's target, so the override adds no TRL-E009; it replaces nothing, as no request can
// reach that binding.
using const_binding = binding<const settings, const settings, trellis::external_t>;
using settings_override = override_binding<binding<settings, settings, trellis::external_t>>;
static_assert(detail::overrides_apply<const_binding, settings_override>);
static_assert(
    std::is_same_v<typename detail::override_layout<const_binding, settings_override>::bindings,
                   detail::type_list<const_binding>>);
static_assert(detail::override_layout<const_binding, settings_override>::missing_bindings::size ==
              0);

// Many bindings: one of 300, overridden, in its place, with one lookup per binding.
template <std::size_t I>
struct item {};
template <std::size_t... Is>
auto many_bindings(std::index_sequence<Is...>) {
  return make_module(bind<item<Is>>...);
}
using module_300 = decltype(many_bindings(std::make_index_sequence<300>{}));
using item_override = override_binding<binding<item<150>, item<150>, trellis::singleton_t>>;
static_assert(bindings_of<module_300, item_override>::size == 300);
static_assert(std::is_same_v<detail::at_t<150, bindings_of<module_300, item_override>>,
                             binding<item<150>, item<150>, trellis::singleton_t>>);
static_assert(std::is_same_v<detail::at_t<149, bindings_of<module_300, item_override>>,
                             binding<item<149>, item<149>, trellis::unique_t>>);

// ---- replacing --------------------------------------------------------------------------------

TEST(Override, ReplacesABindingPassedDirectly) {
  auto injector = make_injector(bind<store>.to<sql_store>().in(trellis::singleton),
                                trellis::override<store>.to<memory_store>().in(trellis::singleton));
  EXPECT_EQ(injector.create<store&>().name(), "memory");
}

TEST(Override, ReplacesABindingInAModule) {
  auto injector = make_injector(app_module(settings{"db"}),
                                trellis::override<store>.to<memory_store>().in(trellis::singleton));
  EXPECT_EQ(injector.create<service>().describe(), "memory@db:1");
}

TEST(Override, ReplacesABindingInANamedModule) {
  auto injector = make_injector(trellis::override<ticker>.to<fixed_ticker>(),
                                named_production{app_module(settings{"db"})});
  EXPECT_EQ(injector.create<service>().describe(), "sql@db:42");
}

TEST(Override, ReplacesBindingsInSeveralModules) {
  auto injector =
      make_injector(make_module(bind<settings>.to_value(settings{"db"})),
                    make_module(bind<store>.to<sql_store>().in(trellis::singleton),
                                bind<ticker>.to<system_ticker>()),
                    trellis::override<ticker>.to<fixed_ticker>(),
                    trellis::override<store>.to<memory_store>().in(trellis::singleton_inline));
  EXPECT_EQ(injector.create<service>().describe(), "memory@db:42");
}

TEST(Override, LeavesTheProductionInjectorAlone) {
  auto production = make_injector(app_module(settings{"db"}));
  auto test = make_injector(app_module(settings{"test"}),
                            trellis::override<store>.to<memory_store>().in(trellis::singleton));
  EXPECT_EQ(production.create<service>().describe(), "sql@db:1");
  EXPECT_EQ(test.create<service>().describe(), "memory@test:1");
}

// ---- every form -------------------------------------------------------------------------------

TEST(Override, ToAnImplementationKeepsTheLifetimeItNames) {
  auto unique = make_injector(app_module(settings{}), trellis::override<ticker>.to<fixed_ticker>());
  const std::unique_ptr<ticker> first = unique.create<std::unique_ptr<ticker>>();
  const std::unique_ptr<ticker> second = unique.create<std::unique_ptr<ticker>>();
  EXPECT_EQ(first->now(), 42);
  EXPECT_NE(first.get(), second.get());
}

TEST(Override, ChangesTheLifetimeOfTheBindingItReplaces) {
  // A unique binding overridden by a singleton: now one object, which a reference may name.
  auto shared = make_injector(app_module(settings{}),
                              trellis::override<ticker>.to<fixed_ticker>().in(trellis::singleton));
  ticker& first = shared.create<ticker&>();
  EXPECT_EQ(&first, &shared.create<ticker&>());
  EXPECT_EQ(first.now(), 42);
  // A singleton overridden by a unique binding: a new object per request.
  auto fresh = make_injector(app_module(settings{}), trellis::override<store>.to<memory_store>());
  const std::unique_ptr<store> one = fresh.create<std::unique_ptr<store>>();
  const std::unique_ptr<store> other = fresh.create<std::unique_ptr<store>>();
  EXPECT_NE(one.get(), other.get());
  EXPECT_EQ(one->name(), "memory");
}

TEST(Override, InSingletonInline) {
  auto injector =
      make_injector(app_module(settings{}),
                    trellis::override<store>.to<memory_store>().in(trellis::singleton_inline));
  store& first = injector.create<store&>();
  EXPECT_EQ(&first, injector.create<store*>());
  EXPECT_EQ(first.name(), "memory");
}

// A class the injector builds with no arguments, and copies where .allow_copy() permits.
struct tally {
  tally() = default;
  int count = 0;
};

TEST(Override, AllowCopyLetsASharedObjectBeCopied) {
  auto injector = make_injector(bind<tally>.in(trellis::singleton),
                                trellis::override<tally>.in(trellis::singleton).allow_copy());
  tally& shared = injector.create<tally&>();
  shared.count = 5;
  EXPECT_EQ(injector.create<tally>().count, 5);
}

TEST(Override, AllowCopyOfAValueOverride) {
  auto injector =
      make_injector(bind<settings>.to_value(settings{"production"}),
                    trellis::override<settings>.to_value(settings{"test"}).allow_copy());
  settings& shared = injector.create<settings&>();
  shared.url = "changed";
  EXPECT_EQ(injector.create<settings>().url, "changed");
}

TEST(Override, ToRefUsesTheCallersObject) {
  memory_store fake;
  auto injector = make_injector(app_module(settings{"db"}), trellis::override<store>.to_ref(fake));
  EXPECT_EQ(&injector.create<store&>(), &fake);
  EXPECT_EQ(injector.create<store*>(), &fake);
  EXPECT_EQ(injector.create<service>().describe(), "memory@db:1");
}

TEST(Override, ToRefOfAConstObjectStaysConst) {
  const settings fixed{"fixed"};
  auto injector = make_injector(app_module(settings{"db"}),
                                trellis::override<settings>.to_ref(fixed).allow_copy());
  EXPECT_EQ(&injector.create<const settings&>(), &fixed);
  EXPECT_EQ(injector.create<settings>().url, "fixed");
  EXPECT_EQ(injector.create<service>().describe(), "sql@fixed:1");
}

TEST(Override, ToPtrUsesTheCallersObject) {
  memory_store fake;
  auto injector = make_injector(app_module(settings{}), trellis::override<store>.to_ptr(&fake));
  EXPECT_EQ(&injector.create<store&>(), &fake);
}

TEST(Override, ToSharedSharesTheCallersObject) {
  const auto fake = std::make_shared<memory_store>();
  auto injector =
      make_injector(app_module(settings{}), trellis::override<store>.to_shared(fake).allow_copy());
  const std::shared_ptr<store> shared = injector.create<std::shared_ptr<store>>();
  EXPECT_EQ(shared.get(), fake.get());
  EXPECT_EQ(injector.create<std::weak_ptr<store>>().lock().get(), fake.get());
  EXPECT_EQ(&injector.create<store&>(), fake.get());
}

TEST(Override, ToValueGivesTheInjectorsCopy) {
  auto injector = make_injector(app_module(settings{"db"}),
                                trellis::override<settings>.to_value(settings{"memory"}));
  EXPECT_EQ(injector.create<settings>().url, "memory");
  settings& stored = injector.create<settings&>();
  EXPECT_EQ(&stored, &injector.create<settings&>());
  EXPECT_EQ(injector.create<service>().describe(), "sql@memory:1");
}

// ---- values -----------------------------------------------------------------------------------

TEST(Override, AnLvalueOverrideIsCopiedAndAnRvalueOneMoved) {
  const auto fake = std::make_shared<memory_store>();
  auto kept = trellis::override<store>.to_shared(fake);
  EXPECT_EQ(fake.use_count(), 2);
  {
    auto injector = make_injector(app_module(settings{}), kept);
    EXPECT_EQ(fake.use_count(), 3);  // fake, kept, and the injector's copy
    EXPECT_EQ(&injector.create<store&>(), fake.get());
  }
  EXPECT_EQ(fake.use_count(), 2);
  {
    auto injector = make_injector(app_module(settings{}), std::move(kept));
    EXPECT_EQ(fake.use_count(), 2);  // fake, and the injector's, moved from kept
    EXPECT_EQ(&injector.create<store&>(), fake.get());
  }
  EXPECT_EQ(fake.use_count(), 1);
}

TEST(Override, AnLvalueModuleIsCopiedAroundTheOverride) {
  const auto production = app_module(settings{"db"});
  auto injector = make_injector(production, trellis::override<ticker>.to<fixed_ticker>());
  EXPECT_EQ(injector.create<service>().describe(), "sql@db:42");
}

TEST(Override, AMoveOnlyValueOverrideIsMovedIn) {
  auto injector =
      make_injector(bind<std::unique_ptr<int>>.to_value(std::make_unique<int>(1)),
                    trellis::override<std::unique_ptr<int>>.to_value(std::make_unique<int>(2)));
  EXPECT_EQ(*injector.create<std::unique_ptr<int>&>(), 2);
}

// ---- eager mode -------------------------------------------------------------------------------

std::vector<std::string>& built() {
  static std::vector<std::string> log;
  return log;
}
struct first_part {
  first_part() { built().emplace_back("first"); }
};
struct middle_part {
  middle_part() = default;
  middle_part(const middle_part&) = delete;
  middle_part& operator=(const middle_part&) = delete;
  virtual ~middle_part() = default;
  [[nodiscard]] virtual bool fake() const = 0;
};
struct real_middle final : middle_part {
  real_middle() { built().emplace_back("real middle"); }
  [[nodiscard]] bool fake() const override { return false; }
};
struct fake_middle final : middle_part {
  fake_middle() { built().emplace_back("fake middle"); }
  [[nodiscard]] bool fake() const override { return true; }
};
struct last_part {
  last_part() { built().emplace_back("last"); }
};

TEST(Override, EagerModeBuildsTheOverrideInThePlaceOfTheBindingItReplaces) {
  built().clear();
  {
    auto injector = make_injector<options{.eager = true}>(
        make_module(bind<first_part>.in(trellis::singleton),
                    bind<middle_part>.to<real_middle>().in(trellis::singleton),
                    bind<last_part>.in(trellis::singleton)),
        trellis::override<middle_part>.to<fake_middle>().in(trellis::singleton));
    EXPECT_EQ(built(), (std::vector<std::string>{"first", "fake middle", "last"}));
    EXPECT_TRUE(injector.create<middle_part&>().fake());
  }
}

}  // namespace
