// Trellis: the C++20 module `trellis.testing` (plan §6.10, §10), `import trellis.testing;` in place
// of `#include <trellis/testing/wiring.hpp>`.
// Copyright (c) 2026 Aryan Patel
// SPDX-License-Identifier: BSL-1.0
//
// The testing helpers live in a header that the umbrella header leaves out, so that only tests,
// which include it themselves, see them. They get a module of their own for the same reason:
// `import trellis;` gives what <trellis/trellis.hpp> gives, and a test adds
// `import trellis.testing;`. Like the header, which includes the injector, this module gives the
// rest of the library too: it re-exports trellis. tools/module_exports.py checks that it exports
// the public names of its header that trellis doesn't (module.exports).
module;

#include <trellis/testing/wiring.hpp>

export module trellis.testing;

export import trellis;

// testing/wiring.hpp
export namespace trellis::inline v1::testing {

using trellis::v1::testing::create_all;
using trellis::v1::testing::wireable;

}  // namespace trellis::inline v1::testing
