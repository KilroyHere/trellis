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
#include <type_traits>

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

// plain_spelling<T>(): T's spelling in signature_of<T>(), in a buffer of its own, without MSVC's
// class keys.
template <class T>
constexpr auto plain_spelling() noexcept {
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

// The same, in static storage, for the spellings that a member pointer's name is built from.
template <class T>
inline constexpr auto plain_spelling_storage = plain_spelling<T>();

// ---- member pointers ----------------------------------------------------------------------------

// Two compilers misspell a member pointer type in their signatures (config.hpp,
// TRELLIS_TYPE_NAME_RESPELLS_MEMBER_POINTERS): Clang 21 drops the class's scope ("int widget::*"
// for int tn::widget::*), and MSVC the space before the class ("inttn::widget::*"). Where they do,
// type_name respells a member pointer type, cv-qualified or not, with its class's own name, which
// both spell right, and a space before it where the text before it ends in an identifier. A member
// pointer inside another type, such as a pointer to one or a template argument, keeps the
// compiler's spelling (docs/internals/meta-toolkit.md).

// The spellings that respell_member_pointer combines, by name, as they are easily confused.
struct member_pointer_spellings {
  std::string_view member_pointer;  // the member pointer type, as the compiler spells it
  std::string_view with_p;          // the same type with the marker class p (below) as its class
  std::string_view with_q;          // the same with q
  std::string_view marker_scope;    // what the compiler spells before a marker's name, if anything
  std::string_view qualified;       // the class's own spelling
};

// respell_member_pointer(spellings, out): spellings.member_pointer with its class replaced by
// spellings.qualified. The markers' names differ in their first and last characters, so the text
// that with_p and with_q share at either end surrounds the class, as signature_prefix finds T in a
// signature, minus the markers' scope where the compiler spells it
// ("trellis::detail::member_marker::"). That depends neither on how the compiler spells the class
// nor on the declarator around it, as in "void (widget::*)(int)" or "int widget::*const". A space
// goes before the class where the text before it ends in an identifier. A spelling that doesn't
// fit that shape is copied unchanged. Writes the result to out unless out is null, and returns its
// length (two passes, as strip_class_keys). Plain constexpr code, so it is tested on every
// compiler, on Clang 21 and MSVC input.
constexpr std::size_t respell_member_pointer(member_pointer_spellings spellings,
                                             char* out) noexcept {
  const std::string_view spelling = spellings.member_pointer;
  std::size_t before = common_prefix_length(spellings.with_p, spellings.with_q);
  const std::size_t after = common_suffix_length(spellings.with_p, spellings.with_q);
  const std::string_view shared = spellings.with_p.substr(0, before);
  const std::string_view scope = spellings.marker_scope;
  if (shared.size() >= scope.size() && shared.substr(shared.size() - scope.size()) == scope) {
    before -= scope.size();
  }
  std::size_t length = 0;
  const auto put = [&](std::string_view part) {
    for (const char letter : part) {
      if (out != nullptr) {
        out[length] = letter;
      }
      ++length;
    }
  };
  const std::string_view qualified = spellings.qualified;
  if (before + after > spelling.size() || qualified.empty()) {
    put(spelling);
    return length;
  }
  const std::string_view prefix = spelling.substr(0, before);
  put(prefix);
  if (!prefix.empty() && is_identifier_char(prefix.back()) && is_identifier_char(qualified[0])) {
    put(" ");
  }
  put(qualified);
  put(spelling.substr(spelling.size() - after));
  return length;
}

// The marker classes. Their names differ in the first and the last character.
namespace member_marker {
struct p {};
struct q {};
}  // namespace member_marker

// member_pointer_parts<T>: for a member pointer type T, cv-qualified or not, its class, and T with
// another class. One partial specialization per cv-qualification, because the qualifiers follow
// the class in the spelling ("int widget::*const"); undefined for any other type.
template <class T>
struct member_pointer_parts;
template <class M, class C>
struct member_pointer_parts<M C::*> {
  using class_type = C;
  template <class X>
  using with_class = M X::*;
};
template <class M, class C>
struct member_pointer_parts<M C::*const> {
  using class_type = C;
  template <class X>
  using with_class = M X::*const;
};
template <class M, class C>
struct member_pointer_parts<M C::*volatile> {
  using class_type = C;
  template <class X>
  using with_class = M X::*volatile;
};
template <class M, class C>
struct member_pointer_parts<M C::*const volatile> {
  using class_type = C;
  template <class X>
  using with_class = M X::*const volatile;
};

// respelled_member_pointer<T>(): T's name, for a member pointer type T, through
// respell_member_pointer. Defined on every compiler, so that the tests can compare it with the
// compiler's own spelling where that is right.
template <class T>
constexpr auto respelled_member_pointer() noexcept {
  using parts = member_pointer_parts<T>;
  constexpr std::string_view spelling = plain_spelling_storage<T>.view();
  constexpr std::string_view with_p =
      plain_spelling_storage<typename parts::template with_class<member_marker::p>>.view();
  constexpr std::string_view with_q =
      plain_spelling_storage<typename parts::template with_class<member_marker::q>>.view();
  constexpr std::string_view marker_p = plain_spelling_storage<member_marker::p>.view();
  constexpr std::string_view marker_scope = marker_p.substr(
      0, common_prefix_length(marker_p, plain_spelling_storage<member_marker::q>.view()));
  constexpr std::string_view qualified = plain_spelling_storage<typename parts::class_type>.view();
  constexpr member_pointer_spellings spellings{.member_pointer = spelling,
                                               .with_p = with_p,
                                               .with_q = with_q,
                                               .marker_scope = marker_scope,
                                               .qualified = qualified};
  fixed_string<respell_member_pointer(spellings, nullptr)> name;
  respell_member_pointer(spellings, name.data());
  return name;
}

// ---- the name -----------------------------------------------------------------------------------

// T's name in a buffer of its own: one per type, '\0'-terminated, and only the name, so that a
// name used at run time takes no more space in the binary than its characters.
// Where member pointers need no respelling, nothing is instantiated to ask whether T is one.
template <class T>
constexpr auto make_type_name() noexcept {
#if TRELLIS_TYPE_NAME_RESPELLS_MEMBER_POINTERS
  if constexpr (std::is_member_pointer_v<T>) {
    return respelled_member_pointer<T>();
  } else {
    return plain_spelling<T>();
  }
#else
  return plain_spelling<T>();
#endif
}

template <class T>
inline constexpr auto type_name_storage = make_type_name<T>();

}  // namespace detail

