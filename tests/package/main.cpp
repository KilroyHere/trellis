// Builds against an installed Trellis: the headers, the version and C++20 must all come through
// the trellis::trellis target of the installed package.
#include <trellis/trellis.hpp>

static_assert(TRELLIS_VERSION_MAJOR == PACKAGE_VERSION_MAJOR &&
                  TRELLIS_VERSION_MINOR == PACKAGE_VERSION_MINOR &&
                  TRELLIS_VERSION_PATCH == PACKAGE_VERSION_PATCH,
              "the installed headers and trellisConfigVersion.cmake disagree");

// trellis::trellis requires C++20 (cxx_std_20), so a concept compiles here even though this
// project asks for no language standard and the compiler may default to an older one.
template <class T>
concept any_type = sizeof(T) > 0;
static_assert(any_type<int>);

int main() { return 0; }
