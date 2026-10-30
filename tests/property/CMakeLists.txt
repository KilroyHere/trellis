# Property tests, label property: the equivalence oracle of plan §9.1. check_sharing.py wires random
# dependency graphs from benchmarks/compile/gen_graph.py, compiles them with this build's compiler
# and flags, and checks that every program shares objects exactly as its graph's lifetimes say,
# through an identity checksum and a count of the objects built per graph. The seeds are fixed
# (DEFAULT_SEEDS in check_sharing.py, split over the shards); TRELLIS_PROPERTY_SEEDS replaces them
# when the tests run, which is how CI tries seeds that local runs never use:
#   TRELLIS_PROPERTY_SEEDS=1000000+64 ctest --preset clang-18-debug -L property
# property.oracle_selftest checks that the oracle catches wirings with deliberate sharing bugs.
set(trellis_property_shards 4)
foreach(shard RANGE 1 ${trellis_property_shards})
  set(name property.sharing.shard${shard}of${trellis_property_shards})
  add_test(NAME ${name}
    COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_CURRENT_SOURCE_DIR}/check_sharing.py"
            --config "${TRELLIS_COMPILER_CONFIG}" --shard ${shard}/${trellis_property_shards})
  set_tests_properties(${name} PROPERTIES LABELS property)
endforeach()
add_test(NAME property.oracle_selftest
  COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_CURRENT_SOURCE_DIR}/check_sharing.py"
          --config "${TRELLIS_COMPILER_CONFIG}" --self-test)
set_tests_properties(property.oracle_selftest PROPERTIES LABELS property)
