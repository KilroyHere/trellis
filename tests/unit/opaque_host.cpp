// The definition of opaque_host::handle, which tests/unit/create_test.cpp only declares (see
// opaque_host.hpp).
#include "opaque_host.hpp"

namespace opaque_host {

struct handle {
  int id = 42;
};

handle& instance() {
  static handle object;
  return object;
}

int id(const handle& object) { return object.id; }

}  // namespace opaque_host
