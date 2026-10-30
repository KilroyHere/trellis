// Spike S1: which conversion operators on a placeholder argument deduce each
// constructor-parameter form exactly?
//
// For every operator set below and every parameter form P, this program checks
// whether `consumer<P>` (a class whose only constructor takes one P) is
// constructible from the placeholder, and if it is, constructs it at run time
// with a tracing placeholder. The operator that overload resolution selected
// records its kind and the request type it deduced (T, T& or const T&, with T
// deduced), then throws, so no value is ever needed. The request is compared
// with the declared parameter type.
//
// Output: one line per (set, form):
//   ROW|set|form|status|operator|request|verdict|set index|form index
// The recommended set ("min") also has expectations; any deviation makes the
// program exit 1, so run.sh fails if a compiler changes behaviour.
//
// Build with -DS1_DIAG_SET=<set index> -DS1_DIAG_FORM=<form index> to compile
// one cell unconditionally, which makes the compiler print why a cell fails.
#include <any>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace forms {

// ---- test vocabulary ------------------------------------------------------
struct X {
  int v = 0;
};
struct I {  // an abstract interface
  virtual ~I() = default;
  virtual int f() const = 0;
};
// Stand-in for a library-owned wrapper such as di::lazy<T&>: no converting
// constructor template, only a private constructor the library befriends.
template <class T>
class lazy {
  lazy() = default;
};

// describe<T>(): a portable, readable name for the request types used here.
template <class T>
struct name_of;
template <class T>
std::string describe() {
  if constexpr (std::is_lvalue_reference_v<T>)
    return describe<std::remove_reference_t<T>>() + "&";
  else if constexpr (std::is_rvalue_reference_v<T>)
    return describe<std::remove_reference_t<T>>() + "&&";
  else if constexpr (std::is_const_v<T>) {
    if constexpr (std::is_pointer_v<T>)
      return describe<std::remove_const_t<T>>() + " const";
    else
      return "const " + describe<std::remove_const_t<T>>();
  } else if constexpr (std::is_pointer_v<T>)
    return describe<std::remove_pointer_t<T>>() + "*";
  else
    return name_of<T>::get();
}
template <>
struct name_of<X> {
  static std::string get() { return "X"; }
};
template <>
struct name_of<I> {
  static std::string get() { return "I"; }
};
template <>
struct name_of<int> {
  static std::string get() { return "int"; }
};
template <>
struct name_of<char> {
  static std::string get() { return "char"; }
};
template <>
struct name_of<std::string> {
  static std::string get() { return "string"; }
};
template <>
struct name_of<std::string_view> {
  static std::string get() { return "string_view"; }
};
template <>
struct name_of<std::any> {
  static std::string get() { return "any"; }
};
template <class T>
struct name_of<std::unique_ptr<T>> {
  static std::string get() { return "unique_ptr<" + describe<T>() + ">"; }
};
template <class T>
struct name_of<std::shared_ptr<T>> {
  static std::string get() { return "shared_ptr<" + describe<T>() + ">"; }
};
template <class T>
struct name_of<std::weak_ptr<T>> {
  static std::string get() { return "weak_ptr<" + describe<T>() + ">"; }
};
template <class T>
struct name_of<std::optional<T>> {
  static std::string get() { return "optional<" + describe<T>() + ">"; }
};
template <class T>
struct name_of<std::span<T>> {
  static std::string get() { return "span<" + describe<T>() + ">"; }
};
template <class T>
struct name_of<std::vector<T>> {
  static std::string get() { return "vector<" + describe<T>() + ">"; }
};
template <class T>
struct name_of<std::reference_wrapper<T>> {
  static std::string get() { return "reference_wrapper<" + describe<T>() + ">"; }
};
template <class T>
struct name_of<lazy<T>> {
  static std::string get() { return "lazy<" + describe<T>() + ">"; }
};
template <class... Ts>
struct name_of<std::tuple<Ts...>> {
  static std::string get() {
    std::string s = "tuple<";
    ((s += describe<Ts>() + ","), ...);
    s.back() = '>';
    return s;
  }
};
template <class... Ts>
struct name_of<std::variant<Ts...>> {
  static std::string get() {
    std::string s = "variant<";
    ((s += describe<Ts>() + ","), ...);
    s.back() = '>';
    return s;
  }
};
template <class R, class... As>
struct name_of<std::function<R(As...)>> {
  static std::string get() {
    std::string s = "function<" + describe<R>() + "(";
    ((s += describe<As>() + ","), ...);
    if (sizeof...(As) > 0) s.pop_back();
    return s + ")>";
  }
};

