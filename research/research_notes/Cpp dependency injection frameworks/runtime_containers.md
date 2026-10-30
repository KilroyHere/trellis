# Runtime-container alternatives to Boost.DI and Fruit: Hypodermic, Kangaru, dingo, inversify-cpp (state as of 2026-10-30)

Provenance: all four repositories exist at the URLs given in the brief; none has moved or been archived (no archive banner on any GitHub page, fetched 2026-10-30). Git metrics (commit dates, counts, authors, tags, branches) come from blobless clones made on 2026-10-30 (`git log`, `git shortlog`, `git for-each-ref`). Stars, forks and issue/PR counts come from the GitHub web pages as rendered by a fetch tool on 2026-10-30. "Past 12 months" = since 2025-10-30; "past 24 months" = since 2024-10-30. Source code was read as data only; nothing was built or run. Reddit, Stack Overflow, lists.isocpp.org, dou.ua, Repology and Homebrew could not be reached from this environment (see Gaps).

## Q1. Hypodermic: full profile, and how its Autofac-style runtime container works

### Takeaway
Hypodermic is a mature but effectively unmaintained (last commit 2023-03-03, last tag v2.5.3 on 2021-09-21) header-only C++11 runtime container. It is modelled on Autofac and **requires Boost headers**. Missing registrations, unresolvable dependencies and cycles are all **runtime exceptions**, everything is injected as `std::shared_ptr`, and a Dec-2025 report says it no longer compiles with Clang 20 in C++20 mode. Nobody has answered that report.

### Cited Findings

