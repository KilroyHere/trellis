// Tiny benchmark harness shared by the per-framework benchmark builds
// (compiled when -DDI_BENCH is defined). Prints one JSON object per case.
#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace bench {

// Forces `value` to be materialised in memory and treated as read and
// modified by opaque code (same idea as benchmark::DoNotOptimize).
template <class T>
inline void do_not_optimize(T& value) {
  asm volatile("" : "+m"(value) : : "memory");
}

struct Options {
  std::size_t iters = 200000;
  int repeats = 7;
};

inline Options parse(int argc, char** argv) {
  Options o;
  if (argc > 1) o.iters = static_cast<std::size_t>(std::strtoull(argv[1], nullptr, 10));
  if (argc > 2) o.repeats = std::atoi(argv[2]);
  return o;
}

// Runs `op` iters times per repeat (after a warm-up) and prints ns/op stats.
template <class Op>
void run(const char* framework, const char* name, const Options& o, Op&& op) {
  for (std::size_t i = 0; i < o.iters / 10 + 1; ++i) op();
  std::vector<double> ns;
  for (int r = 0; r < o.repeats; ++r) {
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < o.iters; ++i) op();
    const auto t1 = std::chrono::steady_clock::now();
    ns.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count() /
                 static_cast<double>(o.iters));
  }
  std::sort(ns.begin(), ns.end());
  std::printf(
      "{\"framework\":\"%s\",\"case\":\"%s\",\"iters\":%zu,\"repeats\":%d,"
      "\"median_ns\":%.2f,\"min_ns\":%.2f,\"max_ns\":%.2f}\n",
      framework, name, o.iters, o.repeats, ns[ns.size() / 2], ns.front(), ns.back());
  std::fflush(stdout);
}

}  // namespace bench
