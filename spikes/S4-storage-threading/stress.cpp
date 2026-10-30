// Spike S4 correctness and stress tests. Built normally and with ThreadSanitizer by run.sh.
//
//   stress [--rounds N]   run every test; exit status 1 if any check fails
//   stress --sizes        print the size table only
//   stress --hazards      demonstrate std::call_once's behaviour on retry after a throw and on
//                         re-entry, with a watchdog, and report what happened (never fails)
//   stress --first-access DESIGN COUNT
//                         first access to COUNT fresh slots of DESIGN (none, once, cv, wait)
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <latch>
#include <memory>
#include <mutex>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "slots.hpp"

#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define S4_TSAN 1
#endif
#endif
#if defined(__SANITIZE_THREAD__)
#define S4_TSAN 1
#endif
// TSan replaces pthread_once with an interceptor that never releases its guard when the callable
// throws, so with libstdc++ (whose call_once is pthread_once) any retry blocks forever. libc++
// implements call_once itself and is unaffected. `--hazards` demonstrates it.
#if defined(S4_TSAN) && !defined(_LIBCPP_VERSION)
#define S4_CALL_ONCE_THROW_HANGS 1
#endif

namespace {

int failures = 0;
int checks = 0;

#define CHECK(cond)                                                                     \
  do {                                                                                  \
    ++checks;                                                                           \
    if (!(cond)) {                                                                      \
      ++failures;                                                                       \
      std::printf("  check failed: %s (%s:%d) [%s]\n", #cond, __FILE__, __LINE__, ctx); \
    }                                                                                   \
  } while (false)

// ---------------------------------------------------------------------------------------------
// A dependency graph whose nodes record construction, destruction and ordering violations.
// ---------------------------------------------------------------------------------------------

struct graph {
  explicit graph(std::size_t n)
      : deps(n), throws_left(n), constructions(n), alive(n), sleep_ms(n), started(n) {}

  std::vector<std::vector<std::size_t>> deps;   // dependencies of each node (lower indices)
  std::vector<std::atomic<int>> throws_left;    // throw this many times before succeeding
  std::vector<std::atomic<int>> constructions;  // completed constructions
  std::vector<std::atomic<int>> alive;          // 1 between constructor and destructor
  std::vector<int> sleep_ms;                    // time spent inside the constructor
  std::vector<std::atomic<bool>> started;       // the constructor has begun
  std::function<void(std::size_t)> hook;        // runs inside construction, before emplace
  std::atomic<int> order_errors{0};             // a dependency was dead when it mustn't be

  std::mutex log_mutex;
  std::vector<int> constructed;
  std::vector<int> destroyed;
  void log(std::vector<int>& v, int id) {
    std::lock_guard lock(log_mutex);
    v.push_back(id);
  }
};

struct node {
  node(int id_, graph* g_, std::vector<int> deps_) : id(id_), g(g_), deps(std::move(deps_)) {
    for (int d : deps)
      if (g->alive[static_cast<std::size_t>(d)].load() != 1) ++g->order_errors;
    g->alive[static_cast<std::size_t>(id)].store(1);
    ++g->constructions[static_cast<std::size_t>(id)];
    g->log(g->constructed, id);
  }
  ~node() {
    // Reverse-order teardown means every dependency is still alive here.
    for (int d : deps)
      if (g->alive[static_cast<std::size_t>(d)].load() != 1) ++g->order_errors;
    g->alive[static_cast<std::size_t>(id)].store(0);
    g->log(g->destroyed, id);
  }
  node(const node&) = delete;
  node& operator=(const node&) = delete;

  int id;
  graph* g;
  std::vector<int> deps;
};

// An injector over N slots of one design. The teardown stack is declared after the slots, so it
// is destroyed (and unwinds) while the storage it points into still exists.
template <template <class> class Design, template <class> class Storage, std::size_t N>
struct test_injector {
  using slot_type = Design<Storage<node>>;
  static constexpr std::size_t size = N;

  explicit test_injector(graph& g_) : g(&g_) {}

  node& get(std::size_t i) {
    return slots[i].get(td, sync, [this, i](auto emplace) {
      std::vector<int> ids;
      for (std::size_t d : g->deps[i]) {
        get(d);  // nested construction of a different slot
        ids.push_back(static_cast<int>(d));
      }
      g->started[i].store(true);
      if (g->hook) g->hook(i);
      if (g->sleep_ms[i] > 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(g->sleep_ms[i]));
      if (g->throws_left[i].fetch_sub(1) > 0) throw std::runtime_error("constructor failed");
      emplace(static_cast<int>(i), g, std::move(ids));
    });
  }
  auto share(std::size_t i) {
    return s4::share(slots[i], td, sync, [this, i](auto emplace) {
      emplace(static_cast<int>(i), g, std::vector<int>{});
    });
  }

  graph* g;
  s4::injector_sync sync;
  std::array<slot_type, N> slots;
  s4::teardown_stack<N, true> td;
};

template <class S>
using once_d = s4::once_slot<S>;
template <class S>
using cv_d = s4::cv_slot<S, true>;
template <class S>
using wait_d = s4::wait_slot<S, true>;
template <class S>
using none_d = s4::plain_slot<S>;
template <class S>
using wait_spin_d = s4::wait_slot<S, true, 128>;

void report(const char* name, int failures_before) {
  std::printf("[%s] %s\n", failures == failures_before ? "PASS" : "FAIL", name);
  std::fflush(stdout);
}

// ---------------------------------------------------------------------------------------------
// Exactly-once under contention, nested construction across threads, and dependency-respecting
// teardown after concurrent construction.
// ---------------------------------------------------------------------------------------------

template <template <class> class D, template <class> class S>
void test_exactly_once(const char* ctx, int rounds) {
  const int before = failures;
  constexpr std::size_t n = 64;
  constexpr int threads = 8;
  for (int round = 0; round < rounds; ++round) {
    graph g(n);
    for (std::size_t i = 1; i < n; ++i) {
      g.deps[i].push_back(i / 2);
      if (i % 3 == 0) g.deps[i].push_back(i - 1);
    }
    {
      auto inj = std::make_unique<test_injector<D, S, n>>(g);
      std::vector<std::vector<const node*>> seen(threads, std::vector<const node*>(n));
      std::latch start(threads);
      std::vector<std::thread> pool;
      for (int t = 0; t < threads; ++t) {
        pool.emplace_back([&, t] {
          std::vector<std::size_t> order(n);
          for (std::size_t i = 0; i < n; ++i) order[i] = i;
          std::mt19937 rng(static_cast<unsigned>(round * threads + t));
          std::shuffle(order.begin(), order.end(), rng);
          start.arrive_and_wait();
          for (std::size_t i : order) seen[static_cast<std::size_t>(t)][i] = &inj->get(i);
        });
      }
      for (auto& th : pool) th.join();
      for (std::size_t i = 0; i < n; ++i) {
        CHECK(g.constructions[i].load() == 1);
        for (int t = 1; t < threads; ++t) CHECK(seen[static_cast<std::size_t>(t)][i] == seen[0][i]);
      }
      CHECK(inj->td.size() == n);
    }
    CHECK(g.order_errors.load() == 0);
    CHECK(g.destroyed.size() == n);
    for (std::size_t i = 0; i < n; ++i) CHECK(g.alive[i].load() == 0);
  }
  report(ctx, before);
}

// ---------------------------------------------------------------------------------------------
// Teardown order: reverse of construction, including nested construction.
// ---------------------------------------------------------------------------------------------

template <template <class> class D, template <class> class S>
void test_teardown_order(const char* ctx) {
  const int before = failures;
  graph g(4);  // 0 = A(C), 1 = B, 2 = C, 3 = D
  g.deps[0] = {2};
  {
    test_injector<D, S, 4> inj(g);
    inj.get(1);
    inj.get(3);
    inj.get(0);  // builds C inside A's construction
    CHECK((g.constructed == std::vector<int>{1, 3, 2, 0}));
  }
  CHECK((g.destroyed == std::vector<int>{0, 2, 3, 1}));
  CHECK(g.order_errors.load() == 0);
  report(ctx, before);
}

// A constructor throws partway through a graph: E(F, G), G throws once.
template <template <class> class D, template <class> class S>
void test_throw_partway(const char* ctx) {
  const int before = failures;
  graph g(4);  // 0 = E(F, G), 1 = F, 2 = G, 3 = H (built first)
  g.deps[0] = {1, 2};
  g.throws_left[2] = 1;
  {
    test_injector<D, S, 4> inj(g);
    inj.get(3);
    bool threw = false;
    try {
      inj.get(0);
    } catch (const std::runtime_error&) {
      threw = true;
    }
    CHECK(threw);
    CHECK(g.constructions[1].load() == 1);  // F stays built: the basic guarantee
    CHECK(g.constructions[2].load() == 0);
    CHECK(g.constructions[0].load() == 0);
    CHECK(inj.td.size() == 2);  // H and F; nothing for the failed attempts
    inj.get(0);                 // retry: G and then E are built now
    CHECK(g.constructions[2].load() == 1);
    CHECK(g.constructions[0].load() == 1);
    CHECK(g.constructions[1].load() == 1);  // F was not built twice
    CHECK(inj.td.size() == 4);
  }
  CHECK((g.constructed == std::vector<int>{3, 1, 2, 0}));
  CHECK((g.destroyed == std::vector<int>{0, 2, 1, 3}));
  CHECK(g.order_errors.load() == 0);
  report(ctx, before);
}

// Eager mode: build everything in dependency order; on a throw, tear down what was built, in
// reverse, and rethrow (the strong guarantee of make_injector).
template <template <class> class D, template <class> class S>
void test_eager_strong_guarantee(const char* ctx) {
  const int before = failures;
  constexpr std::size_t n = 6;
  graph g(n);
  for (std::size_t i = 1; i < n; ++i) g.deps[i] = {i - 1};
  g.throws_left[4] = 1;
  bool threw = false;
  {
    test_injector<D, S, n> inj(g);
    try {
      try {
        for (std::size_t i = 0; i < n; ++i) inj.get(i);
      } catch (...) {
        inj.td.unwind();
        throw;
      }
    } catch (const std::runtime_error&) {
      threw = true;
    }
    CHECK(inj.td.size() == 0);
  }
  CHECK(threw);
  CHECK((g.constructed == std::vector<int>{0, 1, 2, 3}));
  CHECK((g.destroyed == std::vector<int>{3, 2, 1, 0}));
  CHECK(g.order_errors.load() == 0);
  report(ctx, before);
}

// ---------------------------------------------------------------------------------------------
// Retry semantics under contention: the constructor throws for its first three attempts while
// eight threads request the slot. Each failed attempt reaches exactly one caller, waiters retry,
// and exactly one object is built.
// ---------------------------------------------------------------------------------------------

template <template <class> class D>
void test_retry_mt(const char* ctx) {
  const int before = failures;
  constexpr int threads = 8;
  graph g(1);
  g.throws_left[0] = 3;
  g.sleep_ms[0] = 2;  // long enough that other threads block and must be woken
  std::atomic<int> caught{0};
  {
    test_injector<D, s4::shared_storage, 1> inj(g);
    std::vector<const node*> seen(threads);
    std::latch start(threads);
    std::vector<std::thread> pool;
    for (int t = 0; t < threads; ++t) {
      pool.emplace_back([&, t] {
        start.arrive_and_wait();
        for (int attempt = 0; attempt < 100; ++attempt) {
          try {
            seen[static_cast<std::size_t>(t)] = &inj.get(0);
            return;
          } catch (const std::runtime_error&) {
            ++caught;
          }
        }
      });
    }
    for (auto& th : pool) th.join();
    CHECK(caught.load() == 3);
    CHECK(g.constructions[0].load() == 1);
    for (int t = 0; t < threads; ++t) CHECK(seen[static_cast<std::size_t>(t)] == seen[0]);
    CHECK(seen[0] != nullptr);
    CHECK(inj.td.size() == 1);
  }
  CHECK(g.alive[0].load() == 0);
  report(ctx, before);
}

// ---------------------------------------------------------------------------------------------
// R004: a constructor that dereferences a lazy to its own slot.
// ---------------------------------------------------------------------------------------------

template <template <class> class D>
void test_r004(const char* ctx) {
  const int before = failures;
  graph g(3);  // 0 re-enters itself; 1 dereferences a lazy to 2 (a different slot): fine
  test_injector<D, s4::inline_storage, 3> inj(g);
  bool reenter = true;
  g.hook = [&](std::size_t i) {
    if (i == 0 && reenter) inj.get(0);
    if (i == 1) inj.get(2);
  };
  bool reported = false;
  try {
    inj.get(0);
  } catch (const s4::reentrant_construction& e) {
    reported = std::strstr(e.what(), "TRL-R004") != nullptr;
  }
  CHECK(reported);
  CHECK(inj.td.size() == 0);  // nothing half-built was registered
  reenter = false;
  inj.get(0);  // the slot went back to empty, so it can be built now
  CHECK(g.constructions[0].load() == 1);
  inj.get(1);  // nested lazy to another slot is not re-entry
  CHECK(g.constructions[1].load() == 1);
  CHECK(g.constructions[2].load() == 1);
  CHECK(inj.td.size() == 3);
  report(ctx, before);
}

// No false R004: another thread finding the slot in `constructing` must wait, not report.
template <template <class> class D>
void test_no_false_r004(const char* ctx) {
  const int before = failures;
  graph g(1);
  g.sleep_ms[0] = 50;
  test_injector<D, s4::inline_storage, 1> inj(g);
  const node* first = nullptr;
  const node* second = nullptr;
  bool second_threw = false;
  std::thread builder([&] { first = &inj.get(0); });
  while (!g.started[0].load()) std::this_thread::yield();
  std::thread waiter([&] {
    try {
      second = &inj.get(0);
    } catch (const s4::reentrant_construction&) {
      second_threw = true;
    }
  });
  builder.join();
  waiter.join();
  CHECK(!second_threw);
  CHECK(first != nullptr && first == second);
  CHECK(g.constructions[0].load() == 1);
  report(ctx, before);
}

// ---------------------------------------------------------------------------------------------
// shared_ptr storage gives real shared ownership; inline storage cannot answer shared_ptr.
// ---------------------------------------------------------------------------------------------

template <class Slot>
concept answers_shared_ptr =
    requires(Slot& slot, s4::teardown_stack<1, true>& td, s4::injector_sync& sync) {
      s4::share(slot, td, sync, [](auto emplace) { emplace(0, nullptr, std::vector<int>{}); });
    };

static_assert(answers_shared_ptr<s4::wait_slot<s4::shared_storage<node>>>);
static_assert(answers_shared_ptr<s4::cv_slot<s4::shared_storage<node>>>);
static_assert(answers_shared_ptr<s4::once_slot<s4::shared_storage<node>>>);
static_assert(!answers_shared_ptr<s4::wait_slot<s4::inline_storage<node>>>);
static_assert(!answers_shared_ptr<s4::cv_slot<s4::inline_storage<node>>>);
static_assert(!answers_shared_ptr<s4::once_slot<s4::inline_storage<node>>>);
static_assert(!answers_shared_ptr<s4::plain_slot<s4::inline_storage<node>>>);

void test_shared_semantics(const char* ctx) {
  const int before = failures;
  graph g(1);
  std::shared_ptr<node> held;
  std::weak_ptr<node> weak;
  {
    test_injector<wait_d, s4::shared_storage, 1> inj(g);
    held = inj.share(0);
    weak = held;
    CHECK(held.use_count() == 2);  // the injector's reference and ours
    CHECK(&inj.get(0) == held.get());
  }
  CHECK(g.alive[0].load() == 1);  // teardown dropped the injector's reference only
  CHECK(held.use_count() == 1);
  held.reset();
  CHECK(g.alive[0].load() == 0);
  CHECK(weak.expired());
  report(ctx, before);
}

// ---------------------------------------------------------------------------------------------
// Sizes.
// ---------------------------------------------------------------------------------------------

struct one_byte {
  char c;
};
struct eight_bytes {
  void* p;
};
struct sixty_four_bytes {
  char c[64];
};

template <template <class> class Storage>
void print_slot_row(const char* storage_name, const char* payload_name, std::size_t payload_size,
                    std::size_t once_size, std::size_t cv_size, std::size_t wait_size,
                    std::size_t none_size) {
  std::printf("| slot | %s<%s> (%zu B) | %zu | %zu | %zu | %zu |\n", storage_name, payload_name,
              payload_size, once_size, cv_size, wait_size, none_size);
}

template <template <class> class Storage, class T>
void slot_row(const char* storage_name, const char* payload_name) {
  print_slot_row<Storage>(storage_name, payload_name, sizeof(T), sizeof(s4::once_slot<Storage<T>>),
                          sizeof(s4::cv_slot<Storage<T>>), sizeof(s4::wait_slot<Storage<T>>),
                          sizeof(s4::plain_slot<Storage<T>>));
}

void print_sizes() {
#if defined(_LIBCPP_VERSION)
  std::printf("standard library: libc++ %d\n", _LIBCPP_VERSION);
#elif defined(__GLIBCXX__)
  std::printf("standard library: libstdc++ %d\n", __GLIBCXX__);
#endif
  std::printf("| what | type | A call_once | B atomic+cv | C atomic+wait | none |\n");
  std::printf("|---|---|---|---|---|---|\n");
  std::printf(
      "| sync per slot | | %zu (once_flag) | %zu (atomic<u8>) | %zu (atomic<u8>) | 1 (u8) |\n",
      sizeof(std::once_flag), sizeof(std::atomic<std::uint8_t>), sizeof(std::atomic<std::uint8_t>));
  std::printf("| sync per injector | | 0 | %zu (mutex %zu + condition_variable %zu) | 0 | 0 |\n",
              sizeof(s4::injector_sync), sizeof(std::mutex), sizeof(std::condition_variable));
  slot_row<s4::shared_storage, one_byte>("shared", "1B");
  slot_row<s4::shared_storage, sixty_four_bytes>("shared", "64B");
  slot_row<s4::inline_storage, one_byte>("inline", "1B");
  slot_row<s4::inline_storage, eight_bytes>("inline", "8B");
  slot_row<s4::inline_storage, sixty_four_bytes>("inline", "64B");
  std::printf(
      "other: shared_ptr %zu, teardown_entry %zu, teardown_stack<100> %zu, thread::id %zu\n",
      sizeof(std::shared_ptr<int>), sizeof(s4::teardown_entry),
      sizeof(s4::teardown_stack<100, true>), sizeof(std::thread::id));
}

// ---------------------------------------------------------------------------------------------
// std::call_once hazards, each in a detached thread under a watchdog.
// ---------------------------------------------------------------------------------------------

std::once_flag hazard_flag_retry;
std::once_flag hazard_flag_reentry;
std::atomic<int> hazard_retry_done{0};
std::atomic<int> hazard_reentry_done{0};

bool wait_for(const std::atomic<int>& done, int ms) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
  while (std::chrono::steady_clock::now() < until) {
    if (done.load() != 0) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return false;
}

[[noreturn]] void run_hazards() {
#if defined(S4_TSAN)
  const char* build = "TSan";
#else
  const char* build = "plain";
#endif
#if defined(_LIBCPP_VERSION)
  const char* lib = "libc++";
#else
  const char* lib = "libstdc++";
#endif
  std::thread([] {
    try {
      std::call_once(hazard_flag_retry, [] { throw std::runtime_error("first attempt"); });
    } catch (const std::runtime_error&) {
    }
    std::call_once(hazard_flag_retry, [] {});
    hazard_retry_done = 1;
  }).detach();
  const bool retried = wait_for(hazard_retry_done, 2000);
  std::printf("hazard [%s, %s] call_once retry after a throwing callable: %s\n", lib, build,
              retried ? "retried (OK)" : "HANG (no retry; the next call blocks forever)");
  std::fflush(stdout);
  std::thread([] {
    std::call_once(hazard_flag_reentry, [] { std::call_once(hazard_flag_reentry, [] {}); });
    hazard_reentry_done = 1;
  }).detach();
  const bool returned = wait_for(hazard_reentry_done, 1000);
  std::printf("hazard [%s, %s] re-entrant call_once on the same flag: %s\n", lib, build,
              returned ? "returned" : "HANG (deadlock, nothing reported)");
  std::fflush(stdout);
  std::_Exit(0);  // the hung threads cannot be joined
}

// ---------------------------------------------------------------------------------------------
// --first-access DESIGN COUNT: first access to COUNT fresh slots on one thread, so that run.sh
// can count the system calls each design's uncontended cold path makes (under strace).
// ---------------------------------------------------------------------------------------------

template <class Slot>
void first_access(std::size_t count) {
  constexpr std::size_t capacity = 256;
  auto slots = std::make_unique<std::array<Slot, capacity>>();
  s4::injector_sync sync;
  s4::teardown_stack<capacity, true> td;  // declared after the slots: unwinds first
  for (std::size_t i = 0; i < count && i < capacity; ++i)
    (*slots)[i].get(td, sync, [](auto emplace) { emplace(1); });
}

int run_first_access(const char* design, std::size_t count) {
  if (std::strcmp(design, "none") == 0)
    first_access<s4::plain_slot<s4::inline_storage<int>>>(count);
  else if (std::strcmp(design, "once") == 0)
    first_access<s4::once_slot<s4::inline_storage<int>>>(count);
  else if (std::strcmp(design, "cv") == 0)
    first_access<s4::cv_slot<s4::inline_storage<int>>>(count);
  else if (std::strcmp(design, "wait") == 0)
    first_access<s4::wait_slot<s4::inline_storage<int>>>(count);
  else
    return 2;
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  int rounds = 30;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--sizes") == 0) {
      print_sizes();
      return 0;
    }
    if (std::strcmp(argv[i], "--hazards") == 0) run_hazards();
    if (std::strcmp(argv[i], "--first-access") == 0 && i + 2 < argc)
      return run_first_access(argv[i + 1], static_cast<std::size_t>(std::atoi(argv[i + 2])));
    if (std::strcmp(argv[i], "--rounds") == 0 && i + 1 < argc) rounds = std::atoi(argv[++i]);
  }