#### 1. Identity
- Repo: github.com/ybainier/Hypodermic. GitHub About text: "Hypodermic is an IoC container for C++. It provides dependency injection to your existing design." — [GitHub repo page, fetched 2026-10-30](https://github.com/ybainier/Hypodermic)
- Author/maintainer: Yohan Bainier. Under several e-mail identities he wrote about 239 of the 265 commits on `master`, including 95 under a `@abc-arbitrage.com` address. Other contributors are minor: Dmitry Ledentsov 5, n.fauvet 4, Paul Kunysch 3 (git shortlog, 2026-10-30) — [commit history](https://github.com/ybainier/Hypodermic/commits/master)
- License: MIT ("Copyright (c) 2016 Hypodermic Project") — [LICENSE](https://github.com/ybainier/Hypodermic/blob/master/LICENSE)
- First commit: 2011-09-04 (".hgignore initial import", so the history was imported from Mercurial). First git tag v1.0: 2016-01-13. README: "Used in production environments since 2012." — [commits](https://github.com/ybainier/Hypodermic/commits/master); [tags](https://github.com/ybainier/Hypodermic/tags); [README](https://github.com/ybainier/Hypodermic/blob/master/README.md)
- The README says it "was started with the will to mimic the famous .NET Autofac". It credits its constructor deduction to Krzysztof Jusiak's work on Boost.DI — [README](https://github.com/ybainier/Hypodermic/blob/master/README.md)

#### 2. Maintenance (as of 2026-10-30)
- Latest tag: v2.5.3 (2021-09-21). Earlier tags: v2.5.2 (2019-10-15), v2.5.1 (2018-12-27), v2.5 (2018-11-15), v2.4 (2017-05-10), v2.3/v2.2 (2017-04-28), v2.1 (2016-09-23), v1.0 (2016-01-13). The rendered GitHub sidebar showed no Releases entry — [tags](https://github.com/ybainier/Hypodermic/tags); [repo page](https://github.com/ybainier/Hypodermic)
- Latest commit on `master`: 2023-03-03, "Add missing IMutableRegistrationScope.h to visual studio project" — [commit ba5516d](https://github.com/ybainier/Hypodermic/commit/ba5516d4e78230c1187debb01ebdf01e5d11f62d)
- Commit activity: **0 commits in the past 12 months and 0 in the past 24 months**. Commits per year: 2016: 108 (the v2 rewrite), 2017: 17, 2018: 16, 2019: 5, 2020: 0, 2021: 2, 2022: 2, 2023: 2 (git log, 2026-10-30) — [commits](https://github.com/ybainier/Hypodermic/commits/master)
- Bus factor: 1 (see Identity).
- Open issues 7, open PRs 4, 603 stars, 80 forks, 32 watchers (GitHub page, 2026-10-30) — [repo page](https://github.com/ybainier/Hypodermic)
- Open issues (2026-10-30): #62 "Clang compile error" (opened 2025-12-12, 0 comments); #61 autowiring constructors with dependencies not available at compile time (2024-12-09); #60 "Single instance objects holding nested containers are not deleted" (2024-11-02); #58 "Alternatives to std::make_shared" (2023-10-17); #57 "Is there a version without boost" (2023-10-10, 5 comments); #56 "Hypodermic 2.5.3 won't link with Boost" (2023-08-31); #43 "Autowiring of factory methods" (2020-01-16). The most recent batch of maintainer closures was on 2023-06-01 (#42, #49, #52) — [issues list](https://github.com/ybainier/Hypodermic/issues?q=is%3Aissue)
- Open, unmerged PRs: #53 "Added Conan Packaging" (2023-03-10), #54 "Fix issue resolving const dependencies" (2023-06-01), #55 "Resolve const" (2023-06-11), #59 "Introduce asGroup()" (2024-09-17). 15 PRs are closed — [pull requests](https://github.com/ybainier/Hypodermic/pulls)
- No maintenance-mode, deprecation or archival statement was found in the README or on the repo page — [README](https://github.com/ybainier/Hypodermic/blob/master/README.md)
- CI is obsolete. The README badges point to travis-ci.org and AppVeyor. `.travis.yml` targets Ubuntu Trusty with g++-5 and clang-3.8 (plus OS X) and installs `libboost-all-dev`. `appveyor.yml` uses the Visual Studio 2017 image with Boost 1.67 — [.travis.yml](https://github.com/ybainier/Hypodermic/blob/master/.travis.yml); [appveyor.yml](https://github.com/ybainier/Hypodermic/blob/master/appveyor.yml)

#### 3. Adoption
- 603 stars and 80 forks (2026-10-30) — [repo page](https://github.com/ybainier/Hypodermic)
- vcpkg: the `hypodermic` port has `"version-date": "2023-03-03"`, a master snapshot rather than a tag. Earlier port versions were 2.5.2 and 2.5.3. It depends on boost-algorithm, boost-config, boost-format, boost-range, boost-signals2, boost-system and boost-test (vcpkg master, 2026-10-30) — [vcpkg port](https://github.com/microsoft/vcpkg/blob/master/ports/hypodermic/vcpkg.json); [vcpkg versions](https://github.com/microsoft/vcpkg/blob/master/versions/h-/hypodermic.json)
- Conan Center: **no recipe**; `recipes/hypodermic/config.yml` returns 404 on 2026-10-30. The community PR that would add Conan packaging (#53) has been open since 2023 — [conan-center-index recipes](https://github.com/conan-io/conan-center-index/tree/master/recipes); [PR list](https://github.com/ybainier/Hypodermic/pulls)
- Linux distros: a Salix OS 15.0 "extra" package "Hypodermic-2.4" appeared in search results. I saw only the search hit and did not open the page — [Salix package listing](https://backup.salixos.org/daily.5/repo/i486/extra-15.0/salix/libraries/Hypodermic-2.4-i586-1salix15.0.txt). Repology and Homebrew could not be checked (proxy 403).
- A third-party NuGet package `hypodermic_vc120` exists (found via search; not maintained by the author as far as visible) — [nubrowse listing](https://nubrowse.com/packages/hypodermic_vc120)
- Verified production users: **none found**. The only evidence is the author's claim "Used in production environments since 2012" — [README](https://github.com/ybainier/Hypodermic/blob/master/README.md)

#### 4. Technical model
- **Runtime wiring, Autofac style.** You configure a `ContainerBuilder` (`registerType<T>()`, `registerInstance(shared_ptr)`, `registerInstanceFactory(callable)`) and then call `build()`, which returns `std::shared_ptr<Container>`. The container exposes `resolve<T>()` (returns `std::shared_ptr<T>`), `resolveAll<T>()` (returns `std::vector<std::shared_ptr<T>>`) and `resolveNamed<T>(name)` — [ContainerBuilder.h](https://github.com/ybainier/Hypodermic/blob/master/Hypodermic/ContainerBuilder.h); [Container.h](https://github.com/ybainier/Hypodermic/blob/master/Hypodermic/Container.h); [wiki: Resolving components](https://github.com/ybainier/Hypodermic/wiki/Resolving-components)
- **Binding example** (from the wiki):
  ```cpp
  Hypodermic::ContainerBuilder builder;
  builder.registerType<LengthPrefixedMessageSerializer>().as<IMessageSerializer>();
  builder.registerType<ConsoleMessageWriter>().as<IMessageWriter>().singleInstance();
  builder.registerInstance(std::make_shared<DispatcherConfiguration>());       // implicitly single
  builder.registerType<PersistentTransport>().with<ITransport, SocketTransport>().as<ITransport>();
  auto container = builder.build();                                           // std::shared_ptr<Container>
  auto writer = container->resolve<IMessageWriter>();                         // std::shared_ptr<IMessageWriter>
  auto nested = childBuilder.buildNestedContainerFrom(*container);            // child container
  ```
  — [wiki: Getting started](https://github.com/ybainier/Hypodermic/wiki/Getting-started); [wiki: Controlling lifetime](https://github.com/ybainier/Hypodermic/wiki/Controlling-lifetime); [wiki: Explicit dependencies](https://github.com/ybainier/Hypodermic/wiki/Explicit-dependencies)
- **Constructor autowiring.** A compile-time constructor-arity deducer (`ConstructorTypologyDeducer`) probes for constructors with up to `HYPODERMIC_CONSTRUCTOR_ARGUMENT_COUNT` arguments. The default is 20, and `HYPODERMIC_OVERRIDE_CONSTRUCTOR_ARGUMENT_COUNT` changes it — [Config.h](https://github.com/ybainier/Hypodermic/blob/master/Hypodermic/Config.h); [ConstructorTypologyDeducer.h](https://github.com/ybainier/Hypodermic/blob/master/Hypodermic/ConstructorTypologyDeducer.h)
- Injectable constructor parameter forms:
  - `std::shared_ptr<T>`
  - `std::vector<std::shared_ptr<T>>` (all registrations of T)
  - `std::function<std::shared_ptr<T>()>` (a factory, built as a lambda that holds a weak pointer to the container; "a way to avoid circular dependencies")
  - `std::shared_ptr<Container>` (the container itself)

  — [wiki: Expressing dependencies](https://github.com/ybainier/Hypodermic/wiki/Expressing-dependencies)
- **shared_ptr only, by design.** The wiki page "Regarding the shared pointers" explains why `unique_ptr` and reference injection are not offered. An issue asking for unique_ptr resolution (#36) was closed — [wiki](https://github.com/ybainier/Hypodermic/wiki/Regarding-the-shared-pointers); [issues](https://github.com/ybainier/Hypodermic/issues?q=is%3Aissue)
- **Lifetimes:** transient by default, `singleInstance()`, and provided instances (always single). There are no per-request or per-thread scopes beyond nested containers — [wiki: Controlling lifetime](https://github.com/ybainier/Hypodermic/wiki/Controlling-lifetime)
- **Interface bindings:**
  - `as<I>()`, with a compile-time check `static_assert(... "TBase should be a base of T")`
  - `asSelf()`
  - several `as<>` calls on one registration

  — [wiki: Registering components](https://github.com/ybainier/Hypodermic/wiki/Registering-components); [source static_asserts](https://github.com/ybainier/Hypodermic/tree/master/Hypodermic)
- **Named bindings:** `named<I>("LeDispatcher")` and `resolveNamed<T>("...")` — [wiki: Registering components](https://github.com/ybainier/Hypodermic/wiki/Registering-components)
- **Multibindings:** `resolveAll<T>()` and injection of `std::vector<std::shared_ptr<T>>` — [wiki: Expressing dependencies](https://github.com/ybainier/Hypodermic/wiki/Expressing-dependencies)
- **Factories and explicit dependencies:**
  - `registerInstanceFactory([](ComponentContext&){...})`, which must return a `std::shared_ptr`
  - `with<TDependency, TImpl>()` or `with<TDependency>(instance)` to pin a dependency for one registration

  Assisted injection (runtime arguments plus injected ones) is not supported directly; issue #43, "Autowiring of factory methods", is open — [wiki: Registering components](https://github.com/ybainier/Hypodermic/wiki/Registering-components); [wiki: Explicit dependencies](https://github.com/ybainier/Hypodermic/wiki/Explicit-dependencies); [issues](https://github.com/ybainier/Hypodermic/issues?q=is%3Aissue)
- **Fallbacks and overrides:** the last registration wins, and `useIfNone()` marks a default that applies only if nothing else is registered — [wiki: Fallback registrations](https://github.com/ybainier/Hypodermic/wiki/Fallback-registrations)
- **Modules and composition:**
  - one `ContainerBuilder` subclass per module
  - `addRegistrations(otherBuilder)` merges builders
  - `buildNestedContainerFrom(*parent)` builds child containers whose registrations resolve only inside the child but can feed dependencies to parent registrations
  - `updateContainer(container)` adds registrations to an already-built container

  — [wiki: Merging containers](https://github.com/ybainier/Hypodermic/wiki/Merging-containers); [wiki: Controlling lifetime](https://github.com/ybainier/Hypodermic/wiki/Controlling-lifetime); [ContainerBuilder.h](https://github.com/ybainier/Hypodermic/blob/master/Hypodermic/ContainerBuilder.h)
- **Runtime ("on-the-fly") registration:** by default, *unregistered* concrete types are resolved if they are default constructible or their dependencies are resolvable. `Behavior::configureRuntimeRegistration(false)` makes that case throw instead — [wiki: Resolution behavior configuration](https://github.com/ybainier/Hypodermic/wiki/Resolution-behavior-configuration)
- Activation hooks (`onActivated`) and pluggable logging (`ILoggerSink`, `ConsoleLoggerSink`) are available — [wiki: Activation hooks](https://github.com/ybainier/Hypodermic/wiki/Activation-hooks); [wiki: Logging](https://github.com/ybainier/Hypodermic/wiki/Logging)
- **Thread safety:** `RegistrationScope`, `NestedRegistrationScope` and `ResolutionContainer` guard their state with `std::recursive_mutex` / `std::lock_guard`. No documentation statement on thread safety was found — [RegistrationScope.h](https://github.com/ybainier/Hypodermic/blob/master/Hypodermic/RegistrationScope.h); [ResolutionContainer.h](https://github.com/ybainier/Hypodermic/blob/master/Hypodermic/ResolutionContainer.h)
- **RTTI is required.** Type keys use `typeid` and `std::type_index` — [TypeInfo.h](https://github.com/ybainier/Hypodermic/blob/master/Hypodermic/TypeInfo.h)
- **Exceptions are required.** The library throws a hierarchy derived from `HypodermicException`: `ResolutionException`, `CircularDependencyException`, `ActivationException`, `DependencyActivationException`, `InstanceAlreadyActivatingException`, `RegistrationException` — [ExceptionBase.h](https://github.com/ybainier/Hypodermic/blob/master/Hypodermic/ExceptionBase.h)
- **Header-only, but depends on Boost.** Core headers include:
  - `boost/signals2.hpp` (IRegistrationDescriptor.h, RegistrationActivator.h)
  - `boost/range/adaptor/reversed.hpp` (ComponentContext.h)
  - `boost/algorithm/string.hpp` (TypeInfo.h)
  - `boost/range/sub_range.hpp`

  The tests use Boost.Test — [IRegistrationDescriptor.h](https://github.com/ybainier/Hypodermic/blob/master/Hypodermic/IRegistrationDescriptor.h); [ComponentContext.h](https://github.com/ybainier/Hypodermic/blob/master/Hypodermic/ComponentContext.h); [TypeInfo.h](https://github.com/ybainier/Hypodermic/blob/master/Hypodermic/TypeInfo.h)
- **Minimum standard:** C++11. CMake hard-codes `-std=c++11` for GCC (≥ 4.8) and Clang — [CMakeLists.txt](https://github.com/ybainier/Hypodermic/blob/master/CMakeLists.txt)
- **Compilers tested in CI:** g++-5, clang-3.8 and OS X on Travis; VS2017 on AppVeyor. All of these date from 2017–2019 — [.travis.yml](https://github.com/ybainier/Hypodermic/blob/master/.travis.yml); [appveyor.yml](https://github.com/ybainier/Hypodermic/blob/master/appveyor.yml)

#### 5. Diagnostics
- **Compile-time `static_assert` messages** include:
  - "TBase should be a base of T"
  - "T should be a complete type"
  - "Could not autowire T: you should consider registering T either by providing an instance or an instance factory"
  - "TDependency is already registered for instance T"
  - "TCallable should return a std:shared_ptr"

  — [Hypodermic headers](https://github.com/ybainier/Hypodermic/tree/master/Hypodermic)
- **Missing registration is a runtime error.** Resolving an unregistered abstraction throws `ResolutionException` ("Unable to resolve ..."). An unregistered *concrete* type is silently auto-registered unless runtime registration is turned off — [ComponentContext.h](https://github.com/ybainier/Hypodermic/blob/master/Hypodermic/ComponentContext.h); [wiki: Resolution behavior configuration](https://github.com/ybainier/Hypodermic/wiki/Resolution-behavior-configuration)
- **Cycles are a runtime error:** `CircularDependencyException` ("Circular dependency detected while activating ...") — [ResolutionContainer.h](https://github.com/ybainier/Hypodermic/blob/master/Hypodermic/ResolutionContainer.h)
- **Ambiguity:** the last registration wins silently (with `useIfNone` for defaults). Since 2019 a duplicated base-type registration logs a warning "instead of failing to compile" — [wiki: Fallback registrations](https://github.com/ybainier/Hypodermic/wiki/Fallback-registrations); [commit history 2019-10-15](https://github.com/ybainier/Hypodermic/commits/master)
- **Mitigation:** `ContainerBuilder::validate()` builds a container per registration and resolves each one, throwing on the first failure. The wiki recommends calling it in a unit test — [wiki: Validation](https://github.com/ybainier/Hypodermic/wiki/Validation)

#### 6. Performance
- **Author-run benchmark** (wiki page created 2017-05-05). Setup: Windows 10, Intel Core i7-4790 @ 3.60 GHz, 32 GB RAM, 100,000 iterations after 10,000 warm-up cycles; source in `Hypodermic.Tests/PerformanceTests.cpp`. Average times:

  | Case | `std::make_shared` | Registered transient | Registered single instance | Unregistered (on-the-fly) transient |
  |---|---|---|---|---|
  | Default-constructible type | 0.07 µs | 0.67 µs | 0.51 µs | 0.68 µs |
  | Type with one dependency | ≈0.1 µs | ≈1 µs | ≈1 µs (any variant) | ≈1 µs |

  — [wiki: Benchmarks](https://github.com/ybainier/Hypodermic/wiki/Benchmarks); [wiki history](https://github.com/ybainier/Hypodermic/wiki/Benchmarks/_history)
- No independent benchmark of Hypodermic was found. Compile-time cost and binary size: unknown.

#### 7. Documentation and learning curve
- The documentation is a GitHub wiki of 16 pages (Getting started, Registering components, Controlling lifetime, Validation, Fallbacks, Merging containers, Logging, Benchmarks and others). The last substantive edits were on 2018-10-24; the only later edits are typo fixes by third parties in 2021 and 2026-02-04 (wiki git log) — [wiki home](https://github.com/ybainier/Hypodermic/wiki)

#### 8. Criticisms, pitfalls, bugs
- **Clang 20 breakage:** issue #62 reports a compile error with Clang 20.1.8 in C++20 mode (`ConstructorDescriptor.h:38`, "a template argument list is expected after a name prefixed by the template keyword" [-Wmissing-template-arg-list-after-template-kw]). The reporter says VS 2026 and GCC 15.1 pass and supplies a local workaround. No maintainer reply (2026-10-30) — [issue #62](https://github.com/ybainier/Hypodermic/issues/62)
- **Boost friction:** #57 "Is there a version without boost" (open), #56 "won't link with Boost" (open), and the historical #40 "multiple definition of boost::*" and #45 "Fail to build in window boost1.7 gcc9" — [issues](https://github.com/ybainier/Hypodermic/issues?q=is%3Aissue)
- **Nested-container lifetime bugs:**
  - #46 (interface resolved twice from a nested container), closed 2021
  - #47 (singleInstance from nested containers), closed 2022; fix commit "Fix nest container single instance resolution" on 2022-11-30
  - #60 (single instances holding nested containers are never deleted, i.e. a leak), still open

  — [issues](https://github.com/ybainier/Hypodermic/issues?q=is%3Aissue); [commits](https://github.com/ybainier/Hypodermic/commits/master)
- **Const dependencies:** resolving `const` dependencies is broken, and the fixes (PRs #54, #55) have sat unmerged since June 2023 — [pull requests](https://github.com/ybainier/Hypodermic/pulls)
- **Qualitative opinion:** a Russian Habr article called Hypodermic solid, with pleasant, readable code. I saw only a search-engine summary; it is not first-hand verified — [Habr post](https://habr.com/ru/post/722940)

#### 9. Fit
- Hypodermic suits existing C++11/14 codebases that already depend on Boost and want an Autofac-like, fully runtime, shared_ptr-everywhere container with nested containers, named and fallback registrations, and resolve-all — [README](https://github.com/ybainier/Hypodermic/blob/master/README.md); [wiki](https://github.com/ybainier/Hypodermic/wiki)
- Red flags:
  - no commits since 2023-03-03
  - unanswered Clang 20 breakage
  - hard Boost dependency
  - no Conan recipe
  - CI that last ran on 2017-era toolchains

  — [commits](https://github.com/ybainier/Hypodermic/commits/master); [issue #62](https://github.com/ybainier/Hypodermic/issues/62)

### Inferences
- The `@abc-arbitrage.com` commit e-mail suggests the library was developed at least partly inside ABC Arbitrage (a trading firm). That would fit the README's "production since 2012" claim, but it is not verification of current production use.
- Hypodermic needs the Boost headers in the include path, not compiled Boost libraries. Signals2, Range and Algorithm are header-only Boost libraries (general knowledge; not re-verified this session). The vcpkg port still pulls boost-system and boost-test, apparently because of the test build.
- On-the-fly registration of concrete types is on by default. A forgotten registration therefore silently produces a fresh transient instead of an error, a classic runtime-DI pitfall. Production users should call `Behavior::configureRuntimeRegistration(false)` and `validate()` in tests.
- travis-ci.org stopped building open-source projects around 2021 (general knowledge; not verified here). In practice the project has had no working CI for years, which is consistent with the Clang 20 breakage going unnoticed.

### Gaps
- Number of real-world dependents: GitHub's "Used by" did not render, and code search through the API is blocked.
- Compile-time cost and binary size: no measurements found.
- Whether the Boost requirement can be removed easily: no maintainer response (#57).
- Hacker News, r/cpp and Stack Overflow discussions could not be searched (domains blocked for the search tool).

## Q2. Kangaru: full profile, service definitions, compile-time vs runtime checks, and v4 vs v5

### Takeaway
Kangaru v4.3.2 (2024-04-29) is a header-only, dependency-free C++11 container. You describe services as types (`kgr::service`, `kgr::single_service`, ...); dependency graphs are checked at compile time, and instances are created lazily at runtime. It is packaged in both vcpkg and Conan Center. The single maintainer is very active, but almost all of that work goes into an **unreleased C++20 rewrite (v5) on branch `dev-5.x.y`**: 249 commits in the past 12 months, with a v4-compatibility layer in progress as of October 2026. Meanwhile v4 on `master` gets only occasional fixes (5 commits in 12 months, none released).

### Cited Findings

#### 1. Identity
- Repo: github.com/gracicot/kangaru. About: "🦘 A dependency injection container for C++11, C++14 and later". Topics: c-plus-plus, dependency-injection, injection, inversion-of-control, ioc, ioc-container — [repo page, fetched 2026-10-30](https://github.com/gracicot/kangaru)
- Author/maintainer: Guillaume Racicot, about 1,448 of 1,513 commits on `master` under several e-mails. Other contributors: Denis Novikov (about 45) and Frederic Bourdeau (11) (git shortlog, 2026-10-30) — [commits](https://github.com/gracicot/kangaru/commits/master)
- License: MIT ("Copyright (c) 2017 Guillaume Racicot") — [LICENSE](https://github.com/gracicot/kangaru/blob/master/LICENSE)
- First commit 2015-03-27; first tag v0.1 on 2015-04-15; 49 tags in total — [tags](https://github.com/gracicot/kangaru/tags)

#### 2. Maintenance (as of 2026-10-30)
- Latest tags: **v4.3.2 and v4.2.6, both 2024-04-29**. Before that: v4.3.1 (2022-08-15), v4.3.0 (2022-02-13) — [tags](https://github.com/gracicot/kangaru/tags)
- `master` (v4):
  - Latest commit 2026-07-13, "Update catch 2". The same day brought "Fix autowire_service alias (#143)" by contributor aouxwoux plus CI fixes ("Fix CI on master", "Fix tests running by updating vcpkg")
  - 5 commits in the past 12 months, 9 in the past 24 months
  - Commits per year: 2020: 23, 2021: 20, 2022: 22, 2023: 11, 2024: 11, 2025: 1, 2026: 5

  — [master commits](https://github.com/gracicot/kangaru/commits/master); [commit 183580d](https://github.com/gracicot/kangaru/commit/183580dc27b86da190c4a347f9dadeaa7d0ec501)
- `dev-5.x.y` (the v5 rewrite):
  - Latest commit 2026-09-17, "Add Clang 23 in CI"
  - **249 commits in the past 12 months and 324 in the past 24 months**, 323 of the 324 by Racicot (the other is the lgtm-com bot)
  - Commits per year: 2023: 57, 2024: 102, 2025: 113, 2026: 194
  - It branched from `master` at 2024-11-14

  — [dev-5.x.y commits](https://github.com/gracicot/kangaru/commits/dev-5.x.y)
- Side branches:
  - `compatibility-v4-wip`: last commit 2026-10-18, "Activate invoke tests". Recent messages include "Autowire is functional" and "Example 1 to 6 (except 4) is restored and use the compat layer"
  - `reflection-experiments`: 2026-05-26, "First reflection attempt"

  — [compatibility-v4-wip](https://github.com/gracicot/kangaru/commits/compatibility-v4-wip); [reflection-experiments](https://github.com/gracicot/kangaru/commits/reflection-experiments)
- Issues: 108 in total, 9 open (2026-10-30) — [issues](https://github.com/gracicot/kangaru/issues?q=is%3Aissue). Open roadmap issues:
  - #136 "Create a kangaru 4 compatibility layer" (opened 2025-06-20)
  - #137 "Use C++26 reflection" (2025-07-08)
  - #124 "Support C++20 modules" (2024-05-07, 8 comments)
  - #134 "Forward declarations management" (2025-05-14)
  - #119 MSVC C4702 warning (2024-02-10)
- Maintainer responsiveness is good:
  - #140 "Clang 22 builds are broken", #141 "MSVC builds are broken" and #142 "Support Clang 16, 17, 18" were closed 2026-06-18
  - #139 "MSVC vs 2026 is broken" was closed 2026-03-01
  - user bug #126 "Wrong single_service when it should be service" was closed 2026-07-13, the same day as PR #143 landed on master
  - user question #138 was closed 2025-07-21

  — [issues](https://github.com/gracicot/kangaru/issues?q=is%3Aissue)
- Bus factor 1 (see Identity). No deprecation or archival notice. The README still promises: "Please submit an issue and we will respond within a few days" — [README](https://github.com/gracicot/kangaru/blob/master/README.md)
- Maintainer statement on the rewrite: a March-2024 post by the maintainer to the ISO C++ SG7 (reflection) list, replying to the reflection-based DI paper P3096R0, said he was "in the process of completely rewriting it in C++20 with simple and composable primitives". I could not open the page (host unreachable); this is from a search-engine summary only — [SG7 post 2024-03-17](https://lists.isocpp.org/sg7/2024/03/0496.php)

#### 3. Adoption
- 554 stars, 41 forks, 23 watchers (2026-10-30) — [repo page](https://github.com/gracicot/kangaru)
- vcpkg: `kangaru` 4.3.2, header-only, with an optional `hashtypeid` feature; earlier port versions 4.3.1, 4.3.0, 4.2.4 and 4.2.2 — [vcpkg port](https://github.com/microsoft/vcpkg/blob/master/ports/kangaru/vcpkg.json); [versions](https://github.com/microsoft/vcpkg/blob/master/versions/k-/kangaru.json)
- Conan Center: `kangaru` 4.3.2, 4.3.1, 4.3.0, 4.2.4 — [conan-center-index config.yml](https://github.com/conan-io/conan-center-index/blob/master/recipes/kangaru/config.yml)
- Linux distros and Homebrew: not verified (Repology and Homebrew unreachable).
- Users: the README's "Who's Using Kangaru" lists only "Our team's game engine", "The people I helped integrating this library into their projects" and "Surely more!". **No verified named production users** — [README](https://github.com/gracicot/kangaru/blob/master/README.md)
- Third-party write-ups: a DOU.ua article (Russian) compares kangaru and [Boost].DI through a sample banking application. I saw only the title and search snippet; the host was unreachable — [DOU.ua](https://dou.ua/lenta/articles/frameworks-for-dependencies/)

#### 4. Technical model (v4.3.x, the released line)
- **Service definitions are types.** The graph is declared in code:
  ```cpp
  struct Camera {};
  struct Scene { Camera& camera; };
  struct CameraService : kgr::single_service<Camera> {};                        // shared instance, injected by reference
  struct SceneService  : kgr::service<Scene, kgr::dependency<CameraService>> {}; // new value each time
  kgr::container container;
  Scene scene = container.service<SceneService>();
  Camera& camera = container.service<CameraService>();   // same instance as scene.camera
  ```
  — [README](https://github.com/gracicot/kangaru/blob/master/README.md)
- **Autowiring and mapped services.** A type opts in with a friend `service_map` declaration (for example `friend auto service_map(Camera const&) -> kgr::autowire_single;`). The container then deduces constructor parameters "or aggregate initialization". `container.invoke(lambda)` injects the lambda's parameters through the service map — [README](https://github.com/gracicot/kangaru/blob/master/README.md); [doc: autowire](https://github.com/gracicot/kangaru/blob/master/doc/section06_autowire.md); [doc: mapping](https://github.com/gracicot/kangaru/blob/master/doc/section10_mapping.md)
- **Polymorphism and interface binding:**
  - `kgr::polymorphic` on the base service
  - `kgr::overrides<BaseService>` on implementations
  - `kgr::final`
  - `kgr::abstract_service<T>`
  - `kgr::defaults_to<DefaultService>`

  — [doc: polymorphic services](https://github.com/gracicot/kangaru/blob/master/doc/section03_polymorphic.md)
- **Supplied services** (instances provided externally) must be emplaced before use — [doc: supplied services](https://github.com/gracicot/kangaru/blob/master/doc/section05_supplied.md)
- **Lifetimes:** `service` (new value per request), `single_service` (one shared instance per container, returned by reference), plus abstract and supplied variants — [README](https://github.com/gracicot/kangaru/blob/master/README.md); [doc](https://github.com/gracicot/kangaru/tree/master/doc)
- **Operator services:**
  - `kgr::container_service` injects the container
  - `fork_service` injects a fork
  - `generator_service<S>` is a factory
  - `invoker_service` / `mapped_invoker_service<Map>` / `forked_invoker`
  - `lazy_service<S>`

  — [doc: operator services](https://github.com/gracicot/kangaru/blob/master/doc/section08_operator.md)
- **Containers and composition:** `container.fork()` (child container), `merge()` and `rebase()`. Generic services and project structuring are covered in later doc sections — [doc: managing containers](https://github.com/gracicot/kangaru/blob/master/doc/section04_container.md); [doc: structuring projects](https://github.com/gracicot/kangaru/blob/master/doc/section13_structure.md)
- **Other injection modes:** setter injection ("autocall") and function-parameter injection — [README features](https://github.com/gracicot/kangaru/blob/master/README.md); [doc: autocall](https://github.com/gracicot/kangaru/blob/master/doc/section07_autocall.md)
- **Runtime storage:** single-service instances live in a `std::unordered_map` keyed by a type id. With `KANGARU_HASH_TYPE_ID` the id is an FNV-1a hash of the type name; this is off by default in CMake, and the README says to leave it off for older compilers — [container.hpp](https://github.com/gracicot/kangaru/blob/master/include/kangaru/container.hpp); [type_id.hpp](https://github.com/gracicot/kangaru/blob/master/include/kangaru/type_id.hpp); [CMakeLists.txt](https://github.com/gracicot/kangaru/blob/master/CMakeLists.txt)
- **Exceptions are optional.** `KANGARU_NO_EXCEPTION` "replac[es] throws by asserts", and exceptions are also disabled automatically when the compiler has them off. No `typeid` use was found in the v4 headers, so RTTI is not needed — [CMakeLists.txt](https://github.com/gracicot/kangaru/blob/master/CMakeLists.txt); [define.hpp](https://github.com/gracicot/kangaru/blob/master/include/kangaru/detail/define.hpp)
- **Thread safety:** no mutexes or atomics in the v4 headers, and no mention of threads in the docs — [include/](https://github.com/gracicot/kangaru/tree/master/include/kangaru); [doc/](https://github.com/gracicot/kangaru/tree/master/doc)
- **Packaging and toolchain:**
  - header-only, with no third-party runtime dependencies (Catch2 for tests only)
  - minimum standard C++11 (`target_compile_features(kangaru INTERFACE cxx_std_11)`)
  - README minimum compilers: MSVC 2015 Update 3, GCC 4.8.5, Clang 3.6, AppleClang 7.0
  - master CI on GitHub Actions: AppleClang on macos-15, Clang and GCC on ubuntu-22.04, MSVC on windows-2022, with C++14 and C++17 test modes

  — [CMakeLists.txt](https://github.com/gracicot/kangaru/blob/master/CMakeLists.txt); [README](https://github.com/gracicot/kangaru/blob/master/README.md); [all-os.yml](https://github.com/gracicot/kangaru/blob/master/.github/workflows/all-os.yml)

#### 4b. Technical model (v5, unreleased branch `dev-5.x.y`)
- **Build requirements:**
  - `project(kangaru VERSION 5.0.0)`, CMake ≥ 3.31
  - **C++20 required** (`cxx_std_20`)
  - an optional C++20 module target (`src/kangaru.cpp`), enabled for Clang ≥ 19, GCC ≥ 16 and MSVC ≥ 19.42 (not AppleClang)
  - depends on `tl-optional` unless `KANGARU_FALLBACK_TO_EMBEDDED_OPTIONAL` is set

  — [dev-5 CMakeLists.txt](https://github.com/gracicot/kangaru/blob/dev-5.x.y/CMakeLists.txt)
- **New primitives:** sources, injectors, a deducer, polymorphic/modular/recursive/incremental sources, `lazy`, `two_step_init`, allocator support and attributes. Example from the tests: `friend auto attribute(kangaru::allow_runtime_caching<service_a&>) -> std::true_type;`. The exception type is `not_found_exception` — [dev-5 kangaru.hpp](https://github.com/gracicot/kangaru/blob/dev-5.x.y/include/kangaru/kangaru.hpp); [5-container.cpp test](https://github.com/gracicot/kangaru/blob/dev-5.x.y/tests/src/5-container.cpp); [exceptions.hpp](https://github.com/gracicot/kangaru/blob/dev-5.x.y/include/kangaru/detail/exceptions.hpp)
- **The v4 API is kept in the same branch** under `include/kangaru-prev/`. The dev-5 README examples now `#include <kangaru-prev/kangaru.hpp>` — [dev-5 README](https://github.com/gracicot/kangaru/blob/dev-5.x.y/README.md)
- A commit titled "RTTI is optional" landed on 2026-08-31 — [dev-5 commits](https://github.com/gracicot/kangaru/commits/dev-5.x.y)
- **v5 CI:** AppleClang (Xcode 16.3–26.2 on macOS 15/26), Clang 19–23, GCC 13–16 (each at C++20/23/26), and MSVC 2026 at C++20/23 — [dev-5 all-os.yml](https://github.com/gracicot/kangaru/blob/dev-5.x.y/.github/workflows/all-os.yml)

#### 5. Diagnostics
- Kangaru "emulate[s] concepts to provide better error messages": most functions are constrained and "recursively detect errors in constructors, dependencies, definitions". `kgr::debug::service<T>(args...)` reports why a call is invalid — [doc: debugging](https://github.com/gracicot/kangaru/blob/master/doc/section11_debug.md). Example output (Clang): `static_assert failed "The service type is not constructible given passed arguments to kgr::container::service(...)."`
- Other v4 `static_assert` texts include:
  - "A dependency or one of their dependencies is not a service. Be sure to use the service definition in the list of dependencies of that service."
  - "An overriden service is marked as final, thus this service cannot override it."
  - "An abstract service cannot be final"

  — [error.hpp](https://github.com/gracicot/kangaru/blob/master/include/kangaru/detail/error.hpp)
- **Compile time vs runtime.** Dependency lists, constructibility and override validity are compile-time checks. Concrete services need no registration; they are constructed on first request. Two failures are runtime-only:
  - asking for an abstract service with no instance and no `defaults_to` throws `kgr::abstract_not_found` ("No instance found for the requested abstract service")
  - asking for a supplied service that was never emplaced throws `kgr::supplied_not_found`

  — [exception.hpp](https://github.com/gracicot/kangaru/blob/master/include/kangaru/detail/exception.hpp); [doc: polymorphic](https://github.com/gracicot/kangaru/blob/master/doc/section03_polymorphic.md); [doc: supplied](https://github.com/gracicot/kangaru/blob/master/doc/section05_supplied.md)
- The README "What's Next" list still has "Tests for compile-time errors" and "Better messages for compile-time errors (ongoing)" — [README](https://github.com/gracicot/kangaru/blob/master/README.md)

#### 6. Performance
- The README claims "Low runtime overhead". A Google-Benchmark-based suite exists (`benchmark/src/benchmarks.cpp`, CMake option `KANGARU_BENCHMARK`), but **no published results** were found in the README or docs — [CMakeLists.txt](https://github.com/gracicot/kangaru/blob/master/CMakeLists.txt); [benchmarks.cpp](https://github.com/gracicot/kangaru/blob/master/benchmark/src/benchmarks.cpp)
- Compile-time concern in v5 development: "[kangaru 5] GCC 11 compilation is really slow" (#132, closed 2025-03-17) — [issues](https://github.com/gracicot/kangaru/issues?q=is%3Aissue)
- Binary size and startup cost: unknown.

#### 7. Documentation and learning curve
- Documentation: an in-repo `doc/` folder with 14 tutorial chapters, an API reference and a 3.x→4 migration guide, mirrored in the GitHub wiki. The wiki was last edited 2020-10-03 (wiki git log) — [doc/](https://github.com/gracicot/kangaru/tree/master/doc); [wiki](https://github.com/gracicot/kangaru/wiki)
- There is an online "try" example on Wandbox — [README](https://github.com/gracicot/kangaru/blob/master/README.md)
- No v5 user documentation was found beyond tests; the dev-5 README still documents the v4 API — [dev-5 README](https://github.com/gracicot/kangaru/blob/dev-5.x.y/README.md)

#### 8. Criticisms, pitfalls, bugs
- MSVC C4702 "unreachable code" warning when injecting dependencies (#119, open since 2024-02-10) — [issues](https://github.com/gracicot/kangaru/issues?q=is%3Aissue)
- Historical bugs, all closed:
  - #113 inconsistent move-only behaviour on MSVC
  - #116 `std::aligned_storage` deprecation warnings in C++23
  - #118 polymorphic service issue
  - #123 "Incorrect destructor order call" (there is a `KANGARU_REVERSE_DESTRUCTION` CMake option)
  - #125/#126 wrong `single_service` selection

  — [issues](https://github.com/gracicot/kangaru/issues?q=is%3Aissue); [CMakeLists.txt](https://github.com/gracicot/kangaru/blob/master/CMakeLists.txt)
- Users have asked about runtime-value-based mapping (#127, closed 2024-08-07, 9 comments) and parameterised dependent services (#138) — [issues](https://github.com/gracicot/kangaru/issues?q=is%3Aissue)

#### 9. Fit
- See Inferences.

### Inferences
- **Best fit for v4:** C++11–17 codebases (games, tools, embedded with exceptions off) that want compile-time-checked wiring without Boost or RTTI, from vcpkg or Conan. It also suits teams willing to write one small service-definition struct per type, or a `service_map` friend for autowire.
- **Red flags:**
  - v4 is effectively frozen: the last release was 2024-04-29, and the 2026 fix (#143) is not in any tag or package.
  - The future API is a C++20 rewrite with no release date. Its compatibility layer is still a WIP branch (2026-10-18), so adopters face a likely migration.
  - Bus factor 1.
- The docs say nothing about threads and the headers contain no locking, so concurrent access to one `kgr::container` should be assumed unsafe. That includes the lazy creation of single services.
- The 2026-06 issue closures about Clang 22 and MSVC breakage show the maintainer keeps the code compiling on new toolchains. They may concern the v5 branch rather than v4; that was not verified.

### Gaps
- Whether a v4 dependency cycle (`A` depends on `B`, which depends on `A`) is diagnosed with a clear message or ends as raw template-recursion errors: not verified.
- Behaviour when several services override the same polymorphic base: not verified.
- Any v5 release plan or date: no statement found. The SG7 post could not be read first-hand.
- Distro and Homebrew packaging, and named production users: not found.

## Q3. dingo: full profile, design (scopes, static vs runtime registry, type erasure, factories), and benchmarks

### Takeaway
dingo is the most actively developed of the four: 137 commits in the past 12 months, last commit 2026-10-27, release v0.2.0 on 2026-04-02. It is a modern, header-only, dependency-free C++17–C++26 library that offers **both a runtime type-erased container and compile-time `bindings<...>`**, explicit scopes (`unique`, `shared`, `external`, `shared_cyclical`) and pluggable RTTI. But it is pre-1.0, has 48 stars and a single maintainer, is changing fast, and its author says the current iteration is LLM-assisted. It ships benchmark code against Fruit but **publishes no results**.

### Cited Findings

#### 1. Identity
- Repo: github.com/romanpauk/dingo. About: "Dependency Injection Library for C++17/C++20/C++23/C++26". Topics: cpp, dependency-injection, header-only, inversion-of-control, non-intrusive — [repo page, fetched 2026-10-30](https://github.com/romanpauk/dingo)
- Author/maintainer: Roman Paukner, 262 of 272 commits. Others: Octavian Dima (wopss) 4, dependabot 4, gitmodimo 2 (git shortlog, 2026-10-30) — [commits](https://github.com/romanpauk/dingo/commits/master)
- License: MIT ("Copyright (c) 2019 Roman Paukner") — [LICENSE](https://github.com/romanpauk/dingo/blob/master/LICENSE)
- First commit 2019-05-23; first tag v0.1.0 on 2024-07-31 — [tags](https://github.com/romanpauk/dingo/tags)
- **Background:**
  - The author first hit the DI-container problem "on a large legacy project in 2012" (classes with about 80 constructor parameters), and dingo is the fourth iteration of that work.
  - "The fourth iteration became practical with the arrival of LLMs … they changed the economics of working on them."
  - About the third iteration: "compiler errors during refactoring were often hard to decipher".

  — [docs/history.md](https://github.com/romanpauk/dingo/blob/master/docs/history.md)

#### 2. Maintenance (as of 2026-10-30)
- Tags:
  - v0.2.0, 2026-04-02 (annotated tag created by the `release` GitHub Actions workflow)
  - v0.1.2, 2026-03-28
  - v0.1.1, 2026-03-14
  - v0.1.0, 2024-07-31

  `CMakeLists.txt` on master still declares `project(dingo VERSION 0.1.2)` — [tags](https://github.com/romanpauk/dingo/tags); [CMakeLists.txt](https://github.com/romanpauk/dingo/blob/master/CMakeLists.txt); [release.yaml](https://github.com/romanpauk/dingo/blob/master/.github/workflows/release.yaml)
- Latest commit on `master`: 2026-10-27, a dependabot bump. The same day brought the substantive change "Allow replacing runtime registrations until the container is first resolved (#198)" — [commit 1d3bf0a](https://github.com/romanpauk/dingo/commit/1d3bf0af4a37234d145654767615334c0db82b8a); [commits](https://github.com/romanpauk/dingo/commits/master)
- **137 commits in the past 12 months and 168 in the past 24 months**; 131 of the past-12-month commits are by Paukner. Activity is bursty:

  | Month | Commits |
  |---|---|
  | 2025-06 | 17 |
  | 2026-03 | 20 |
  | 2026-04 | 21 |
  | 2026-06 | 23 |
  | 2026-07 | 32 |
  | 2026-08 | 19 |
  | 2026-10 | 18 |

  There were no commits from 2025-09 through 2026-02 or in 2026-09. About 39 remote branches exist, several updated in Oct 2026 (feature/shared-kernel, feature/spec-driven-architecture, …) — [commits](https://github.com/romanpauk/dingo/commits/master); [branches](https://github.com/romanpauk/dingo/branches)
- Large refactors were merged in Oct 2026:
  - "Compute the static graph as constexpr data (#184)"
  - "Drop runtime dependency axis and per-registration frames (#183)"
  - "Unify the container resolve front-ends (#182)"
  - "Layer the headers into an enforced hierarchy (#191)"

  — [commits](https://github.com/romanpauk/dingo/commits/master)
- Issues: 7 in total, **all closed**; 1 open PR (2026-10-30) — [issues](https://github.com/romanpauk/dingo/issues?q=is%3Aissue); [repo page](https://github.com/romanpauk/dingo)
- Responsiveness is fast. #177 ("`dingo::container` loses move semantics", by wopss) was closed 2026-10-17, and "Support moving runtime containers (#178)" was merged the same day. #43 "Is there plan to release updated version?" (19 comments) was closed 2026-04-03, a day after v0.2.0 — [issues](https://github.com/romanpauk/dingo/issues?q=is%3Aissue); [commits](https://github.com/romanpauk/dingo/commits/master)
- Bus factor 1. No API-stability, deprecation or archival statement was found — [README](https://github.com/romanpauk/dingo/blob/master/README.md)

#### 3. Adoption
- 48 stars, 5 forks, 3 watchers (2026-10-30) — [repo page](https://github.com/romanpauk/dingo)
- vcpkg: `dingo` 0.2.0 (vcpkg master, 2026-10-30). The vcpkg.io page snapshot seen in search still showed 0.1.0 — [vcpkg port](https://github.com/microsoft/vcpkg/blob/master/ports/dingo/vcpkg.json); [vcpkg.io](https://vcpkg.io/en/package/dingo.html)
- Conan Center: **no recipe** (404 on 2026-10-30) — [conan-center-index recipes](https://github.com/conan-io/conan-center-index/tree/master/recipes)
- The documented integration path is CMake `FetchContent` with `GIT_TAG v0.2.0`, linking the `dingo::dingo` target — [README](https://github.com/romanpauk/dingo/blob/master/README.md)
- Distros and Homebrew: not verified. **Verified production users: none found.**

#### 4. Technical model
- **Two registration modes sharing one resolution API:**
  - Runtime: `container<>` with `register_type<...>()`, "which fits graphs assembled by ordinary control flow"
  - Compile time: `bindings<dingo::bind<...>...>` with `container<bindings<...>>` or `static_container<bindings<...>>`

  — [README](https://github.com/romanpauk/dingo/blob/master/README.md)
- **Example** (README):
  ```cpp
  struct A { A() = default; };
  struct B { B(A&, std::shared_ptr<A>) {} };
  struct C { C(B*, std::unique_ptr<B>&, A&) {} };
  container<> container;                                                 // runtime registry
  container.register_type<scope<shared>, storage<std::shared_ptr<A>>>();
  container.register_type<scope<shared>, storage<std::unique_ptr<B>>>();
  container.register_type<scope<unique>, storage<C>>();
  C c = container.resolve<C>();                                          // ownership conversions applied
  // compile-time variant:
  using app = bindings<dingo::bind<scope<shared>, storage<std::shared_ptr<A>>>,
                       dingo::bind<scope<shared>, storage<std::unique_ptr<B>>>,
                       dingo::bind<scope<unique>, storage<C>>>;
  container<app> c2;  C c3 = c2.resolve<C>();
  ```
  — [README](https://github.com/romanpauk/dingo/blob/master/README.md)
- **Scopes:**
  - `scope<external>`: an existing instance
  - `scope<unique>`: a new instance per resolution
  - `scope<shared>`: one cached instance
  - `scope<shared_cyclical>`: "allow cyclic graphs with two-phase construction", described as "a constrained escape hatch rather than the default model"

  — [docs/getting-started.md](https://github.com/romanpauk/dingo/blob/master/docs/getting-started.md); [advanced-topics: Runtime Notes](https://github.com/romanpauk/dingo/blob/master/docs/advanced-topics.md)
- **Storage forms** are explicit: `T`, `std::shared_ptr<T>`, `std::unique_ptr<T>`, arrays and variants. Resolution "applies supported ownership and interface conversions automatically", so one registration can satisfy references, pointers, smart pointers and interface lookups — [README](https://github.com/romanpauk/dingo/blob/master/README.md); [docs/core-concepts.md](https://github.com/romanpauk/dingo/blob/master/docs/core-concepts.md)
- **Constructor discovery** is automatic: dingo uses "the highest-arity constructor shape that Dingo can prove is constructible". Constructor templates and catch-all constructors are rejected and need an explicit factory: `factory<constructor<T(Args...)>>`, `factory<function<...>>`, `callable(...)` or `callable<Signature>(...)`. An unregistered `resolve<T>()` auto-constructs only aggregates or types opted in through `is_auto_constructible<T>`. `construct<T>()` and `invoke(callable)` build unmanaged objects — [core-concepts: Factories / Constructor-Deduction Limits](https://github.com/romanpauk/dingo/blob/master/docs/core-concepts.md)
- **Bindings beyond the basics:**
  - interfaces: `register_type<scope<shared>, storage<A>, interfaces<IA>>()` and then `resolve<IA&>()`
  - multibindings: `lookups<collection<IProcessor>>`, with standard or custom aggregation
  - named and annotated bindings: `dingo::annotated<database&, primary_tag>`
  - indexed resolution

  — [core-concepts](https://github.com/romanpauk/dingo/blob/master/docs/core-concepts.md); [advanced-topics](https://github.com/romanpauk/dingo/blob/master/docs/advanced-topics.md)
- **Composition and runtime configuration:**
  - parent-child container nesting
  - replacing registrations: otherwise `lookup_already_registered_exception`; replacement is allowed until the first resolve (#198), and a sealed container throws `container_sealed_exception`
  - movable containers (#178)
  - custom allocators

  — [advanced-topics](https://github.com/romanpauk/dingo/blob/master/docs/advanced-topics.md); [commits](https://github.com/romanpauk/dingo/commits/master)
- **Type erasure:** "runtime registration resolves through type-erased bindings and its depth does not depend on the length of the chain" — [advanced-topics: Template Depth](https://github.com/romanpauk/dingo/blob/master/docs/advanced-topics.md)
- **RTTI is pluggable:** a `typeid`-based dynamic provider or a static provider built on template specializations — [advanced-topics: Custom RTTI](https://github.com/romanpauk/dingo/blob/master/docs/advanced-topics.md)
- **Exceptions are used for runtime errors.** "Container changes are atomic. If a registration or resolution operation throws, the container is restored to the state it had when the operation began" — [core-concepts: Runtime Model And Error Handling](https://github.com/romanpauk/dingo/blob/master/docs/core-concepts.md)
- **Thread safety:** no mutexes or atomics in `include/`. The only threading construct is a `thread_local` recursion guard, and the docs make no thread-safety statement — [recursion_guard.h](https://github.com/romanpauk/dingo/blob/master/include/dingo/context/recursion_guard.h); [docs/](https://github.com/romanpauk/dingo/tree/master/docs)
- **Packaging:** header-only (97 header files), "no runtime dependencies"; Google Benchmark and Fruit are used only for benchmarks — [README](https://github.com/romanpauk/dingo/blob/master/README.md); [benchmark/CMakeLists.txt](https://github.com/romanpauk/dingo/blob/master/benchmark/CMakeLists.txt)
- **Standards and compilers:** the default standard is C++17, and CMake accepts 17/20/23/26. The README says "Tested with C++17, C++20, C++23, C++26; GCC 13-16; Clang 18-20; Visual Studio 2022, 2026" — [CMakeLists.txt](https://github.com/romanpauk/dingo/blob/master/CMakeLists.txt); [README](https://github.com/romanpauk/dingo/blob/master/README.md)
- **CI matrix:**
  - gcc-13 at C++17, gcc-14 at C++23, clang-18 at C++20, clang-19 at C++26, all with sanitizers
  - Ubuntu 26.04 images with gcc-15 and clang-20 (C++23/26)
  - Ubuntu 26.10 with gcc-16
  - MSVC, plus a Windows ARM64 workflow

  — [build-base.yaml](https://github.com/romanpauk/dingo/blob/master/.github/workflows/build-base.yaml); [workflows](https://github.com/romanpauk/dingo/tree/master/.github/workflows)
- **Testing:** "Extensive generated matrix tests across features, registration modes, scopes, stored types, resolved types, and containers" — [README](https://github.com/romanpauk/dingo/blob/master/README.md)

#### 5. Diagnostics
- The docs set out how errors are reported in each mode — [core-concepts](https://github.com/romanpauk/dingo/blob/master/docs/core-concepts.md); [architecture/containers.md](https://github.com/romanpauk/dingo/blob/master/docs/architecture/containers.md); [advanced-topics](https://github.com/romanpauk/dingo/blob/master/docs/advanced-topics.md):

  | Error | Runtime `container<>` | Compile-time `bindings<...>` |
  |---|---|---|
  | Missing dependency | exception during resolution | diagnosed at compile time |
  | Cycle through declared dependencies (`dependencies<...>`, explicit constructor, function factory) | `type_recursion_exception` | rejected at compile time |
  | Cycle through auto-detected constructors | `type_recursion_exception` | still runtime `type_recursion_exception` |
  | Duplicate registration | `lookup_already_registered_exception` | — |
  | Unsupported conversion | `type_not_convertible_exception` | — |

  Constructor ambiguity must be resolved with an explicit factory.
- The author's own history notes that template-heavy errors were "often hard to decipher" in the previous iteration — [docs/history.md](https://github.com/romanpauk/dingo/blob/master/docs/history.md)

#### 6. Performance
- The benchmark suite (Google Benchmark) contains `benchmark/comparison/fruit.cpp` with `resolve_fruit_unique` and `resolve_fruit_shared`, built only when Fruit is available. Other files are `container/basic.cpp`, `container/dingo.cpp` and `index/index.cpp`. There is **no Boost.DI comparison** and **no published results** in the README or docs (2026-10-30) — [fruit.cpp](https://github.com/romanpauk/dingo/blob/master/benchmark/comparison/fruit.cpp); [benchmark/CMakeLists.txt](https://github.com/romanpauk/dingo/blob/master/benchmark/CMakeLists.txt)
- **Compile-time cost is documented** for compile-time bindings: template nesting grows with the longest dependency chain. Measured with Clang:

  | Binding in the chain | Levels per link | Longest chain at default depth (1024) |
  |---|---|---|
  | `shared`, constructor detected | 7 | 144 links |
  | `shared`, `factory<constructor<...>>` | 6 | 168 links |
  | `shared`, `factory<function<...>>` | 6 | 168 links |
  | `unique`, constructor detected | 8 | 126 links |

  GCC uses about 17 levels per link and reaches 52 links at its default depth (900). A 100-link chain with detected constructors needs about `-ftemplate-depth=720`. The runtime container avoids the limit — [advanced-topics: Template Depth](https://github.com/romanpauk/dingo/blob/master/docs/advanced-topics.md)
- Recent commits target compile time and runtime: "Reduce resolution template instantiation (#181)", "Reduce static request resolution instantiation (#171)", "Remove indirect calls from runtime request matching and cache publish (#190)" — [commits](https://github.com/romanpauk/dingo/commits/master)
- Binary size and startup cost: unknown.

#### 7. Documentation and learning curve
- Documentation is extensive and in-repo: getting-started, core-concepts, advanced-topics, architecture (overview, containers, conversion model, registration and resolution, extending), examples, development and history. README code is included from compiled example files — [docs/](https://github.com/romanpauk/dingo/tree/master/docs); [README](https://github.com/romanpauk/dingo/blob/master/README.md)
- The scope × storage × conversion model is powerful but has many concepts — [core-concepts](https://github.com/romanpauk/dingo/blob/master/docs/core-concepts.md)

#### 8. Criticisms, pitfalls, bugs
- Closed issues show the kinds of bugs users have hit:
  - #7: a pure virtual interface could not be resolved as `T&` when stored as `std::unique_ptr<Impl>` in unique scope
  - #32: annotated abstract types
  - #8: MSVC warnings leaking from public headers
  - #2: sources in the CMake interface
  - #177: move semantics

  — [issues](https://github.com/romanpauk/dingo/issues?q=is%3Aissue)
- The author calls the constructor detector "intentionally useful, not magical": it "prefers the highest-arity constructible shape, which is not always the overload to commit to in public code" — [core-concepts](https://github.com/romanpauk/dingo/blob/master/docs/core-concepts.md)

#### 9. Fit
- See Inferences.

### Inferences
- **Best fit:** new C++17+ projects that need runtime registration (modules, plugins, configuration-driven graphs) but want value and unique-ownership semantics rather than shared_ptr-everything. Teams can move hot or static subgraphs to compile-time `bindings<...>` to get compile-time checking. RTTI-free builds are possible through the static RTTI provider.
- **Red flags:**
  - pre-1.0 versioning with no stated API-stability policy
  - large internal refactors as recently as 2026-10-17 to 2026-10-27
  - bus factor 1
  - tiny adoption (48 stars, no Conan recipe)
  - openly LLM-assisted development, which some organisations scrutinise

  Production adopters should pin a tag (v0.2.0) and run their own tests.
- The docs make no thread-safety statement and the headers have no locks, so concurrent registration or resolution of `shared` scopes should be assumed unsafe without external synchronization.
- The Fruit comparison code exists, but no numbers are published. The question "what do dingo's benchmarks show" therefore cannot be answered from primary sources, and no independent corroboration was found.

### Gaps
- Benchmark results vs Fruit (none published) and vs Boost.DI (no code). Compile-time cost compared with Fruit or Boost.DI: unknown.
- Whether dingo works with exceptions disabled: not documented in what I read.
- Concrete examples of compile-time error messages: none found in the docs.
- Distro and Homebrew packaging, and any production users: not found.

## Q4. inversify-cpp: full profile, InversifyJS-style API, maturity

### Takeaway
inversify-cpp is a small, single-author C++17 header-only library that copies InversifyJS's symbol/bind/get API. Its last release is 2.1.0 (2021-04-12) and its last commit on `main` is 2023-09-19. Its two 2025 PRs are unmerged, it has 16 stars and 0 forks, it is packaged only on Conan Center, and its README still marks "Resolution scope" as TODO. It is **not production-credible**.

### Cited Findings

#### 1. Identity
- Repo: github.com/mosure/inversify-cpp. About: "C++17 inversion of control and dependency injection container library." — [repo page, fetched 2026-10-30](https://github.com/mosure/inversify-cpp)
- Author/maintainer: Mitchell Mosure, all 60 commits on `main` under several e-mails (git shortlog, 2026-10-30) — [commits](https://github.com/mosure/inversify-cpp/commits/main)
- License: MIT ("Copyright (c) 2020 Mitchell Mosure") — [LICENSE](https://github.com/mosure/inversify-cpp/blob/main/LICENSE)
- First commit 2020-12-16; first tag 1.0.0 on 2021-01-10. A 2021-03-01 commit "docs: fix readme links (repo rename)" shows the repo was renamed once; the current URL is valid — [commits](https://github.com/mosure/inversify-cpp/commits/main); [tags](https://github.com/mosure/inversify-cpp/tags)

#### 2. Maintenance (as of 2026-10-30)
- Latest tag: **2.1.0 (2021-04-12)**. Earlier: 2.0.0 (2021-03-21), 1.1.0 (2021-01-14), 1.0.0–1.0.3 (January 2021) — [tags](https://github.com/mosure/inversify-cpp/tags)
- Latest commit on `main`: **2023-09-19**, "fix: linux test build via catch2 upgrade". **0 commits in the past 12 and 24 months** on `main` — [commit a137e98](https://github.com/mosure/inversify-cpp/commit/a137e98e659eba1d57787e33a5e462388b0cd1af)
- Open, unmerged PRs by the author: #8 "fix: dependency tuple initialization" (opened 2025-05-17) and #9 "Add resolution scoped dependency caching" (opened 2025-10-02). Their branches are named `codex/verify-project-build-and-functionality` and `codex/implement-resolution-scoped-dependencies`. Of the 7 earlier PRs, #1 and #3–#7 were the author's own and were merged in 2021; #2 (a Codacy-badge bot PR) was closed unmerged — [pull requests](https://github.com/mosure/inversify-cpp/pulls?q=is%3Apr); [branches](https://github.com/mosure/inversify-cpp/branches)
- 16 stars, 0 forks, 3 watchers, 2 open PRs. The Issues tab showed no count (2026-10-30) — [repo page](https://github.com/mosure/inversify-cpp)
- No deprecation or archival statement — [README](https://github.com/mosure/inversify-cpp/blob/main/README.md)

#### 3. Adoption
- Conan Center: `inversify-cpp` 2.1.0 is the only version — [conan-center-index config.yml](https://github.com/conan-io/conan-center-index/blob/master/recipes/inversify-cpp/config.yml); [Conan Center page](https://conan.io/center/recipes/inversify-cpp)
- vcpkg: **not available** (port 404 on 2026-10-30) — [vcpkg ports](https://github.com/microsoft/vcpkg/tree/master/ports)
- Distros and Homebrew: not verified.
- Companion project: inversify-cpp-visualizer, which generates dependency graphs — [README](https://github.com/mosure/inversify-cpp/blob/main/README.md)
- Verified users: none found. The DeepWiki pages that appear in search are auto-generated summaries, not evidence of use — [DeepWiki](https://deepwiki.com/mosure/inversify-cpp)

#### 4. Technical model
- **Symbols are types** (`inversify::Symbol<T>`). The container is a class template over the full symbol list, and bindings are stored in a `std::tuple` and looked up with `std::get` at compile time:
  ```cpp
  namespace inversify = mosure::inversify;
  namespace symbols {
    using foo   = inversify::Symbol<int>;
    using bar   = inversify::Symbol<double>;
    using fizz  = inversify::Symbol<std::shared_ptr<IFizz>>;
  }
  template <> struct inversify::Injectable<Fizz>
      : inversify::Inject<symbols::foo, symbols::bar> {};       // explicit ctor-dependency list
  inversify::Container<symbols::foo, symbols::bar, symbols::fizz> container;
  container.bind<symbols::foo>().toConstantValue(10);
  container.bind<symbols::bar>().toConstantValue(1.618);
  container.bind<symbols::fizz>().to<Fizz>().inSingletonScope();
  auto fizz = container.get<symbols::fizz>();
  ```
  — [README](https://github.com/mosure/inversify-cpp/blob/main/README.md); [container.hpp](https://github.com/mosure/inversify-cpp/blob/main/include/mosure/container.hpp); [injectable.hpp](https://github.com/mosure/inversify-cpp/blob/main/include/mosure/injectable.hpp)
- **Resolvers:**
  - `toConstantValue` (always singleton)
  - `toDynamicValue([](auto& ctx){ ... ctx.container.template get<...>() ... })`, which is also how factories (a symbol of type `std::function<...>`) are written
  - `to<Impl>()` automatic binding, which can produce `T`, `unique_ptr<T>` or `shared_ptr<T>` depending on the symbol's type

  — [README](https://github.com/mosure/inversify-cpp/blob/main/README.md)
- **Scopes:** unique (the default) and singleton (`inSingletonScope()`). The README lists "resolution (TODO)", yet it also says dynamic bindings "by default … have resolution scope", which is inconsistent. PR #9 would add resolution scope but is unmerged — [README](https://github.com/mosure/inversify-cpp/blob/main/README.md); [PR list](https://github.com/mosure/inversify-cpp/pulls?q=is%3Apr)
- **Named bindings** are done with distinct symbol structs that inherit `inversify::Symbol` — [README](https://github.com/mosure/inversify-cpp/blob/main/README.md)
- **Not found in the README or headers:** multibindings, modules, child containers, cycle detection.
- **Dependency discovery is fully explicit** (the `Injectable` specialization); there is no constructor deduction — [injectable.hpp](https://github.com/mosure/inversify-cpp/blob/main/include/mosure/injectable.hpp)
- A "containerless" static-binding variant lives on a branch last touched 2021-03-03 — [containerless branch](https://github.com/mosure/inversify-cpp/tree/containerless)
- **Thread safety:** the singleton `CachedResolver` uses a `std::atomic<bool> hasCached_`. `resolve()` sets the flag to true *before* assigning the non-atomic `cached_ = parent_->resolve(context)`. It also `static_assert`s that cached types are copy-constructible ("Are you caching a unique_ptr?") — [resolver.hpp](https://github.com/mosure/inversify-cpp/blob/main/include/mosure/resolver.hpp)
- **Exceptions are used:** `ResolutionException("inversify::Resolver not found. Malformed binding.")` — [binding.hpp](https://github.com/mosure/inversify-cpp/blob/main/include/mosure/binding.hpp)
- **RTTI** (`typeid(T).name()`) is used only for the optional binding-inspection feature (`INVERSIFY_BINDING_INSPECTION`) — [resolver.hpp](https://github.com/mosure/inversify-cpp/blob/main/include/mosure/resolver.hpp)
- **Packaging:** header-only, with a `single_include` amalgamation and no third-party runtime dependencies. The build and tests are **Bazel only** (no CMake), with C++17 set in `.bazelrc` — [repo tree](https://github.com/mosure/inversify-cpp); [.bazelrc](https://github.com/mosure/inversify-cpp/blob/main/.bazelrc)
- **CI:** GitHub Actions on ubuntu, macos and windows via Bazelisk. Specific compiler versions are not pinned — [workflows](https://github.com/mosure/inversify-cpp/tree/main/.github/workflows)

#### 5. Diagnostics
- **Compile time:**
  - "inversify::Container symbols must be of type inversify::Symbol"
  - "inversify::Container must register at least one symbol"
  - "inversify::Container symbol not registered", raised by `bind<T>()` or `get<T>()` when T is not in the container's template parameter list
  - "inversify::Injectable dependencies must be of type inversify::Symbol"

  — [container.hpp](https://github.com/mosure/inversify-cpp/blob/main/include/mosure/container.hpp); [injectable.hpp](https://github.com/mosure/inversify-cpp/blob/main/include/mosure/injectable.hpp)
- **Runtime:** a symbol that is declared but never bound throws `ResolutionException` "inversify::Resolver not found. Malformed binding." — [binding.hpp](https://github.com/mosure/inversify-cpp/blob/main/include/mosure/binding.hpp)
- The README's TODO list includes "More compile-time checks" — [README](https://github.com/mosure/inversify-cpp/blob/main/README.md)

#### 6. Performance
- A speed test (`test/speed.cpp`) and a callgrind profiling example exist. **No published numbers** — [README](https://github.com/mosure/inversify-cpp/blob/main/README.md); [repo tree](https://github.com/mosure/inversify-cpp)

#### 7. Documentation and learning curve
- The README is the only documentation. Its examples are slightly inconsistent (`IFizzUniquePtr` and `IFizzSharedPtr` are used but never declared). The API is easy to learn for InversifyJS users — [README](https://github.com/mosure/inversify-cpp/blob/main/README.md)

#### 8. Criticisms, pitfalls, bugs
- The README admits the main gaps itself: "Resolution scope" and "More compile-time checks" are TODO — [README](https://github.com/mosure/inversify-cpp/blob/main/README.md)
- No third-party discussion was found.

#### 9. Fit
- See Inferences.

### Inferences
- The `CachedResolver` sets its flag before writing the value. Two threads calling `get()` on a singleton at the same time can race: one may return a default-constructed or partially written `cached_`. Singletons should therefore be treated as **not thread-safe** (inference from reading the code, not tested).
- I found no recursion guard and only one throw site, so a dependency cycle most likely recurses until the stack overflows rather than raising a diagnostic (inference; not tested).
- The `codex/` branch prefix matches the naming convention of OpenAI's Codex agent. That suggests the 2025 PRs were machine-generated and never reviewed or merged (inference).
- **Fit:** learning, prototypes, or teams coming from InversifyJS. **Red flags:** dormant since 2023-09 apart from unmerged PRs, bus factor 1, 16 stars, Bazel-only build, Conan only, explicit per-type `Injectable` boilerplate, and documented missing features.

### Gaps
- Open-issue count: the counter did not render; GitHub usually hides it when zero (not verified).
- Any production users: none found.
- Behaviour under cycles: not tested.

## Q5. Cross-comparison, and which of the four is the most credible production choice in 2026 (and for what kind of project)

### Takeaway
None of the four is a low-risk, multi-maintainer production dependency: each has a bus factor of 1.
- **Kangaru v4.3.2** is the most credible *today* for C++11–17 projects that want compile-time-checked, dependency-free, package-manager-available DI. The caveats are a release-frozen v4 and an unreleased C++20 v5 rewrite.
- **dingo** is the strongest *technical* choice for new C++17+ projects that need runtime registration, if the team accepts pre-1.0 churn and pins a version.
- **Hypodermic** is legacy only: Boost-bound, unmaintained since 2023, and broken on Clang 20.
- **inversify-cpp** is not production-ready.

### Cited Findings

#### Cross-comparison table (metrics as of 2026-10-30; every cell is sourced in Q1–Q4 above)

| | Hypodermic | Kangaru | dingo | inversify-cpp |
|---|---|---|---|---|
| Wiring model | Runtime container (builder → container), keyed by `std::type_index` | Service definitions are types (compile-time graph) plus a lazy runtime instance map | Both: type-erased runtime `container<>` or compile-time `bindings<...>` | Compile-time symbol list (`Container<Symbols...>`) plus runtime resolver objects |
| Dependency discovery | Constructor-arity deduction (≤ 20 args); everything is `shared_ptr` | `kgr::dependency<...>`, or autowire (constructor or aggregate) through a `service_map` friend | Highest-arity constructor deduction, or explicit factories | Explicit `Injectable<T> : Inject<Symbols...>` specialization |
| Lifetimes | transient, `singleInstance`, provided instance; nested containers | `service`, `single_service`, abstract/supplied; `fork()` | `unique`, `shared`, `external`, `shared_cyclical`; nesting | unique, singleton ("resolution" scope is TODO) |
| Missing binding | Runtime `ResolutionException`; unregistered concrete types are auto-built by default | Graph checked at compile time; abstract/supplied lookups fail at runtime | Runtime exception (runtime mode); compile-time error (static bindings) | Compile-time error if the symbol is not in the list; runtime `ResolutionException` if declared but unbound |
| Cycles | Runtime `CircularDependencyException` | Not verified | Compile time for declared static dependencies; runtime `type_recursion_exception` otherwise; `shared_cyclical` allows cycles | No detection found |
| Min C++ / CI toolchains | C++11; Travis/AppVeyor (g++-5, clang-3.8, VS2017) | v4: C++11 (GCC 4.8.5+); v5: C++20, CI with Clang 19–23, GCC 13–16, MSVC 2026 | C++17 (CI covers 17–26), GCC 13–16, Clang 18–20, MSVC | C++17; Bazel on 3 OSes |
| Third-party deps | **Boost** (signals2, range, algorithm) | None (v4); tl-optional (v5, with an embedded fallback) | None | None |
| RTTI / exceptions | Requires both | v4: no RTTI; exceptions optional | Pluggable RTTI; uses exceptions | Uses exceptions; RTTI only for inspection |
| Thread safety | Internal recursive mutexes (undocumented) | No locking (undocumented) | No locking (undocumented) | None; race in the singleton cache |
| Latest release | v2.5.3 (2021-09-21) | v4.3.2 (2024-04-29) | v0.2.0 (2026-04-02) | 2.1.0 (2021-04-12) |
| Last commit, default branch | 2023-03-03 | master 2026-07-13 (dev-5.x.y 2026-09-17) | 2026-10-27 | 2023-09-19 |
| Commits, past 12 / 24 months | 0 / 0 | master 5 / 9; dev-5 249 / 324 | 137 / 168 | 0 / 0 |
| Stars / forks | 603 / 80 | 554 / 41 | 48 / 5 | 16 / 0 |
| vcpkg / Conan Center | 2023-03-03 snapshot / none | 4.3.2 / 4.3.2 | 0.2.0 / none | none / 2.1.0 |
| Published benchmarks | Author-run (Windows, ~2017): resolve ≈0.5–1 µs vs make_shared ≈0.07–0.1 µs | None published | Fruit comparison code only; no results | None |
| Bus factor | 1 | 1 | 1 | 1 |

Primary sources for the table:
- [Hypodermic repo](https://github.com/ybainier/Hypodermic) and [wiki](https://github.com/ybainier/Hypodermic/wiki)
- [Kangaru master](https://github.com/gracicot/kangaru) and [dev-5.x.y](https://github.com/gracicot/kangaru/tree/dev-5.x.y)
- [dingo](https://github.com/romanpauk/dingo)
- [inversify-cpp](https://github.com/mosure/inversify-cpp)
- vcpkg ports: [hypodermic](https://github.com/microsoft/vcpkg/blob/master/ports/hypodermic/vcpkg.json), [kangaru](https://github.com/microsoft/vcpkg/blob/master/ports/kangaru/vcpkg.json), [dingo](https://github.com/microsoft/vcpkg/blob/master/ports/dingo/vcpkg.json)
- Conan Center recipes: [kangaru](https://github.com/conan-io/conan-center-index/blob/master/recipes/kangaru/config.yml), [inversify-cpp](https://github.com/conan-io/conan-center-index/blob/master/recipes/inversify-cpp/config.yml)

#### Evidence most relevant to the production verdict
- Kangaru is the only one of the four with a tagged release packaged in **both** vcpkg and Conan Center, and its maintainer still triages issues and merges fixes in 2026 (#126/#143 on 2026-07-13). But nothing has been tagged since 2024-04-29, and the C++20 v5 with its v4-compat layer is still unreleased branch work (2026-10-18) — [Kangaru tags](https://github.com/gracicot/kangaru/tags); [issues](https://github.com/gracicot/kangaru/issues?q=is%3Aissue); [compatibility-v4-wip](https://github.com/gracicot/kangaru/commits/compatibility-v4-wip)
- dingo has by far the most recent activity (137 commits in 12 months, release 2026-04-02, last commit 2026-10-27) and the broadest modern CI (C++17–26, GCC 13–16, Clang 18–20, MSVC, sanitizers). It is still at 0.x with 48 stars — [dingo commits](https://github.com/romanpauk/dingo/commits/master); [build-base.yaml](https://github.com/romanpauk/dingo/blob/master/.github/workflows/build-base.yaml); [repo page](https://github.com/romanpauk/dingo)
- Hypodermic has the most stars (603) and a long production claim (since 2012). It also has zero commits in 24 months, an unanswered Clang 20 C++20 compile failure (#62, 2025-12-12) and a hard Boost dependency — [repo page](https://github.com/ybainier/Hypodermic); [issue #62](https://github.com/ybainier/Hypodermic/issues/62)
- inversify-cpp has had no merges since 2023-09-19. It has 16 stars and only a Conan package, and its README lists missing features as TODO — [repo](https://github.com/mosure/inversify-cpp); [README](https://github.com/mosure/inversify-cpp/blob/main/README.md)

### Inferences
- **Most credible for production in 2026: Kangaru v4.3.2.** It fits C++11–17 codebases (game engines, desktop tools, embedded or no-exceptions builds) that want compile-time-checked wiring with zero dependencies and easy vcpkg/Conan consumption. Mitigations:
  - vendor or pin 4.3.2, or a master commit to pick up the unreleased #143 fix
  - expect to evaluate v5 (C++20) when it ships
  - treat containers as single-threaded
- **Best option for a new, modern codebase that needs runtime registration (plugins, modules, config-driven graphs): dingo**, with eyes open. Pin v0.2.0 through vcpkg or FetchContent, budget for API churn, and add your own tests. Its compile-time `bindings<...>` mode gives Fruit/Boost.DI-like static checking for parts of the graph. dingo is likely to become the most credible of the four if it reaches 1.0 and gains outside contributors.
- **Hypodermic only for existing users** who already depend on Boost and want Autofac-style semantics. A new project would be taking on an unmaintained dependency that is broken on Clang 20. If chosen anyway, fork it, apply the #62 workaround, disable on-the-fly registration and call `validate()` in tests.
- **inversify-cpp: do not use in production.** It is acceptable as a learning reference for InversifyJS-style APIs.
- **Common to all four:**
  - none publishes independent, reproducible benchmarks against Boost.DI or Fruit
  - none documents thread-safety guarantees
  - all have a bus factor of 1

  Production selection should therefore include a local spike measuring compile time, binary size and error-message quality.

### Gaps
- No independent (non-author) benchmarks or compile-time studies covering any of the four were found.
- Community sentiment (r/cpp, Hacker News, Stack Overflow) could not be gathered: the search tool refused reddit.com and stackoverflow.com, and lists.isocpp.org and dou.ua were unreachable.
- Linux-distro and Homebrew packaging could not be checked: Repology and formulae.brew.sh were blocked by the proxy (HTTP 403). Only the search hit for a Salix package for Hypodermic 2.4 was seen.
- GitHub "Used by" counts did not render, and code search through the API is blocked, so notable open-source dependents could not be enumerated for any library.
- GitHub Releases entries did not render on any repo page, so all release dates here are git tag dates. dingo's tag was created by a release workflow that may also publish GitHub Releases.
