// The research scenario program, wired with Trellis (trellis.hpp): the variant measured against
// manual.cpp for the scenario's extra compile time (NFR-03) and stripped binary size (NFR-06).
// benchmarks/compile/run.py compiles it, as benchmarks/scenario/<variant>.cpp for each variant, and
// expects the line "trellis: wiring_errors=0 handled=6 log_lines=8 repo_size=6". It is manual.cpp
// with the other wiring class.
#include "trellis.hpp"

#include "domain.hpp"

int main() {
  const demo::Config config{"demo", 100};
  const demo::trellis_wiring wiring(config);
  demo::App a = wiring.make_app();
  demo::App b = wiring.make_app();
  return demo::report("trellis", a, b, config);
}
