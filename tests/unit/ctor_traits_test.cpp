// Tests for include/trellis/discovery/ctor_traits.hpp: ctor<Args...>, ctor_traits<T> and the
// explicit strategy's entry points declared_ctor_t, ctor_matches_v and explicit_ctor_t.
//
// Most checks are static_asserts, so a wrong answer breaks the build; the TESTs repeat the main
// cases in ordinary code. Besides each source and their order, they pin the documented edges:
// inherited member aliases, cv-qualified types, a specialization declared but never defined, and
// the build rule the check follows (braces for aggregates, parentheses otherwise).
// tests/compile_fail/discovery/ has the declarations that must not compile: parameter types or a
// parameter count that no constructor takes, declared by a specialization, a member alias or a
// binding, also used by two consumers; a narrowing declaration for an aggregate; and a member
// alias that names no ctor.
#include <trellis/discovery/ctor_traits.hpp>

#include <array>
#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace ct {

struct config {};
struct clock {};
struct logger {};

// Declares nothing.
struct undeclared {
  explicit undeclared(const config& /*settings*/) {}
};

// A member alias.
struct with_alias {
  using trellis_ctor = trellis::ctor<const config&, std::unique_ptr<clock>>;
  with_alias(const config& /*settings*/, std::unique_ptr<clock> /*time*/) {}
};

// A private member alias, which Trellis can't see: the class declares nothing.
class private_alias {
  using trellis_ctor = trellis::ctor<const config&>;

 public:
  explicit private_alias(const config& /*settings*/) {}
};

// A specialization, at global scope below.
struct specialized {
  specialized(const config& /*settings*/, logger& /*log*/) {}
};

// A specialization and a member alias that disagree: the specialization wins.
struct both {
  using trellis_ctor = trellis::ctor<const config&>;
  explicit both(const config& /*settings*/) {}
  both(const config& /*settings*/, logger& /*log*/) {}
};

// A partial specialization for a class template, which wins over the template's member alias.
template <class U>
struct family {
  using trellis_ctor = trellis::ctor<>;
  family() = default;
  explicit family(U /*value*/) {}
};

// A specialization that derives from no ctor declares nothing, and hides the member alias.
struct empty_specialization {
  using trellis_ctor = trellis::ctor<>;
  empty_specialization() = default;
};

// A specialization may derive from a ctor through a class of the user's own.
struct config_only : trellis::ctor<const config&> {};
struct indirect {
  explicit indirect(const config& /*settings*/) {}
};

// Declarations whose parameter types differ only in & and &&, plain, nested and inside
// std::function, by member alias and by specialization. GCC 13 and 14 match T& against a T&& base
// when a template argument is given and compared with the bases (T1.1); reading the declaration
// gives none, and these check that every compiler reads it exactly. Only declared, never built.
template <class U>
struct wrapper {};
struct rvalue_alias {
  using trellis_ctor = trellis::ctor<clock&&, wrapper<int&&>, std::function<void(int&&)>>;
};
struct lvalue_alias {
  using trellis_ctor = trellis::ctor<clock&, wrapper<int&>, std::function<void(int&)>>;
};
struct rvalue_specialized {};
struct lvalue_specialized {};

// A member alias that names no ctor. ctor_traits<malformed_alias> must not be instantiated here,
// or its static_assert would fail this file (tests/compile_fail/discovery/
// member_alias_not_a_ctor.cpp): the tests below use it to show that a binding's declaration
// doesn't consult ctor_traits.
struct malformed_alias {
  using trellis_ctor = int;
  explicit malformed_alias(int /*value*/) {}
};

// Constructors that std::is_constructible sees, or doesn't.
class private_constructor {
  explicit private_constructor(int /*value*/) {}
};
struct aggregate {
  int port;
  double ratio;
};
// Braces elide the inner braces of the std::array member; parentheses can't.
struct with_array {
  std::array<int, 2> values;
};
struct takes_rvalue {
  explicit takes_rvalue(clock&& /*time*/) {}
};
struct abstract_service {
  abstract_service() = default;
  abstract_service(const abstract_service&) = delete;
  abstract_service& operator=(const abstract_service&) = delete;
  virtual ~abstract_service() = default;
  virtual void run() = 0;
};

