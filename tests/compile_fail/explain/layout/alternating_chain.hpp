// A chain of 20 classes with deduced constructors whose requests alternate between by value and
// by reference: c0 takes c1 by value, c1 takes c2&, c2 takes c3 by value, and so on, and the last,
// c19, takes sensor&, which nothing owns (TRL-E012). The classes taken by reference are
// singletons (alternating_module); the others are unbound, so each is a new object. The classes
// of the alternating-chain layout tests in explain/, which include this header directly or through
// others, as an application's headers would.
//
// The explain pass builds a class requested by value from one conversion operator and a class
// requested by reference from another, on two lines of explain.hpp, so the notes of this chain
// alternate between the two lines, which neither GCC nor Clang folds into one: the shape that
// reaches the compilers' backtrace limit (docs/internals/explain-pass.md, "What still grows").
#pragma once

#include <trellis/trellis.hpp>

namespace app {

struct sensor {};
struct c19 {
  explicit c19(sensor& input) : input_(&input) {}
  sensor* input_;
};
struct c18 {
  explicit c18(c19 next) : next_(next) {}
  c19 next_;
};
struct c17 {
  explicit c17(c18& next) : next_(&next) {}
  c18* next_;
};
struct c16 {
  explicit c16(c17 next) : next_(next) {}
  c17 next_;
};
struct c15 {
  explicit c15(c16& next) : next_(&next) {}
  c16* next_;
};
struct c14 {
  explicit c14(c15 next) : next_(next) {}
  c15 next_;
};
struct c13 {
  explicit c13(c14& next) : next_(&next) {}
  c14* next_;
};
struct c12 {
  explicit c12(c13 next) : next_(next) {}
  c13 next_;
};
struct c11 {
  explicit c11(c12& next) : next_(&next) {}
  c12* next_;
};
struct c10 {
  explicit c10(c11 next) : next_(next) {}
  c11 next_;
};
struct c9 {
  explicit c9(c10& next) : next_(&next) {}
  c10* next_;
};
struct c8 {
  explicit c8(c9 next) : next_(next) {}
  c9 next_;
};
struct c7 {
  explicit c7(c8& next) : next_(&next) {}
  c8* next_;
};
struct c6 {
  explicit c6(c7 next) : next_(next) {}
  c7 next_;
};
struct c5 {
  explicit c5(c6& next) : next_(&next) {}
  c6* next_;
};
struct c4 {
  explicit c4(c5 next) : next_(next) {}
  c5 next_;
};
struct c3 {
  explicit c3(c4& next) : next_(&next) {}
  c4* next_;
};
struct c2 {
  explicit c2(c3 next) : next_(next) {}
  c3 next_;
};
struct c1 {
  explicit c1(c2& next) : next_(&next) {}
  c2* next_;
};
struct c0 {
  explicit c0(c1 next) : next_(next) {}
  c1 next_;
};

// The classes taken by reference, as singletons, which serve references (plan §6.3).
inline auto alternating_module() {
  return trellis::make_module(
      trellis::bind<c2>.in(trellis::singleton), trellis::bind<c4>.in(trellis::singleton),
      trellis::bind<c6>.in(trellis::singleton), trellis::bind<c8>.in(trellis::singleton),
      trellis::bind<c10>.in(trellis::singleton), trellis::bind<c12>.in(trellis::singleton),
      trellis::bind<c14>.in(trellis::singleton), trellis::bind<c16>.in(trellis::singleton),
      trellis::bind<c18>.in(trellis::singleton));
}

}  // namespace app
