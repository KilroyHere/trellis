// Tests for include/trellis/dsl/request_forms.hpp: how a request splits into a form and a key,
// and the request-form table of plan §6.3, cell by cell.
//
// Every cell of the table has a static_assert below, in the table's layout: one block per row
// (request form), one line per column (lifetime). The rules on top of the table (.allow_copy()
// and constness) follow. tests/unit/injector_test.cpp checks the same cells again end to end,
// through real bindings and request_check (static/validate.hpp).
#include <trellis/dsl/lifetimes.hpp>
#include <trellis/dsl/request_forms.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>

#include <gtest/gtest.h>

namespace {

using trellis::detail::adjusted_verdict;
using trellis::detail::allows;
using trellis::detail::form_verdict;
using trellis::detail::lifetime_kind;
using trellis::detail::request_form;
using trellis::detail::request_form_count;
using trellis::detail::request_form_table;
using trellis::detail::request_traits;
using trellis::detail::table_verdict;

struct widget {};
struct abstract {
  abstract() = default;
  abstract(const abstract&) = delete;
  abstract& operator=(const abstract&) = delete;
  virtual ~abstract() = default;
  virtual void run() = 0;
};

// ---- request_traits: form, key and access -----------------------------------------------------

template <class R, request_form Form, class Key, bool MutableAccess>
constexpr bool classified_as() {
  return request_traits<R>::form == Form && std::is_same_v<typename request_traits<R>::key, Key> &&
         request_traits<R>::mutable_access == MutableAccess;
}

// By value: T, and T with top-level cv qualifiers, as in a function type.
static_assert(classified_as<widget, request_form::value, widget, false>());
static_assert(classified_as<const widget, request_form::value, widget, false>());
static_assert(classified_as<volatile widget, request_form::value, widget, false>());
static_assert(classified_as<const volatile widget, request_form::value, widget, false>());
static_assert(classified_as<int, request_form::value, int, false>());
// X&& and const X&& are the request X (amended by ADR-0003).
static_assert(classified_as<widget&&, request_form::value, widget, false>());
static_assert(classified_as<const widget&&, request_form::value, widget, false>());
static_assert(classified_as<std::unique_ptr<widget>&&, request_form::unique_ptr, widget, true>());
static_assert(classified_as<std::shared_ptr<widget>&&, request_form::shared_ptr, widget, true>());
// References: the key without cv qualifiers; const decides the access.
static_assert(classified_as<widget&, request_form::reference, widget, true>());
static_assert(classified_as<const widget&, request_form::reference, widget, false>());
static_assert(classified_as<abstract&, request_form::reference, abstract, true>());
static_assert(classified_as<const abstract&, request_form::reference, abstract, false>());
// Raw pointers, including a pointer that is itself const (a by-value T* const parameter).
static_assert(classified_as<widget*, request_form::pointer, widget, true>());
static_assert(classified_as<const widget*, request_form::pointer, widget, false>());
static_assert(classified_as<widget* const, request_form::pointer, widget, true>());
static_assert(classified_as<abstract*, request_form::pointer, abstract, true>());
// Smart pointers with the default deleter.
static_assert(classified_as<std::unique_ptr<widget>, request_form::unique_ptr, widget, true>());
static_assert(
    classified_as<std::unique_ptr<const widget>, request_form::unique_ptr, widget, false>());
static_assert(
    classified_as<const std::unique_ptr<widget>, request_form::unique_ptr, widget, true>());
static_assert(classified_as<std::shared_ptr<widget>, request_form::shared_ptr, widget, true>());
static_assert(
    classified_as<std::shared_ptr<const widget>, request_form::shared_ptr, widget, false>());
static_assert(classified_as<std::weak_ptr<widget>, request_form::weak_ptr, widget, true>());
static_assert(classified_as<std::weak_ptr<const widget>, request_form::weak_ptr, widget, false>());
static_assert(classified_as<std::shared_ptr<abstract>, request_form::shared_ptr, abstract, true>());
// A reference to a smart pointer is a reference to a smart-pointer object, whose key is the smart
// pointer type itself (ADR-0004's open question about const shared_ptr<T>&; T1.4 keeps the
// table's rejection: unbound, that is TRL-E012).
static_assert(classified_as<const std::shared_ptr<widget>&, request_form::reference,
                            std::shared_ptr<widget>, false>());
// Pointers to non-objects and smart pointers that aren't the table's are values: they can only
// be bound as values themselves.
using callback = void (*)(int);
static_assert(classified_as<callback, request_form::value, callback, false>());
static_assert(classified_as<void*, request_form::value, void*, false>());
static_assert(classified_as<const void*, request_form::value, const void*, false>());
struct custom_deleter {
  void operator()(widget* object) const noexcept { delete object; }
};
static_assert(classified_as<std::unique_ptr<widget, custom_deleter>, request_form::value,
                            std::unique_ptr<widget, custom_deleter>, false>());
// NOLINTNEXTLINE(modernize-avoid-c-arrays): the array form of unique_ptr, on purpose
using widget_array = widget[];
static_assert(classified_as<std::unique_ptr<widget_array>, request_form::value,
                            std::unique_ptr<widget_array>, false>());
static_assert(
    classified_as<std::function<int()>, request_form::value, std::function<int()>, false>());

// ---- the table, cell by cell (plan §6.3) ------------------------------------------------------

constexpr bool cell(request_form form, lifetime_kind lifetime, form_verdict expected) {
  return table_verdict(form, lifetime) == expected;
}

// T
static_assert(cell(request_form::value, lifetime_kind::unique, form_verdict::allowed));
static_assert(cell(request_form::value, lifetime_kind::singleton,
                   form_verdict::copies_hide_sharing));
static_assert(cell(request_form::value, lifetime_kind::singleton_inline,
                   form_verdict::copies_hide_sharing));
static_assert(cell(request_form::value, lifetime_kind::scoped, form_verdict::copies_hide_sharing));
static_assert(cell(request_form::value, lifetime_kind::external,
                   form_verdict::copies_hide_sharing));
static_assert(cell(request_form::value, lifetime_kind::shared_external,
                   form_verdict::copies_hide_sharing));
static_assert(cell(request_form::value, lifetime_kind::value, form_verdict::allowed));
static_assert(cell(request_form::value, lifetime_kind::unbound, form_verdict::allowed));

// T&, const T&
static_assert(cell(request_form::reference, lifetime_kind::unique,
                   form_verdict::nothing_keeps_it_alive));
static_assert(cell(request_form::reference, lifetime_kind::singleton, form_verdict::allowed));
static_assert(cell(request_form::reference, lifetime_kind::singleton_inline,
                   form_verdict::allowed));
static_assert(cell(request_form::reference, lifetime_kind::scoped, form_verdict::allowed_captive));
static_assert(cell(request_form::reference, lifetime_kind::external, form_verdict::allowed));
static_assert(cell(request_form::reference, lifetime_kind::shared_external, form_verdict::allowed));
static_assert(cell(request_form::reference, lifetime_kind::value, form_verdict::allowed));
static_assert(cell(request_form::reference, lifetime_kind::unbound, form_verdict::unbound_shared));

// T*, const T*
static_assert(cell(request_form::pointer, lifetime_kind::unique,
                   form_verdict::ambiguous_ownership));
static_assert(cell(request_form::pointer, lifetime_kind::singleton, form_verdict::allowed));
static_assert(cell(request_form::pointer, lifetime_kind::singleton_inline, form_verdict::allowed));
static_assert(cell(request_form::pointer, lifetime_kind::scoped, form_verdict::allowed_captive));
static_assert(cell(request_form::pointer, lifetime_kind::external, form_verdict::allowed));
static_assert(cell(request_form::pointer, lifetime_kind::shared_external, form_verdict::allowed));
static_assert(cell(request_form::pointer, lifetime_kind::value, form_verdict::allowed));
static_assert(cell(request_form::pointer, lifetime_kind::unbound, form_verdict::unbound_shared));

// std::unique_ptr<T>
static_assert(cell(request_form::unique_ptr, lifetime_kind::unique, form_verdict::allowed));
static_assert(cell(request_form::unique_ptr, lifetime_kind::singleton,
                   form_verdict::duplicates_ownership));
static_assert(cell(request_form::unique_ptr, lifetime_kind::singleton_inline,
                   form_verdict::duplicates_ownership));
static_assert(cell(request_form::unique_ptr, lifetime_kind::scoped,
                   form_verdict::duplicates_ownership));
static_assert(cell(request_form::unique_ptr, lifetime_kind::external,
                   form_verdict::duplicates_ownership));
static_assert(cell(request_form::unique_ptr, lifetime_kind::shared_external,
                   form_verdict::duplicates_ownership));
static_assert(cell(request_form::unique_ptr, lifetime_kind::value,
                   form_verdict::duplicates_ownership));
static_assert(cell(request_form::unique_ptr, lifetime_kind::unbound, form_verdict::allowed));

// std::shared_ptr<T>
static_assert(cell(request_form::shared_ptr, lifetime_kind::unique, form_verdict::allowed));
static_assert(cell(request_form::shared_ptr, lifetime_kind::singleton, form_verdict::allowed));
static_assert(cell(request_form::shared_ptr, lifetime_kind::singleton_inline,
                   form_verdict::not_heap_owned));
static_assert(cell(request_form::shared_ptr, lifetime_kind::scoped, form_verdict::allowed_captive));
static_assert(cell(request_form::shared_ptr, lifetime_kind::external,
                   form_verdict::caller_owns_it));
static_assert(cell(request_form::shared_ptr, lifetime_kind::shared_external,
                   form_verdict::allowed));
static_assert(cell(request_form::shared_ptr, lifetime_kind::value, form_verdict::not_heap_owned));
static_assert(cell(request_form::shared_ptr, lifetime_kind::unbound, form_verdict::unbound_shared));

// std::weak_ptr<T>
static_assert(cell(request_form::weak_ptr, lifetime_kind::unique,
                   form_verdict::nothing_keeps_it_alive));
static_assert(cell(request_form::weak_ptr, lifetime_kind::singleton, form_verdict::allowed));
static_assert(cell(request_form::weak_ptr, lifetime_kind::singleton_inline,
                   form_verdict::not_heap_owned));
static_assert(cell(request_form::weak_ptr, lifetime_kind::scoped, form_verdict::allowed));
static_assert(cell(request_form::weak_ptr, lifetime_kind::external, form_verdict::caller_owns_it));
static_assert(cell(request_form::weak_ptr, lifetime_kind::shared_external, form_verdict::allowed));
static_assert(cell(request_form::weak_ptr, lifetime_kind::value, form_verdict::not_heap_owned));
static_assert(cell(request_form::weak_ptr, lifetime_kind::unbound, form_verdict::unbound_shared));

// lazy<...>, provider<...> (Phase 2): allowed in every column.
static_assert(cell(request_form::deferred, lifetime_kind::unique, form_verdict::allowed));
static_assert(cell(request_form::deferred, lifetime_kind::singleton, form_verdict::allowed));
static_assert(cell(request_form::deferred, lifetime_kind::singleton_inline, form_verdict::allowed));
static_assert(cell(request_form::deferred, lifetime_kind::scoped, form_verdict::allowed));
static_assert(cell(request_form::deferred, lifetime_kind::external, form_verdict::allowed));
static_assert(cell(request_form::deferred, lifetime_kind::shared_external, form_verdict::allowed));
static_assert(cell(request_form::deferred, lifetime_kind::value, form_verdict::allowed));
static_assert(cell(request_form::deferred, lifetime_kind::unbound, form_verdict::allowed));

// The table holds exactly these cells: 7 forms by 8 columns.
static_assert(request_form_count == 7 && request_form_table.size() == 7);
static_assert(trellis::detail::lifetime_kind_count == 8 && request_form_table[0].size() == 8);

// The lifetime types map to their columns.
static_assert(trellis::detail::lifetime_traits<trellis::unique_t>::kind == lifetime_kind::unique);
static_assert(trellis::detail::lifetime_traits<trellis::singleton_t>::kind ==
              lifetime_kind::singleton);
static_assert(trellis::detail::lifetime_traits<trellis::singleton_inline_t>::kind ==
              lifetime_kind::singleton_inline);
static_assert(trellis::detail::lifetime_traits<trellis::external_t>::kind ==
              lifetime_kind::external);
static_assert(trellis::detail::lifetime_traits<trellis::shared_external_t>::kind ==
              lifetime_kind::shared_external);
static_assert(trellis::detail::lifetime_traits<trellis::value_t>::kind == lifetime_kind::value);

// ---- the rules on top of the table ------------------------------------------------------------

// allows: only the two allowed verdicts let a request through.
static_assert(allows(form_verdict::allowed) && allows(form_verdict::allowed_captive));
static_assert(!allows(form_verdict::copies_hide_sharing) &&
              !allows(form_verdict::nothing_keeps_it_alive) &&
              !allows(form_verdict::ambiguous_ownership) &&
              !allows(form_verdict::duplicates_ownership) &&
              !allows(form_verdict::not_heap_owned) && !allows(form_verdict::caller_owns_it) &&
              !allows(form_verdict::const_object) && !allows(form_verdict::unbound_shared) &&
              !allows(form_verdict::no_binding) && !allows(form_verdict::keys_repeat));

// The smart pointer forms, which a reference to an unbound smart pointer names (its E012 message
// says to take the smart pointer by value).
static_assert(trellis::detail::is_smart_pointer_form(request_form::unique_ptr) &&
              trellis::detail::is_smart_pointer_form(request_form::shared_ptr) &&
              trellis::detail::is_smart_pointer_form(request_form::weak_ptr));
static_assert(!trellis::detail::is_smart_pointer_form(request_form::value) &&
              !trellis::detail::is_smart_pointer_form(request_form::reference) &&
              !trellis::detail::is_smart_pointer_form(request_form::pointer) &&
              !trellis::detail::is_smart_pointer_form(request_form::deferred));

// .allow_copy() turns exactly the "copies hide sharing" cells into allowed ones.
static_assert(adjusted_verdict(form_verdict::copies_hide_sharing, true, false, false) ==
              form_verdict::allowed);
static_assert(adjusted_verdict(form_verdict::copies_hide_sharing, false, false, false) ==
              form_verdict::copies_hide_sharing);
static_assert(adjusted_verdict(form_verdict::duplicates_ownership, true, false, false) ==
              form_verdict::duplicates_ownership);
static_assert(adjusted_verdict(form_verdict::nothing_keeps_it_alive, true, false, true) ==
              form_verdict::nothing_keeps_it_alive);
// A copy of a const object is fine, so constness doesn't affect an allowed copy.
static_assert(adjusted_verdict(form_verdict::copies_hide_sharing, true, true, false) ==
              form_verdict::allowed);

// Constness carries through: a const object refuses mutable access, in allowed cells only.
static_assert(adjusted_verdict(form_verdict::allowed, false, true, true) ==
              form_verdict::const_object);
static_assert(adjusted_verdict(form_verdict::allowed_captive, false, true, true) ==
              form_verdict::const_object);
static_assert(adjusted_verdict(form_verdict::allowed, false, true, false) == form_verdict::allowed);
static_assert(adjusted_verdict(form_verdict::allowed, false, false, true) == form_verdict::allowed);
static_assert(adjusted_verdict(form_verdict::caller_owns_it, false, true, true) ==
              form_verdict::caller_owns_it);

// The same checks at run time, so that a regression also shows up as a failing test.
TEST(RequestForms, TableMatchesThePlan) {
  EXPECT_EQ(table_verdict(request_form::reference, lifetime_kind::unique),
            form_verdict::nothing_keeps_it_alive);
  EXPECT_EQ(table_verdict(request_form::value, lifetime_kind::singleton),
            form_verdict::copies_hide_sharing);
  EXPECT_EQ(table_verdict(request_form::shared_ptr, lifetime_kind::unique), form_verdict::allowed);
  EXPECT_EQ(table_verdict(request_form::shared_ptr, lifetime_kind::unbound),
            form_verdict::unbound_shared);
  // Every column allows something, and every row is allowed somewhere.
  for (std::size_t form = 0; form != request_form_count; ++form) {
    bool row_allows = false;
    for (const form_verdict verdict : request_form_table[form]) {
      row_allows = row_allows || allows(verdict);
    }
    EXPECT_TRUE(row_allows) << "row " << form;
  }
}

TEST(RequestForms, RulesOnTopOfTheTable) {
  EXPECT_EQ(adjusted_verdict(form_verdict::copies_hide_sharing, true, false, false),
            form_verdict::allowed);
  EXPECT_EQ(adjusted_verdict(form_verdict::allowed, false, true, true), form_verdict::const_object);
}

}  // namespace
