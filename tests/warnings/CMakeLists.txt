# Warnings that Trellis's own code must still give its users, for the compilers that have them
# (T1.15). Each snippet is compiled with the build's compiler and flags and the project warnings,
# without -Werror, and its test passes when the output holds the warning, located in the Trellis
# header that the snippet's call instantiates. Label compile_fail: like those tests, each checks
# what the compiler says about a snippet.
#
# dangling_field_with_ctor.cpp: Clang's -Wdangling-field, on by default, for a reference member
# that a .with_ctor<Args...>() declaration binds to a temporary, at static/declared_aggregate.hpp,
# where only -Wmissing-field-initializers is switched off (config.hpp,
# TRELLIS_PARTIAL_AGGREGATE_INIT_BEGIN). GCC has no such warning.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT MSVC)
  separate_arguments(trellis_cxx_flags UNIX_COMMAND "${CMAKE_CXX_FLAGS}")
  add_test(NAME warnings.dangling_field_with_ctor
    COMMAND "${CMAKE_CXX_COMPILER}" ${trellis_cxx_flags} -std=c++20 -fsyntax-only
            -fno-color-diagnostics ${TRELLIS_WARNING_FLAGS} "-I${PROJECT_SOURCE_DIR}/include"
            "${CMAKE_CURRENT_SOURCE_DIR}/dangling_field_with_ctor.cpp")
  set_tests_properties(warnings.dangling_field_with_ctor PROPERTIES
    LABELS compile_fail
    PASS_REGULAR_EXPRESSION
      "declared_aggregate\\.hpp:[0-9]+:[0-9]+: warning: temporary bound to reference member of allocated object will be destroyed at the end of the full-expression \\[-Wdangling-field\\]")
endif()
