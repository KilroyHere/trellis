// Trellis: header-only C++20 dependency injection. The umbrella header: includes the public API.
// Copyright (c) 2026 Aryan Patel
// SPDX-License-Identifier: BSL-1.0
#ifndef TRELLIS_TRELLIS_HPP
#define TRELLIS_TRELLIS_HPP

#include <trellis/config.hpp>                 // IWYU pragma: export
#include <trellis/diagnostics/errors.hpp>     // IWYU pragma: export
#include <trellis/discovery/ctor_traits.hpp>  // IWYU pragma: export
#include <trellis/dsl/bind.hpp>               // IWYU pragma: export
#include <trellis/dsl/lifetimes.hpp>          // IWYU pragma: export
#include <trellis/dsl/module.hpp>             // IWYU pragma: export
#include <trellis/dsl/options.hpp>            // IWYU pragma: export
#include <trellis/meta/fixed_string.hpp>      // IWYU pragma: export
#include <trellis/meta/result.hpp>            // IWYU pragma: export
#include <trellis/meta/type_name.hpp>         // IWYU pragma: export
#include <trellis/static/injector.hpp>        // IWYU pragma: export

#endif  // TRELLIS_TRELLIS_HPP
