// Smoke test of the C++20 module trellis (plan §10, R15): the public API through `import trellis;`
// alone, with no Trellis header included. Each public entity of include/ is named at least once,
// as a user names it, and so are the constructs that a module boundary can break: a specialization
// of an exported template declared in the importer, class template argument deduction through a
// deduction guide, a class type as a template argument, a struct derived from a module, and
// type_name<T>(), which reads the compiler's function signatures. Objects are created through the
// import, with declared and deduced constructors, and a binding is overridden.
// tools/module_exports.py checks that the module exports every public name (the test
// module.exports); this test checks that the exports work. headers_test.cpp checks that the module
// and the headers agree, and testing_test.cpp covers the module trellis.testing.
#include <cstddef>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "app.hpp"

import trellis;

// Macros don't cross a module boundary (docs/internals/delivery.md). This translation unit
// includes no Trellis header, so it sees none of config.hpp's macros; config_test.cpp shows how an
// importer gets them.
#ifdef TRELLIS_VERSION
#error "TRELLIS_VERSION is defined: a Trellis header was included, or the import leaked a macro"
#endif

namespace mod {

// The intrusive constructor declaration (T1.2): a public member alias.
class declared {
 public:
  using trellis_ctor = trellis::ctor<const config&>;
  explicit declared(const config& cfg) : port_(cfg.port) {}
  [[nodiscard]] int port() const { return port_; }

 private:
  int port_;
};

// A second implementation, for an override.
class fixed_clock final : public clock_source {
 public:
  [[nodiscard]] long now() const override { return 7; }
};

// A class whose constructor deduced discovery finds (T1.3), through the import.
class reporter {
 public:
  reporter(clock_source& clock, const config& cfg) : value_(clock.now() + cfg.port) {}
  [[nodiscard]] long value() const { return value_; }

 private:
  long value_;
};

// Defined in headers_test.cpp, which includes the headers instead of importing the module. The
// types here are the module's, there the headers'; the program links only if they are the same
// entities.
std::string_view service_name_from_headers();
trellis::binding<clock_source, system_clock, trellis::singleton_t> clock_binding_from_headers();
trellis::failure failure_from_headers();

}  // namespace mod

// The non-intrusive constructor declaration (T1.2): a specialization of the exported class template
// trellis::ctor_traits, declared in the importer.
template <>
struct trellis::ctor_traits<mod::service> : trellis::ctor<int> {};