// A specialization that is declared and never defined declares nothing, and hides the alias.
struct undefined_specialization {
  using trellis_ctor = trellis::ctor<>;
};

// A derived class inherits its base's member alias, which C++20 can't tell from its own
// (documented with ctor_traits). Each case below pins what that means.
struct declared_base {
  using trellis_ctor = trellis::ctor<const config&>;
  explicit declared_base(const config& /*settings*/) {}
};
// It inherits the constructors too, so the base's declaration matches: the intended use.
struct inherits_constructors : declared_base {
  using declared_base::declared_base;
};
// A constructor of its own and no declaration: the base's declaration matches none of its
// constructors, which explicit_ctor_t reports as TRL-E002, and deduction (T1.3) never sees it.
struct own_constructor : declared_base {
  explicit own_constructor(logger& /*log*/) : declared_base(config{}) {}
};
// One more constructor that takes the base's parameters: the base's choice, silently.
struct extra_constructor : declared_base {
  explicit extra_constructor(const config& settings) : declared_base(settings) {}
  extra_constructor(const config& settings, logger& /*log*/) : declared_base(settings) {}
};
// Two bases with aliases make the name ambiguous, so the class declares nothing.
struct other_declared_base {
  using trellis_ctor = trellis::ctor<>;
};
struct two_declared_bases : declared_base, other_declared_base {
  explicit two_declared_bases(const config& settings) : declared_base(settings) {}
};
// The fixes: an alias of its own hides the base's, and a specialization wins over both.
struct own_alias : declared_base {
  using trellis_ctor = trellis::ctor<logger&>;
  explicit own_alias(logger& /*log*/) : declared_base(config{}) {}
};
struct own_specialization : declared_base {
  explicit own_specialization(logger& /*log*/) : declared_base(config{}) {}
};
// Specializations aren't inherited.
struct specialized_base {
  explicit specialized_base(const config& /*settings*/) {}
};
struct derives_specialized : specialized_base {
  using specialized_base::specialized_base;
};

// Whether T{args...} compiles. A requires-expression turns the error into false only inside a
// template.
template <class T, class... Args>
constexpr bool brace_initializes() noexcept {
  return requires { T{std::declval<Args>()...}; };
}

}  // namespace ct

template <>
struct trellis::ctor_traits<ct::specialized> : trellis::ctor<const ct::config&, ct::logger&> {};
template <>
struct trellis::ctor_traits<ct::both> : trellis::ctor<const ct::config&, ct::logger&> {};
template <class U>
struct trellis::ctor_traits<ct::family<U>> : trellis::ctor<U> {};
template <>
struct trellis::ctor_traits<ct::empty_specialization> {};
template <>
struct trellis::ctor_traits<ct::indirect> : ct::config_only {};
template <>
struct trellis::ctor_traits<ct::rvalue_specialized>
    : trellis::ctor<ct::clock&&, ct::wrapper<int&&>, std::function<void(int&&)>> {};
template <>
struct trellis::ctor_traits<ct::lvalue_specialized>
    : trellis::ctor<ct::clock&, ct::wrapper<int&>, std::function<void(int&)>> {};
template <>
struct trellis::ctor_traits<ct::undefined_specialization>;  // declared, never defined
template <>
struct trellis::ctor_traits<ct::own_specialization> : trellis::ctor<ct::logger&> {};
template <>
struct trellis::ctor_traits<ct::specialized_base> : trellis::ctor<const ct::config&> {};