  test_exactly_once<once_d, s4::inline_storage>("exactly once, 8 threads: A call_once, inline",
                                                rounds);
  test_exactly_once<cv_d, s4::inline_storage>("exactly once, 8 threads: B atomic+cv, inline",
                                              rounds);
  test_exactly_once<wait_d, s4::inline_storage>("exactly once, 8 threads: C atomic+wait, inline",
                                                rounds);
  test_exactly_once<cv_d, s4::shared_storage>("exactly once, 8 threads: B atomic+cv, shared",
                                              rounds);
  test_exactly_once<wait_d, s4::shared_storage>("exactly once, 8 threads: C atomic+wait, shared",
                                                rounds);

  test_teardown_order<once_d, s4::inline_storage>("teardown order: A call_once");
  test_teardown_order<cv_d, s4::inline_storage>("teardown order: B atomic+cv");
  test_teardown_order<wait_d, s4::shared_storage>("teardown order: C atomic+wait, shared");
  test_teardown_order<none_d, s4::inline_storage>("teardown order: none");

#if defined(S4_CALL_ONCE_THROW_HANGS)
  std::printf(
      "[SKIP] throw partway, retry, teardown: A call_once (hangs under TSan with libstdc++; see "
      "--hazards)\n");
#else
  test_throw_partway<once_d, s4::inline_storage>("throw partway, retry, teardown: A call_once");
#endif
  test_throw_partway<cv_d, s4::inline_storage>("throw partway, retry, teardown: B atomic+cv");
  test_throw_partway<wait_d, s4::inline_storage>("throw partway, retry, teardown: C atomic+wait");
  test_throw_partway<none_d, s4::shared_storage>("throw partway, retry, teardown: none, shared");

