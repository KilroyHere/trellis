// The research scenario program, wired by hand (research/experiments/manual/main.cpp): the baseline
// for the scenario's compile time (NFR-03) and stripped binary size (NFR-06). benchmarks/compile/
// run.py compiles it, as benchmarks/scenario/<variant>.cpp for each variant, and expects the line
// "manual: wiring_errors=0 handled=6 log_lines=8 repo_size=6".
#include "manual.hpp"

#include "domain.hpp"

int main() {
  const demo::Config config{"demo", 100};
  const demo::manual_wiring wiring(config);
  demo::App a = wiring.make_app();
  demo::App b = wiring.make_app();
  return demo::report("manual", a, b, config);
}
