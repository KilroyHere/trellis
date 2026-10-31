// Trellis meta toolkit: type_name<T>(), T's name as a constant expression (plan §7.1).
// Copyright (c) 2026 Aryan Patel
// SPDX-License-Identifier: BSL-1.0
//
// The names feed runtime messages, C++26 static_assert messages and the name-based type_key
// (plan §7.1, ADR-0006, ADR-0011). They are the compiler's own spelling, which differs between
// compilers and standard libraries (spike S3 §3.4): GCC writes "const app::repo&" and
// "trellis::v1::singleton" where Clang writes "const app::repo &" and "trellis::singleton".
//
// C++26's std::meta::display_string_of (plan §7.1) is not used yet: no compiler available here
// implements reflection (spike S5 is deferred), so that branch waits for Phase 6.
#ifndef TRELLIS_META_TYPE_NAME_HPP
#define TRELLIS_META_TYPE_NAME_HPP

#include <trellis/config.hpp>
#include <trellis/meta/fixed_string.hpp>

#include <cstddef>
#include <initializer_list>
#include <string_view>

#ifndef TRELLIS_FUNCTION_SIGNATURE
#error "trellis: type_name needs __PRETTY_FUNCTION__ (GCC, Clang) or __FUNCSIG__ (MSVC)"
#endif

namespace trellis::inline v1 {
namespace detail {

// The compiler spells T inside the signature of signature_of<T>(), for example GCC's
// "constexpr std::string_view trellis::v1::detail::probe::signature_of() [with T = int; ...]".
//
// The function has a namespace to itself, one that declares no types, because GCC leaves out the
// qualification of a type declared in the function's own namespace: in trellis::v1::detail,
// trellis::detail::type_list<int> came out as "type_list<int>" (gcc-13 and gcc-14, T1.1).
namespace probe {
template <class T>
constexpr std::string_view signature_of() noexcept {
  return TRELLIS_FUNCTION_SIGNATURE;
}
}  // namespace probe
using probe::signature_of;

constexpr std::size_t common_prefix_length(std::string_view a, std::string_view b) noexcept {
  std::size_t length = 0;
  while (length != a.size() && length != b.size() && a[length] == b[length]) {
    ++length;
  }
  return length;
}
constexpr std::size_t common_suffix_length(std::string_view a, std::string_view b) noexcept {
  std::size_t length = 0;
  while (length != a.size() && length != b.size() &&
         a[a.size() - 1 - length] == b[b.size() - 1 - length]) {
    ++length;
  }
  return length;
}

// Where T's spelling sits in signature_of<T>(). The text before it and the text after it are the
// same for every T, so comparing the signatures for two types whose spellings differ in their
// first and in their last character ("int", "double") measures both lengths, with no offsets
// hard-coded for any compiler (spike S3 searched for "int" instead, which the function's own name
// could contain).
inline constexpr std::size_t signature_prefix =
    common_prefix_length(signature_of<int>(), signature_of<double>());
inline constexpr std::size_t signature_suffix =
    common_suffix_length(signature_of<int>(), signature_of<double>());

// T's spelling, cut out of signature_of<T>().
constexpr std::string_view spelling_in(std::string_view signature) noexcept {
  return signature.substr(signature_prefix, signature.size() - signature_prefix - signature_suffix);
}

static_assert(spelling_in(signature_of<int>()) == "int" &&
                  spelling_in(signature_of<char>()) == "char",
              "trellis: type_name can't find the type in this compiler's function signatures");

constexpr bool is_identifier_char(char letter) noexcept {
  return letter == '_' || (letter >= 'a' && letter <= 'z') || (letter >= 'A' && letter <= 'Z') ||
         (letter >= '0' && letter <= '9');
}

// strip_class_keys(raw, out): MSVC's spelling without its class keys: "const struct a::b &"
// becomes "const a::b &". A key counts only as a whole word followed by a space, so "my_class"
// and "classifier" stay. Trailing spaces go too: MSVC writes "x<int> " before the '>' that closes
// signature_of's template argument list. Writes the result to out unless out is null, and returns
// its length; the two-pass use (count, then fill) builds a buffer of exactly the right size.
constexpr std::size_t strip_class_keys(std::string_view raw, char* out) noexcept {
  while (!raw.empty() && raw.back() == ' ') {
    raw.remove_suffix(1);
  }
  std::size_t length = 0;
  std::size_t i = 0;
  while (i != raw.size()) {
    std::size_t skip = 0;
    if (i == 0 || !is_identifier_char(raw[i - 1])) {
      for (const std::string_view key : {std::string_view("class "), std::string_view("struct "),
                                         std::string_view("enum "), std::string_view("union ")}) {
        if (raw.substr(i, key.size()) == key) {
          skip = key.size();
          break;
        }
      }
    }
    if (skip != 0) {
      i += skip;
      continue;
    }
    if (out != nullptr) {
      out[length] = raw[i];
    }
    ++length;
    ++i;
  }
  return length;
}

// T's name in a buffer of its own: one per type, '\0'-terminated, and only the name, so that a
// name used at run time takes no more space in the binary than its characters.
template <class T>
constexpr auto make_type_name() noexcept {
  constexpr std::string_view spelling = spelling_in(signature_of<T>());
  if constexpr (TRELLIS_FUNCTION_SIGNATURE_CLASS_KEYS) {
    fixed_string<strip_class_keys(spelling, nullptr)> name;
    strip_class_keys(spelling, name.data());
    return name;
  } else {
    fixed_string<spelling.size()> name;
    for (std::size_t i = 0; i != spelling.size(); ++i) {
      name[i] = spelling[i];
    }
    return name;
  }
}

template <class T>
inline constexpr auto type_name_storage = make_type_name<T>();

}  // namespace detail

/// @brief The name of `T`, as the compiler spells it, as a constant expression.
///
/// For example `app::service`, `std::shared_ptr<app::repo>` or `const app::repo&`. The spelling is
/// the compiler's own and differs between compilers and standard libraries: GCC writes
/// `const app::repo&` and `trellis::v1::singleton` where Clang writes `const app::repo &` and
/// `trellis::singleton`, and MSVC drops the space after a comma. The view points to static
/// storage that holds just the name, followed by a `'\0'`, so it stays valid for the whole program
/// and its `data()` is a C string.
///
/// @tparam T Any type: complete or not, a reference, cv-qualified, a function or an array. `T` is
///           not instantiated.
/// @return The name, one copy per type.
///
/// @par Constraints
/// - Names are stable only between binaries built with the same compiler and standard library
///   (ADR-0011).
///
/// @par Errors
/// None.
///
/// @par Example
/// @code
/// namespace app { struct service {}; }
/// static_assert(trellis::type_name<app::service>() == "app::service");
/// std::printf("%s\n", trellis::type_name<app::service>().data());
/// @endcode
template <class T>
[[nodiscard]] constexpr std::string_view type_name() noexcept {
  return detail::type_name_storage<T>.view();
}

}  // namespace trellis::inline v1

#endif  // TRELLIS_META_TYPE_NAME_HPP
