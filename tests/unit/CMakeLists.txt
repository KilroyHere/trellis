# Unit tests (GoogleTest and gMock, label unit).

function(trellis_unit_test_executable target standard)
  add_executable(${target} ${ARGN})
  target_link_libraries(${target} PRIVATE trellis::trellis GTest::gmock_main)
  target_compile_definitions(${target} PRIVATE
    TRELLIS_TEST_STANDARD=${standard}
    TRELLIS_TEST_PROJECT_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    TRELLIS_TEST_PROJECT_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    TRELLIS_TEST_PROJECT_VERSION_PATCH=${PROJECT_VERSION_PATCH})
endfunction()

# The tests that are compiled in every language mode the compiler supports: config.hpp's feature
# macros differ by mode, and so do parts of the meta toolkit (result<T> is std::expected from C++23
# where the standard library has it, at_t can use C++26 pack indexing, and a fixed_string can be a
# C++26 static_assert message).
set(trellis_per_mode_tests
  config_test.cpp
  error_namespace_test.cpp
  fixed_string_test.cpp
  result_after_injector_test.cpp
  result_test.cpp
  type_list_test.cpp
  type_map_test.cpp
  type_name_test.cpp)

# Tests whose subject is the same in every language mode, compiled in C++20 only: the
# explicit constructor declarations (T1.2), and the binding vocabulary, modules and the
# injector (T1.4), which use no TRELLIS_HAS_* feature, create<T>() with the injector's storage
# (T1.5), the explain pass (T1.6), overrides with the testing helpers, the gMock example among
# them (T1.7), and the constructor declared on a binding, with aggregates declared in part (T1.15).
set(trellis_cxx20_tests
  bind_test.cpp
  create_test.cpp
  ctor_traits_test.cpp
  declared_aggregate_test.cpp
  deduced_test.cpp
  explain_test.cpp
  injector_test.cpp
  module_test.cpp
  opaque_host.cpp
  override_test.cpp
  request_forms_test.cpp
  resolve_forms_test.cpp
  smoke_test.cpp
  testing_test.cpp
  threading_test.cpp
  with_ctor_engine_test.cpp
  with_ctor_test.cpp)

find_package(Threads REQUIRED)

trellis_unit_test_executable(trellis_unit_tests 20 ${trellis_cxx20_tests} ${trellis_per_mode_tests})
target_link_libraries(trellis_unit_tests PRIVATE Threads::Threads)
gtest_discover_tests(trellis_unit_tests DISCOVERY_MODE PRE_TEST PROPERTIES LABELS unit)

# The singleton slots with the fallback for standard libraries without atomic::wait, a mutex and a
# condition variable per injector (ADR-0012 item 3), forced here, where atomic::wait exists. The
# macro changes the injector's members, so this program defines it in its only translation unit.
trellis_unit_test_executable(trellis_wait_fallback_tests 20 threading_test.cpp)
target_compile_definitions(trellis_wait_fallback_tests PRIVATE TRELLIS_HAS_ATOMIC_WAIT=0)
target_link_libraries(trellis_wait_fallback_tests PRIVATE Threads::Threads)
gtest_discover_tests(trellis_wait_fallback_tests
  DISCOVERY_MODE PRE_TEST TEST_PREFIX wait_fallback. PROPERTIES LABELS unit)

# The research scenario wired with Trellis, against the hand-wired version (T1.5). Its own
# program, with the research's domain header on the include path (and its warnings checked as the
# project's), and test names prefixed scenario., which `ctest -R scenario` selects.
trellis_unit_test_executable(trellis_scenario_tests 20 scenario_test.cpp)
target_include_directories(trellis_scenario_tests PRIVATE
  "${PROJECT_SOURCE_DIR}/research/experiments/common")
gtest_discover_tests(trellis_scenario_tests
  DISCOVERY_MODE PRE_TEST TEST_PREFIX scenario. PROPERTIES LABELS unit)

# The meta toolkit without exceptions and RTTI (NFR-11). GoogleTest needs both, so this is a plain
# program that returns 0 when its checks pass. GCC and Clang only: MSVC spells the flags otherwise.
if(NOT MSVC)
  add_executable(trellis_no_exceptions_test no_exceptions_test.cpp)
  target_link_libraries(trellis_no_exceptions_test PRIVATE trellis::trellis)
  target_compile_options(trellis_no_exceptions_test PRIVATE -fno-exceptions -fno-rtti)
  add_test(NAME unit.no_exceptions COMMAND trellis_no_exceptions_test)
  set_tests_properties(unit.no_exceptions PROPERTIES LABELS unit)

  # create<T>() and the injector's storage in the same mode (T1.5).
  add_executable(trellis_no_exceptions_create_test no_exceptions_create_test.cpp)
  target_link_libraries(trellis_no_exceptions_create_test PRIVATE trellis::trellis)
  target_compile_options(trellis_no_exceptions_create_test PRIVATE -fno-exceptions -fno-rtti)
  add_test(NAME unit.no_exceptions_create COMMAND trellis_no_exceptions_create_test)
  set_tests_properties(unit.no_exceptions_create PROPERTIES LABELS unit)
endif()

# The same tests again in C++23 and C++26 mode, where the TRELLIS_HAS_* macros can be 1, so that
# each macro, and the code it selects, is tested both ways. GCC and Clang only: these modes are
# probed with GCC-style flags.
if(NOT MSVC)
  if(cxx_std_23 IN_LIST CMAKE_CXX_COMPILE_FEATURES)
    trellis_unit_test_executable(trellis_unit_tests_cxx23 23 ${trellis_per_mode_tests})
    set_target_properties(trellis_unit_tests_cxx23 PROPERTIES CXX_STANDARD 23)
    gtest_discover_tests(trellis_unit_tests_cxx23
      DISCOVERY_MODE PRE_TEST TEST_PREFIX cxx23. PROPERTIES LABELS unit)
  endif()

  # CMake 3.28 knows no C++26 flag for GCC or Clang, so fall back to -std=c++2c where the compiler
  # accepts it (GCC 14, Clang 17 and later). It comes after CMake's own -std flag, so it wins; the
  # test checks the resulting __cplusplus.
  include(CheckCXXCompilerFlag)
  check_cxx_compiler_flag(-std=c++2c TRELLIS_COMPILER_ACCEPTS_STD_CXX2C)
  if(cxx_std_26 IN_LIST CMAKE_CXX_COMPILE_FEATURES OR TRELLIS_COMPILER_ACCEPTS_STD_CXX2C)
    trellis_unit_test_executable(trellis_unit_tests_cxx26 26 ${trellis_per_mode_tests})
    if(cxx_std_26 IN_LIST CMAKE_CXX_COMPILE_FEATURES)
      set_target_properties(trellis_unit_tests_cxx26 PROPERTIES CXX_STANDARD 26)
    else()
      target_compile_options(trellis_unit_tests_cxx26 PRIVATE -std=c++2c)
    endif()
    gtest_discover_tests(trellis_unit_tests_cxx26
      DISCOVERY_MODE PRE_TEST TEST_PREFIX cxx26. PROPERTIES LABELS unit)
  endif()
endif()
