// Trellis static engine: the creation code of an aggregate built from an explicit constructor
// declaration (plan §7.2, ADR-0003 item 1).
// Copyright (c) 2026 Aryan Patel
// SPDX-License-Identifier: BSL-1.0
//
// Internal (trellis::detail): part of static/resolve.hpp's creation code, which includes this
// header at its end. A declaration may list fewer members than its aggregate has, as braces allow,
// and GCC and Clang warn about each member it leaves out without a default member initializer
// (-Wmissing-field-initializers) where the aggregate is built, which would be an error under a
// user's -Werror (NFR-10). So that one warning is off around the code below
// (TRELLIS_PARTIAL_AGGREGATE_INIT_BEGIN, config.hpp). The code is in a header of its own for the
// one build where that isn't enough, the C++20 module's on Clang before 20, which makes the header
// a system header (TRELLIS_MODULE_SYSTEM_HEADER, config.hpp, which says why and what it costs), so
// that nothing else becomes one. tests/unit/declared_aggregate_test.cpp builds such declarations
// under the project warnings and -Werror, and tests/warnings/ checks that the other warnings of
// this code still reach users.
#ifndef TRELLIS_STATIC_DECLARED_AGGREGATE_HPP
#define TRELLIS_STATIC_DECLARED_AGGREGATE_HPP

#include <trellis/config.hpp>
#include <trellis/static/resolve.hpp>
#include <trellis/static/storage.hpp>

#include <cstddef>
#include <memory>
#include <new>  // IWYU pragma: keep

// After the includes, so that in the one build where it does anything only this file's own code
// becomes a system header.
TRELLIS_MODULE_SYSTEM_HEADER

namespace trellis::inline v1::detail {

TRELLIS_PARTIAL_AGGREGATE_INIT_BEGIN

// build<declared_node<Impl, Ctx, true, Args...>, Access>: a declared aggregate, built as
// static/resolve.hpp's build<Node, Access> describes, with braces (ADR-0003 item 5) from the
// requests its declaration lists, each resolved in the class's own context. The members the
// declaration leaves out keep their default member initializers or are value-initialized, as
// braces leave them.
template <class Impl, class Ctx, slot_access Access, class... Args>
struct build<declared_node<Impl, Ctx, true, Args...>, Access> {
  using own = with_access_t<Ctx, Access>;
  static Impl value([[maybe_unused]] own& ctx) { return Impl{ctx.template resolve<Args>()...}; }
  static Impl* fresh([[maybe_unused]] own& ctx) {
    return new Impl{ctx.template resolve<Args>()...};
  }
  template <class Element>
  static std::shared_ptr<Element> share([[maybe_unused]] own& ctx) {
    auto emplace = [&](std::byte* where) { ::new (where) Impl{ctx.template resolve<Args>()...}; };
    return detail::make_shared_object<Impl, Element>(emplace);
  }
  template <class Cell>
  static teardown_entry make_in(typename Cell::state_type& state, void* injector) {
    [[maybe_unused]] own ctx{*static_cast<typename Ctx::injector_type*>(injector)};
    auto emplace = [&](std::byte* where) { ::new (where) Impl{ctx.template resolve<Args>()...}; };
    return Cell::make_with(state, emplace);
  }
};

TRELLIS_PARTIAL_AGGREGATE_INIT_END

}  // namespace trellis::inline v1::detail

#endif  // TRELLIS_STATIC_DECLARED_AGGREGATE_HPP