/// @brief The name of `T`, as the compiler spells it, as a constant expression.
///
/// For example `app::service`, `std::shared_ptr<app::repo>` or `const app::repo&`. The spelling is
/// the compiler's own and differs between compilers and standard libraries: GCC writes
/// `const app::repo&` and `trellis::v1::singleton` where Clang writes `const app::repo &` and
/// `trellis::singleton`, and MSVC drops the space after a comma. A member pointer type is named
/// from its class's own name, as `int app::widget::*` on every compiler, because MSVC and Clang 21
/// misspell it. The view points to static storage that holds just the name, followed by a `'\0'`,
/// so it stays valid for the whole program and its `data()` is a C string.
///
/// @tparam T Any type: complete or not, a reference, cv-qualified, a function or an array. `T` is
///           not instantiated.
/// @return The name, one copy per type.
///
/// @par Constraints
/// - Names are stable only between binaries built with the same compiler and standard library
///   (ADR-0011).
/// - A member pointer inside another type, such as `std::unique_ptr<int app::widget::*>`, keeps
///   the compiler's spelling: Clang 21 leaves out the class's namespace (`int widget::*`), and
///   MSVC the space before the class at the outermost level (`int app::widget::**` comes out as
///   `intapp::widget::* *`).
///
/// @par Errors
/// None.
///
/// @par Example
/// @code
/// namespace app { struct service {}; }
/// static_assert(trellis::type_name<app::service>() == "app::service");
/// void print() { std::printf("%s\n", trellis::type_name<app::service>().data()); }
/// @endcode
template <class T>
[[nodiscard]] constexpr std::string_view type_name() noexcept {
  return detail::type_name_storage<T>.view();
}

}  // namespace trellis::inline v1

#endif  // TRELLIS_META_TYPE_NAME_HPP
