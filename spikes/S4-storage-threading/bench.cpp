// Spike S4 microbenchmarks (Google Benchmark 1.8.3). Every benchmark reports wall time; the
// `batch` counter is the number of slot accesses one iteration of one thread performs, so the
// summary divides by it to get nanoseconds per access.
//
//   warm/<storage>/<design>            1 thread, slot already constructed: the fast path
//   warm8_same/<storage>/<design>      8 threads reading the same constructed slot
//   warm8_distinct/<storage>/<design>  8 threads, thread t reads slot t (adjacent slots)
//   share/shared/<design>              a shared_ptr<T> request: copy and drop a shared_ptr
//   share8_same/shared/<design>        the same from 8 threads on one slot (refcount contention)
//   cold/<storage>/<design>            1 thread, first access to 1,024 fresh slots per batch
//   cold8_same/inline/<design>         8 threads race through the same 4,096 fresh slots
//   cold8_same_spin1us/inline/<design> the same with a 1 us constructor, so losers must block
//   cold8_distinct/inline/<design>     8 threads, each constructs its own interleaved slots
// Designs: eager, none, once (A), cv (B), wait (C), wait_spin (C with a bounded spin before a
// waiter announces itself); *_nodetect drop the R004 bookkeeping.
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>

#include <benchmark/benchmark.h>

#include "slots.hpp"

namespace {

struct payload {
  std::uint64_t value;
  payload(std::uint64_t v, std::int64_t spin_ns) : value(v) {
    if (spin_ns > 0) {
      const auto until = std::chrono::steady_clock::now() + std::chrono::nanoseconds(spin_ns);
      while (std::chrono::steady_clock::now() < until) {
      }
    }
  }
};

template <class S>
using eager_d = s4::eager_slot<S>;
template <class S>
using none_d = s4::plain_slot<S>;
template <class S>
using once_d = s4::once_slot<S>;
template <class S>
using cv_d = s4::cv_slot<S, true>;
template <class S>
using cv_nodetect_d = s4::cv_slot<S, false>;
template <class S>
using wait_d = s4::wait_slot<S, true>;
template <class S>
using wait_nodetect_d = s4::wait_slot<S, false>;
// Spin count for variant C+spin; run.sh uses the default, a sweep can override it.
#ifndef S4_SPIN
#define S4_SPIN 128
#endif
template <class S>
using wait_spin_d = s4::wait_slot<S, true, S4_SPIN>;

template <template <class> class Design, template <class> class Storage, std::size_t N>
struct harness {
  using slot_type = Design<Storage<payload>>;
  static constexpr std::size_t size = N;
  static constexpr bool is_eager = std::is_same_v<slot_type, s4::eager_slot<Storage<payload>>>;

  s4::injector_sync sync;
  std::array<slot_type, N> slots;
  s4::teardown_stack<N, true> td;
  std::int64_t spin_ns = 0;

  auto make() {
    return [this](auto emplace) { emplace(std::uint64_t{42}, spin_ns); };
  }
  payload& get(std::size_t i) { return slots[i].get(td, sync, make()); }
  auto share(std::size_t i) { return s4::share(slots[i], td, sync, make()); }
  void prime_all() {
    for (std::size_t i = 0; i < N; ++i) {
      if constexpr (is_eager)
        slots[i].prime(td, make());
      else
        get(i);
    }
  }
  // Destroys the objects (teardown), then rebuilds every slot in its initial state.
  void reset() {
    td.unwind();
    for (auto& s : slots) {
      std::destroy_at(&s);
      std::construct_at(&s);
    }
  }
};

// One shared instance per harness type, for the multi-threaded benchmarks. Thread 0 prepares it
// before the timed loop; Google Benchmark holds the other threads at a barrier until then.
template <class H>
H& instance() {
  static const auto h = std::make_unique<H>();
  return *h;
}

void set_batch(benchmark::State& st, double batch) {
  st.counters["batch"] = benchmark::Counter(batch, benchmark::Counter::kAvgThreads);
}

template <class H, bool Distinct>
void bm_warm(benchmark::State& st) {
  H& h = instance<H>();
  if (st.thread_index() == 0) {
    h.reset();
    h.prime_all();
  }
  const std::size_t i = Distinct ? static_cast<std::size_t>(st.thread_index()) : 0;
  for (auto _ : st) benchmark::DoNotOptimize(&h.get(i));
  set_batch(st, 1);
}

template <class H>
void bm_share(benchmark::State& st) {
  H& h = instance<H>();
  if (st.thread_index() == 0) {
    h.reset();
    h.prime_all();
  }
  for (auto _ : st) {
    auto p = h.share(0);
    benchmark::DoNotOptimize(p.get());
  }
  set_batch(st, 1);
}

// Manual timing around each batch, so resetting the slots is not measured.
template <class H>
void bm_cold(benchmark::State& st) {
  const auto h = std::make_unique<H>();
  for (auto _ : st) {
    h->reset();
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < H::size; ++i) benchmark::DoNotOptimize(&h->get(i));
    const auto t1 = std::chrono::steady_clock::now();
    st.SetIterationTime(std::chrono::duration<double>(t1 - t0).count());
  }
  set_batch(st, static_cast<double>(H::size));
}

