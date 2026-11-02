# Getting started

Trellis builds the objects of a C++ program and connects them, and it checks that wiring when the program compiles. This guide takes you from installing Trellis to a wired and tested application. It describes v0.1, the first release, as it is: Trellis is pre-alpha, and before 1.0 a minor release may still change the API ([plan §10](plan.md#10-build-packaging-and-releases)).

Every C++ block below is an excerpt of a file that the tests build, with warnings as errors, and run: [`examples/hello_world.cpp`](../examples/hello_world.cpp), [`examples/getting_started.cpp`](../examples/getting_started.cpp) and [`tests/unit/testing_test.cpp`](../tests/unit/testing_test.cpp). The check `tools/doc_snippets.py` keeps the blocks and the files in step.

1. [Requirements and installation](#1-requirements-and-installation)
2. [Hello world](#2-hello-world)
3. [Constructors](#3-constructors)
4. [Bindings and lifetimes](#4-bindings-and-lifetimes)
5. [Request forms](#5-request-forms)
6. [Testing](#6-testing)
7. [When it doesn't compile](#7-when-it-doesnt-compile)
8. [Known limitations in v0.1](#8-known-limitations-in-v01)

## 1. Requirements and installation

**C++20.** Trellis is header-only and needs only the standard library. Its CMake target, `trellis::trellis`, requires C++20, so CMake adds the flag for you.

**Compilers.** Trellis aims to support GCC 12, Clang 16, Apple Clang 15 and MSVC 19.38 (Visual Studio 2022 17.8) or later, with libstdc++, libc++ or Microsoft's standard library, on Linux, macOS and Windows (NFR-13, [plan §3.2](plan.md#32-non-functional-targets)). v0.1 is tested with these:

| Compilers | Tested |
|---|---|
| GCC 13 and 14, Clang 18 and 20, and Clang 18 with libc++, on Linux | on every push |
| Apple Clang 21 on macOS, and MSVC 19.51 (Visual Studio 2026) on Windows | weekly, and on demand |

The minimum versions that NFR-13 names aren't tested yet. [Section 8](#8-known-limitations-in-v01) lists what doesn't work yet on MSVC and Apple Clang.

**With CMake** (3.21 or later), in one of three ways:

- **An installed package.** Install the headers and the package files, without building the tests:

  ```bash
  cmake -S trellis -B trellis/build -DTRELLIS_BUILD_TESTS=OFF
  cmake --install trellis/build --prefix /opt/trellis
  ```

  Then find it from your project, configured with `-DCMAKE_PREFIX_PATH=/opt/trellis`:

  ```cmake
  find_package(trellis 0.1 CONFIG REQUIRED)
  target_link_libraries(app PRIVATE trellis::trellis)
  ```

  Before 1.0, a request for version 0.1 accepts only 0.1.x releases, as a minor release may break the API.

- **A subdirectory** of your source tree:

  ```cmake
  add_subdirectory(third_party/trellis)
  target_link_libraries(app PRIVATE trellis::trellis)
  ```

- **FetchContent:**

  ```cmake
  include(FetchContent)
  FetchContent_Declare(trellis
    GIT_REPOSITORY https://github.com/KilroyHere/trellis.git
    GIT_TAG        <commit or tag>)
  FetchContent_MakeAvailable(trellis)
  target_link_libraries(app PRIVATE trellis::trellis)
  ```

  As a subproject, by either of the last two routes, Trellis builds none of its tests.

**The single header.** Generate it, copy `single_include/trellis.hpp` into your project, and include it in place of `<trellis/trellis.hpp>`. It holds every header, including the testing helpers of [section 6](#6-testing). It isn't stored in the repository; releases will attach it.

```bash
python3 tools/amalgamate.py          # writes single_include/trellis.hpp
python3 tools/amalgamate.py --check  # generates it in memory, compiles it, and fails if the file is stale
```

**The C++20 module.** It needs CMake 3.28, the Ninja generator (1.11 or later) or Visual Studio 2022's, and Clang 16 or later with its `clang-scan-deps` (Debian and Ubuntu ship it in `clang-tools-<major>`). It is tested with Clang 18 and 20, with libstdc++ and with libc++, and other versions get a warning at configure time. CMake refuses GCC before 15, as GCC 14 builds the module but its importers get wrong results; GCC 15 or later and MSVC 19.34 or later are accepted, untested. Turn the module on before adding Trellis, link `trellis::module`, and write `import trellis;` in place of the include:

```cmake
set(CMAKE_CXX_STANDARD 20)
set(TRELLIS_ENABLE_MODULE ON)  # before add_subdirectory() or FetchContent_MakeAvailable()
add_subdirectory(third_party/trellis)
target_link_libraries(app PRIVATE trellis::module)
```

- Tests write `import trellis.testing;`, which also gives everything `import trellis;` gives.
- Put every `#include` before `import trellis;`. The module exports no macros, so include `<trellis/config.hpp>` if you need `TRELLIS_VERSION`, and no standard library names, so include the standard headers you use.
- Build the module and your program with the same language options: set `CMAKE_CXX_STANDARD` and `CMAKE_CXX_EXTENSIONS` before adding Trellis. If your project requires a CMake older than 3.28, set `CXX_SCAN_FOR_MODULES` on the targets that import the module. Don't build those targets with ccache, which can return a stale object file after Trellis changes.
- `find_package(trellis)` gives the headers only: the module isn't installed yet.

[docs/internals/delivery.md](internals/delivery.md) explains both forms in detail.

## 2. Hello world

This is the hello world of [plan §6.1](plan.md#61-hello-world), which the README shows too. The domain classes are plain C++: none of them includes Trellis, and none needs a macro for its constructor to be found. From [`examples/hello_world.cpp`](../examples/hello_world.cpp), which the tests build and run:

<!-- snippet: examples/hello_world.cpp -->
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

The composition root, usually `main`, binds each interface to an implementation and says how long each object lives:

<!-- snippet: examples/hello_world.cpp -->
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

`create<App>()` builds the graph from the bottom up. `App` has no binding, so it is a new object, returned by value. `Service` takes a `std::shared_ptr` to the repository singleton, a reference to the logger singleton and a new clock of its own; `SqlRepository` takes a reference to the caller's `Config`. `App` takes the `Service` by reference, and a reference needs an owner, which the singleton binding provides: delete that line, and the program no longer compiles ([TRL-E012](errors/TRL-E012.md)). When `injector` goes out of scope, it destroys its singletons in the reverse order of their construction.

## 3. Constructors

### Deduced: the default

A class that declares nothing for Trellis has its constructor deduced at compile time, as the hello world's classes do. Trellis tries the class with placeholder arguments, which convert to any parameter type, and takes the constructor with the most parameters; copy and move constructors never count. Each parameter is then a request, served as its type says ([section 5](#5-request-forms)): by value, by reference or pointer, or as a smart pointer. A parameter `X&&` is the request `X`.

- Deduction finds constructors of up to `max_ctor_arity` parameters, 16 by default ([section 4](#4-bindings-and-lifetimes)).
- Every parameter is injected, even one with a default argument. A private constructor is invisible.
- Two constructors with the same, largest number of parameters are [TRL-E002](errors/TRL-E002.md), unless the class also has a shorter one, which deduction then takes without a word: declare the constructor.
- A parameter of type `std::optional`, `std::tuple` or `std::any` can't be deduced (TRL-E002): declare the constructor.
- Configuration values need a binding: `int` and other types that aren't classes can't be deduced (TRL-E002), and a library type such as `std::string` would be deduced from its own constructors. Gather them in a struct and bind it with `.to_ref()` or `.to_value()`, as the hello world binds `Config`.

### Declaring a constructor

When deduction would take another constructor, or can't take one, declare the constructor to call as `trellis::ctor<Args...>`, with the parameter types as the constructor spells them. A declaration can go in one of three places, and the first that applies wins:

1. the binding, with `.with_ctor<Args...>()`, which concerns that binding of that injector only;
2. a specialization of `trellis::ctor_traits<T>`, outside the class;
3. a public member alias of the class, `using trellis_ctor = trellis::ctor<Args...>;`.

A declaration that matches no constructor is [TRL-E002](errors/TRL-E002.md), reported where the injector uses it. A member alias, from [`examples/getting_started.cpp`](../examples/getting_started.cpp):

<!-- snippet: examples/getting_started.cpp -->
```cpp
// Two constructors: deduction would take the one with more parameters, so the class declares the
// one the injector calls.
class Mailer {
 public:
  using trellis_ctor = trellis::ctor<ILogger&>;

  explicit Mailer(ILogger& log) : log_(log) {}
  Mailer(ILogger& log, std::unique_ptr<ITransport> transport)
      : log_(log), transport_(std::move(transport)) {}
  // ...
};
```

Without the alias, deduction would take the second constructor, and the injector of [section 4](#4-bindings-and-lifetimes), which binds no `ITransport`, would report [TRL-E001](errors/TRL-E001.md). A specialization declares the constructor of a class you can't change:

<!-- snippet: examples/getting_started.cpp -->
```cpp
namespace vendor {  // a library's class, which knows nothing of Trellis

class EventBus {
 public:
  using handler = std::function<void(std::string_view)>;

  // Subscribes any number of handlers, of any types that convert to handler.
  template <class... Handlers>
  explicit EventBus(Handlers... handlers) : handlers_{handler(std::move(handlers))...} {}
  // ...
};

}  // namespace vendor

// Deduction can't count the parameters of a constructor template that takes anything (TRL-E004),
// so a specialization of trellis::ctor_traits declares the constructor to call, outside the class:
// here, the one with no arguments.
template <>
struct trellis::ctor_traits<vendor::EventBus> : trellis::ctor<> {};
```

Declare a specialization as a `struct`, at global scope as here or in namespace `trellis`, where every file that uses the class with Trellis sees it, such as a header that the composition root includes.

A binding's declaration wins over the class's own, so one injector can call another constructor. `.with_ctor<Args...>()` combines with `.to<C>()`, `.in()` and `.allow_copy()` in any order:

<!-- snippet: examples/getting_started.cpp -->
```cpp
  // The binding's declaration wins over the class's own, for this binding only.
  auto injector = di::make_injector(
      di::bind<Config>.to_ref(cfg), di::bind<ILogger>.to<ConsoleLogger>().in(di::singleton),
      di::bind<ITransport>.to<SmtpTransport>(),
      di::bind<Mailer>.with_ctor<ILogger&, std::unique_ptr<ITransport>>().in(di::singleton));
  injector.create<Mailer&>().send("sent through SMTP");
```

### Aggregates

An aggregate, such as a struct with public members and no constructors, is built with braces. Each member is a request, in the order of declaration, a base class first, and a member's default initializer is overridden:

<!-- snippet: examples/getting_started.cpp -->
```cpp
// An aggregate: each member is a request, in order, as a constructor's parameters are.
struct OrderDeps {
  std::shared_ptr<IRepository> repository;
  Mailer& mailer;
  vendor::EventBus& events;
};

class OrderService {
 public:
  OrderService(OrderDeps deps, ILogger& log, std::unique_ptr<IClock> clock)
      : deps_(std::move(deps)), log_(log), clock_(std::move(clock)) {}
  // ...
};
```

- An aggregate with an array member, or with a member that a placeholder can't initialize, such as a `std::optional`, is [TRL-E004](errors/TRL-E004.md). Declare its constructor instead, with one parameter per array element.
- A class that derives from a base and declares no constructor is an aggregate too, and its base is requested by value. Give such a class a constructor, even `X() = default;`, to build it without arguments. On GCC, such an aggregate whose base can't be moved doesn't compile ([section 8](#8-known-limitations-in-v01)).
- An aggregate member of type `X&&` dangles: see [section 8](#8-known-limitations-in-v01).

### Limits

- **Arity.** A deduced class whose constructors all take more parameters than `max_ctor_arity` is [TRL-E010](errors/TRL-E010.md) when one takes at most 16 more, and [TRL-E002](errors/TRL-E002.md) beyond that. Raise the bound, up to 32, at some cost in compile time, or declare the constructor: declarations have no limit.
- **Constructor templates.** One that accepts any argument, such as `template <class T> C(T&&)` or a variadic one, is [TRL-E004](errors/TRL-E004.md), and so is a C variadic constructor, `C(...)`. One that accepts only some arguments may be deduced with a placeholder as its argument. Declare the constructor of a class whose constructors are templates, as for `vendor::EventBus` above.

[docs/internals/deduction.md](internals/deduction.md#what-each-limit-produces) lists every case and what deduction gives for it.

## 4. Bindings and lifetimes

`trellis::bind<I>.to<C>()` binds the key `I` to the class `C`, which is `I` or derives from it: every request of an `I` is served by a `C`. `bind<T>` alone binds `T` to itself. `.in(lifetime)` says how long the objects live:

- `trellis::unique`, the default: a new object for every request, which the consumer owns.
- `trellis::singleton`: one object per injector, held in a `std::shared_ptr` and built on its first request.
- `trellis::singleton_inline`: one object per injector, stored inside the injector without a heap allocation. It can't be requested as `std::shared_ptr` or `std::weak_ptr`.

Four bindings serve an object you give them, in place of `.to<C>()` and `.in()`:

- `.to_ref(x)` and `.to_ptr(p)`: the caller's object, which the injector refers to and never destroys. A const object is served only as `const T&` or `const T*`, or as a copy with `.allow_copy()`.
- `.to_shared(sp)`: an object that the caller shares through a `std::shared_ptr`, which consumers can then request as one too.
- `.to_value(v)`: a copy that the injector owns. Requests by value get copies of it.

A request by value of a shared object, a singleton or the caller's, is [TRL-E005](errors/TRL-E005.md), as a copy would hide the sharing; `.allow_copy()` permits the copy. A concrete class without a binding is unique, and an abstract one needs a binding ([TRL-E001](errors/TRL-E001.md)). Each key is bound once per injector ([TRL-E003](errors/TRL-E003.md)).

`trellis::make_module(...)` collects bindings, and other modules, into one module. A function that returns one keeps a part of the wiring together:

<!-- snippet: examples/getting_started.cpp -->
```cpp
// The production wiring, as a function that returns a module.
auto app_module(const Config& cfg) {
  return di::make_module(
      di::bind<Config>.to_ref(cfg),                                 // the caller's object
      di::bind<ILogger>.to<ConsoleLogger>().in(di::singleton),      // one per injector
      di::bind<IRepository>.to<SqlRepository>().in(di::singleton),  // one per injector
      di::bind<IClock>.to<SystemClock>(),                           // unique: one per request
      di::bind<Mailer>.in(di::singleton),                           // its constructor: the alias
      di::bind<vendor::EventBus>.in(di::singleton_inline));         // stored in the injector
}
```

`trellis::make_injector(...)` takes bindings, modules and the test overrides of [section 6](#6-testing), in any order, and flattens them into one injector. Its options are a template argument, written with designated initializers:

<!-- snippet: examples/getting_started.cpp -->
```cpp
// The injector, with an option: eager builds every singleton in make_injector.
auto make_app_injector(const Config& cfg) {
  return di::make_injector<di::options{.eager = true}>(app_module(cfg));
}
```

The fields of `trellis::options`, in the order designated initializers must follow:

| Field | Default | Effect |
|---|---|---|
| `strict` | `false` | Every type the injector creates needs a binding, concrete classes included ([TRL-E013](errors/TRL-E013.md)). |
| `eager` | `false` | `make_injector` builds every singleton, dependencies first. If a constructor throws, the singletons already built are destroyed and the exception propagates. Recommended for applications: a broken singleton fails at startup. |
| `threading` | `trellis::threading::safe` | `safe` builds each singleton once, even when threads request it at the same time. `none` uses a plain flag, for an injector that one thread uses at a time. |
| `max_ctor_arity` | `16` | The most parameters deduction looks for, at most 32. A higher bound costs compile time. |
| `cycle_check` | `false` | Accepted and ignored in v0.1; a later release checks for dependency cycles with it. A cycle doesn't compile either way. |

The bindings that serve an object you give them, and `.allow_copy()`:

<!-- snippet: examples/getting_started.cpp -->
```cpp
  auto bus = std::make_shared<vendor::EventBus>();
  auto injector = di::make_injector(
      di::bind<Config>.to_ref(cfg).allow_copy(),       // a Config by value is a copy of cfg
      di::bind<RetryPolicy>.to_value(RetryPolicy{5}),  // the injector owns a copy
      di::bind<vendor::EventBus>.to_shared(bus));      // the caller and the injector share it
  const Config copy = injector.create<Config>();
  const RetryPolicy policy = injector.create<RetryPolicy>();
  const std::shared_ptr<vendor::EventBus> shared =
      injector.create<std::shared_ptr<vendor::EventBus>>();
```

The injector owns its singletons and destroys them, in the reverse order of their construction, when it is destroyed. An object that holds a singleton by reference or pointer must not outlive the injector; one that holds it by `std::shared_ptr` may.

## 5. Request forms

A request, the `R` of `injector.create<R>()` or a constructor parameter, takes one of eight forms: `T`, `T&`, `const T&`, `T*`, `const T*`, `std::unique_ptr<T>`, `std::shared_ptr<T>` and `std::weak_ptr<T>`. With the module of [section 4](#4-bindings-and-lifetimes):

<!-- snippet: examples/getting_started.cpp -->
```cpp
  auto injector = di::make_injector(app_module(cfg));
  OrderService service = injector.create<OrderService>();          // a new object
  ILogger& logger = injector.create<ILogger&>();                   // the singleton
  const ILogger* same_logger = injector.create<const ILogger*>();  // the same singleton
  const Config& config = injector.create<const Config&>();         // the caller's object
  std::unique_ptr<IClock> clock = injector.create<std::unique_ptr<IClock>>();  // a new clock
  std::shared_ptr<IRepository> repository = injector.create<std::shared_ptr<IRepository>>();
  std::weak_ptr<IRepository> weak = injector.create<std::weak_ptr<IRepository>>();
  vendor::EventBus& events = injector.create<vendor::EventBus&>();
```

Each lifetime allows some forms, so that ownership is always clear; a form it rejects is [TRL-E005](errors/TRL-E005.md), whose page gives the reasons:

| Requested as | unique | singleton | singleton_inline | `to_ref`, `to_ptr` | `to_shared` | `to_value` | no binding |
|---|---|---|---|---|---|---|---|
| `T` | ✓ new object | `.allow_copy()` | `.allow_copy()` | `.allow_copy()` | `.allow_copy()` | ✓ copy | ✓ new object |
| `T&`, `const T&` | ✗ | ✓ | ✓ | ✓ | ✓ | ✓ | ✗ E012 |
| `T*`, `const T*` | ✗ | ✓ | ✓ | ✓ | ✓ | ✓ | ✗ E012 |
| `std::unique_ptr<T>` | ✓ | ✗ | ✗ | ✗ | ✗ | ✗ | ✓ |
| `std::shared_ptr<T>` | ✓ sole owner | ✓ shared | ✗ | ✗ | ✓ | ✗ | ✗ E012 |
| `std::weak_ptr<T>` | ✗ | ✓ | ✗ | ✗ | ✓ | ✗ | ✗ E012 |

This is the v0.1 part of the table in [plan §6.3](plan.md#63-the-request-form-compatibility-table), which also has the scoped lifetimes and the `lazy` and `provider` wrappers of later releases.

- **No binding:** a concrete class without one is unique, unless strict mode is on ([TRL-E013](errors/TRL-E013.md)), and an abstract class needs one ([TRL-E001](errors/TRL-E001.md)). A sharing form of an unbound class is [TRL-E012](errors/TRL-E012.md): nothing would own the object.
- **Constness carries through:** a const object bound with `.to_ref()`, `.to_ptr()` or `.to_shared()` is served only as a const object, such as `const T&` or, from `.to_shared()`, `std::shared_ptr<const T>`, and as copies with `.allow_copy()`.
- **A request by value** returns a new object or a copy as a prvalue, so even a class that can't be copied or moved can be created by value.
- **`std::unique_ptr<I>`** of a binding to a class derived from `I` needs `I`'s destructor to be virtual, or it is [TRL-E014](errors/TRL-E014.md).
- **Take `std::shared_ptr<T>` by value.** A parameter `const std::shared_ptr<T>&` asks for a `std::shared_ptr<T>` object, a key of its own, and is [TRL-E012](errors/TRL-E012.md) unless something binds it.

## 6. Testing

A test keeps the production wiring and replaces one binding. `trellis::override<K>` has `bind<K>`'s vocabulary. Passed to `make_injector` beside the production modules, it replaces the binding of `K` wherever it is among the other arguments, inside modules too, and keeps its place in the wiring. The plan §6.10 example, from [`tests/unit/testing_test.cpp`](../tests/unit/testing_test.cpp):

<!-- snippet: tests/unit/testing_test.cpp -->
```cpp
inline auto app_module(Config cfg) {
  return di::make_module(di::bind<Config>.to_value(std::move(cfg)),
                         di::bind<ILogger>.to<MemoryLogger>().in(di::singleton),
                         di::bind<IRepository>.to<SqlRepository>().in(di::singleton));
}
```

<!-- snippet: tests/unit/testing_test.cpp -->
```cpp
TEST(GMockExample, SavesThroughRepository) {
  testing::StrictMock<shop::MockRepository> repo;
  auto injector = di::make_injector(shop::app_module(shop::test_config()),
                                    di::override<shop::IRepository>.to_ref(repo));
  EXPECT_CALL(repo, save(42));
  injector.create<shop::Service>().handle(42);
}
```

- Each injector owns its singletons, so tests that make injectors from the same module share nothing.
- An override must suit every request of its key, as the binding it replaces did. `.to_ref(mock)` serves references and pointers; a consumer that takes a `std::shared_ptr<IRepository>` needs `.to_shared(mock)` instead, or the request is [TRL-E005](errors/TRL-E005.md).
- Two overrides of one key are [TRL-E003](errors/TRL-E003.md), and an override whose key no other argument binds is [TRL-E009](errors/TRL-E009.md), which catches a misspelt key or a module left out. Only `make_injector` takes overrides: `make_module` rejects them.

`<trellis/testing/wiring.hpp>`, which tests include themselves, as the umbrella header leaves it out, has two helpers. `trellis::testing::wireable<Injector, Root>` is a concept that is true exactly when `make_injector` makes an injector of that type and `create<Root>()` compiles for it. It never reports an error, so a test can assert on the wiring both ways:

<!-- snippet: tests/unit/testing_test.cpp -->
```cpp
using production_injector = decltype(di::make_injector(shop::app_module(shop::test_config())));

static_assert(di::testing::wireable<production_injector, shop::Service>);
```

It is false where `create<Root>()` would fail to compile, here for a reference to a unique object:

<!-- snippet: tests/unit/testing_test.cpp -->
```cpp
using unique_widget = decltype(di::make_injector(di::bind<widget>));

static_assert(di::testing::wireable<unique_widget, widget>);
static_assert(!di::testing::wireable<unique_widget, widget&>);  // TRL-E005
```

`trellis::testing::create_all(injector)` builds every singleton, in binding order and each after its dependencies, through `create<Key&>()`, and stops at the first constructor that throws: a smoke test of the whole wiring. Unique bindings build nothing until they are requested:

<!-- snippet: tests/unit/testing_test.cpp -->
```cpp
    auto injector = di::make_injector(di::bind<cache>.in(di::singleton), di::bind<request>,
                                      di::bind<mailer>.in(di::singleton_inline),
                                      di::bind<database>.in(di::singleton));
    EXPECT_TRUE(events().empty());
    di::testing::create_all(injector);
    // cache needs the database, so the database comes first; request is unique, so nothing
    // builds it until it is requested.
    EXPECT_EQ(events(), (std::vector<std::string>{"+database", "+cache", "+mailer"}));
```

## 7. When it doesn't compile

Trellis checks the whole graph at compile time. Apart from the cases that [section 8](#8-known-limitations-in-v01) lists, a graph that can't be built is one compile error per mistake, a `static_assert` whose first error line names a stable ID, `TRL-E0xx`. Each ID has a page in the [error catalog](errors/README.md) with the cause, an example and the fix:

| ID | Meaning |
|---|---|
| [TRL-E001](errors/TRL-E001.md) | No binding for an abstract type |
| [TRL-E002](errors/TRL-E002.md) | Type not constructible: no constructor found, a declaration that matches none, a copy that can't be made, or a class that is only declared |
| [TRL-E003](errors/TRL-E003.md) | A key bound twice, or two overrides of one key |
| [TRL-E004](errors/TRL-E004.md) | Ambiguous constructor: deduction can't tell what it needs |
| [TRL-E005](errors/TRL-E005.md) | Request form incompatible with the lifetime |
| [TRL-E009](errors/TRL-E009.md) | Override without an existing binding |
| [TRL-E010](errors/TRL-E010.md) | Constructor with more parameters than `max_ctor_arity` |
| [TRL-E012](errors/TRL-E012.md) | Unbound concrete type requested by reference, pointer or `shared_ptr` |
| [TRL-E013](errors/TRL-E013.md) | Unbound type requested in strict mode (provisional ID) |
| [TRL-E014](errors/TRL-E014.md) | `std::unique_ptr<I>` that would delete through a non-virtual destructor (provisional ID) |

For example, [`tests/compile_fail/explain/e001_abstract_below_declared.cpp`](../tests/compile_fail/explain/e001_abstract_below_declared.cpp) creates a `front`, which takes a `store&`; `store`'s declared constructor takes a `std::shared_ptr<repository>`, and the abstract `repository` has no binding. Clang 18 says, in the first lines of the test's golden output ([`tests/diagnostics_golden/clang-18/explain/`](../tests/diagnostics_golden/clang-18/explain/e001_abstract_below_declared.txt), where paths start at `<src>` and `L:C` stands for line and column numbers):

```text
<src>/include/trellis/diagnostics/errors.hpp:L:C: error: static assertion failed due to requirement 'trellis::error::E001<app::repository, trellis::error::request<std::shared_ptr<app::repository>>>::reported': TRL-E001: no binding for an abstract type: bind it to an implementation with trellis::bind<I>.to<C>()
<src>/include/trellis/diagnostics/explain.hpp:L:C: note: in instantiation of function template specialization 'trellis::detail::raise<trellis::error::E001<app::repository, trellis::error::request<std::shared_ptr<app::repository>>>, trellis::detail::error_message::no_binding, trellis::injector<options{false, false, 0, 16, false}, trellis::binding<app::store, app::store, trellis::singleton_t>>>' requested here
<src>/include/trellis/diagnostics/explain.hpp:L:C: note: in instantiation of member function 'trellis::detail::explain_cause<trellis::error::path<app::front, app::store>, 1, std::shared_ptr<app::repository>, trellis::injector<options{false, false, 0, 16, false}, trellis::binding<app::store, app::store, trellis::singleton_t>>>::run' requested here
```

- **The first line** has the ID, the message with the fix, and the error type, `E001<app::repository, request<std::shared_ptr<app::repository>>>`: the type without a binding, and the request as the consumer spelled it.
- **The path** is in the note of `explain_cause`, Clang's second note: `path<app::front, app::store>` lists the classes from the root of `create<T>()` down to the one whose parameter fails, and the number after it, `1`, is that parameter's position, counted from 1, followed by its request. GCC prints the same note before the error, as its first "required from" line: `[with Path = trellis::error::path<app::front, app::store>; long unsigned int Parameter = 1; R = std::shared_ptr<app::repository>; ...]`.
- **One error per cause.** A request that several roots or consumers fail on is reported once, and so are several forms of one missing binding under one root. The rest of the output is the compiler's context for that one error.
- **MSVC** prints the message without the condition, so the error type and the path are in its instantiation notes, the "see reference to ... being compiled" lines. [Section 8](#8-known-limitations-in-v01) lists how else its output differs.

Misuse of the binding vocabulary itself, such as `.to<C>()` with a class that doesn't derive from the key, is a `static_assert` whose message starts with `trellis:` and names the step; on MSVC, a key that `bind<I>` rejects can add errors after it. A dependency cycle gets the compiler's own errors in v0.1. To ask whether something wires without an error, use `testing::wireable` ([section 6](#6-testing)).

## 8. Known limitations in v0.1

- **An injector can't be moved or copied,** as the objects it creates may refer to it and to its singletons. `auto injector = make_injector(...)` works through C++17's guaranteed copy elision, and so does returning one from a function. `make_unique_injector` ([plan §7.5](plan.md#75-storage-lifetimes-and-teardown)) isn't in v0.1, but two forms keep an injector in a class or on the heap:

  <!-- snippet: examples/getting_started.cpp -->
  ```cpp
  // The injector can't be moved, so it is made where it lives: a member initialized from
  // make_injector, or a heap object that new auto(...) initializes.
  using app_injector = decltype(make_app_injector(std::declval<const Config&>()));

  class Application {
   public:
    explicit Application(const Config& cfg) : injector_(make_app_injector(cfg)) {}
    void run() { injector_.create<OrderService>().place(42); }

   private:
    app_injector injector_;
  };

  std::unique_ptr<app_injector> make_heap_injector(const Config& cfg) {
    return std::unique_ptr<app_injector>(new auto(make_app_injector(cfg)));
  }
  ```

- **An aggregate member of type `X&&` dangles.** Deduction takes it as the request `X` and binds it to a temporary that is destroyed once the aggregate is built, with GCC and Clang alike. Trellis doesn't reject such a member yet, so avoid rvalue-reference members in aggregates that Trellis builds.
- **On GCC, an aggregate whose base class can't be moved,** such as `struct derived : base {};`, passes the injector's compile-time checks, and then `create<T>()` fails with GCC's own error, "use of deleted function", without a TRL ID: GCC wants the base's move constructor to initialize the base from braces (CWG 2403). Clang builds such an aggregate. Give the class a constructor instead.
- **MSVC:**
  - cl stops a chain of nested aggregates, each holding the next by value, at 15 levels, with its C1054 limit ("initializers nested too deeply"), which plain C++ reaches too.
  - On cl 19.51, a chain of `std::unique_ptr` requests reaches 198 hops (C2999, the template depth), short of the 200 that NFR-09 asks for; the other request forms reach 246. cl 19.44 reaches 161–162 hops in the deduced forms, so it meets only the 150-hop bound that NFR-09 sets for MSVC before 19.50.
  - cl can't pass a class that can't be moved by value to a deduced constructor: it wants the move constructor that GCC and Clang elide (CWG 2327).
  - cl reports only the first Trellis error in a translation unit.
  - A key that `bind<I>` rejects, such as `const I`, can add errors on cl after its `static_assert`.
  - cl's output for one error can be long: 49 to 612 lines in the tests whose line budget it exceeds, where NFR-07 asks GCC and Clang for at most 40.
  - When two roots fail on one request, cl names the second root's path: `path<app::back, app::store>` in the example of [section 7](#7-when-it-doesnt-compile), where GCC and Clang name `path<app::front, app::store>`.
- **Apple Clang** compiles a chain of aggregates, each holding the next by value, up to 213 hops, close to the 200 that NFR-09 asks for ([plan §3.2](plan.md#32-non-functional-targets)): deeper, it warns that its stack is nearly exhausted (`-Wstack-exhausted`).
