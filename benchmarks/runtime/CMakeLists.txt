# Runtime microbenchmarks (plan §8.4), built when TRELLIS_BUILD_BENCHMARKS is on.
# scripts/bench-runtime.sh configures each release preset with it in a build directory of its own,
# builds trellis_runtime_bench and runs it. Needs Google Benchmark 1.8 or later (Ubuntu:
# libbenchmark-dev): the script passes --benchmark_min_time=<iterations>x, which 1.8.0 introduced.
find_package(benchmark 1.8 CONFIG REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The research harness measured at -O2 -DNDEBUG, and CMake's Release flags are -O3 -DNDEBUG. This
# directory uses the research setting, so that the results compare with
# research/experiments/results/summary.md.
string(REPLACE "-O3" "-O2" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
  message(WARNING "The runtime benchmarks are built with CMAKE_BUILD_TYPE='${CMAKE_BUILD_TYPE}', "
                  "not Release, so their timings mean little")
endif()

include("${PROJECT_SOURCE_DIR}/cmake/TrellisWarnings.cmake")
trellis_enable_project_warnings()

add_executable(trellis_runtime_bench scenario_bench.cpp)
target_include_directories(trellis_runtime_bench PRIVATE "${PROJECT_SOURCE_DIR}/benchmarks/scenario")
target_link_libraries(trellis_runtime_bench PRIVATE trellis::trellis benchmark::benchmark)