namespace {

using ct::config;
using trellis::ctor;
using trellis::ctor_traits;
using trellis::detail::ctor_matches_v;
using trellis::detail::declared_ctor_t;
using trellis::detail::explicit_ctor_t;
using trellis::detail::no_ctor_declared;
using trellis::detail::type_list;

// ---- ctor -------------------------------------------------------------------------------------

// args lists the parameter types exactly as declared: references, cv-qualifiers and X&& stay.
static_assert(std::is_same_v<ctor<>::args, type_list<>>);
static_assert(std::is_same_v<ctor<int>::args, type_list<int>>);
static_assert(
    std::is_same_v<ctor<const config&, ct::logger&, ct::clock&&, std::unique_ptr<int>>::args,
                   type_list<const config&, ct::logger&, ct::clock&&, std::unique_ptr<int>>>);
static_assert(ctor<int, long, char>::args::size == 3);
static_assert(!std::is_same_v<ctor<config>, ctor<const config&>>);
static_assert(std::is_empty_v<ctor<int, long>>);

// ---- ctor_traits and declared_ctor_t: the type's own declaration ------------------------------

// Nothing declared: the primary template derives from the nothing-declared state, for any type.
static_assert(std::is_same_v<declared_ctor_t<ct::undeclared>, no_ctor_declared>);
static_assert(std::is_base_of_v<no_ctor_declared, ctor_traits<ct::undeclared>>);
static_assert(std::is_same_v<declared_ctor_t<int>, no_ctor_declared>);
static_assert(std::is_same_v<declared_ctor_t<void>, no_ctor_declared>);
static_assert(std::is_same_v<declared_ctor_t<ct::abstract_service>, no_ctor_declared>);
static_assert(std::is_same_v<declared_ctor_t<ct::private_alias>, no_ctor_declared>);
// The member alias: the primary template derives from it.
static_assert(std::is_same_v<declared_ctor_t<ct::with_alias>,
                             ctor<const config&, std::unique_ptr<ct::clock>>>);
static_assert(std::is_base_of_v<ctor<const config&, std::unique_ptr<ct::clock>>,
                                ctor_traits<ct::with_alias>>);
// A specialization.
static_assert(std::is_same_v<declared_ctor_t<ct::specialized>, ctor<const config&, ct::logger&>>);
// Both: the specialization wins over the member alias, as it replaces the primary template.
static_assert(std::is_same_v<declared_ctor_t<ct::both>, ctor<const config&, ct::logger&>>);
static_assert(!std::is_base_of_v<ctor<const config&>, ctor_traits<ct::both>>);
// A partial specialization wins too.
static_assert(std::is_same_v<declared_ctor_t<ct::family<long>>, ctor<long>>);
static_assert(std::is_same_v<declared_ctor_t<ct::family<config>>, ctor<config>>);
// A specialization that derives from no ctor declares nothing and hides the member alias.
static_assert(std::is_same_v<declared_ctor_t<ct::empty_specialization>, no_ctor_declared>);
// A specialization that derives from a ctor indirectly declares that ctor.
static_assert(std::is_same_v<declared_ctor_t<ct::indirect>, ctor<const config&>>);
// Parameter types that differ only in & and && come out exactly as declared, by either route.
using rvalue_ctor = ctor<ct::clock&&, ct::wrapper<int&&>, std::function<void(int&&)>>;
using lvalue_ctor = ctor<ct::clock&, ct::wrapper<int&>, std::function<void(int&)>>;
static_assert(std::is_same_v<declared_ctor_t<ct::rvalue_alias>, rvalue_ctor>);
static_assert(std::is_same_v<declared_ctor_t<ct::lvalue_alias>, lvalue_ctor>);
static_assert(std::is_same_v<declared_ctor_t<ct::rvalue_specialized>, rvalue_ctor>);
static_assert(std::is_same_v<declared_ctor_t<ct::lvalue_specialized>, lvalue_ctor>);
// A specialization that is declared and never defined declares nothing and hides the alias.
static_assert(std::is_same_v<declared_ctor_t<ct::undefined_specialization>, no_ctor_declared>);
static_assert(std::is_same_v<explicit_ctor_t<ct::undefined_specialization>, no_ctor_declared>);

// ---- inherited member aliases -----------------------------------------------------------------

// With the constructors inherited too, the base's declaration matches.
static_assert(std::is_same_v<declared_ctor_t<ct::inherits_constructors>, ctor<const config&>>);
static_assert(std::is_same_v<explicit_ctor_t<ct::inherits_constructors>, ctor<const config&>>);
// With a constructor of its own, the base's declaration matches none: TRL-E002 in explicit_ctor_t.
static_assert(std::is_same_v<declared_ctor_t<ct::own_constructor>, ctor<const config&>>);
static_assert(!ctor_matches_v<ct::own_constructor, declared_ctor_t<ct::own_constructor>>);
// With one more constructor that takes the base's parameters, the base's choice.
static_assert(std::is_same_v<explicit_ctor_t<ct::extra_constructor>, ctor<const config&>>);
// Two bases with aliases: nothing declared.
static_assert(std::is_same_v<declared_ctor_t<ct::two_declared_bases>, no_ctor_declared>);
// The fixes: an alias of its own, or a specialization.
static_assert(std::is_same_v<explicit_ctor_t<ct::own_alias>, ctor<ct::logger&>>);
static_assert(std::is_same_v<explicit_ctor_t<ct::own_specialization>, ctor<ct::logger&>>);
// A specialization of the base doesn't apply to the derived class.
static_assert(std::is_same_v<declared_ctor_t<ct::specialized_base>, ctor<const config&>>);
static_assert(std::is_same_v<declared_ctor_t<ct::derives_specialized>, no_ctor_declared>);

// ---- cv-qualified types -----------------------------------------------------------------------

// The entry points ignore cv-qualifiers, so const X finds the specialization written for X, not
// only the member alias, and gives the same result as X everywhere.
static_assert(std::is_same_v<declared_ctor_t<const ct::both>, ctor<const config&, ct::logger&>>);
static_assert(
    std::is_same_v<declared_ctor_t<volatile ct::specialized>, declared_ctor_t<ct::specialized>>);
static_assert(std::is_same_v<declared_ctor_t<const volatile ct::with_alias>,
                             declared_ctor_t<ct::with_alias>>);
static_assert(std::is_same_v<declared_ctor_t<const ct::family<long>>, ctor<long>>);
static_assert(std::is_same_v<declared_ctor_t<const ct::undeclared>, no_ctor_declared>);
static_assert(
    std::is_same_v<declared_ctor_t<const ct::both, ctor<const config&>>, ctor<const config&>>);
static_assert(std::is_same_v<explicit_ctor_t<const ct::both>, ctor<const config&, ct::logger&>>);
static_assert(std::is_same_v<explicit_ctor_t<const ct::undeclared>, no_ctor_declared>);
static_assert(ctor_matches_v<const ct::undeclared, ctor<const config&>>);
static_assert(!ctor_matches_v<const ct::aggregate, ctor<double, double>>);

// ---- declared_ctor_t: the binding's declaration (.with_ctor<Args...>()) -----------------------

// The binding's declaration wins over everything the type declares, and over nothing.
static_assert(
    std::is_same_v<declared_ctor_t<ct::undeclared, ctor<const config&>>, ctor<const config&>>);
static_assert(
    std::is_same_v<declared_ctor_t<ct::with_alias, ctor<config, std::unique_ptr<ct::clock>>>,
                   ctor<config, std::unique_ptr<ct::clock>>>);
static_assert(std::is_same_v<declared_ctor_t<ct::both, ctor<const config&>>, ctor<const config&>>);
static_assert(std::is_same_v<declared_ctor_t<ct::family<long>, ctor<>>, ctor<>>);
// It doesn't consult ctor_traits at all: ctor_traits<malformed_alias> would fail to compile.
static_assert(std::is_same_v<declared_ctor_t<ct::malformed_alias, ctor<int>>, ctor<int>>);
static_assert(std::is_same_v<explicit_ctor_t<ct::malformed_alias, ctor<int>>, ctor<int>>);
// A binding that declares nothing leaves the decision to the type.
static_assert(
    std::is_same_v<declared_ctor_t<ct::both, no_ctor_declared>, declared_ctor_t<ct::both>>);
static_assert(std::is_same_v<declared_ctor_t<ct::undeclared, no_ctor_declared>, no_ctor_declared>);

// ---- ctor_matches_v: how a T will be built ----------------------------------------------------

static_assert(ctor_matches_v<ct::undeclared, ctor<const config&>>);
// Other spellings that the same constructor accepts.
static_assert(ctor_matches_v<ct::undeclared, ctor<config>>);
static_assert(ctor_matches_v<ct::undeclared, ctor<config&>>);
// Wrong parameter types, wrong arity.
static_assert(!ctor_matches_v<ct::undeclared, ctor<int>>);
static_assert(!ctor_matches_v<ct::undeclared, ctor<ct::logger&>>);
static_assert(!ctor_matches_v<ct::undeclared, ctor<>>);
static_assert(!ctor_matches_v<ct::undeclared, ctor<const config&, int>>);
// Only accessible constructors count.
static_assert(!ctor_matches_v<ct::private_constructor, ctor<int>>);
// An aggregate is checked with braces, as it will be built, members left out included.
static_assert(ctor_matches_v<ct::aggregate, ctor<int, double>>);
static_assert(ctor_matches_v<ct::aggregate, ctor<int>>);
static_assert(ctor_matches_v<ct::aggregate, ctor<>>);
static_assert(!ctor_matches_v<ct::aggregate, ctor<int, double, int>>);
// Braces reject a narrowing argument, which parentheses accept in C++20 (P0960).
static_assert(!ctor_matches_v<ct::aggregate, ctor<double, double>>);
static_assert(std::is_constructible_v<ct::aggregate, double, double>);
// Braces elide the inner braces of a member, which parentheses can't.
static_assert(ctor_matches_v<ct::with_array, ctor<int, int>>);
static_assert(!std::is_constructible_v<ct::with_array, int, int>);
// Any other class is checked, and will be built, with parentheses: std::vector<int>(n, value)
// holds n copies. Braces would pick the initializer_list constructor, which here can't even take
// the std::size_t without narrowing.
static_assert(ctor_matches_v<std::vector<int>, ctor<std::size_t, int>>);
static_assert(!ct::brace_initializes<std::vector<int>, std::size_t, int>());
static_assert(ctor_matches_v<std::vector<int>, ctor<int, int>>);
// Rvalue-reference parameters: X&& and X match, X& doesn't.
static_assert(ctor_matches_v<ct::takes_rvalue, ctor<ct::clock&&>>);
static_assert(ctor_matches_v<ct::takes_rvalue, ctor<ct::clock>>);
static_assert(!ctor_matches_v<ct::takes_rvalue, ctor<ct::clock&>>);
// An abstract class is never constructible.
static_assert(!ctor_matches_v<ct::abstract_service, ctor<>>);
// Nothing declared, nothing to check.
static_assert(ctor_matches_v<ct::undeclared, no_ctor_declared>);
static_assert(ctor_matches_v<ct::abstract_service, no_ctor_declared>);

// ---- explicit_ctor_t: the checked declaration -------------------------------------------------

static_assert(std::is_same_v<explicit_ctor_t<ct::undeclared>, no_ctor_declared>);
static_assert(std::is_same_v<explicit_ctor_t<int>, no_ctor_declared>);
static_assert(std::is_same_v<explicit_ctor_t<ct::with_alias>,
                             ctor<const config&, std::unique_ptr<ct::clock>>>);
static_assert(std::is_same_v<explicit_ctor_t<ct::specialized>, ctor<const config&, ct::logger&>>);
static_assert(std::is_same_v<explicit_ctor_t<ct::both>, ctor<const config&, ct::logger&>>);
static_assert(std::is_same_v<explicit_ctor_t<ct::family<long>>, ctor<long>>);
static_assert(std::is_same_v<explicit_ctor_t<ct::indirect>, ctor<const config&>>);
static_assert(std::is_same_v<explicit_ctor_t<ct::undeclared, ctor<config>>, ctor<config>>);
static_assert(std::is_same_v<explicit_ctor_t<ct::aggregate, ctor<int, double>>, ctor<int, double>>);

}  // namespace