  test_eager_strong_guarantee<wait_d, s4::inline_storage>(
      "eager build throws partway: strong guarantee");

#if defined(S4_CALL_ONCE_THROW_HANGS)
  std::printf(
      "[SKIP] retry under contention: A call_once (hangs under TSan with libstdc++; see "
      "--hazards)\n");
#else
  test_retry_mt<once_d>("retry under contention, 8 threads: A call_once");
#endif
  test_retry_mt<cv_d>("retry under contention, 8 threads: B atomic+cv");
  test_retry_mt<wait_d>("retry under contention, 8 threads: C atomic+wait");

  test_r004<none_d>("R004 re-entry detected: none");
  test_r004<cv_d>("R004 re-entry detected: B atomic+cv");
  test_r004<wait_d>("R004 re-entry detected: C atomic+wait");
  test_no_false_r004<cv_d>("no false R004 for another thread: B atomic+cv");
  test_no_false_r004<wait_d>("no false R004 for another thread: C atomic+wait");

  test_exactly_once<wait_spin_d, s4::inline_storage>("exactly once, 8 threads: C+spin, inline",
                                                     rounds);
  test_throw_partway<wait_spin_d, s4::inline_storage>("throw partway, retry, teardown: C+spin");
  test_retry_mt<wait_spin_d>("retry under contention, 8 threads: C+spin");
  test_r004<wait_spin_d>("R004 re-entry detected: C+spin");
  test_no_false_r004<wait_spin_d>("no false R004 for another thread: C+spin");

  test_shared_semantics("shared_ptr storage: real shared ownership past teardown");

  std::printf("%d checks, %d failed\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