// ---- tracing ---------------------------------------------------------------
enum class op { value, lref, clref, rref };
inline const char* op_name(op o) {
  switch (o) {
    case op::value: return "operator T()";
    case op::lref: return "operator T&()";
    case op::clref: return "operator const T&()";
    case op::rref: return "operator T&&()";
  }
  return "?";
}
struct hit {
  op kind;
  std::string request;
  bool exact;
};

// The policy the operators call. hit<R> is instantiated only for the operator
// that overload resolution selected, and only in evaluated code.
template <class Expected>
struct trace {
  template <class R>
  [[noreturn]] static R on(op kind) {
    throw hit{kind, describe<R>(), std::is_same_v<R, Expected>};
  }
};

// The consumer's own type is never a conversion target (excludes copy/move ctors).
template <class Consumer, class T>
concept not_self = !std::same_as<std::remove_cvref_t<T>, Consumer>;

// ---- operator sets under test ----------------------------------------------
// plan: the sketch in plan section 7.2 (both operators const).
template <class C, class P>
struct set_plan {
  template <class T>
    requires not_self<C, T>
  operator T() const {
    return P::template on<T>(op::value);
  }
  template <class T>
    requires not_self<C, T>
  operator T&() const {
    return P::template on<T&>(op::lref);
  }
};
// boost: the shape of Boost.DI's any_type_ref_fwd (value non-const; three const
// reference operators).
template <class C, class P>
struct set_boost {
  template <class T>
    requires not_self<C, T>
  operator T() {
    return P::template on<T>(op::value);
  }
  template <class T>
    requires not_self<C, T>
  operator T&() const {
    return P::template on<T&>(op::lref);
  }
  template <class T>
    requires not_self<C, T>
  operator T&&() const {
    return P::template on<T&&>(op::rref);
  }
  template <class T>
    requires not_self<C, T>
  operator const T&() const {
    return P::template on<const T&>(op::clref);
  }
};
// min: the recommended pair. Value operator non-const, reference operator const.
template <class C, class P>
struct set_min {
  template <class T>
    requires not_self<C, T>
  operator T() {
    return P::template on<T>(op::value);
  }
  template <class T>
    requires not_self<C, T>
  operator T&() const {
    return P::template on<T&>(op::lref);
  }
};
// min3: min plus an explicit const T& operator.
template <class C, class P>
struct set_min3 {
  template <class T>
    requires not_self<C, T>
  operator T() {
    return P::template on<T>(op::value);
  }
  template <class T>
    requires not_self<C, T>
  operator T&() const {
    return P::template on<T&>(op::lref);
  }
  template <class T>
    requires not_self<C, T>
  operator const T&() const {
    return P::template on<const T&>(op::clref);
  }
};
// min_rr: min plus a const T&& operator (candidate workaround for gcc-13, which
// otherwise picks operator T&() for an rvalue-reference parameter and fails).
template <class C, class P>
struct set_min_rr {
  template <class T>
    requires not_self<C, T>
  operator T() {
    return P::template on<T>(op::value);
  }
  template <class T>
    requires not_self<C, T>
  operator T&() const {
    return P::template on<T&>(op::lref);
  }
  template <class T>
    requires not_self<C, T>
  operator T&&() const {
    return P::template on<T&&>(op::rref);
  }
};
// refq: the same ranking expressed with ref-qualifiers instead of const.
template <class C, class P>
struct set_refq {
  template <class T>
    requires not_self<C, T>
  operator T() && {
    return P::template on<T>(op::value);
  }
  template <class T>
    requires not_self<C, T>
  operator T&() const& {
    return P::template on<T&>(op::lref);
  }
};
// rref_first: make T&& the preferred operator, to show T and T&& stay
// indistinguishable from the other side.
template <class C, class P>
struct set_rref_first {
  template <class T>
    requires not_self<C, T>
  operator T() const {
    return P::template on<T>(op::value);
  }
  template <class T>
    requires not_self<C, T>
  operator T&() const {
    return P::template on<T&>(op::lref);
  }
  template <class T>
    requires not_self<C, T>
  operator T&&() {
    return P::template on<T&&>(op::rref);
  }
};
// value: a value operator only.
template <class C, class P>
struct set_value {
  template <class T>
    requires not_self<C, T>
  operator T() {
    return P::template on<T>(op::value);
  }
};

