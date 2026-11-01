// Tests for include/trellis/dsl/bind.hpp and dsl/lifetimes.hpp: bind<I> and the bindings its
// chain functions make.
//
// Each chain step returns one binding type, with one part replaced, so the static_asserts check
// the types; the TESTs check what external, shared and value bindings hold. Misuse that must not
// compile is in tests/compile_fail/dsl/.
#include <trellis/dsl/bind.hpp>
#include <trellis/dsl/lifetimes.hpp>
#include <trellis/dsl/request_forms.hpp>

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

namespace {

using trellis::bind;
using trellis::binding;
using trellis::detail::binding_access;
using trellis::detail::binding_traits;

struct logger {
  logger() = default;
  logger(const logger&) = delete;
  logger& operator=(const logger&) = delete;
  virtual ~logger() = default;
  [[nodiscard]] virtual int level() const = 0;
};
struct console_logger final : logger {
  [[nodiscard]] int level() const override { return 1; }
};
struct config {
  int port = 0;
};
struct derived_config : config {
  int extra = 0;
};

template <class B>
using plain = std::remove_cvref_t<B>;

// ---- the chain: one binding type per step (ADR-0010) ------------------------------------------

// bind<I> is the binding of I to itself, unique, and a binding in its own right.
static_assert(
    std::is_same_v<plain<decltype(bind<config>)>, binding<config, config, trellis::unique_t>>);
static_assert(binding_traits<plain<decltype(bind<config>)>>::is_binding);
static_assert(!binding_traits<config>::is_binding && !binding_traits<int>::is_binding);

// .to<C>() replaces the implementation, .in() the lifetime, .allow_copy() the copy flag; the
// order of .to<C>() and .in() doesn't matter.
static_assert(std::is_same_v<decltype(bind<logger>.to<console_logger>()),
                             binding<logger, console_logger, trellis::unique_t>>);
static_assert(std::is_same_v<decltype(bind<logger>.to<console_logger>().in(trellis::singleton)),
                             binding<logger, console_logger, trellis::singleton_t>>);
static_assert(std::is_same_v<decltype(bind<logger>.in(trellis::singleton).to<console_logger>()),
                             binding<logger, console_logger, trellis::singleton_t>>);
static_assert(std::is_same_v<decltype(bind<config>.in(trellis::singleton_inline)),
                             binding<config, config, trellis::singleton_inline_t>>);
static_assert(std::is_same_v<decltype(bind<config>.in(trellis::singleton).in(trellis::unique)),
                             binding<config, config, trellis::unique_t>>);
static_assert(std::is_same_v<decltype(bind<config>.in(trellis::singleton).allow_copy()),
                             binding<config, config, trellis::singleton_t, true>>);
static_assert(std::is_same_v<decltype(bind<config>.allow_copy().in(trellis::singleton)),
                             binding<config, config, trellis::singleton_t, true>>);
// A key may be bound to itself explicitly.
static_assert(std::is_same_v<decltype(bind<config>.to<config>()),
                             binding<config, config, trellis::unique_t>>);

// binding_traits reads the parts back.
using singleton_logger = decltype(bind<logger>.to<console_logger>().in(trellis::singleton));
static_assert(std::is_same_v<binding_traits<singleton_logger>::key, logger>);
static_assert(std::is_same_v<binding_traits<singleton_logger>::impl, console_logger>);
static_assert(std::is_same_v<binding_traits<singleton_logger>::lifetime, trellis::singleton_t>);
static_assert(!binding_traits<singleton_logger>::allow_copies);

// Bindings that the injector constructs are empty values, usable in constant expressions.
static_assert(std::is_empty_v<singleton_logger>);
constexpr auto constant_binding = bind<logger>.to<console_logger>().in(trellis::singleton);
static_assert(std::is_same_v<plain<decltype(constant_binding)>, singleton_logger>);

// ---- the binding functions that name an object ------------------------------------------------

const config shared_constant{8080};

// .to_ref() and .to_ptr(): external, and const when the object is.
static_assert(std::is_same_v<decltype(bind<config>.to_ref(std::declval<config&>())),
                             binding<config, config, trellis::external_t>>);
static_assert(std::is_same_v<decltype(bind<config>.to_ref(shared_constant)),
                             binding<config, const config, trellis::external_t>>);
static_assert(std::is_same_v<decltype(bind<config>.to_ptr(std::declval<config*>())),
                             binding<config, config, trellis::external_t>>);
static_assert(std::is_same_v<decltype(bind<config>.to_ptr(&shared_constant)),
                             binding<config, const config, trellis::external_t>>);
// A derived object binds through the key's type.
static_assert(std::is_same_v<decltype(bind<logger>.to_ref(std::declval<console_logger&>())),
                             binding<logger, logger, trellis::external_t>>);
static_assert(std::is_same_v<decltype(bind<logger>.to_ref(std::declval<const console_logger&>())),
                             binding<logger, const logger, trellis::external_t>>);
// .to_shared(): shared external, const for a pointer to const.
static_assert(std::is_same_v<decltype(bind<logger>.to_shared(std::shared_ptr<console_logger>())),
                             binding<logger, logger, trellis::shared_external_t>>);
static_assert(
    std::is_same_v<decltype(bind<logger>.to_shared(std::shared_ptr<const console_logger>())),
                   binding<logger, const logger, trellis::shared_external_t>>);
// .to_value(): the value keeps its own type, so a derived value isn't sliced, and a const or
// reference argument stores a plain copy.
static_assert(std::is_same_v<decltype(bind<config>.to_value(config{})),
                             binding<config, config, trellis::value_t>>);
static_assert(std::is_same_v<decltype(bind<config>.to_value(shared_constant)),
                             binding<config, config, trellis::value_t>>);
static_assert(std::is_same_v<decltype(bind<config>.to_value(derived_config{})),
                             binding<config, derived_config, trellis::value_t>>);
// .allow_copy() works after them too, and keeps their object.
static_assert(std::is_same_v<decltype(bind<config>.to_ref(shared_constant).allow_copy()),
                             binding<config, const config, trellis::external_t, true>>);
static_assert(std::is_same_v<decltype(bind<config>.allow_copy().to_ref(shared_constant)),
                             binding<config, const config, trellis::external_t, true>>);

// .to_ref() takes lvalues only.
template <class B, class Arg>
concept can_bind_ref =
    requires(const B& binding, Arg&& object) { binding.to_ref(std::forward<Arg>(object)); };
static_assert(can_bind_ref<binding<config, config, trellis::unique_t>, config&>);
static_assert(can_bind_ref<binding<config, config, trellis::unique_t>, const config&>);
static_assert(!can_bind_ref<binding<config, config, trellis::unique_t>, config>);
static_assert(!can_bind_ref<binding<config, config, trellis::unique_t>, const config>);

// Bindings copy and move like values. A value binding's constructor template must not stand in
// for its copy constructor: for a non-const lvalue, an unconstrained V&& would match better, and
// it is private, so direct-initialization from one would not compile.
using value_config = binding<config, config, trellis::value_t>;
static_assert(std::is_constructible_v<value_config, value_config&>);
static_assert(std::is_constructible_v<value_config, const value_config&>);
static_assert(std::is_constructible_v<value_config, value_config&&>);
static_assert(std::is_copy_assignable_v<value_config> && std::is_move_assignable_v<value_config>);
// The constructor template is not a way to make a value binding from a value.
static_assert(!std::is_constructible_v<value_config, config>);
using move_only = binding<std::unique_ptr<int>, std::unique_ptr<int>, trellis::value_t>;
static_assert(std::is_move_constructible_v<move_only> && !std::is_copy_constructible_v<move_only>);
static_assert(std::is_copy_constructible_v<binding<config, const config, trellis::external_t>> &&
              std::is_copy_constructible_v<binding<config, config, trellis::shared_external_t>>);

// The lifetime variables are constants of the lifetime types.
static_assert(std::is_same_v<plain<decltype(trellis::unique)>, trellis::unique_t>);
static_assert(std::is_same_v<plain<decltype(trellis::singleton)>, trellis::singleton_t>);
static_assert(
    std::is_same_v<plain<decltype(trellis::singleton_inline)>, trellis::singleton_inline_t>);
static_assert(trellis::detail::is_constructed_lifetime<trellis::unique_t> &&
              trellis::detail::is_constructed_lifetime<trellis::singleton_t> &&
              trellis::detail::is_constructed_lifetime<trellis::singleton_inline_t>);
static_assert(!trellis::detail::is_constructed_lifetime<trellis::external_t> &&
              !trellis::detail::is_constructed_lifetime<trellis::shared_external_t> &&
              !trellis::detail::is_constructed_lifetime<trellis::value_t>);

// Keys: object types without cv qualifiers, not arrays.
static_assert(trellis::detail::is_bindable_key<config> && trellis::detail::is_bindable_key<int> &&
              trellis::detail::is_bindable_key<logger> &&
              trellis::detail::is_bindable_key<std::shared_ptr<config>>);
static_assert(!trellis::detail::is_bindable_key<const config> &&
              !trellis::detail::is_bindable_key<config&> &&
              !trellis::detail::is_bindable_key<config&&> &&
              // An array type, on purpose: arrays aren't keys.
              !trellis::detail::is_bindable_key<config[2]> &&  // NOLINT(modernize-avoid-c-arrays)
              !trellis::detail::is_bindable_key<void> && !trellis::detail::is_bindable_key<void()>);

// Implementations: the key or a class publicly derived from it.
struct private_logger : private logger {
  [[nodiscard]] int level() const override { return 0; }
};
static_assert(trellis::detail::is_implementation_of<console_logger, logger>);
static_assert(trellis::detail::is_implementation_of<config, config>);
static_assert(trellis::detail::is_implementation_of<derived_config, config>);
static_assert(!trellis::detail::is_implementation_of<config, logger>);
static_assert(!trellis::detail::is_implementation_of<const console_logger, logger>);
static_assert(!trellis::detail::is_implementation_of<private_logger, logger>);
static_assert(trellis::detail::is_object_of<const console_logger, logger>);
static_assert(!trellis::detail::is_object_of<volatile console_logger, logger>);

TEST(Bind, ToRefAndToPtrHoldTheCallersObject) {
  config settings{443};
  const auto by_ref = bind<config>.to_ref(settings);
  const auto by_ptr = bind<config>.to_ptr(&settings);
  EXPECT_EQ(binding_access::object(by_ref), &settings);
  EXPECT_EQ(binding_access::object(by_ptr), &settings);
  settings.port = 8443;
  EXPECT_EQ(binding_access::object(by_ref)->port, 8443);

  const auto constant = bind<config>.to_ref(shared_constant).allow_copy();
  EXPECT_EQ(binding_access::object(constant), &shared_constant);
}

TEST(Bind, ToRefOfADerivedObjectPointsToItsBase) {
  console_logger console;
  const auto binding = bind<logger>.to_ref(console);
  const logger& base = console;
  EXPECT_EQ(binding_access::object(binding), &base);
  EXPECT_EQ(binding_access::object(binding)->level(), 1);
}

TEST(Bind, ToSharedSharesOwnership) {
  auto console = std::make_shared<console_logger>();
  const auto binding = bind<logger>.to_shared(console);
  EXPECT_EQ(binding_access::object(binding).get(), console.get());
  EXPECT_EQ(console.use_count(), 2);

  auto moved = bind<logger>.to_shared(console).allow_copy();
  EXPECT_EQ(console.use_count(), 3);
  const auto copied = moved.allow_copy();
  EXPECT_EQ(console.use_count(), 4);
  const auto from_rvalue = std::move(moved).allow_copy();
  EXPECT_EQ(console.use_count(), 4);  // moved along, not copied
  EXPECT_EQ(binding_access::object(from_rvalue).get(), console.get());
  EXPECT_EQ(binding_access::object(copied).get(), console.get());
}

TEST(Bind, ToValueStoresACopy) {
  config settings{80};
  const auto binding = bind<config>.to_value(settings);
  settings.port = 81;
  EXPECT_EQ(binding_access::object(binding).port, 80);

  const auto text = bind<std::string>.to_value(std::string("trellis"));
  EXPECT_EQ(binding_access::object(text), "trellis");
  const auto copied = text.allow_copy();
  EXPECT_EQ(binding_access::object(copied), "trellis");

  derived_config derived;
  derived.extra = 7;
  const auto not_sliced = bind<config>.to_value(derived);
  EXPECT_EQ(binding_access::object(not_sliced).extra, 7);

  // The injector owns the stored value, which the table lets consumers request as T&.
  auto owned = bind<config>.to_value(config{1});
  static_assert(std::is_same_v<decltype(binding_access::object(owned)), config&>);
  binding_access::object(owned).port = 2;
  EXPECT_EQ(binding_access::object(std::as_const(owned)).port, 2);
}

TEST(Bind, MoveOnlyValuesMoveIn) {
  auto binding = bind<std::unique_ptr<int>>.to_value(std::make_unique<int>(5));
  ASSERT_NE(binding_access::object(binding), nullptr);
  EXPECT_EQ(*binding_access::object(binding), 5);
  const auto moved = std::move(binding).allow_copy();
  EXPECT_EQ(*binding_access::object(moved), 5);
}

}  // namespace
