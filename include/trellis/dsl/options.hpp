// Trellis binding vocabulary: per-injector options (plan §6.13).
// Copyright (c) 2026 Aryan Patel
// SPDX-License-Identifier: BSL-1.0
//
// options is a structural class type, so it can be a template argument written with designated
// initializers: make_injector<options{.strict = true}>(...). Options are per injector rather than
// global macros, which would be fragile under the one-definition rule (plan §6.13).
//
// T1.4 defines the fields. Each takes effect with the task that implements it, as its comment
// says: strict, eager, threading and max_ctor_arity with create<T>() (T1.5); cycle_check is
// accepted and has no effect until Phase 2.
#ifndef TRELLIS_DSL_OPTIONS_HPP
#define TRELLIS_DSL_OPTIONS_HPP

#include <cstddef>

namespace trellis::inline v1 {

/// @brief How an injector's lazily built singletons behave when several threads use the
///        injector (plan §7.6, ADR-0012).
///
/// @par Constraints
/// None.
///
/// @par Errors
/// None.
///
/// @par Example
/// @code
/// auto injector = trellis::make_injector<trellis::options{.threading = trellis::threading::none}>(
///     app_module());
/// @endcode
enum class threading : unsigned char {
  /// @brief The default: a singleton is built once even when threads ask for it at the same time.
  ///
  /// @par Constraints
  /// None.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// static_assert(trellis::options{}.threading == trellis::threading::safe);
  /// @endcode
  safe,
  /// @brief Single-threaded use only: plain state flags, no atomics.
  ///
  /// @par Constraints
  /// - Only one thread uses the injector at a time; otherwise the behaviour is undefined.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// constexpr trellis::options single_threaded{.threading = trellis::threading::none};
  /// @endcode
  none,
};

/// @brief An injector's options: a template argument of `trellis::make_injector`, written with
///        designated initializers.
///
/// Every field has a default, so `options{}` is the default configuration, and
/// `options{.eager = true}` changes one field. Designated initializers must follow the order of
/// the fields below.
///
/// @par Constraints
/// None.
///
/// @par Errors
/// None.
///
/// @par Example
/// @code
/// auto injector = trellis::make_injector<trellis::options{
///     .strict = true,
///     .eager = true,
///     .threading = trellis::threading::none,
///     .max_ctor_arity = 12,
/// }>(app_module(config));
/// @endcode
struct options {
  /// @brief Turns off the implicit unique binding of unbound concrete types (ADR-0004): every
  ///        type the injector creates needs a binding. Default: false.
  ///
  /// Takes effect with `create<T>()` (T1.5): a request, or a constructor parameter, whose type
  /// has no binding is then a compile error, even for a concrete class requested by value or as
  /// `std::unique_ptr`, which the implicit binding would otherwise serve.
  ///
  /// @par Constraints
  /// None.
  ///
  /// @par Errors
  /// - `TRL-E013`: with `strict` set, `create<T>()` meets an unbound type requested by value or
  ///   as `std::unique_ptr` (provisional ID). Other requests of unbound types are `TRL-E001` or
  ///   `TRL-E012`, as without `strict`.
  ///
  /// @par Example
  /// @code
  /// auto injector = trellis::make_injector<trellis::options{.strict = true}>(app_module());
  /// @endcode
  bool strict = false;

  /// @brief Constructs every singleton inside `make_injector`, in dependency order, instead of on
  ///        first use (plan §7.5, FR-13). Default: false.
  ///
  /// Takes effect with `create<T>()` (T1.5). `make_injector` then gives the strong guarantee: if
  /// a constructor throws, the singletons already built are destroyed in reverse order and the
  /// exception propagates. Afterwards, requests read the singletons without testing whether they
  /// are built (plan §7.6). The docs recommend it for applications: it fails fast at startup
  /// (ADR-0012).
  ///
  /// @par Constraints
  /// - Every singleton can be built, even one that nothing requests; otherwise compilation fails
  ///   in `make_injector` with the errors `create<T>()` lists.
  ///
  /// @par Errors
  /// None of its own: `make_injector` reports those of `create<T>()` for each singleton.
  ///
  /// @par Example
  /// @code
  /// auto injector = trellis::make_injector<trellis::options{.eager = true}>(app_module());
  /// @endcode
  bool eager = false;

  /// @brief How lazily built singletons behave across threads. Default: `threading::safe`.
  ///
  /// Both take effect with `create<T>()` (T1.5): `threading::safe` builds each singleton once
  /// even under concurrent requests, with an atomic state per singleton (ADR-0012), and
  /// `threading::none` uses a plain flag.
  ///
  /// @par Constraints
  /// None.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// constexpr trellis::options single_threaded{.threading = trellis::threading::none};
  /// auto injector = trellis::make_injector<single_threaded>(app_module());
  /// @endcode
  ::trellis::threading threading = ::trellis::threading::safe;

  /// @brief The largest constructor arity that deduced discovery probes for (plan §7.2,
  ///        ADR-0003). Default: 16.
  ///
  /// Takes effect with `create<T>()` (T1.5), which passes it to deduced discovery (T1.3) as its
  /// bound. Discovery probes every deduced class from the bound down, so a higher bound costs
  /// compile time and memory: for a graph of 100 classes, 32 costs 4% (GCC) to 11% (Clang) more
  /// instructions than 16 (T1.11, docs/benchmarks/v0.1.md).
  ///
  /// @par Constraints
  /// - At most 32; otherwise `trellis::make_injector` fails to compile with a static_assert. A
  ///   constructor with more parameters can be declared explicitly, which has no limit.
  ///
  /// @par Errors
  /// - `TRL-E010`: `create<T>()` meets a deduced class whose constructors all take more
  ///   parameters than the bound.
  ///
  /// @par Example
  /// @code
  /// auto injector = trellis::make_injector<trellis::options{.max_ctor_arity = 12}>(app_module());
  /// @endcode
  std::size_t max_ctor_arity = 16;

  /// @brief Checks deduced-mode graphs for dependency cycles with a depth-bounded pass, which
  ///        costs 1.2 to 2.4 times the compile time (ADR-0014). Default: false.
  ///
  /// For debugging and CI builds. Takes effect in Phase 2 (cycles, plan §12.2); until then it is
  /// accepted and ignored.
  ///
  /// @par Constraints
  /// None.
  ///
  /// @par Errors
  /// None.
  ///
  /// @par Example
  /// @code
  /// auto injector = trellis::make_injector<trellis::options{.cycle_check = true}>(app_module());
  /// @endcode
  bool cycle_check = false;
};

}  // namespace trellis::inline v1

#endif  // TRELLIS_DSL_OPTIONS_HPP
