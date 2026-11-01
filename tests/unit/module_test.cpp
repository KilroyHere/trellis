// Tests for include/trellis/dsl/module.hpp: make_module, which flattens bindings and modules into
// one module, in order, with one pack expansion (ADR-0010).
//
// The static_asserts check the flattened types and the positions table behind them; the TESTs
// check that bindings are moved out of rvalues and copied from lvalues. A module of 300 bindings
// checks that nothing folds over a module's bindings (Clang allows 256 operands per fold).
#include <trellis/dsl/bind.hpp>
#include <trellis/dsl/lifetimes.hpp>
#include <trellis/dsl/module.hpp>

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

namespace {

using trellis::bind;
using trellis::binding;
using trellis::make_module;
using trellis::module;
using trellis::detail::binding_access;
using trellis::detail::module_access;

struct apple {};
struct banana {};
struct cherry {};
struct config {
  int port = 0;
};

using unique_apple = binding<apple, apple, trellis::unique_t>;
using singleton_banana = binding<banana, banana, trellis::singleton_t>;
using inline_cherry = binding<cherry, cherry, trellis::singleton_inline_t>;
using value_config = binding<config, config, trellis::value_t>;

// ---- flattening -------------------------------------------------------------------------------

static_assert(std::is_same_v<decltype(make_module()), module<>>);
static_assert(std::is_same_v<decltype(make_module(bind<apple>)), module<unique_apple>>);
static_assert(
    std::is_same_v<decltype(make_module(bind<apple>, bind<banana>.in(trellis::singleton))),
                   module<unique_apple, singleton_banana>>);
// A module argument contributes its bindings in place, so modules never nest, and argument order
// is kept.
static_assert(std::is_same_v<decltype(make_module(bind<cherry>.in(trellis::singleton_inline),
                                                  make_module(bind<apple>,
                                                              bind<banana>.in(trellis::singleton)),
                                                  make_module(), bind<config>.to_value(config{}))),
                             module<inline_cherry, unique_apple, singleton_banana, value_config>>);
// A module of modules is as flat as one written out.
static_assert(std::is_same_v<decltype(make_module(make_module(make_module(bind<apple>)))),
                             module<unique_apple>>);

// A struct derived from a module is a module: a named module.
struct named : decltype(make_module(bind<apple>, bind<banana>.in(trellis::singleton))){};
static_assert(trellis::detail::module_part<named>);
static_assert(
    std::is_same_v<decltype(make_module(named{}, bind<cherry>.in(trellis::singleton_inline))),
                   module<unique_apple, singleton_banana, inline_cherry>>);
static_assert(trellis::detail::binding_part<unique_apple> && !trellis::detail::module_part<apple>);
static_assert(!trellis::detail::binding_part<named> && !trellis::detail::binding_part<int>);

// Arguments are bindings or modules; anything else is reported by make_module and make_injector
// (tests/compile_fail/dsl/), and contributes nothing.
static_assert(trellis::detail::bindings_and_modules<>);
static_assert(trellis::detail::bindings_and_modules<unique_apple, named, module<>>);
static_assert(!trellis::detail::bindings_and_modules<unique_apple, int>);
static_assert(trellis::detail::part_bindings<int>::type::size == 0);

// An lvalue or const argument is copied, so its bindings must be copyable; an rvalue is moved.
// (The arguments are given as forwarding-reference types: X& for an lvalue, X for an rvalue.)
using move_only_module =
    module<binding<std::unique_ptr<int>, std::unique_ptr<int>, trellis::value_t>>;
static_assert(trellis::detail::bindings_can_be_taken<move_only_module>);
static_assert(!trellis::detail::bindings_can_be_taken<move_only_module&>);
static_assert(!trellis::detail::bindings_can_be_taken<const move_only_module&>);
static_assert(!trellis::detail::bindings_can_be_taken<const move_only_module>);
static_assert(trellis::detail::bindings_can_be_taken<named&, const named&, unique_apple&>);
static_assert(trellis::detail::bindings_can_be_taken<>);

// The positions table: arguments of 2, 0 and 3 bindings give five bindings, from arguments 0, 0,
// 2, 2 and 2, at indices 0, 1, 0, 1 and 2 there.
constexpr auto positions = trellis::detail::locate<5>({2, 0, 3});
static_assert(positions.part[0] == 0 && positions.part[1] == 0 && positions.part[2] == 2 &&
              positions.part[3] == 2 && positions.part[4] == 2);
static_assert(positions.element[0] == 0 && positions.element[1] == 1 && positions.element[2] == 0 &&
              positions.element[3] == 1 && positions.element[4] == 2);
static_assert(trellis::detail::total({2, 0, 3}) == 5 && trellis::detail::total({}) == 0);

// Modules of empty bindings take no space beyond the class's own byte, on GCC and Clang
// ([[no_unique_address]]); MSVC ignores the attribute.
#if !defined(_MSC_VER)
static_assert(sizeof(module<unique_apple, singleton_banana, inline_cherry>) == 1);
#endif

// ---- many bindings ----------------------------------------------------------------------------

template <std::size_t I>
struct key {};
template <std::size_t... Is>
auto many_bindings(std::index_sequence<Is...>) {
  return make_module(bind<key<Is>>...);
}
using module_300 = decltype(many_bindings(std::make_index_sequence<300>{}));
static_assert(std::is_same_v<decltype(module_access::binding<299>(std::declval<module_300&>())),
                             binding<key<299>, key<299>, trellis::unique_t>&>);
// 300 one-binding arguments, flattened in one expansion.
template <std::size_t... Is>
auto many_modules(std::index_sequence<Is...>) {
  return make_module(make_module(bind<key<Is>>)...);
}
static_assert(std::is_same_v<decltype(many_modules(std::make_index_sequence<300>{})), module_300>);

// ---- values -----------------------------------------------------------------------------------

// A value that counts how it was copied and moved.
struct counted {
  int copies = 0;
  int moves = 0;
  counted() = default;
  counted(const counted& other) : copies(other.copies + 1), moves(other.moves) {}
  counted(counted&& other) noexcept : copies(other.copies), moves(other.moves + 1) {}
  counted& operator=(const counted&) = delete;
  counted& operator=(counted&&) = delete;
  ~counted() = default;
};

TEST(Module, CarriesTheObjectsOfItsBindings) {
  config settings{443};
  auto bindings = make_module(bind<apple>, bind<config>.to_ref(settings),
                              bind<banana>.to_shared(std::make_shared<banana>()));
  EXPECT_EQ(binding_access::object(module_access::binding<1>(bindings)), &settings);
  EXPECT_NE(binding_access::object(module_access::binding<2>(bindings)), nullptr);

  // Flattening keeps them: the same objects, at their new positions.
  auto outer = make_module(bind<cherry>, bindings);
  EXPECT_EQ(binding_access::object(module_access::binding<2>(outer)), &settings);
  EXPECT_EQ(binding_access::object(module_access::binding<3>(outer)).get(),
            binding_access::object(module_access::binding<2>(bindings)).get());
}

TEST(Module, MovesFromRvaluesAndCopiesFromLvalues) {
  auto inner = make_module(bind<counted>.to_value(counted{}));
  const int moves_into_inner = binding_access::object(module_access::binding<0>(inner)).moves;
  EXPECT_EQ(binding_access::object(module_access::binding<0>(inner)).copies, 0);

  const auto copied = make_module(inner);
  EXPECT_EQ(binding_access::object(module_access::binding<0>(copied)).copies, 1);

  const auto moved = make_module(std::move(inner));
  EXPECT_EQ(binding_access::object(module_access::binding<0>(moved)).copies, 0);
  EXPECT_EQ(binding_access::object(module_access::binding<0>(moved)).moves, moves_into_inner + 1);
}

TEST(Module, MoveOnlyBindingsMoveThrough) {
  auto bindings = make_module(bind<std::unique_ptr<int>>.to_value(std::make_unique<int>(9)));
  const auto outer = make_module(bind<apple>, std::move(bindings));
  ASSERT_NE(binding_access::object(module_access::binding<1>(outer)), nullptr);
  EXPECT_EQ(*binding_access::object(module_access::binding<1>(outer)), 9);
}

TEST(Module, NamedModulesDefaultConstruct) {
  const auto bindings = make_module(named{}, bind<cherry>);
  EXPECT_TRUE(
      (std::is_same_v<
          std::remove_const_t<decltype(bindings)>,
          module<unique_apple, singleton_banana, binding<cherry, cherry, trellis::unique_t>>>));
  // A module with an external binding has no default constructor.
  EXPECT_FALSE(
      (std::is_default_constructible_v<module<binding<config, config, trellis::external_t>>>));
  EXPECT_TRUE(std::is_default_constructible_v<module_300>);
}

// A struct derived from a module, holding a value, in each shape and value category.
struct counted_module : decltype(make_module(bind<counted>.to_value(counted{}))){};

TEST(Module, NamedModulesCopyAndMoveInEveryShape) {
  counted_module named_counted{make_module(bind<counted>.to_value(counted{}))};
  const int base_moves = binding_access::object(module_access::binding<0>(named_counted)).moves;

  // One module, as an lvalue, a const lvalue and an rvalue.
  const auto one_copied = make_module(named_counted);
  EXPECT_EQ(binding_access::object(module_access::binding<0>(one_copied)).copies, 1);
  const counted_module& as_const = named_counted;
  const auto one_const = make_module(as_const);
  EXPECT_EQ(binding_access::object(module_access::binding<0>(one_const)).copies, 1);

  // Mixed with another binding: the named module's binding is found through its base.
  const auto mixed_copied = make_module(bind<apple>, named_counted);
  EXPECT_EQ(binding_access::object(module_access::binding<1>(mixed_copied)).copies, 1);
  const auto mixed_const = make_module(as_const, bind<apple>);
  EXPECT_EQ(binding_access::object(module_access::binding<0>(mixed_const)).copies, 1);
  const auto mixed_moved = make_module(bind<apple>, std::move(named_counted));
  EXPECT_EQ(binding_access::object(module_access::binding<1>(mixed_moved)).copies, 0);
  EXPECT_EQ(binding_access::object(module_access::binding<1>(mixed_moved)).moves, base_moves + 1);
}

}  // namespace
