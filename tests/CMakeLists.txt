# Everything the project builds for testing. Third-party code comes first; then the project warning
# set is switched on, as errors, for every target defined below and in the subdirectories.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Golden tests compare compiler output with snapshots made with libstdc++. A standard library that
# spells its names differently (libc++: std::__1, __memory/ headers) can't match them, so the
# libc++ preset turns them off. tools/golden.py then refuses to compare or update in that build.
option(TRELLIS_GOLDEN_TESTS "Register the golden-diagnostics tests" ON)

# GoogleTest and gMock: an installed package if there is one (Ubuntu's libgtest-dev and
# libgmock-dev provide GTestConfig.cmake), otherwise built from source, pinned to the v1.18.0
# release. Presets whose standard library differs from the installed package's (libc++) set
# CMAKE_DISABLE_FIND_PACKAGE_GTest=ON to force the source build.
find_package(GTest 1.11 CONFIG QUIET)
if(GTest_FOUND)
  message(STATUS "GoogleTest: installed package ${GTest_VERSION} (${GTest_DIR})")
else()
  message(STATUS "GoogleTest: building v1.18.0 from source")
  include(FetchContent)
  FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG 063de7e9578f82b369302001269680b4b1553359)  # v1.18.0
  set(INSTALL_GTEST OFF)
  set(gtest_force_shared_crt ON)  # MSVC: same runtime library as the tests
  FetchContent_MakeAvailable(googletest)
  # Mark GoogleTest's headers as system headers, as an installed package's are, so that the project
  # warning set doesn't apply to them.
  foreach(target IN ITEMS gtest gtest_main gmock gmock_main)
    get_target_property(include_dirs ${target} INTERFACE_INCLUDE_DIRECTORIES)
    set_target_properties(${target} PROPERTIES INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${include_dirs}")
  endforeach()
endif()
include(GoogleTest)

find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)

include("${PROJECT_SOURCE_DIR}/cmake/TrellisWarnings.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/TrellisTesting.cmake")
trellis_enable_project_warnings()

set(TRELLIS_COMPILER_CONFIG "${PROJECT_BINARY_DIR}/trellis_compiler.json")
trellis_write_compiler_config("${TRELLIS_COMPILER_CONFIG}")

trellis_add_header_check()
trellis_add_compile_fail_tests("${TRELLIS_COMPILER_CONFIG}")
if(TRELLIS_GOLDEN_TESTS)
  trellis_add_golden_tests("${TRELLIS_COMPILER_CONFIG}")
endif()
# The manifest check compiles nothing and needs no compiler, so it runs in every build: every test
# tests/golden_manifest.txt lists exists and has its goldens, and every golden is listed.
add_test(NAME tools.golden_manifest
  COMMAND "${Python3_EXECUTABLE}" -I "${PROJECT_SOURCE_DIR}/tools/golden.py" --check-manifest)
set_tests_properties(tools.golden_manifest PROPERTIES LABELS tools)

# Self-tests of the two runners, label tools: the compile-fail runner must report a snippet that
# compiles, or one that misses an expectation, as a failure; the golden runner must normalize
# compiler output and update goldens as documented. The compile-fail self-test also runs under
# -L compile_fail. The golden one stays out of -L golden, so that on a compiler without goldens
# `ctest -L golden` finds no test and fails instead of passing on the self-test alone.
foreach(selftest IN ITEMS compile_fail golden)
  add_test(NAME tools.${selftest}_selftest
    COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_CURRENT_SOURCE_DIR}/tools/${selftest}_selftest.py"
            --config "${TRELLIS_COMPILER_CONFIG}")
endforeach()
set_tests_properties(tools.compile_fail_selftest PROPERTIES LABELS "compile_fail;tools")
set_tests_properties(tools.golden_selftest PROPERTIES LABELS tools)
# The budget checker of benchmarks/compile/run.py, on synthetic records; it compiles nothing.
add_test(NAME tools.bench_budgets_selftest
  COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_CURRENT_SOURCE_DIR}/tools/bench_budgets_selftest.py")
set_tests_properties(tools.bench_budgets_selftest PROPERTIES LABELS tools)
# The error catalog: every TRL error ID that include/ uses has a page in docs/errors/, linked from
# the catalog's index. Like the manifest check, it compiles nothing and runs in every build. Its
# self-test plants IDs in temporary trees, where the check must fail.
add_test(NAME tools.error_catalog
  COMMAND "${Python3_EXECUTABLE}" -I "${PROJECT_SOURCE_DIR}/tools/error_catalog.py")
add_test(NAME tools.error_catalog_selftest
  COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_CURRENT_SOURCE_DIR}/tools/error_catalog_selftest.py")
set_tests_properties(tools.error_catalog tools.error_catalog_selftest PROPERTIES LABELS tools)
# README.md shows the parts of examples/hello_world.cpp it marks, line for line, both ways; the
# test example.hello_world builds and runs the example (examples/CMakeLists.txt). Compiles nothing.
# Its self-test runs the check on files where it must fail.
add_test(NAME tools.readme_example
  COMMAND "${Python3_EXECUTABLE}" -I "${PROJECT_SOURCE_DIR}/tools/readme_example.py")
add_test(NAME tools.readme_example_selftest
  COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_CURRENT_SOURCE_DIR}/tools/readme_example_selftest.py")
set_tests_properties(tools.readme_example tools.readme_example_selftest PROPERTIES LABELS tools)

add_subdirectory(unit)
add_subdirectory(property)
