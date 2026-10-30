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

trellis_unit_test_executable(trellis_unit_tests 20 smoke_test.cpp config_test.cpp)
gtest_discover_tests(trellis_unit_tests DISCOVERY_MODE PRE_TEST PROPERTIES LABELS unit)

# config_test.cpp again in C++23 and C++26 mode, where the TRELLIS_HAS_* macros can be 1, so that
# each macro is tested both ways. GCC and Clang only: these modes are probed with GCC-style flags.
if(NOT MSVC)
  if(cxx_std_23 IN_LIST CMAKE_CXX_COMPILE_FEATURES)
    trellis_unit_test_executable(trellis_config_tests_cxx23 23 config_test.cpp)
    set_target_properties(trellis_config_tests_cxx23 PROPERTIES CXX_STANDARD 23)
    gtest_discover_tests(trellis_config_tests_cxx23
      DISCOVERY_MODE PRE_TEST TEST_PREFIX cxx23. PROPERTIES LABELS unit)
  endif()

  # CMake 3.28 knows no C++26 flag for GCC or Clang, so fall back to -std=c++2c where the compiler
  # accepts it (GCC 14, Clang 17 and later). It comes after CMake's own -std flag, so it wins; the
  # test checks the resulting __cplusplus.
  include(CheckCXXCompilerFlag)
  check_cxx_compiler_flag(-std=c++2c TRELLIS_COMPILER_ACCEPTS_STD_CXX2C)
  if(cxx_std_26 IN_LIST CMAKE_CXX_COMPILE_FEATURES OR TRELLIS_COMPILER_ACCEPTS_STD_CXX2C)
    trellis_unit_test_executable(trellis_config_tests_cxx26 26 config_test.cpp)
    if(cxx_std_26 IN_LIST CMAKE_CXX_COMPILE_FEATURES)
      set_target_properties(trellis_config_tests_cxx26 PROPERTIES CXX_STANDARD 26)
    else()
      target_compile_options(trellis_config_tests_cxx26 PRIVATE -std=c++2c)
    endif()
    gtest_discover_tests(trellis_config_tests_cxx26
      DISCOVERY_MODE PRE_TEST TEST_PREFIX cxx26. PROPERTIES LABELS unit)
  endif()
endif()
