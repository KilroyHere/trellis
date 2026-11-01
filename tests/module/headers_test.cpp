// The headers, in a program that also imports the C++20 module trellis: this translation unit
// includes <trellis/trellis.hpp> and defines functions that import_test.cpp declares with the
// module's names. The module includes the same headers in its global module fragment, so their
// entities belong to no module, and both ways of declaring them must give the same entities:
// otherwise the program doesn't link (the names would be mangled differently) or the tests there
// fail. That is what lets a code base import the module in some files and include the headers in
// others.
#include <trellis/trellis.hpp>

#include <string_view>

#include "app.hpp"

namespace mod {

std::string_view service_name_from_headers() { return trellis::type_name<service>(); }

trellis::binding<clock_source, system_clock, trellis::singleton_t> clock_binding_from_headers() {
  return trellis::bind<clock_source>.to<system_clock>().in(trellis::singleton);
}

trellis::failure failure_from_headers() { return trellis::failure{"from the headers"}; }

}  // namespace mod
