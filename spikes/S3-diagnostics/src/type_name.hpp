// Spike S3 (throwaway): constexpr type names from __PRETTY_FUNCTION__ / __FUNCSIG__.
//
// What it does: the compiler spells the template argument inside the signature of
// raw_name<T>(). We locate T's spelling by probing raw_name<int>() once: the text
// before "int" is the prefix and the text after it the suffix, so no compiler- or
// version-specific offsets are hard-coded.
// Why: C++26 static_assert messages (P2741) and the explain pass need type names as
// constant expressions; there is no standard facility before reflection.
#pragma once

#include <cstddef>
#include <string_view>

namespace s3 {

template <class T>
constexpr std::string_view raw_name() {
#if defined(__clang__) || defined(__GNUC__)
  return __PRETTY_FUNCTION__;
#elif defined(_MSC_VER)
  return __FUNCSIG__;
#else
#error "no function-signature intrinsic"
#endif
}

inline constexpr std::string_view probe = raw_name<int>();
inline constexpr std::size_t prefix_len = probe.find("int");
inline constexpr std::size_t suffix_len = probe.size() - prefix_len - 3;

template <class T>
constexpr std::string_view type_name() {
  constexpr std::string_view raw = raw_name<T>();
  return raw.substr(prefix_len, raw.size() - prefix_len - suffix_len);
}

}  // namespace s3
