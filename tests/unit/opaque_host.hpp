// A host's handle type, only declared here, as an application would see it: tests/unit/
// create_test.cpp binds an object of it with .to_ptr() and .to_ref() and requests it by pointer
// and reference (IncompleteTypesBoundByPointerOrReference; T1.6 review). opaque_host.cpp defines
// it.
#ifndef TRELLIS_TESTS_UNIT_OPAQUE_HOST_HPP
#define TRELLIS_TESTS_UNIT_OPAQUE_HOST_HPP

namespace opaque_host {

struct handle;

// The host's one handle, whose id is 42.
handle& instance();
int id(const handle& object);

}  // namespace opaque_host

#endif  // TRELLIS_TESTS_UNIT_OPAQUE_HOST_HPP
