// Runtime microbenchmarks of the research scenario (plan §8.4, NFR-01 and NFR-02), with Google
// Benchmark. Ported from the -DDI_BENCH mode of research/experiments/manual/main.cpp:
//
//   graph/<variant>    create the wiring (the injector), resolve App, destroy everything
//   resolve/<variant>  resolve App from an existing wiring, whose shared objects already exist
//
// A variant is a wiring class of the shape benchmarks/scenario/manual.hpp describes. Each one must
// wire the scenario correctly (demo::check_wiring) before it is measured. The hand-written wiring
// is the baseline; T1.9 adds Trellis in main(). scripts/bench-runtime.sh builds this program with
// the release presets and runs it pinned to one CPU.
#include <cstdio>
#include <string>

#include <benchmark/benchmark.h>

#include "domain.hpp"
#include "manual.hpp"

namespace {

// The research harness's benchmark configuration: a repository capacity no run reaches.
demo::Config bench_config() { return demo::Config{"demo", 1000}; }

template <class Wiring>
void graph(benchmark::State& state) {
  const demo::Config config = bench_config();
  for (auto _ : state) {
    Wiring wiring(config);
    demo::App app = wiring.make_app();
    benchmark::DoNotOptimize(app);
  }
}

template <class Wiring>
void resolve(benchmark::State& state) {
  const demo::Config config = bench_config();
  const Wiring wiring(config);
  for (auto _ : state) {
    demo::App app = wiring.make_app();
    benchmark::DoNotOptimize(app);
  }
}

// Registers graph/<name> and resolve/<name> if the variant wires the scenario correctly.
template <class Wiring>
bool add_variant(const std::string& name) {
  const demo::Config config = bench_config();
  const Wiring wiring(config);
  const demo::App a = wiring.make_app();
  const demo::App b = wiring.make_app();
  const int errors = demo::check_wiring(a, b, config);
  if (errors != 0) {
    std::fprintf(stderr, "%s: wiring_errors=%d; not benchmarked\n", name.c_str(), errors);
    return false;
  }
  benchmark::RegisterBenchmark("graph/" + name, graph<Wiring>);
  benchmark::RegisterBenchmark("resolve/" + name, resolve<Wiring>);
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv)) return 1;
  // The variants, the hand-written baseline first.
  if (!add_variant<demo::manual_wiring>("manual")) return 2;
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
  return 0;
}
