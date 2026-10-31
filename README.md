# Trellis

A header-only C++20 dependency injection library. It builds and connects the objects in a C++ program for you, checks the wiring when the program compiles, and aims to cost no more at run time than wiring by hand.

> **Status: pre-alpha, not usable yet.** Phase 0 (foundations) is complete. Phase 1 (the core injector, v0.1) is in progress. There is no public injector API yet.

## Goals

- **Compile-time checks.** Wiring is checked at compile time and resolves as fast as hand-written code.
- **Clear errors.** One error per mistake, naming the missing type and the path that needs it.
- **Lifetime safety.** Lifetime rules turn dangling references and captive dependencies into compile errors.
- **Plugins.** Plugins and run-time registration come through a runtime container, planned for 1.0.

The design is in [`docs/plan.md`](docs/plan.md), and the decisions behind it are in [`docs/adr/`](docs/adr/README.md).

## What it will look like

> **Planned API, not implemented yet.** This is the design in [plan §6.1](docs/plan.md#61-hello-world). Names may still change before v0.1. Once `create<T>()` exists (task T1.5), this example becomes a compiled test.

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
};

class Service {
 public:
  Service(std::shared_ptr<IRepository> repo, ILogger& log, std::unique_ptr<IClock> clock);
};

class App {
 public:
  explicit App(Service& service);
  void run();
};
```

The composition root says which implementation goes with each interface, and how long each object lives:

```cpp
#include <trellis/trellis.hpp>
namespace di = trellis;

int main() {
  const Config cfg = load_config();
  auto injector = di::make_injector(
      di::bind<ILogger>.to<ConsoleLogger>().in(di::singleton),      // one per injector
      di::bind<IRepository>.to<SqlRepository>().in(di::singleton),
      di::bind<IClock>.to<SystemClock>(),                           // unique: a new one per request
      di::bind<Service>.in(di::singleton),                          // App takes Service&, so it needs an owner
      di::bind<Config>.to_ref(cfg));                                // external: used, never owned
  injector.create<App>().run();
}
```

Trellis checks the wiring at compile time. For example, a missing binding is one error that names the type and the path to it. This is the planned format on GCC 14+ and Clang 20+; older compilers show the same ID and type in a shorter form:

```text
error: static assertion failed: trellis [TRL-E001]: no binding for abstract type 'IRepository'
  requested as 'std::shared_ptr<IRepository>' by 'Service' (parameter 1)
  path: App -> Service -> IRepository
  fix: add di::bind<IRepository>.to<YourImplementation>() (docs: errors/TRL-E001)
```

Each error ID gets a page in [`docs/errors/`](docs/errors/README.md) when the library first reports it.

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
| `tools/`, `scripts/` | Test runners and developer scripts |
| [`research/`](research/README.md) | Background research on existing C++ DI libraries |
| [`docs/agent/PROMPTS.md`](docs/agent/PROMPTS.md) | How this project is built with Claude Code |

## Building and testing

You need:

- CMake 3.21 or later (3.28 for the C++20 module);
- Ninja;
- GCC 13 or 14, or Clang 18 or 20 (libc++ optional).

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

## Contributing

See [`CONTRIBUTING.md`](CONTRIBUTING.md).

## License

[Boost Software License 1.0](LICENSE). Copyright (c) 2026 Aryan Patel.