TEST(CtorTraits, MemberAlias) {
  EXPECT_TRUE((std::is_same_v<declared_ctor_t<ct::with_alias>,
                              ctor<const config&, std::unique_ptr<ct::clock>>>));
  EXPECT_TRUE((std::is_same_v<declared_ctor_t<ct::private_alias>, no_ctor_declared>));
}

TEST(CtorTraits, SpecializationWinsOverMemberAlias) {
  EXPECT_TRUE((std::is_same_v<declared_ctor_t<ct::specialized>, ctor<const config&, ct::logger&>>));
  EXPECT_TRUE((std::is_same_v<declared_ctor_t<ct::both>, ctor<const config&, ct::logger&>>));
  EXPECT_TRUE((std::is_same_v<declared_ctor_t<ct::family<long>>, ctor<long>>));
  EXPECT_TRUE((std::is_same_v<declared_ctor_t<ct::empty_specialization>, no_ctor_declared>));
}

TEST(CtorTraits, BindingWinsOverType) {
  EXPECT_TRUE(
      (std::is_same_v<declared_ctor_t<ct::both, ctor<const config&>>, ctor<const config&>>));
  EXPECT_TRUE((std::is_same_v<declared_ctor_t<ct::malformed_alias, ctor<int>>, ctor<int>>));
}

