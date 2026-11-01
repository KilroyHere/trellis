# The C++20 modules trellis and trellis.testing (plan §10, ADR-0008): modules/trellis.cppm and
# modules/trellis.testing.cppm, built as the target trellis::module when TRELLIS_ENABLE_MODULE is
# on. A program links trellis::module and writes `import trellis;`, and a test may add
# `import trellis.testing;`. The interface units include the headers in their global module
# fragments, so trellis::module also carries trellis::trellis, for <trellis/config.hpp> and its
# macros, which an import doesn't provide. docs/internals/delivery.md explains the choices below.
#
# What the build needs, checked here so that a missing piece fails at configure time with its
# reason rather than in the middle of a build:
# - CMake 3.28 or later, the first version with C++20 named modules outside experimental gates;
# - a generator that builds them: Ninja (1.11 or later, which CMake checks itself) or Visual
#   Studio 2022;
# - Clang 16 or later with its clang-scan-deps, which CMake uses to find each source's imports and
#   which Debian and Ubuntu package separately (clang-tools-<major>). Clang 18 and 20 are tested,
#   with libstdc++ and with libc++;
# - GCC is refused before GCC 15. GCC 14 builds the module, but its importers can't use it:
#   type_name<T>() returns wrong names, because __PRETTY_FUNCTION__ spells a function of the
#   global module fragment one way inside the module and another way in an importer; and class
#   template argument deduction ignores the deduction guides, so trellis::fixed_string{"text"}
#   fails. GCC 15 or later is accepted untested, as is MSVC 19.34 (Visual Studio 17.4) or later,
#   with a warning, as is any compiler but Clang 18 and 20.
#
# Each missing piece is one bullet of the message, appended with string(APPEND), which joins its
# arguments as they are: a list would split a reason at every argument and every ";".
set(trellis_module_missing "")
if(CMAKE_VERSION VERSION_LESS 3.28)
  string(APPEND trellis_module_missing
    "\n  - CMake 3.28 or later (this is CMake ${CMAKE_VERSION})")
endif()
if(NOT CMAKE_GENERATOR MATCHES "^Ninja" AND NOT CMAKE_GENERATOR MATCHES "^Visual Studio 1[7-9]")
  string(APPEND trellis_module_missing
    "\n  - the Ninja or the Visual Studio 2022 generator (this is ${CMAKE_GENERATOR})")
endif()
string(REGEX MATCH "^[0-9]+" trellis_module_major "${CMAKE_CXX_COMPILER_VERSION}")
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 16)
    string(APPEND trellis_module_missing
      "\n  - Clang 16 or later (this is ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION})")
  elseif(NOT CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS)
    string(APPEND trellis_module_missing
      "\n  - clang-scan-deps for ${CMAKE_CXX_COMPILER} (Debian and Ubuntu: the package "
      "clang-tools-${trellis_module_major}), which CMake looks for next to the compiler")
  endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 15)
    string(APPEND trellis_module_missing
      "\n  - a compiler whose modules work for Trellis: GCC ${CMAKE_CXX_COMPILER_VERSION} builds "
      "the module, but in importers type_name<T>() gives wrong names and fixed_string's "
      "deduction guide is ignored (docs/internals/delivery.md). Use Clang 16 or later")
  endif()
elseif(MSVC)
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 19.34)
    string(APPEND trellis_module_missing
      "\n  - MSVC 19.34 (Visual Studio 17.4) or later (this is ${CMAKE_CXX_COMPILER_VERSION})")
  endif()
else()
  string(APPEND trellis_module_missing
    "\n  - Clang, GCC 15 or MSVC (this is ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION})")
endif()
if(trellis_module_missing)
  message(FATAL_ERROR "TRELLIS_ENABLE_MODULE is ON, but the C++20 module trellis needs:"
    "${trellis_module_missing}\n"
    "Turn TRELLIS_ENABLE_MODULE off to build without the module.")
endif()
# A compiler can build the module and still give importers wrong results without an error, as
# GCC 14 does with type_name<T>(), so a compiler other than the tested ones gets a warning that
# says how to check it: the module's tests, whose static_asserts catch GCC 14's faults.
if(NOT (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND trellis_module_major MATCHES "^(18|20)$"
        AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU"))
  message(WARNING "Trellis: the C++20 module is untested with ${CMAKE_CXX_COMPILER_ID} "
    "${CMAKE_CXX_COMPILER_VERSION}; it is tested with Clang 18 and 20. A compiler may build it "
    "and still give importers wrong results, as GCC 14 does with type_name<T>(). Check it with "
    "the module's tests: configure with TRELLIS_BUILD_TESTS=ON and run ctest -L module.")
endif()

# A static library: each interface unit compiles to an object file (its module initializer) as well
# as to the compiled module interface that importers read. It holds both modules: trellis, and
# trellis.testing for <trellis/testing/wiring.hpp>, which re-exports trellis.
add_library(trellis_module STATIC)
add_library(trellis::module ALIAS trellis_module)
target_sources(trellis_module PUBLIC FILE_SET CXX_MODULES FILES trellis.cppm trellis.testing.cppm)
target_link_libraries(trellis_module PUBLIC trellis::trellis)
target_compile_features(trellis_module PUBLIC cxx_std_20)
# The module's own build, and only that (PRIVATE): config.hpp reads TRELLIS_BUILDING_MODULE to make
# static/declared_aggregate.hpp a system header where Clang before 20 needs one to keep a warning
# from importers (TRELLIS_MODULE_SYSTEM_HEADER). Importers read the compiled interface and don't
# need it.
target_compile_definitions(trellis_module PRIVATE TRELLIS_BUILDING_MODULE=1)
# No compiler launcher, so no ccache, for the module or for its importers (trellis_module_importer
# below): ccache 4.9 doesn't hash the compiled module interface that an importer reads, so after a
# header changes it can return an importer's stale object file. It doesn't cache the interface
# unit itself either ("unsupported source language"), so nothing is lost.
set_target_properties(trellis_module PROPERTIES CXX_COMPILER_LAUNCHER "")
message(STATUS
  "Trellis: the C++20 modules trellis and trellis.testing are enabled (target trellis::module)")

# trellis_module_importer(TARGET): sets up TARGET, a target of this project that writes
# `import trellis;`, as the module's tests do. CMake scans a source for imports only when policy
# CMP0155 is NEW, which needs cmake_minimum_required(VERSION 3.28), or when the target asks for it,
# as here. Consumers outside the project that require CMake 3.28 get the scan by default; they
# should also turn off a ccache launcher for their importers, for the reason above.
function(trellis_module_importer target)
  target_link_libraries(${target} PRIVATE trellis::module)
  set_target_properties(${target} PROPERTIES CXX_SCAN_FOR_MODULES ON CXX_COMPILER_LAUNCHER "")
endfunction()
