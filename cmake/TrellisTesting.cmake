# Test registration helpers used by tests/CMakeLists.txt.
#
# tools/compile_fail.py and tools/golden.py compile snippets themselves, calling the build's compiler
# directly (never through ccache) with the build's flags. trellis_write_compiler_config() records
# both in a JSON file at configure time; the tools read it through --config, or through --preset,
# which finds build/<preset>/trellis_compiler.json.

function(_trellis_json_string out value)
  string(REPLACE "\\" "\\\\" value "${value}")
  string(REPLACE "\"" "\\\"" value "${value}")
  set(${out} "\"${value}\"" PARENT_SCOPE)
endfunction()

# Sets TRELLIS_GOLDEN_NAME in the caller to "<compiler>-<major>", the directory under
# tests/diagnostics_golden/ that holds this compiler's goldens: gcc-13, clang-18, msvc-19, ...
function(trellis_golden_name)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(family gcc)
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    set(family clang-cl)
  else()
    string(TOLOWER "${CMAKE_CXX_COMPILER_ID}" family)
  endif()
  string(REGEX MATCH "^[0-9]+" major "${CMAKE_CXX_COMPILER_VERSION}")
  set(TRELLIS_GOLDEN_NAME "${family}-${major}" PARENT_SCOPE)
endfunction()

# Writes the compiler and flags for the compile-fail and golden runners to FILE: the preset's
# CMAKE_CXX_FLAGS and build-type flags (so sanitizer and libc++ presets test what they build),
# -std=c++20, the include directory and the project warnings, but not -Werror.
function(trellis_write_compiler_config file)
  trellis_golden_name()
  string(TOUPPER "${CMAKE_BUILD_TYPE}" build_type)
  separate_arguments(flags NATIVE_COMMAND "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${build_type}}")
  if(CMAKE_CXX_COMPILER_TARGET)
    list(APPEND flags "--target=${CMAKE_CXX_COMPILER_TARGET}")
  endif()
  if(CMAKE_SYSROOT)
    list(APPEND flags "--sysroot=${CMAKE_SYSROOT}")
  endif()
  if(APPLE AND IS_DIRECTORY "${CMAKE_OSX_SYSROOT}")  # untested: macOS runs only in CI
    list(APPEND flags -isysroot "${CMAKE_OSX_SYSROOT}")
  endif()
  list(APPEND flags ${CMAKE_CXX20_STANDARD_COMPILE_OPTION} "-I${PROJECT_SOURCE_DIR}/include"
       ${TRELLIS_WARNING_FLAGS})
  # -ftime-trace only profiles the compiler. With -fsyntax-only, Clang warns that it is unused,
  # which would add a line to every diagnostic the runners check.
  list(FILTER flags EXCLUDE REGEX "^-ftime-trace")

  if(MSVC)
    set(frontend MSVC)
  else()
    set(frontend GNU)
  endif()
  if(TRELLIS_GOLDEN_TESTS)
    set(golden_tests true)
  else()
    set(golden_tests false)
  endif()
  set(json_flags "")
  foreach(flag IN LISTS flags)
    _trellis_json_string(quoted "${flag}")
    string(APPEND json_flags "\n    ${quoted},")
  endforeach()
  string(REGEX REPLACE ",$" "" json_flags "${json_flags}")
  foreach(name IN ITEMS CMAKE_CXX_COMPILER CMAKE_CXX_COMPILER_ID CMAKE_CXX_COMPILER_VERSION
                        TRELLIS_GOLDEN_NAME frontend PROJECT_SOURCE_DIR PROJECT_BINARY_DIR)
    _trellis_json_string(json_${name} "${${name}}")
  endforeach()
  file(CONFIGURE OUTPUT "${file}" @ONLY CONTENT [[{
  "compiler": @json_CMAKE_CXX_COMPILER@,
  "compiler_id": @json_CMAKE_CXX_COMPILER_ID@,
  "compiler_version": @json_CMAKE_CXX_COMPILER_VERSION@,
  "frontend": @json_frontend@,
  "golden_name": @json_TRELLIS_GOLDEN_NAME@,
  "golden_tests": @golden_tests@,
  "source_dir": @json_PROJECT_SOURCE_DIR@,
  "binary_dir": @json_PROJECT_BINARY_DIR@,
  "flags": [@json_flags@
  ]
}
]])
endfunction()

