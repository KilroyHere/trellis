// A declaration that lists fewer members than its aggregate has (plan §7.2's rule as amended:
// braces, which leave out the last members), through each of the three explicit sources: a member
// alias trellis_ctor, a ctor_traits specialization and a binding's .with_ctor<Args...>(). The
// injector builds the aggregate with braces in every creation path, a new object by value, as
// std::unique_ptr and as std::shared_ptr, a singleton and an inline singleton, so a member left
// out keeps its default member initializer or is value-initialized, as braces leave it.
//
// The file is compiled with the project warnings and -Werror (NFR-10), as every unit test is, on
// GCC and Clang, against the headers, the single header and the C++20 module (it names only public
// entities): both compilers warn about each member left out without a default member initializer
// (-Wmissing-field-initializers, part of -Wextra), at Trellis's creation code, unless the code
// switches that warning off (TRELLIS_PARTIAL_AGGREGATE_INIT_BEGIN, config.hpp, in
// static/declared_aggregate.hpp) and, through the module built by Clang before 20, is a system
// header (TRELLIS_MODULE_SYSTEM_HEADER). So without them this file doesn't compile.
#include <trellis/trellis.hpp>

#include <memory>

#include <gtest/gtest.h>

namespace pa {

// Each declares only the port: timeout is value-initialized, retries keeps its initializer.
struct aliased_endpoint {
  using trellis_ctor = trellis::ctor<int>;
  int port;
  double timeout;
  int retries = 3;
};
struct specialized_endpoint {
  int port;
  double timeout;
  int retries = 3;
};
struct bound_endpoint {
  int port;
  double timeout;
  int retries = 3;
};

}  // namespace pa

template <>
struct trellis::ctor_traits<pa::specialized_endpoint> : trellis::ctor<int> {};

namespace {

using trellis::bind;
using trellis::make_injector;

// Every creation path of an aggregate built from its declaration: as a new object for each of
// the forms of a unique binding, in a singleton's shared storage, and inline in the injector.
template <class Endpoint, class Unique, class Shared, class Inline>
void expect_every_path_builds(Unique unique, Shared shared, Inline inline_storage) {
  auto check = [](const Endpoint& endpoint) {
    EXPECT_EQ(endpoint.port, 8080);
    EXPECT_DOUBLE_EQ(endpoint.timeout, 0.0);
    EXPECT_EQ(endpoint.retries, 3);
  };
  auto by_value = make_injector(bind<int>.to_value(8080), unique);
  check(by_value.template create<Endpoint>());
  check(*by_value.template create<std::unique_ptr<Endpoint>>());
  check(*by_value.template create<std::shared_ptr<Endpoint>>());
  auto singleton = make_injector(bind<int>.to_value(8080), shared);
  check(singleton.template create<Endpoint&>());
  auto in_place = make_injector(bind<int>.to_value(8080), inline_storage);
  check(in_place.template create<Endpoint&>());
}

TEST(DeclaredAggregate, MemberAliasLeavesOutTheLastMembers) {
  using pa::aliased_endpoint;
  expect_every_path_builds<aliased_endpoint>(bind<aliased_endpoint>,
                                             bind<aliased_endpoint>.in(trellis::singleton),
                                             bind<aliased_endpoint>.in(trellis::singleton_inline));
}

TEST(DeclaredAggregate, SpecializationLeavesOutTheLastMembers) {
  using pa::specialized_endpoint;
  expect_every_path_builds<specialized_endpoint>(
      bind<specialized_endpoint>, bind<specialized_endpoint>.in(trellis::singleton),
      bind<specialized_endpoint>.in(trellis::singleton_inline));
}

TEST(DeclaredAggregate, BindingDeclarationLeavesOutTheLastMembers) {
  using pa::bound_endpoint;
  expect_every_path_builds<bound_endpoint>(
      bind<bound_endpoint>.with_ctor<int>(),
      bind<bound_endpoint>.with_ctor<int>().in(trellis::singleton),
      bind<bound_endpoint>.with_ctor<int>().in(trellis::singleton_inline));
}

TEST(DeclaredAggregate, EagerInjectorBuildsItInMakeInjector) {
  auto injector = make_injector<trellis::options{.eager = true}>(
      bind<int>.to_value(7), bind<pa::bound_endpoint>.with_ctor<int>().in(trellis::singleton));
  EXPECT_EQ(injector.create<pa::bound_endpoint&>().port, 7);
  EXPECT_EQ(injector.create<pa::bound_endpoint&>().retries, 3);
}

}  // namespace
