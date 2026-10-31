// Tests for include/trellis/meta/type_name.hpp: type_name<T>(), a constant expression naming T.
//
// Compilers spell some types differently (spike S3 §3.4): GCC writes "const w&", "box<box<int> >"
// and "int [3]" where Clang writes "const w &", "box<box<int>>" and "int[3]", and MSVC drops the
// space after a comma. So the checks compare spellings with same_spelling(), which ignores
// whitespace that doesn't separate two identifier characters, and are exact only for names that
// every compiler spells the same. Where compilers word a name differently (an anonymous namespace,
// an inline namespace), the checks accept each spelling. The TESTs print every name to the test
// log, which records what each compiler produced.
#include <trellis/config.hpp>
#include <trellis/meta/fixed_string.hpp>
#include <trellis/meta/type_list.hpp>
#include <trellis/meta/type_name.hpp>

#include <array>
#include <cstddef>
#include <iostream>
#include <memory>
#include <string_view>

#include <gtest/gtest.h>

namespace tn {

struct widget {};
class gadget {};
template <class T>
struct box {};
template <class A, class B>
struct pair_of {};
enum class color : unsigned char { red };
union number {
  int whole;
  float real;
};
struct classifier {};  // starts with a class key: stripping on MSVC must leave it alone
struct my_struct {};   // ends with one

}  // namespace tn

namespace {

struct hidden {};

using trellis::type_name;

constexpr bool is_identifier_char(char letter) {
  return letter == '_' || (letter >= 'a' && letter <= 'z') || (letter >= 'A' && letter <= 'Z') ||
         (letter >= '0' && letter <= '9');
}

// text with its insignificant spaces removed: a space stays only between two identifier
// characters, as in "const tn::widget" or "int tn::widget::*".
struct squeezed {
  std::array<char, 512> chars = {};
  std::size_t size = 0;