template <template <class, class> class S>
struct set_tag {
  template <class C, class P>
  using type = S<C, P>;
};
struct set_entry_plan : set_tag<set_plan> {
  static constexpr const char* name = "plan";
};
struct set_entry_boost : set_tag<set_boost> {
  static constexpr const char* name = "boost";
};
struct set_entry_min : set_tag<set_min> {
  static constexpr const char* name = "min";
};
struct set_entry_min3 : set_tag<set_min3> {
  static constexpr const char* name = "min3";
};
struct set_entry_min_rr : set_tag<set_min_rr> {
  static constexpr const char* name = "min_rr";
};
struct set_entry_refq : set_tag<set_refq> {
  static constexpr const char* name = "refq";
};
struct set_entry_rref_first : set_tag<set_rref_first> {
  static constexpr const char* name = "rref_first";
};
struct set_entry_value : set_tag<set_value> {
  static constexpr const char* name = "value";
};
using sets = std::tuple<set_entry_min, set_entry_min3, set_entry_min_rr, set_entry_refq, set_entry_boost,
                        set_entry_plan, set_entry_rref_first, set_entry_value>;

// ---- parameter forms -------------------------------------------------------
// expect: what the recommended set (min) must report. 'exact' means the request
// equals the declared parameter type (top-level const dropped for by-value
// parameters, as in the function type); 'as' names the documented alternative;
// 'fails' means not constructible from the placeholder.
enum class expect { exact, as_value, fails };
template <class P>
struct consumer {
  consumer(P) {}
};
template <class P, expect E = expect::exact, bool Gcc13Fails = false>
struct form {
  using param = P;
  static constexpr expect expected = E;
  static constexpr bool gcc13_fails = Gcc13Fails;
};
using forms_list = std::tuple<
    form<X>,                                       // 0  T
    form<const X>,                                 // 1  const T (by value)
    form<X&>,                                      // 2  T&
    form<const X&>,                                // 3  const T&
    form<X&&, expect::as_value, true>,                   // 4  T&&
    form<const X&&, expect::as_value, true>,             // 5  const T&&
    form<X*>,                                      // 6  T*
    form<const X*>,                                // 7  const T*
    form<X* const>,                                // 8  T* const
    form<std::unique_ptr<X>>,                      // 9  unique_ptr<T>
    form<std::unique_ptr<X>&&, expect::as_value, true>,  // 10 unique_ptr<T>&&
    form<std::shared_ptr<X>>,                      // 11 shared_ptr<T>
    form<const std::shared_ptr<X>&>,               // 12 const shared_ptr<T>&
    form<std::shared_ptr<const X>>,                // 13 shared_ptr<const T>
    form<std::weak_ptr<X>>,                        // 14 weak_ptr<T>
    form<std::function<int()>>,                    // 15 function<int()>
    form<std::function<std::unique_ptr<I>(int)>>,  // 16 function<unique_ptr<I>(int)> (factory)
    form<std::span<X>>,                            // 17 span<T>
    form<std::span<const X>>,                      // 18 span<const T>
    form<std::optional<X>, expect::fails>,         // 19 optional<T>
    form<int>,                                     // 20 int
    form<const int&>,                              // 21 const int&
    form<std::string>,                             // 22 string
    form<std::string_view>,                        // 23 string_view
    form<std::vector<X>>,                          // 24 vector<T>
    form<I&>,                                      // 25 I& (abstract)
    form<const I&>,                                // 26 const I&
    form<I*>,                                      // 27 I*
    form<std::unique_ptr<I>>,                      // 28 unique_ptr<I>
    form<std::shared_ptr<I>>,                      // 29 shared_ptr<I>
    form<std::reference_wrapper<X>, expect::exact, true>,               // 30 reference_wrapper<T>
    form<lazy<X&>>,                                // 31 library wrapper lazy<T&>
    form<std::tuple<X>, expect::fails>,            // 32 tuple<T>
    form<std::any, expect::fails>,                 // 33 any
    form<std::variant<X, int>>                     // 34 variant<T, int>
    >;

