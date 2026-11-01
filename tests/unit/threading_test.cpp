// Concurrency tests for the singleton slots under threading::safe (T1.5; plan §7.6, ADR-0012,
// spike S4 §3.4): a graph of singletons with nested dependencies, requested from 8 threads in
// different orders, is built exactly once per injector and every thread sees the same objects,
// and teardown destroys each node before its dependencies, whichever threads built them;
// threads that arrive during a slow construction block and then get the same object; and a
// constructor that throws while threads wait leads to exactly as many exceptions as failures,
// each reaching one caller, and then to one object.
//
// The tsan preset runs these under ThreadSanitizer (ctest -L unit, through scripts/check-full.sh).
// tests/unit/CMakeLists.txt builds this file twice: as part of the unit tests, with atomic::wait,
// and on its own with TRELLIS_HAS_ATOMIC_WAIT defined as 0, which forces the fallback of a mutex
// and a condition variable per injector (ADR-0012 item 3; test names prefixed wait_fallback.).
#include <trellis/trellis.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace {

using trellis::bind;
using trellis::make_injector;

constexpr int thread_count = 8;
constexpr std::size_t node_count = 16;

std::array<std::atomic<int>, node_count>& constructions() {
  static std::array<std::atomic<int>, node_count> counts{};
  return counts;
}

// The nodes in the order they were destroyed. Only teardown writes it, on the thread that
// destroys the injector, after the threads that built the nodes have been joined.
std::vector<std::size_t>& destructions() {
  static std::vector<std::size_t> order;
  return order;
}

// Node I depends on node I / 2 (a tree), and nodes with I % 3 == 0 also on node I - 1, so that
// a thread that requests a node builds a chain of dependencies, and threads that request
// different nodes meet on shared ones.
template <std::size_t I>
struct node {
  node() { constructions()[I].fetch_add(1, std::memory_order_relaxed); }
  node(const node&) = delete;
  node& operator=(const node&) = delete;
  ~node() { destructions().push_back(I); }
  int value = static_cast<int>(I);
};
template <std::size_t I>
  requires(I > 0 && I % 3 != 0)
struct node<I> {
  explicit node(node<I / 2>& parent) : parent_(&parent) {
    constructions()[I].fetch_add(1, std::memory_order_relaxed);
  }
  node(const node&) = delete;
  node& operator=(const node&) = delete;
  ~node() { destructions().push_back(I); }
  node<I / 2>* parent_;
  int value = static_cast<int>(I);
};
template <std::size_t I>
  requires(I > 0 && I % 3 == 0)
struct node<I> {
  node(node<I / 2>& parent, node<I - 1>& previous) : parent_(&parent), previous_(&previous) {
    constructions()[I].fetch_add(1, std::memory_order_relaxed);
  }
  node(const node&) = delete;
  node& operator=(const node&) = delete;
  ~node() { destructions().push_back(I); }
  node<I / 2>* parent_;
  node<I - 1>* previous_;
  int value = static_cast<int>(I);
};

// Even nodes in std::shared_ptr storage, odd ones inline.
template <std::size_t I>
constexpr auto node_binding() {
  if constexpr (I % 2 == 0) {
    return bind<node<I>>.in(trellis::singleton);
  } else {
    return bind<node<I>>.in(trellis::singleton_inline);
  }
}

template <std::size_t... Is>
auto make_graph_injector(std::index_sequence<Is...> /*nodes*/) {
  return make_injector(node_binding<Is>()...);
}

using graph_injector = decltype(make_graph_injector(std::make_index_sequence<node_count>{}));

// One function per node that requests it, so that threads can request the nodes in different
// orders at run time.
using getter = const void* (*)(graph_injector&);
template <std::size_t I>
const void* get_node(graph_injector& injector) {
  return &injector.create<node<I>&>();
}
template <std::size_t... Is>
constexpr std::array<getter, node_count> make_getters(std::index_sequence<Is...> /*nodes*/) {
  return {&get_node<Is>...};
}
constexpr std::array<getter, node_count> getters =
    make_getters(std::make_index_sequence<node_count>{});

// Starts thread_count threads that wait for one signal, then run `work(thread_index)`.
template <class Work>
void race(Work work) {
  std::atomic<bool> go{false};
  std::vector<std::thread> threads;
  threads.reserve(thread_count);
  for (int index = 0; index != thread_count; ++index) {
    threads.emplace_back([&go, &work, index] {
      while (!go.load(std::memory_order_acquire)) {
        std::this_thread::yield();
      }
      work(index);
    });
  }
  go.store(true, std::memory_order_release);
  for (std::thread& thread : threads) {
    thread.join();
  }
}