  [[nodiscard]] constexpr std::string_view view() const { return {chars.data(), size}; }
};
constexpr squeezed squeeze(std::string_view text) {
  squeezed out;
  for (std::size_t i = 0; i != text.size(); ++i) {
    if (text[i] == ' ' && !(out.size != 0 && is_identifier_char(out.chars[out.size - 1]) &&
                            i + 1 != text.size() && is_identifier_char(text[i + 1]))) {
      continue;
    }
    out.chars[out.size++] = text[i];
  }
  return out;
}
constexpr bool same_spelling(std::string_view actual, std::string_view expected) {
  return squeeze(actual).view() == squeeze(expected).view();
}

// The normalization itself.
static_assert(same_spelling("const tn::widget &", "const tn::widget&"));
static_assert(same_spelling("tn::box<tn::box<int> >", "tn::box<tn::box<int>>"));
static_assert(same_spelling("tn::pair_of<int,tn::widget>", "tn::pair_of<int, tn::widget>"));
static_assert(same_spelling("tn::widget *const", "tn::widget* const"));
static_assert(!same_spelling("unsigned int", "unsignedint"));
static_assert(!same_spelling("tn::widget", "tn::gadget"));

// ---- the same spelling on every compiler ------------------------------------------------------

// A constant expression: usable in static_assert, as plan §7.8's C++26 messages need.
static_assert(type_name<int>() == "int");
static_assert(type_name<double>() == "double");
static_assert(type_name<tn::widget>() == "tn::widget");  // struct
static_assert(type_name<tn::gadget>() == "tn::gadget");  // class
static_assert(type_name<tn::color>() == "tn::color");    // enum
static_assert(type_name<tn::number>() == "tn::number");  // union
static_assert(type_name<tn::box<tn::widget>>() == "tn::box<tn::widget>");
static_assert(type_name<tn::classifier>() == "tn::classifier");
static_assert(type_name<tn::my_struct>() == "tn::my_struct");

// ---- spellings that differ only in whitespace -------------------------------------------------

// Class templates.
static_assert(same_spelling(type_name<tn::box<tn::box<int>>>(), "tn::box<tn::box<int>>"));
static_assert(same_spelling(type_name<tn::pair_of<int, tn::widget>>(),
                            "tn::pair_of<int, tn::widget>"));
static_assert(same_spelling(type_name<tn::pair_of<tn::box<tn::gadget>, tn::color>>(),
                            "tn::pair_of<tn::box<tn::gadget>, tn::color>"));
static_assert(same_spelling(type_name<std::shared_ptr<tn::widget>>(),
                            "std::shared_ptr<tn::widget>"));
// Pointers.
static_assert(same_spelling(type_name<tn::widget*>(), "tn::widget*"));
static_assert(same_spelling(type_name<const tn::widget*>(), "const tn::widget*"));
static_assert(same_spelling(type_name<tn::widget* const>(), "tn::widget* const"));
static_assert(same_spelling(type_name<tn::widget**>(), "tn::widget**"));
static_assert(same_spelling(type_name<tn::box<tn::widget>*>(), "tn::box<tn::widget>*"));
static_assert(same_spelling(type_name<int tn::widget::*>(), "int tn::widget::*"));
// References.
static_assert(same_spelling(type_name<tn::widget&>(), "tn::widget&"));
static_assert(same_spelling(type_name<const tn::widget&>(), "const tn::widget&"));
static_assert(same_spelling(type_name<tn::widget&&>(), "tn::widget&&"));
static_assert(same_spelling(type_name<const tn::box<int>&>(), "const tn::box<int>&"));
static_assert(same_spelling(type_name<tn::widget*&>(), "tn::widget*&"));
// cv-qualified types and arrays.
static_assert(same_spelling(type_name<const int>(), "const int"));
static_assert(same_spelling(type_name<const tn::widget>(), "const tn::widget"));
// Naming an array type is the point here.
// NOLINTNEXTLINE(modernize-avoid-c-arrays)
static_assert(same_spelling(type_name<int[3]>(), "int[3]"));

// ---- names that compilers word differently ----------------------------------------------------

// An anonymous namespace: GCC writes {anonymous}::, Clang (anonymous namespace)::, MSVC
// `anonymous namespace'::.
constexpr std::string_view hidden_name = type_name<hidden>();
static_assert(hidden_name.find("anonymous") != std::string_view::npos);
static_assert(hidden_name.ends_with("::hidden"));
// An inline namespace: GCC and MSVC spell out trellis::v1::, Clang leaves it out. Library types
// are fully qualified, including those in trellis::detail, the namespace type_name's own code is in
// (GCC drops the qualification of a type from the namespace of the function whose signature
// names it, unless that namespace is a different one).
static_assert(same_spelling(type_name<trellis::detail::type_list<int>>(),
                            "trellis::detail::type_list<int>") ||
              same_spelling(type_name<trellis::detail::type_list<int>>(),
                            "trellis::v1::detail::type_list<int>"));
static_assert(type_name<trellis::fixed_string<3>>() == "trellis::fixed_string<3>" ||
              type_name<trellis::fixed_string<3>>() == "trellis::v1::fixed_string<3>");
// A function type: MSVC adds a calling convention, so only the parts are checked.
constexpr std::string_view function_name = type_name<void(int, char)>();
static_assert(function_name.starts_with("void") &&
              function_name.find("int") != std::string_view::npos &&
              function_name.ends_with("char)"));

// ---- storage ----------------------------------------------------------------------------------

// The name lives in static storage of its own: one copy per type, terminated by '\0' so that
// data() is a C string, and the same address on every call.
static_assert(type_name<tn::widget>().data()[type_name<tn::widget>().size()] == '\0');
static_assert(type_name<tn::widget>().data() == type_name<tn::widget>().data());
static_assert(type_name<tn::widget>().data() != type_name<tn::gadget>().data());

// ---- MSVC's class keys ------------------------------------------------------------------------

// __FUNCSIG__ spells class types with their keys ("struct tn::widget"); type_name drops them
// there. The stripping is plain constexpr code, so it is tested on every compiler, on MSVC-style
// input.
constexpr bool strips_to(std::string_view raw, std::string_view expected) {
  std::array<char, 128> buffer = {};
  const std::size_t length = trellis::detail::strip_class_keys(raw, buffer.data());
  return length == trellis::detail::strip_class_keys(raw, nullptr) &&
         std::string_view(buffer.data(), length) == expected;
}
static_assert(strips_to("int", "int"));
static_assert(strips_to("struct tn::widget", "tn::widget"));
static_assert(strips_to("class tn::gadget", "tn::gadget"));
static_assert(strips_to("enum tn::color", "tn::color"));
static_assert(strips_to("union tn::number", "tn::number"));
static_assert(strips_to("const struct tn::widget &", "const tn::widget &"));
static_assert(strips_to("struct tn::widget *const ", "tn::widget *const"));
static_assert(strips_to("class std::shared_ptr<struct tn::widget> ",
                        "std::shared_ptr<tn::widget>"));
static_assert(strips_to("struct tn::pair_of<int,class tn::box<enum tn::color> >",
                        "tn::pair_of<int,tn::box<tn::color> >"));
static_assert(strips_to("struct `anonymous namespace'::hidden", "`anonymous namespace'::hidden"));
// Keys only as whole words: identifiers that contain one are left alone.
static_assert(strips_to("struct tn::classifier", "tn::classifier"));
static_assert(strips_to("struct tn::my_struct", "tn::my_struct"));
static_assert(strips_to("struct tn::my_struct *", "tn::my_struct *"));
static_assert(strips_to("subclass", "subclass"));

// ---- the signature macro behind it ------------------------------------------------------------

// TRELLIS_FUNCTION_SIGNATURE spells the enclosing function's template arguments.
template <class T>
constexpr std::string_view signature_of_template() {
  return TRELLIS_FUNCTION_SIGNATURE;
}
static_assert(signature_of_template<tn::gadget>().find("gadget") != std::string_view::npos);
static_assert(signature_of_template<tn::gadget>().find("signature_of_template") !=
              std::string_view::npos);
#if defined(__clang__) || defined(__GNUC__)
static_assert(TRELLIS_FUNCTION_SIGNATURE_CLASS_KEYS == 0);
#endif

template <class T>
void print_name() {
  std::cout << "  type_name: " << type_name<T>() << '\n';
}

}  // namespace

