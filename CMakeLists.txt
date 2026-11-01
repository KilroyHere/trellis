# Trellis: a header-only C++20 dependency injection library.
cmake_minimum_required(VERSION 3.21)

# The version is defined once, in include/trellis/config.hpp, so that the headers work without
# CMake. Read it from there.
set(trellis_config_header "${CMAKE_CURRENT_SOURCE_DIR}/include/trellis/config.hpp")
file(STRINGS "${trellis_config_header}" trellis_version_defines
     REGEX "^#define TRELLIS_VERSION_(MAJOR|MINOR|PATCH) +[0-9]+")
foreach(define IN LISTS trellis_version_defines)
  string(REGEX MATCH "^#define TRELLIS_VERSION_(MAJOR|MINOR|PATCH) +([0-9]+)" unused "${define}")
  set(trellis_version_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}")
endforeach()
if(NOT DEFINED trellis_version_MAJOR OR NOT DEFINED trellis_version_MINOR
   OR NOT DEFINED trellis_version_PATCH)
  message(FATAL_ERROR "Could not read TRELLIS_VERSION_MAJOR/MINOR/PATCH from ${trellis_config_header}")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${trellis_config_header}")

project(trellis
  VERSION ${trellis_version_MAJOR}.${trellis_version_MINOR}.${trellis_version_PATCH}
  DESCRIPTION "Header-only C++20 dependency injection"
  LANGUAGES CXX)

option(TRELLIS_BUILD_TESTS "Build the Trellis tests" ${PROJECT_IS_TOP_LEVEL})
# The runtime benchmarks need Google Benchmark 1.8 or later; scripts/bench-runtime.sh turns this on
# in build directories of its own (build/bench/runtime/<preset>). The compile-time benchmarks
# (benchmarks/compile/run.py) don't use CMake.
option(TRELLIS_BUILD_BENCHMARKS "Build the runtime benchmarks (needs Google Benchmark)" OFF)
# The presets turn this on, except for timing builds (timetrace), which must not measure cache hits.
option(TRELLIS_USE_CCACHE "Use ccache as the compiler launcher if it is installed" OFF)
# The C++20 module trellis (target trellis::module), which needs CMake 3.28, Ninja and Clang 16 or
# later with clang-scan-deps; modules/CMakeLists.txt checks them. The Clang presets turn it on.
option(TRELLIS_ENABLE_MODULE "Build the C++20 module trellis (needs CMake 3.28)" OFF)

if(TRELLIS_USE_CCACHE AND NOT CMAKE_CXX_COMPILER_LAUNCHER)
  find_program(TRELLIS_CCACHE_PROGRAM ccache)
  if(TRELLIS_CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${TRELLIS_CCACHE_PROGRAM}")
  endif()
endif()

include(GNUInstallDirs)

add_library(trellis INTERFACE)
add_library(trellis::trellis ALIAS trellis)
target_include_directories(trellis INTERFACE
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(trellis INTERFACE cxx_std_20)

# Installation and the CMake package: find_package(trellis CONFIG) gives trellis::trellis.
include(CMakePackageConfigHelpers)
set(trellis_install_cmakedir "${CMAKE_INSTALL_DATADIR}/cmake/trellis")
install(TARGETS trellis EXPORT trellis-targets)
install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}" FILES_MATCHING PATTERN "*.hpp")
install(EXPORT trellis-targets
  NAMESPACE trellis::
  FILE trellisTargets.cmake
  DESTINATION "${trellis_install_cmakedir}")
configure_package_config_file(cmake/trellisConfig.cmake.in
  "${PROJECT_BINARY_DIR}/trellisConfig.cmake"
  INSTALL_DESTINATION "${trellis_install_cmakedir}")
# Releases 0.x may break the API in any minor release (plan §10), so a 0.x request accepts only the
# same minor version. From 1.0 on, any release with the same major version is compatible.
if(PROJECT_VERSION_MAJOR EQUAL 0)
  set(trellis_version_compatibility SameMinorVersion)
else()
  set(trellis_version_compatibility SameMajorVersion)
endif()
write_basic_package_version_file("${PROJECT_BINARY_DIR}/trellisConfigVersion.cmake"
  COMPATIBILITY ${trellis_version_compatibility}
  ARCH_INDEPENDENT)
install(FILES
  "${PROJECT_BINARY_DIR}/trellisConfig.cmake"
  "${PROJECT_BINARY_DIR}/trellisConfigVersion.cmake"
  DESTINATION "${trellis_install_cmakedir}")
install(FILES LICENSE DESTINATION "${CMAKE_INSTALL_DOCDIR}")

# The module is for builds that add Trellis as a subdirectory (add_subdirectory, FetchContent). It
# isn't installed yet: find_package(trellis) provides the headers only.
if(TRELLIS_ENABLE_MODULE)
  add_subdirectory(modules)
endif()
if(TRELLIS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
  add_subdirectory(examples)
endif()
if(TRELLIS_BUILD_BENCHMARKS)
  add_subdirectory(benchmarks/runtime)
endif()
