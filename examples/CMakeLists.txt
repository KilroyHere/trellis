# The examples (label examples), built with the project's warning set as errors and run as tests,
# so that what README.md shows compiles and works (plan §9.3).
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include("${PROJECT_SOURCE_DIR}/cmake/TrellisWarnings.cmake")
trellis_enable_project_warnings()

add_executable(trellis_example_hello_world hello_world.cpp)
target_link_libraries(trellis_example_hello_world PRIVATE trellis::trellis)
add_test(NAME example.hello_world COMMAND trellis_example_hello_world)
set_tests_properties(example.hello_world PROPERTIES LABELS examples)