template <class P>
using expected_request = std::conditional_t<std::is_reference_v<P>, P, std::remove_cv_t<P>>;

int failures = 0;

// gcc-13 (fixed in gcc-14) picks operator T&() for an rvalue-reference
// parameter, or for reference_wrapper's constraint, and fails.
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 14
constexpr bool is_gcc13 = true;
#else
constexpr bool is_gcc13 = false;
#endif

template <class Set, class Form, std::size_t SetIndex, std::size_t FormIndex>
void run_cell() {
  using P = typename Form::param;
  using C = consumer<P>;
  using Arg = typename Set::template type<C, trace<expected_request<P>>>;
  const std::string form_label = describe<P>();
  std::string status, oper = "-", request = "-", verdict;
  bool ok_for_min = true;
  if constexpr (std::is_constructible_v<C, Arg>) {
    status = "resolves";
    try {
      C c(Arg{});
      (void)c;
      verdict = "constructed without a conversion?";
    } catch (const hit& h) {
      oper = op_name(h.kind);
      request = h.request;
      verdict = h.exact ? "exact" : "differs";
      if (Form::expected == expect::exact) ok_for_min = h.exact;
      if (Form::expected == expect::as_value)
        ok_for_min = !h.exact && h.kind == op::value;
      if (Form::expected == expect::fails) ok_for_min = false;
      if (is_gcc13 && Form::gcc13_fails) ok_for_min = false;
    }
  } else {
    status = "not-constructible";
    verdict = "see diag";
    ok_for_min = Form::expected == expect::fails || (is_gcc13 && Form::gcc13_fails);
  }
  if (std::strcmp(Set::name, "min") == 0 && !ok_for_min) {
    ++failures;
    verdict += " UNEXPECTED";
  }
  std::printf("ROW|%s|%s|%s|%s|%s|%s|%zu|%zu\n", Set::name, form_label.c_str(), status.c_str(),
              oper.c_str(), request.c_str(), verdict.c_str(), SetIndex, FormIndex);
}

template <class Set, std::size_t SetIndex, std::size_t... FormIndex>
void run_set(std::index_sequence<FormIndex...>) {
  (run_cell<Set, std::tuple_element_t<FormIndex, forms_list>, SetIndex, FormIndex>(), ...);
}
template <std::size_t... SetIndex>
void run_all(std::index_sequence<SetIndex...>) {
  (run_set<std::tuple_element_t<SetIndex, sets>, SetIndex>(
       std::make_index_sequence<std::tuple_size_v<forms_list>>{}),
   ...);
}

}  // namespace forms

#if defined(S1_DIAG_SET) && defined(S1_DIAG_FORM)
// Diagnostic build: compile one cell unconditionally.
using diag_set = std::tuple_element_t<S1_DIAG_SET, forms::sets>;
using diag_param = std::tuple_element_t<S1_DIAG_FORM, forms::forms_list>::param;
using diag_consumer = forms::consumer<diag_param>;
using diag_arg = diag_set::type<diag_consumer, forms::trace<diag_param>>;
void diag() { diag_consumer c(diag_arg{}); }
int main() {}
#else
int main() {
  forms::run_all(std::make_index_sequence<std::tuple_size_v<forms::sets>>{});
  std::printf("SETS|%zu\nFORMS|%zu\n", std::tuple_size_v<forms::sets>, std::tuple_size_v<forms::forms_list>);
  if (forms::failures != 0) {
    std::printf("FAIL: %d cell(s) of the recommended set deviate from the expectations\n", forms::failures);
    return 1;
  }
  std::printf("OK: recommended set matches the expectations for every form\n");
  return 0;
}
#endif
