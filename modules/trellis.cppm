// Trellis: the C++20 module `trellis` (plan §10, ADR-0008), `import trellis;` in place of
// `#include <trellis/trellis.hpp>`.
// Copyright (c) 2026 Aryan Patel
// SPDX-License-Identifier: BSL-1.0
//
// The headers stay the library: the global module fragment includes the umbrella header, so the
// declarations keep their names, mangling and attachment to the global module, and a program may
// import the module in some translation units and include the headers in others. The module
// exports every public name of the umbrella header, each with a using-declaration in the namespace
// that declares it. Nothing in trellis::detail is exported; templates still reach it when they are
// instantiated. The testing helpers, which the umbrella header leaves out because tests include
// <trellis/testing/wiring.hpp> themselves, are the module trellis.testing (trellis.testing.cppm).
//
// The export list must name every public entity of the headers this module includes:
// tools/module_exports.py compares it with the names Clang finds in them, in the test
// module.exports (`ctest -L module`), and fails on a missing or an extra name. Each
// using-declaration sits in the namespace that declares the entity, trellis::inline v1 rather than
// trellis: GCC 14 doesn't make a using-declaration in trellis visible to importers when the entity
// lives in the inline namespace v1.
//
// Macros don't cross a module boundary, and neither does the standard library. An importer that
// needs TRELLIS_VERSION or a TRELLIS_HAS_* macro includes <trellis/config.hpp>, which defines
// macros only, and it includes the standard headers it uses, all before the import. See
// docs/internals/delivery.md for what an importer gets, and the build requirements (CMake 3.28,
// Ninja, Clang 16 or later; GCC 14 isn't sufficient).
module;

#include <trellis/trellis.hpp>

export module trellis;

// The public API: everything in trellis::v1 outside trellis::detail.
export namespace trellis::inline v1 {

// dsl/bind.hpp
using trellis::v1::bind;
using trellis::v1::binding;

// dsl/lifetimes.hpp
using trellis::v1::external_t;
using trellis::v1::shared_external_t;
using trellis::v1::singleton;
using trellis::v1::singleton_inline;
using trellis::v1::singleton_inline_t;
using trellis::v1::singleton_t;
using trellis::v1::unique;
using trellis::v1::unique_t;
using trellis::v1::value_t;

// dsl/module.hpp
using trellis::v1::make_module;
using trellis::v1::module;

// dsl/options.hpp
using trellis::v1::options;
using trellis::v1::threading;

// dsl/override.hpp
using trellis::v1::override;
using trellis::v1::override_binding;

// discovery/ctor_traits.hpp
using trellis::v1::ctor;
using trellis::v1::ctor_traits;

// meta/fixed_string.hpp
using trellis::v1::fixed_string;
using trellis::v1::operator==;
using trellis::v1::operator+;

// meta/result.hpp
using trellis::v1::failure;
using trellis::v1::make_unexpected;
using trellis::v1::result;

// meta/type_name.hpp
using trellis::v1::type_name;

// static/injector.hpp
using trellis::v1::injector;
using trellis::v1::make_injector;

}  // namespace trellis::inline v1

// The compile-error vocabulary (diagnostics/errors.hpp), outside v1 (ADR-0006). Users rarely name
// these types, but they are public: a diagnostic names them, and a test may assert on them.
export namespace trellis::error {

using trellis::error::E001;
using trellis::error::E002;
using trellis::error::E003;
using trellis::error::E004;
using trellis::error::E005;
using trellis::error::E009;
using trellis::error::E010;
using trellis::error::E012;
using trellis::error::E013;
using trellis::error::E014;
using trellis::error::path;
using trellis::error::request;

}  // namespace trellis::error
