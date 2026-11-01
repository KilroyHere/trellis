# Trellis

A header-only C++20 dependency injection library. It builds and connects the objects in a C++ program for you, checks the wiring when the program compiles, and aims to cost no more at run time than wiring by hand.

> **Status: pre-alpha.** Phase 0 (foundations) is complete. Phase 1 (the core injector, v0.1) is in progress: bindings, modules, lifetimes, `create<T>()` with its diagnostics, and test overrides work, as in the example below, and the API may still change before v0.1. Packaging is next.

## Goals

- **Compile-time checks.** Wiring is checked at compile time and resolves as fast as hand-written code.
- **Clear errors.** One error per mistake, naming the missing type and the path that needs it.
- **Lifetime safety.** Lifetime rules turn dangling references and captive dependencies into compile errors.
- **Plugins.** Plugins and run-time registration come through a runtime container, planned for 1.0.

The design is in [`docs/plan.md`](docs/plan.md), and the decisions behind it are in [`docs/adr/`](docs/adr/README.md).

## What it looks like

This is the hello world of [plan §6.1](docs/plan.md#61-hello-world), from [`examples/hello_world.cpp`](examples/hello_world.cpp), which the tests build and run. Names may still change before v0.1.

Your classes stay plain C++: none of them includes Trellis, and constructors are discovered without macros.

```cpp
struct ILogger     { virtual ~ILogger() = default;     virtual void log(std::string_view) = 0; };
struct IRepository { virtual ~IRepository() = default; virtual void save(int) = 0; };
struct IClock      { virtual ~IClock() = default;      virtual long now() const = 0; };
struct Config      { std::string db_url; };

class ConsoleLogger final : public ILogger     { /* ... */ };
class SystemClock   final : public IClock      { /* ... */ };
class SqlRepository final : public IRepository {
 public:
  SqlRepository(const Config& cfg, ILogger& log);
  void save(int value) override;
  // ...
};

class Service {
 public:
  Service(std::shared_ptr<IRepository> repo, ILogger& log, std::unique_ptr<IClock> clock);
  void handle(int value);
  // ...
};

class App {
 public:
  explicit App(Service& service);
  void run();
  // ...
};
```

The composition root says which implementation goes with each interface, and how long each object lives:

```cpp
#include <trellis/trellis.hpp>
namespace di = trellis;

int main() {
  const Config cfg = load_config();
  auto injector = di::make_injector(
      di::bind<ILogger>.to<ConsoleLogger>().in(di::singleton),  // one per injector
      di::bind<IRepository>.to<SqlRepository>().in(di::singleton),
      di::bind<IClock>.to<SystemClock>(),   // unique: a new one per request
      di::bind<Service>.in(di::singleton),  // App takes Service&, so it needs an owner
      di::bind<Config>.to_ref(cfg));        // external: used, never owned
  injector.create<App>().run();
}
```

`create<App>()` builds the App, its Service and everything they need, once per singleton, and the injector destroys its singletons in reverse order of construction when it goes. Any constructor parameter form works that the binding's lifetime allows: by value, by reference or pointer, or as `std::unique_ptr`, `std::shared_ptr` or `std::weak_ptr` ([plan §6.3](docs/plan.md#63-the-request-form-compatibility-table)).

A test keeps the same wiring and swaps one piece: passed to `make_injector` beside the production bindings, `di::override<ILogger>.to_ref(fake_logger)` replaces the `ILogger` binding wherever the other arguments hold it, modules included, and each test's injector owns its own singletons. `<trellis/testing/wiring.hpp>` adds `trellis::testing::wireable<Injector, App>`, for a `static_assert` that the graph can be built, and `trellis::testing::create_all(injector)`, which builds every singleton ([plan §6.10](docs/plan.md#610-testing)).

Trellis checks the wiring at compile time, and each mistake is one error with an ID. Delete the `di::bind<Service>` line, and nothing owns the Service that App takes by reference; Clang 18 says:

```text
error: static assertion failed due to requirement 'trellis::error::E012<Service, trellis::error::request<Service &>>::reported': TRL-E012: unbound concrete type requested by reference, pointer or shared_ptr: nothing would own it; bind it with an owning lifetime such as trellis::singleton
```

The ID leads to a page in [`docs/errors/`](docs/errors/README.md). The instantiation note below the error names the path from the root, `trellis::error::path<App>` here, and the parameter that makes the request. On GCC 14+ and Clang 20+, a multi-line message like this one is planned (Phase 4):

```text
error: static assertion failed: trellis [TRL-E001]: no binding for abstract type 'IRepository'
  requested as 'std::shared_ptr<IRepository>' by 'Service' (parameter 1)
  path: App -> Service -> IRepository
  fix: add di::bind<IRepository>.to<YourImplementation>() (docs: errors/TRL-E001)
```

## Repository map

| Path | What it holds |
|---|---|
| [`docs/plan.md`](docs/plan.md) | The design and implementation plan |
| [`docs/adr/`](docs/adr/README.md) | Architecture decision records |
| [`docs/errors/`](docs/errors/README.md) | The error catalog: one page per error ID |
| [`docs/internals/`](docs/internals/README.md) | Maintainer notes, and the doc-comment convention for public headers |
| [`CHANGELOG.md`](CHANGELOG.md) | User-visible changes, release by release |
| [`docs/PROGRESS.md`](docs/PROGRESS.md) | Progress log: what is done, the evidence, decisions, open items |
| `docs/tasks.json` | The task list, managed with `scripts/tasks.py` |
| `docs/spikes/`, `spikes/` | The Phase 0 experiments behind the decisions |
| `include/trellis/` | The library (header-only) |
| `tests/` | Unit, compile-fail, golden-diagnostics and property tests |
| `benchmarks/` | Compile-time and runtime benchmarks, and their budgets |
| [`docs/benchmarks/`](docs/benchmarks/v0.1.md) | Benchmark results against hand-written wiring and the targets, per release |
| `tools/`, `scripts/` | Test runners and developer scripts |
| [`research/`](research/README.md) | Background research on existing C++ DI libraries |
| [`docs/agent/PROMPTS.md`](docs/agent/PROMPTS.md) | How this project is built with Claude Code |

## Building and testing

You need:

- CMake 3.28 or later, because the Clang presets build the C++20 module (the headers alone need 3.21);
- Ninja;
- GCC 13 or 14, or Clang 18 or 20 (libc++ optional), with Clang's `clang-scan-deps` (`clang-tools-18` and `clang-tools-20` on Debian and Ubuntu).

GoogleTest is found on the system or fetched. The benchmarks need Google Benchmark 1.8 or later.

To check tools, configure, build and test the clang-18-debug preset:

```bash
scripts/init.sh
```

To build and test every local preset, including the sanitizer builds:

```bash
scripts/check-full.sh
```

CI runs Linux on every push. macOS and Windows (MSVC) run weekly and on demand.

### The single header and the C++20 module

Besides `include/`, Trellis comes in two other forms, both made from the headers. Every unit test runs against the single header too. The modules run their own smoke tests and the unit tests that use only public names; the others test internals, which a module doesn't export. [docs/internals/delivery.md](docs/internals/delivery.md) explains both.

**Single header.** Generate it, then copy `single_include/trellis.hpp` into your project and include it in place of the headers:

```bash
python3 tools/amalgamate.py          # writes single_include/trellis.hpp
python3 tools/amalgamate.py --check  # generates it in memory, compiles it, and fails if the file is stale
```

**Module.** With CMake 3.28, Ninja and Clang 16 or later with `clang-scan-deps`, add Trellis to your build with the module turned on, and import it:

```cmake
set(TRELLIS_ENABLE_MODULE ON)  # before add_subdirectory() or FetchContent_MakeAvailable()
add_subdirectory(trellis)
target_link_libraries(app PRIVATE trellis::module)
```

```cpp
import trellis;
```

Tests that use the testing helpers of `<trellis/testing/wiring.hpp>` write `import trellis.testing;` instead, which also gives everything `trellis` gives. Build the module and your program in the same language mode. A module carries no macros, so include `<trellis/config.hpp>` before the import if you need `TRELLIS_VERSION` or the `TRELLIS_HAS_*` macros. Nor does it export the standard library: include the standard headers whose names or operators you use, such as `<string_view>` to compare what `type_name<T>()` returns. Put every `#include` before `import trellis;`: Clang 20 with libstdc++ and Clang 18 with libc++ reject some standard headers included after it. A configuration macro such as `TRELLIS_HAS_ATOMIC_WAIT` reaches the module only from its own build: `target_compile_definitions(trellis_module PUBLIC TRELLIS_HAS_ATOMIC_WAIT=0)`. If your project requires a CMake older than 3.28, set `CXX_SCAN_FOR_MODULES` on the targets that import the module. Don't use ccache for those targets: it can return a stale object file after Trellis changes. GCC 14 can't use the module, and installed packages (`find_package(trellis)`) don't include it yet.

## Contributing

See [`CONTRIBUTING.md`](CONTRIBUTING.md).

## License

[Boost Software License 1.0](LICENSE). Copyright (c) 2026 Aryan Patel.