// Fixed iteration count: iteration i of thread t touches slot i (same) or i * threads + t
// (distinct), so every access is the first access to that slot from that thread.
template <class H, bool Distinct>
void bm_cold_mt(benchmark::State& st, std::int64_t spin_ns) {
  H& h = instance<H>();
  if (st.thread_index() == 0) {
    h.reset();
    h.spin_ns = spin_ns;
  }
  const auto t = static_cast<std::size_t>(st.thread_index());
  const auto n = static_cast<std::size_t>(st.threads());
  std::size_t i = 0;
  for (auto _ : st) {
    const std::size_t index = (Distinct ? i * n + t : i) % H::size;
    benchmark::DoNotOptimize(&h.get(index));
    ++i;
  }
  set_batch(st, 1);
}

constexpr int threads = 8;
constexpr std::size_t warm_slots = 1024;
constexpr std::size_t cold_slots = 1024;
constexpr std::int64_t mt_iterations = 4096;
constexpr std::int64_t mt_slow_iterations = 512;

template <template <class> class D, template <class> class S>
void register_warm(const std::string& storage, const std::string& design) {
  using H = harness<D, S, warm_slots>;
  benchmark::RegisterBenchmark("warm/" + storage + "/" + design, bm_warm<H, false>);
}

template <template <class> class D, template <class> class S>
void register_warm_mt(const std::string& storage, const std::string& design) {
  using H = harness<D, S, warm_slots>;
  benchmark::RegisterBenchmark("warm8_same/" + storage + "/" + design, bm_warm<H, false>)
      ->Threads(threads)
      ->UseRealTime();
  benchmark::RegisterBenchmark("warm8_distinct/" + storage + "/" + design, bm_warm<H, true>)
      ->Threads(threads)
      ->UseRealTime();
}

template <template <class> class D>
void register_share(const std::string& design) {
  using H = harness<D, s4::shared_storage, warm_slots>;
  benchmark::RegisterBenchmark("share/shared/" + design, bm_share<H>);
  benchmark::RegisterBenchmark("share8_same/shared/" + design, bm_share<H>)
      ->Threads(threads)
      ->UseRealTime();
}

template <template <class> class D, template <class> class S>
void register_cold(const std::string& storage, const std::string& design) {
  using H = harness<D, S, cold_slots>;
  benchmark::RegisterBenchmark("cold/" + storage + "/" + design, bm_cold<H>)->UseManualTime();
}

template <template <class> class D>
void register_cold_mt(const std::string& design) {
  using same = harness<D, s4::inline_storage, static_cast<std::size_t>(mt_iterations)>;
  using distinct =
      harness<D, s4::inline_storage, static_cast<std::size_t>(mt_iterations) * threads>;
  benchmark::RegisterBenchmark("cold8_same/inline/" + design,
                               [](benchmark::State& st) { bm_cold_mt<same, false>(st, 0); })
      ->Threads(threads)
      ->Iterations(mt_iterations)
      ->UseRealTime();
  benchmark::RegisterBenchmark("cold8_same_spin1us/inline/" + design,
                               [](benchmark::State& st) { bm_cold_mt<same, false>(st, 1000); })
      ->Threads(threads)
      ->Iterations(mt_slow_iterations)
      ->UseRealTime();
  benchmark::RegisterBenchmark("cold8_distinct/inline/" + design,
                               [](benchmark::State& st) { bm_cold_mt<distinct, true>(st, 0); })
      ->Threads(threads)
      ->Iterations(mt_iterations)
      ->UseRealTime();
}

void register_all() {
  register_warm<eager_d, s4::inline_storage>("inline", "eager");
  register_warm<none_d, s4::inline_storage>("inline", "none");
  register_warm<once_d, s4::inline_storage>("inline", "once");
  register_warm<cv_d, s4::inline_storage>("inline", "cv");
  register_warm<wait_d, s4::inline_storage>("inline", "wait");
  register_warm<eager_d, s4::shared_storage>("shared", "eager");
  register_warm<none_d, s4::shared_storage>("shared", "none");
  register_warm<once_d, s4::shared_storage>("shared", "once");
  register_warm<cv_d, s4::shared_storage>("shared", "cv");
  register_warm<wait_d, s4::shared_storage>("shared", "wait");

  register_warm_mt<eager_d, s4::inline_storage>("inline", "eager");
  register_warm_mt<once_d, s4::inline_storage>("inline", "once");
  register_warm_mt<cv_d, s4::inline_storage>("inline", "cv");
  register_warm_mt<wait_d, s4::inline_storage>("inline", "wait");
  register_warm_mt<wait_d, s4::shared_storage>("shared", "wait");

  register_share<once_d>("once");
  register_share<wait_d>("wait");

  register_cold<none_d, s4::inline_storage>("inline", "none");
  register_cold<once_d, s4::inline_storage>("inline", "once");
  register_cold<cv_d, s4::inline_storage>("inline", "cv");
  register_cold<cv_nodetect_d, s4::inline_storage>("inline", "cv_nodetect");
  register_cold<wait_d, s4::inline_storage>("inline", "wait");
  register_cold<wait_nodetect_d, s4::inline_storage>("inline", "wait_nodetect");
  register_cold<wait_spin_d, s4::inline_storage>("inline", "wait_spin");
  register_cold<none_d, s4::shared_storage>("shared", "none");
  register_cold<once_d, s4::shared_storage>("shared", "once");
  register_cold<cv_d, s4::shared_storage>("shared", "cv");
  register_cold<wait_d, s4::shared_storage>("shared", "wait");

  register_cold_mt<once_d>("once");
  register_cold_mt<cv_d>("cv");
  register_cold_mt<wait_d>("wait");
  register_cold_mt<wait_spin_d>("wait_spin");
}

}  // namespace

int main(int argc, char** argv) {
  register_all();
  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv)) return 1;
  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
  return 0;
}
