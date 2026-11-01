// alternating_chain.hpp's chain with aggregates mixed in: 20 classes whose requests alternate
// between by value and by reference, c0 taking c1 by value, c1 taking c2&, and so on, where
// c2-c3, c6-c7, c10-c11, c14-c15 and c18-c19 are aggregates, whose members deduced discovery
// finds. The last, c19, holds sensor&, which nothing owns (TRL-E012). The classes taken by
// reference are singletons (alternating_aggregates_module); the others are unbound.
//
// The explain pass builds a class from one of three constructions in explain.hpp: the conversion
// operator for a request by value, the one for a request by reference, and the aggregate one. This
// chain moves between all three, so no two consecutive notes come from one line and nothing
// folds: the largest output of the alternating chains (docs/internals/explain-pass.md, "What still
// grows").
#pragma once

#include <trellis/trellis.hpp>

namespace app {

struct sensor {};
struct c19 {
  sensor& input;
};
struct c18 {
  c19 next;
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
  c16& next;
};
struct c14 {
  c15 next;
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
  c12& next;
};
struct c10 {
  c11 next;
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
  c8& next;
};
struct c6 {
  c7 next;
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
  c4& next;
};
struct c2 {
  c3 next;
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
inline auto alternating_aggregates_module() {
  return trellis::make_module(
      trellis::bind<c2>.in(trellis::singleton), trellis::bind<c4>.in(trellis::singleton),
      trellis::bind<c6>.in(trellis::singleton), trellis::bind<c8>.in(trellis::singleton),
      trellis::bind<c10>.in(trellis::singleton), trellis::bind<c12>.in(trellis::singleton),
      trellis::bind<c14>.in(trellis::singleton), trellis::bind<c16>.in(trellis::singleton),
      trellis::bind<c18>.in(trellis::singleton));
}

}  // namespace app