TEST(TypeName, NamesClassesAndTemplates) {
  EXPECT_EQ(type_name<tn::widget>(), "tn::widget");
  EXPECT_EQ(type_name<tn::box<tn::widget>>(), "tn::box<tn::widget>");
  EXPECT_TRUE(
      same_spelling(type_name<tn::pair_of<int, tn::widget>>(), "tn::pair_of<int, tn::widget>"));
  EXPECT_TRUE(
      same_spelling(type_name<std::shared_ptr<tn::widget>>(), "std::shared_ptr<tn::widget>"));
}

TEST(TypeName, NamesPointersAndReferences) {
  EXPECT_TRUE(same_spelling(type_name<tn::widget*>(), "tn::widget*"));
  EXPECT_TRUE(same_spelling(type_name<const tn::widget&>(), "const tn::widget&"));
  EXPECT_TRUE(same_spelling(type_name<tn::widget&&>(), "tn::widget&&"));
}

TEST(TypeName, WorksAtRunTime) {
  // Called in ordinary code, the view still points at the name's static storage.
  const std::string_view name = type_name<tn::gadget>();
  EXPECT_EQ(name, "tn::gadget");
  EXPECT_EQ(name.data()[name.size()], '\0');
  EXPECT_EQ(std::string_view(name.data()), "tn::gadget");
}

TEST(TypeName, PrintsWhatThisCompilerProduces) {
  print_name<tn::widget>();
  print_name<tn::box<tn::box<int>>>();
  print_name<tn::pair_of<int, tn::widget>>();
  print_name<std::shared_ptr<tn::widget>>();
  print_name<tn::widget*>();
  print_name<tn::widget* const>();
  print_name<const tn::widget&>();
  print_name<tn::widget&&>();
  print_name<int[3]>();  // NOLINT(modernize-avoid-c-arrays): naming an array type
  print_name<hidden>();
  print_name<trellis::detail::type_list<int>>();
  print_name<trellis::fixed_string<3>>();
  print_name<void(int, char)>();
  SUCCEED();
}
