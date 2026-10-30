# The project warning set (plan NFR-10), applied as errors to every target the project builds:
# tests now, benchmarks and examples later. Consumers of trellis::trellis never get these flags.
#
# TRELLIS_WARNING_FLAGS holds the warnings without the "as errors" switch, because the compile-fail
# and golden runners need them without it: misuse must be a hard error even without -Werror (NFR-07).
if(MSVC)
  # cl and clang-cl. Untested here: MSVC is not installed locally and runs only in CI.
  set(TRELLIS_WARNING_FLAGS /W4 /permissive-)
  set(TRELLIS_WARNINGS_AS_ERRORS /WX)
else()
  set(TRELLIS_WARNING_FLAGS
    -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
    -Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual)
  set(TRELLIS_WARNINGS_AS_ERRORS -Werror)
endif()

# Turns the warning set on, as errors, for every target defined after this call in the calling
# directory and its subdirectories. Add third-party code (such as a fetched GoogleTest) before the
# call, so that its own sources are not compiled with these flags.
macro(trellis_enable_project_warnings)
  add_compile_options(${TRELLIS_WARNING_FLAGS} ${TRELLIS_WARNINGS_AS_ERRORS})
endmacro()