# Dots instead of slashes, so "sample/static_assert_message" becomes a CTest name part.
function(_trellis_test_name out relative_path)
  string(REGEX REPLACE "\\.[^./]+$" "" name "${relative_path}")
  string(REPLACE "/" "." name "${name}")
  set(${out} "${name}" PARENT_SCOPE)
endfunction()

# One test per tests/compile_fail/**/*.cpp: compile_fail.<area>.<name>, label compile_fail.
function(trellis_add_compile_fail_tests config)
  set(root "${PROJECT_SOURCE_DIR}/tests/compile_fail")
  file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${root}/*.cpp")
  foreach(source IN LISTS sources)
    file(RELATIVE_PATH relative "${root}" "${source}")
    _trellis_test_name(name "${relative}")
    add_test(NAME compile_fail.${name}
      COMMAND "${Python3_EXECUTABLE}" -I "${PROJECT_SOURCE_DIR}/tools/compile_fail.py"
              --config "${config}" "${source}")
    set_tests_properties(compile_fail.${name} PROPERTIES LABELS compile_fail)
  endforeach()
endfunction()

# One test per compile-fail test that tests/golden_manifest.txt lists for this compiler:
# golden.<area>.<name>, label golden. The tests come from the manifest, not from the golden files,
# so a missing or deleted golden fails its test instead of making it vanish. A compiler the
# manifest doesn't mention gets no golden tests. tools/golden.py --check-manifest validates the
# lines; here each one is just a name followed by compiler names.
function(trellis_add_golden_tests config)
  trellis_golden_name()
  set(manifest "${PROJECT_SOURCE_DIR}/tests/golden_manifest.txt")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${manifest}")
  file(STRINGS "${manifest}" lines LENGTH_MINIMUM 1)
  foreach(line IN LISTS lines)
    string(STRIP "${line}" line)
    if(line STREQUAL "" OR line MATCHES "^#")
      continue()
    endif()
    string(REGEX REPLACE "[ \t]+" ";" fields "${line}")
    list(GET fields 0 snippet)
    list(REMOVE_AT fields 0)
    if(NOT TRELLIS_GOLDEN_NAME IN_LIST fields)
      continue()
    endif()
    _trellis_test_name(name "${snippet}")
    add_test(NAME golden.${name}
      COMMAND "${Python3_EXECUTABLE}" -I "${PROJECT_SOURCE_DIR}/tools/golden.py"
              --config "${config}" "${snippet}")
    set_tests_properties(golden.${name} PROPERTIES LABELS golden)
  endforeach()
endfunction()

# The generated header test: one translation unit per public header, each including only that
# header (twice, so a missing include guard fails too), built with the project warning set. A header
# that needs something it doesn't include breaks the build. The test then runs the program, which
# lists the headers it covers.
function(trellis_add_header_check)
  set(include_dir "${PROJECT_SOURCE_DIR}/include")
  file(GLOB_RECURSE headers CONFIGURE_DEPENDS "${include_dir}/trellis/*.hpp")
  set(sources "")
  set(header_list "")
  foreach(header IN LISTS headers)
    file(RELATIVE_PATH relative "${include_dir}" "${header}")
    set(stub "${CMAKE_CURRENT_BINARY_DIR}/header_check/${relative}.cpp")
    file(CONFIGURE OUTPUT "${stub}" @ONLY CONTENT [[
// Generated by cmake/TrellisTesting.cmake: <@relative@> must compile on its own.
#include <@relative@>
#include <@relative@>  // a second inclusion must be harmless
]])
    list(APPEND sources "${stub}")
    string(APPEND header_list "    \"${relative}\",\n")
  endforeach()
  list(LENGTH headers count)
  set(main "${CMAKE_CURRENT_BINARY_DIR}/header_check/main.cpp")
  file(CONFIGURE OUTPUT "${main}" @ONLY CONTENT [[
// Generated by cmake/TrellisTesting.cmake: lists the public headers whose stand-alone
// translation units were compiled into this program.
#include <cstdio>

int main() {
  const char* const headers[] = {
@header_list@  };
  for (const char* header : headers) {
    std::printf("compiles on its own: %s\n", header);
  }
  std::printf("%d public headers checked\n", @count@);
}
]])
  add_executable(trellis_header_check ${sources} "${main}")
  target_link_libraries(trellis_header_check PRIVATE trellis::trellis)
  # #if reads an undefined name as 0, so a header that tests TRELLIS_HAS_* without including
  # config.hpp would silently take the "feature missing" branch. -Wundef (an error here, with the
  # project's -Werror) catches that. MSVC's counterpart, C4668, also fires inside Windows SDK
  # headers, so it is left off there; untested, as MSVC runs only in CI.
  if(NOT MSVC)
    target_compile_options(trellis_header_check PRIVATE -Wundef)
  endif()
  add_test(NAME headers.self_contained COMMAND trellis_header_check)
  set_tests_properties(headers.self_contained PROPERTIES LABELS headers)

  # The public headers together, in sorted order and in reverse, in every language mode the unit
  # tests use: a header that means something else after another one, as with an unqualified name
  # that the other header declares in the same namespace, breaks one of the orders. The check above
  # can't see that, as it compiles each header alone, nor can C++20 alone, where TRELLIS_HAS_* code
  # isn't compiled. The umbrella header comes last in both, so that it doesn't fix the order first.
  list(SORT headers)
  set(umbrella "${include_dir}/trellis/trellis.hpp")
  list(REMOVE_ITEM headers "${umbrella}")
  set(forward_includes "")
  foreach(header IN LISTS headers)
    file(RELATIVE_PATH relative "${include_dir}" "${header}")
    string(APPEND forward_includes "#include <${relative}>\n")
  endforeach()
  list(REVERSE headers)
  set(reverse_includes "")
  foreach(header IN LISTS headers)
    file(RELATIVE_PATH relative "${include_dir}" "${header}")
    string(APPEND reverse_includes "#include <${relative}>\n")
  endforeach()
  set(order_sources "")
  foreach(order IN ITEMS forward reverse)
    set(order_source "${CMAKE_CURRENT_BINARY_DIR}/header_check/order_${order}.cpp")
    set(includes "${${order}_includes}")
    file(CONFIGURE OUTPUT "${order_source}" @ONLY CONTENT [[
// Generated by cmake/TrellisTesting.cmake: every public header, in @order@ order, then the
// umbrella header.
@includes@#include <trellis/trellis.hpp>
]])
    list(APPEND order_sources "${order_source}")
  endforeach()
  set(order_main "${CMAKE_CURRENT_BINARY_DIR}/header_check/order_main.cpp")
  file(CONFIGURE OUTPUT "${order_main}" @ONLY CONTENT [[
// Generated by cmake/TrellisTesting.cmake: reports the language mode in which every public header
// compiled in sorted and in reverse order.
#include <cstdio>

int main() {
  std::printf("public headers compile in both orders, __cplusplus %ld\n", __cplusplus);
}
]])
  set(order_standards 20)
  if(NOT MSVC)  # as for the unit tests: C++23 and C++26 are probed with GCC-style flags
    if(cxx_std_23 IN_LIST CMAKE_CXX_COMPILE_FEATURES)
      list(APPEND order_standards 23)
    endif()
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag(-std=c++2c TRELLIS_COMPILER_ACCEPTS_STD_CXX2C)
    if(cxx_std_26 IN_LIST CMAKE_CXX_COMPILE_FEATURES OR TRELLIS_COMPILER_ACCEPTS_STD_CXX2C)
      list(APPEND order_standards 26)
    endif()
  endif()
  foreach(standard IN LISTS order_standards)
    set(target trellis_header_order_cxx${standard})
    add_executable(${target} ${order_sources} "${order_main}")
    target_link_libraries(${target} PRIVATE trellis::trellis)
    if(standard EQUAL 26 AND NOT cxx_std_26 IN_LIST CMAKE_CXX_COMPILE_FEATURES)
      target_compile_options(${target} PRIVATE -std=c++2c)  # after CMake's own -std, so it wins
    else()
      set_target_properties(${target} PROPERTIES CXX_STANDARD ${standard})
    endif()
    if(NOT MSVC)
      target_compile_options(${target} PRIVATE -Wundef)
    endif()
    add_test(NAME headers.both_orders.cxx${standard} COMMAND ${target})
    set_tests_properties(headers.both_orders.cxx${standard} PROPERTIES LABELS headers)
  endforeach()
endfunction()
