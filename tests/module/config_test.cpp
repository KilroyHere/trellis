// The macros, for an importer of the C++20 module trellis: <trellis/config.hpp> next to
// `import trellis;`. A module exports no macros, so an importer that tests TRELLIS_VERSION or a
// TRELLIS_HAS_* macro includes config.hpp, which defines macros and declares nothing; the include
// comes before the import (docs/internals/delivery.md).
#include <trellis/config.hpp>

#include <string_view>

#include <gtest/gtest.h>

#if !defined(TRELLIS_VERSION) || !defined(TRELLIS_HAS_EXPECTED)
#error "<trellis/config.hpp> must provide the configuration macros next to import trellis;"
#endif

#if TRELLIS_HAS_EXPECTED
#include <expected>
#include <type_traits>
#endif

import trellis;

namespace {

// The macros describe the module's own build when the importer uses the same compiler, language
// mode and standard library, which a compiled module interface requires anyway. result<T> is
// std::expected exactly where TRELLIS_HAS_EXPECTED says so.
#if TRELLIS_HAS_EXPECTED
static_assert(std::is_same_v<trellis::result<int>, std::expected<int, trellis::failure>>);
#endif

}  // namespace

TEST(ModuleConfig, ConfigHeaderGivesTheMacros) {
  EXPECT_EQ(TRELLIS_VERSION,
            TRELLIS_VERSION_MAJOR * 10000 + TRELLIS_VERSION_MINOR * 100 + TRELLIS_VERSION_PATCH);
  EXPECT_EQ(trellis::type_name<long>(), std::string_view{"long"});
}