TEST(CtorTraits, NothingDeclared) {
  EXPECT_TRUE((std::is_same_v<explicit_ctor_t<ct::undeclared>, no_ctor_declared>));
  EXPECT_TRUE((std::is_base_of_v<no_ctor_declared, ctor_traits<ct::undeclared>>));
}

TEST(CtorTraits, InheritedMemberAlias) {
  EXPECT_TRUE((std::is_same_v<explicit_ctor_t<ct::inherits_constructors>, ctor<const config&>>));
  EXPECT_FALSE((ctor_matches_v<ct::own_constructor, declared_ctor_t<ct::own_constructor>>));
  EXPECT_TRUE((std::is_same_v<declared_ctor_t<ct::two_declared_bases>, no_ctor_declared>));
  EXPECT_TRUE((std::is_same_v<explicit_ctor_t<ct::own_alias>, ctor<ct::logger&>>));
  EXPECT_TRUE((std::is_same_v<declared_ctor_t<ct::derives_specialized>, no_ctor_declared>));
}

TEST(CtorTraits, CvQualifiersIgnored) {
  EXPECT_TRUE((std::is_same_v<declared_ctor_t<const ct::both>, declared_ctor_t<ct::both>>));
  EXPECT_TRUE((std::is_same_v<explicit_ctor_t<const ct::both>, explicit_ctor_t<ct::both>>));
}

TEST(CtorTraits, CheckedAsTheEngineBuilds) {
  EXPECT_FALSE((ctor_matches_v<ct::aggregate, ctor<double, double>>));
  EXPECT_TRUE((ctor_matches_v<ct::with_array, ctor<int, int>>));
  EXPECT_TRUE((ctor_matches_v<std::vector<int>, ctor<std::size_t, int>>));
}

TEST(CtorTraits, DeclarationMatchesConstructor) {
  EXPECT_TRUE((ctor_matches_v<ct::undeclared, ctor<const config&>>));
  EXPECT_FALSE((ctor_matches_v<ct::undeclared, ctor<int>>));
  EXPECT_FALSE((ctor_matches_v<ct::undeclared, ctor<const config&, int>>));
  EXPECT_FALSE((ctor_matches_v<ct::private_constructor, ctor<int>>));
  EXPECT_TRUE((ctor_matches_v<ct::aggregate, ctor<int, double>>));
}
