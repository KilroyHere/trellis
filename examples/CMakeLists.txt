# The examples (label examples), built with the project's warning set as errors and run as tests,
# so that what README.md and docs/getting-started.md show compiles and works (plan §9.3).
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include("${PROJECT_SOURCE_DIR}/cmake/TrellisWarnings.cmake")
trellis_enable_project_warnings()

# hello_world.cpp: plan §6.1's hello world, which README.md shows. getting_started.cpp: the
# examples of docs/getting-started.md; its test fails if a request doesn't return what the guide
# says.
foreach(example IN ITEMS hello_world getting_started)
  add_executable(trellis_example_${example} ${example}.cpp)
  target_link_libraries(trellis_example_${example} PRIVATE trellis::trellis)
  add_test(NAME example.${example} COMMAND trellis_example_${example})
  set_tests_properties(example.${example} PROPERTIES LABELS examples)
endforeach()