namespace {

using mod::clock_source;
using mod::config;
using mod::service;
using mod::system_clock;

// ---- discovery/ctor_traits.hpp ----------------------------------------------------------------

static_assert(std::is_base_of_v<trellis::ctor<int>, trellis::ctor_traits<service>>);
static_assert(std::is_base_of_v<trellis::ctor<const config&>, trellis::ctor_traits<mod::declared>>);

// ---- dsl/bind.hpp and dsl/lifetimes.hpp -------------------------------------------------------

static_assert(std::is_same_v<decltype(trellis::bind<clock_source>.to<system_clock>()),
                             trellis::binding<clock_source, system_clock, trellis::unique_t>>);
static_assert(
    std::is_same_v<decltype(trellis::bind<clock_source>.to<system_clock>().in(trellis::singleton)),
                   trellis::binding<clock_source, system_clock, trellis::singleton_t>>);
static_assert(std::is_same_v<decltype(trellis::bind<service>.in(trellis::unique)),
                             trellis::binding<service, service, trellis::unique_t>>);
static_assert(std::is_same_v<decltype(trellis::bind<service>.in(trellis::singleton_inline)),
                             trellis::binding<service, service, trellis::singleton_inline_t>>);
static_assert(std::is_same_v<decltype(trellis::bind<service>.in(trellis::singleton).allow_copy()),
                             trellis::binding<service, service, trellis::singleton_t, true>>);
static_assert(std::is_same_v<decltype(trellis::bind<config>.to_ref(std::declval<const config&>())),
                             trellis::binding<config, const config, trellis::external_t>>);
static_assert(std::is_same_v<decltype(trellis::bind<config>.to_ptr(std::declval<config*>())),
                             trellis::binding<config, config, trellis::external_t>>);
static_assert(std::is_same_v<decltype(trellis::bind<config>.to_value(config{})),
                             trellis::binding<config, config, trellis::value_t>>);
static_assert(
    std::is_same_v<decltype(trellis::bind<clock_source>.to_shared(std::shared_ptr<system_clock>())),
                   trellis::binding<clock_source, clock_source, trellis::shared_external_t>>);
// The entities keep their inline namespace: trellis::v1::X names the same entity as trellis::X.
static_assert(std::is_same_v<trellis::v1::unique_t, trellis::unique_t>);

// ---- dsl/module.hpp, dsl/options.hpp and static/injector.hpp ----------------------------------

// A named module: a struct derived from a module (ADR-0010).
struct clock_module : decltype(trellis::make_module(
                          trellis::bind<clock_source>.to<system_clock>().in(trellis::singleton))){};
static_assert(
    std::is_same_v<decltype(trellis::make_module(trellis::bind<service>)),
                   trellis::module<trellis::binding<service, service, trellis::unique_t>>>);

static_assert(
    std::is_same_v<decltype(trellis::override<clock_source>.to<mod::fixed_clock>()),
                   trellis::override_binding<
                       trellis::binding<clock_source, mod::fixed_clock, trellis::unique_t>>>);

constexpr trellis::options single_threaded{.threading = trellis::threading::none};
static_assert(!single_threaded.strict && single_threaded.max_ctor_arity == 16);

// ---- meta/fixed_string.hpp --------------------------------------------------------------------

// Class template argument deduction through fixed_string's deduction guide, as a variable and as a
// class-type template argument.
constexpr trellis::fixed_string primary{"primary"};
static_assert(std::is_same_v<decltype(primary), const trellis::fixed_string<7>>);
template <trellis::fixed_string Name>
struct named {};
static_assert(!std::is_same_v<named<"primary">, named<"replica">>);
static_assert(primary == trellis::fixed_string{"primary"});
static_assert(primary == std::string_view{"primary"});
static_assert(primary + trellis::fixed_string{"/1"} == std::string_view{"primary/1"});
static_assert(primary + "/2" == std::string_view{"primary/2"});
static_assert("db/" + primary == std::string_view{"db/primary"});

// ---- meta/type_name.hpp -----------------------------------------------------------------------

// Whether two spellings differ at most in spaces, which compilers place differently in pointer
// types ("const mod::config *" or "const mod::config*").
constexpr bool same_spelling(std::string_view a, std::string_view b) {
  std::size_t i = 0;
  std::size_t j = 0;
  while (true) {
    while (i != a.size() && a[i] == ' ') {
      ++i;
    }
    while (j != b.size() && b[j] == ' ') {
      ++j;
    }
    if (i == a.size() || j == b.size()) {
      return i == a.size() && j == b.size();
    }
    if (a[i++] != b[j++]) {
      return false;
    }
  }
}

static_assert(trellis::type_name<int>() == "int");
static_assert(trellis::type_name<service>() == "mod::service");
static_assert(same_spelling(trellis::type_name<const config*>(), "const mod::config*"));
static_assert(same_spelling(trellis::type_name<int config::*>(), "int mod::config::*"));

// ---- meta/result.hpp --------------------------------------------------------------------------

constexpr trellis::result<int> parse(std::string_view text) {
  if (text.empty()) {
    return trellis::make_unexpected(trellis::failure{"empty input"});
  }
  return static_cast<int>(text.size());
}
static_assert(*parse("abc") == 3);
static_assert(!parse("").has_value());

// ---- diagnostics/errors.hpp: the compile-error vocabulary -------------------------------------

static_assert(
    !trellis::error::E001<clock_source, trellis::error::request<clock_source&>>::reported);
static_assert(!trellis::error::E002<service, trellis::ctor<long>>::reported);
static_assert(!trellis::error::E003<clock_source, clock_module, clock_module>::reported);
static_assert(!trellis::error::E004<service>::reported);
static_assert(!trellis::error::E005<clock_source, trellis::error::request<clock_source&>,
                                    trellis::unique_t>::reported);
static_assert(!trellis::error::E009<clock_source, mod::fixed_clock>::reported);
static_assert(!trellis::error::E010<service, 16>::reported);
static_assert(!trellis::error::E012<config, trellis::error::request<const config&>>::reported);
static_assert(!trellis::error::E013<service, trellis::error::request<service>>::reported);
static_assert(
    !trellis::error::E014<clock_source, trellis::error::request<std::unique_ptr<clock_source>>,
                          system_clock>::reported);
static_assert(std::is_empty_v<trellis::error::path<mod::reporter, clock_source>>);

}  // namespace