TEST(Threading, EachSingletonIsBuiltOnceAndSeenByEveryThread) {
  constexpr int rounds = 30;
  for (int round = 0; round != rounds; ++round) {
    for (std::atomic<int>& count : constructions()) {
      count.store(0, std::memory_order_relaxed);
    }
    graph_injector injector = make_graph_injector(std::make_index_sequence<node_count>{});
    std::array<std::array<const void*, node_count>, thread_count> seen{};
    race([&](int index) {
      // Each thread requests every node once, in its own order: 5 is coprime with 16, so
      // k -> 5k + 3 * index runs through all nodes.
      std::array<const void*, node_count>& addresses = seen[static_cast<std::size_t>(index)];
      for (std::size_t k = 0; k != node_count; ++k) {
        const std::size_t n = (k * 5 + static_cast<std::size_t>(index) * 3) % node_count;
        addresses[n] = getters[n](injector);
      }
    });
    for (std::size_t n = 0; n != node_count; ++n) {
      EXPECT_EQ(constructions()[n].load(), 1) << "node " << n << ", round " << round;
      for (std::size_t t = 1; t != thread_count; ++t) {
        EXPECT_EQ(seen[t][n], seen[0][n]) << "node " << n << ", round " << round;
      }
    }
    EXPECT_EQ(injector.create<node<15>&>().parent_, &injector.create<node<7>&>());
    EXPECT_EQ(injector.create<node<6>&>().previous_, &injector.create<node<5>&>());
  }
}

// Teardown runs in reverse order of the teardown stack, where a slot pushes its entry after its
// constructor returns and before it publishes the object (ADR-0005, plan §7.5). A dependency is
// published before its dependent's constructor runs, so its entry lies below the dependent's even
// when another thread built it, and each node is destroyed before the nodes it depends on.
TEST(Threading, TeardownDestroysDependentsFirstWhicheverThreadsBuiltThem) {
  constexpr int rounds = 30;
  for (int round = 0; round != rounds; ++round) {
    destructions().clear();
    {
      graph_injector injector = make_graph_injector(std::make_index_sequence<node_count>{});
      race([&](int index) {
        // Each thread starts from another node, going down, so that the chains of dependencies
        // that threads build cross.
        for (std::size_t k = 0; k != node_count; ++k) {
          const std::size_t n =
              (node_count - 1 - k + static_cast<std::size_t>(index) * 5) % node_count;
          static_cast<void>(getters[n](injector));
        }
      });
    }
    ASSERT_EQ(destructions().size(), node_count) << "round " << round;
    std::array<std::size_t, node_count> position{};
    for (std::size_t place = 0; place != node_count; ++place) {
      position[destructions()[place]] = place;
    }
    for (std::size_t n = 1; n != node_count; ++n) {
      EXPECT_LT(position[n], position[n / 2]) << "node " << n << ", round " << round;
      if (n % 3 == 0) {
        EXPECT_LT(position[n], position[n - 1]) << "node " << n << ", round " << round;
      }
    }
  }
}

std::atomic<int> slow_constructions{0};

struct slow_service {
  slow_service() {
    slow_constructions.fetch_add(1, std::memory_order_relaxed);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  slow_service(const slow_service&) = delete;
  slow_service& operator=(const slow_service&) = delete;
  ~slow_service() = default;
};

TEST(Threading, ThreadsArrivingDuringAConstructionWaitForIt) {
  slow_constructions.store(0);
  auto injector = make_injector(bind<slow_service>.in(trellis::singleton));
  std::array<const slow_service*, thread_count> seen{};
  race([&](int index) {
    seen[static_cast<std::size_t>(index)] = &injector.create<slow_service&>();
  });
  EXPECT_EQ(slow_constructions.load(), 1);
  for (const slow_service* address : seen) {
    EXPECT_EQ(address, seen[0]);
  }
}

std::atomic<int> remaining_failures{0};
std::atomic<int> flaky_constructions{0};

struct flaky_service {
  flaky_service() {
    // Slow enough that other threads wait for each attempt.
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    if (remaining_failures.fetch_sub(1, std::memory_order_relaxed) > 0) {
      throw std::runtime_error("flaky");
    }
    flaky_constructions.fetch_add(1, std::memory_order_relaxed);
  }
  flaky_service(const flaky_service&) = delete;
  flaky_service& operator=(const flaky_service&) = delete;
  ~flaky_service() = default;
};

TEST(Threading, AThrowingConstructorIsRetriedByTheNextCaller) {
  constexpr int failures = 3;
  remaining_failures.store(failures);
  flaky_constructions.store(0);
  auto injector = make_injector(bind<flaky_service>.in(trellis::singleton_inline));
  std::atomic<int> exceptions{0};
  std::array<const flaky_service*, thread_count> seen{};
  race([&](int index) {
    for (;;) {
      try {
        seen[static_cast<std::size_t>(index)] = &injector.create<flaky_service&>();
        return;
      } catch (const std::runtime_error&) {
        exceptions.fetch_add(1, std::memory_order_relaxed);
      }
    }
  });
  EXPECT_EQ(exceptions.load(), failures);  // each failure reached exactly one caller
  EXPECT_EQ(flaky_constructions.load(), 1);
  for (const flaky_service* address : seen) {
    EXPECT_EQ(address, seen[0]);
  }
}

}  // namespace
