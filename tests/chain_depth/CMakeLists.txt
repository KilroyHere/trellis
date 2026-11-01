# The chain-depth test (NFR-09, ADR-0014 item 3, T1.11). check_chain_depth.py generates a dependency
# chain of 200 hops (150 on MSVC before 19.50) in each of its shapes, one per request form plus
# declared constructors, aggregates, an eager injector and a mix, and compiles it with this build's
# compiler and flags at the compiler's default template depth. One test per shape,
# chain_depth.<shape>, label chain_depth, so that the shapes run in parallel and
# `ctest -R chain_depth` selects them all; chain_depth.selftest, label tools, builds, links and runs
# each shape's program at a few hops. The shapes come from the script, so a new one gets its test.
# The deepest chain per shape is measured on demand, and recorded in deepest.json:
#   python3 tests/chain_depth/check_chain_depth.py --preset <preset> --deepest --record
set(script "${CMAKE_CURRENT_SOURCE_DIR}/check_chain_depth.py")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${script}")
execute_process(
  COMMAND "${Python3_EXECUTABLE}" -I "${script}" --list-shapes
  OUTPUT_VARIABLE listing
  RESULT_VARIABLE status)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "check_chain_depth.py --list-shapes failed with status ${status}")
endif()
# One "name: summary" line per shape.
string(REPLACE "\n" ";" lines "${listing}")
foreach(line IN LISTS lines)
  if(NOT line MATCHES "^([a-z0-9_]+): ")
    continue()
  endif()
  set(shape "${CMAKE_MATCH_1}")
  add_test(NAME chain_depth.${shape}
    COMMAND "${Python3_EXECUTABLE}" -I "${script}" --config "${TRELLIS_COMPILER_CONFIG}"
            --shapes ${shape})
  set_tests_properties(chain_depth.${shape} PROPERTIES LABELS chain_depth)
endforeach()
add_test(NAME chain_depth.selftest
  COMMAND "${Python3_EXECUTABLE}" -I "${script}" --config "${TRELLIS_COMPILER_CONFIG}" --self-test)
set_tests_properties(chain_depth.selftest PROPERTIES LABELS tools)