TEST(ModuleImport, MakesInjectorsFromBindingsAndModules) {
  const config cfg{8080};
  auto injector = trellis::make_injector<single_threaded>(
      clock_module{}, trellis::bind<config>.to_ref(cfg), trellis::bind<service>.in(trellis::unique),
      trellis::bind<mod::declared>.in(trellis::singleton));
  static_assert(
      std::is_same_v<
          decltype(injector),
          trellis::injector<single_threaded, clock_module,
                            trellis::binding<config, const config, trellis::external_t>,
                            trellis::binding<service, service, trellis::unique_t>,
                            trellis::binding<mod::declared, mod::declared, trellis::singleton_t>>>);
  static_assert(!std::is_copy_constructible_v<decltype(injector)>);
  static_assert(!std::is_move_constructible_v<decltype(injector)>);
}

TEST(ModuleImport, CreatesObjects) {
  const config cfg{8000};
  auto injector =
      trellis::make_injector(trellis::bind<clock_source>.to<system_clock>().in(trellis::singleton),
                             trellis::bind<config>.to_ref(cfg), trellis::bind<int>.to_value(3),
                             trellis::bind<mod::declared>.in(trellis::singleton));
  clock_source& clock = injector.create<clock_source&>();
  EXPECT_EQ(clock.now(), 42);
  EXPECT_EQ(&injector.create<clock_source&>(), &clock);  // one singleton
  EXPECT_EQ(injector.create<std::shared_ptr<clock_source>>().get(), &clock);
  EXPECT_EQ(injector.create<service>().port(), 3);  // ctor_traits<service>, specialized above
  EXPECT_EQ(injector.create<mod::declared&>().port(), 8000);  // the member alias trellis_ctor
  EXPECT_EQ(injector.create<mod::reporter>().value(), 8042);  // a deduced constructor
}

TEST(ModuleImport, OverridesABinding) {
  const config cfg{1};
  auto injector = trellis::make_injector(
      clock_module{}, trellis::bind<config>.to_ref(cfg),
      trellis::override<clock_source>.to<mod::fixed_clock>().in(trellis::singleton));
  EXPECT_EQ(injector.create<clock_source&>().now(), 7);
  EXPECT_EQ(injector.create<mod::reporter>().value(), 8);
}

TEST(ModuleImport, NamesTypesAsTheHeadersDo) {
  EXPECT_EQ(trellis::type_name<system_clock>(), "mod::system_clock");
  EXPECT_EQ(trellis::type_name<std::unique_ptr<clock_source>>().substr(0, 16), "std::unique_ptr<");
  EXPECT_EQ(trellis::type_name<service>(), mod::service_name_from_headers());
}

TEST(ModuleImport, ReturnsResultsAndFailures) {
  const trellis::result<int> missing = parse("");
  ASSERT_FALSE(missing.has_value());
  EXPECT_EQ(missing.error().message(), "empty input");
  EXPECT_EQ(missing.value_or(-1), -1);
  EXPECT_EQ(missing.error(), trellis::failure{"empty input"});
  EXPECT_EQ(mod::failure_from_headers(), trellis::failure{"from the headers"});
}

TEST(ModuleImport, AcceptsBindingsMadeWithTheHeaders) {
  // The binding comes from the translation unit that includes the headers, and enters an
  // injector made through the module: one type, whichever way it was declared.
  auto injector = trellis::make_injector(mod::clock_binding_from_headers(),
                                         trellis::bind<service>.in(trellis::singleton));
  static_assert(
      std::is_same_v<
          decltype(injector),
          trellis::injector<trellis::options{},
                            trellis::binding<clock_source, system_clock, trellis::singleton_t>,
                            trellis::binding<service, service, trellis::singleton_t>>>);
}
