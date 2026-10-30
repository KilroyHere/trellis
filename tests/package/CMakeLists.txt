# A consumer of an installed Trellis, found with find_package in config mode. Not part of the main
# build. The T0.1 verify command runs it:
#   cmake --install build/clang-18-debug --prefix build/install-test
#   cmake -S tests/package -B build/package-test -G Ninja -DCMAKE_CXX_COMPILER=clang++-18 \
#         -DCMAKE_PREFIX_PATH=$PWD/build/install-test
#   cmake --build build/package-test
cmake_minimum_required(VERSION 3.21)
project(trellis_package_test LANGUAGES CXX)

find_package(trellis CONFIG REQUIRED)
message(STATUS "Using trellis ${trellis_VERSION} from ${trellis_DIR}")

add_executable(package_test main.cpp)
target_link_libraries(package_test PRIVATE trellis::trellis)
# main.cpp checks that the installed headers match the installed package version.
target_compile_definitions(package_test PRIVATE
  PACKAGE_VERSION_MAJOR=${trellis_VERSION_MAJOR}
  PACKAGE_VERSION_MINOR=${trellis_VERSION_MINOR}
  PACKAGE_VERSION_PATCH=${trellis_VERSION_PATCH})
