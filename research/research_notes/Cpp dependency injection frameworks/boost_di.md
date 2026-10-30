# Boost.DI ([Boost].DI / boost-ext/di) and its C++20 sibling qlibs/di: profile as of 2026-10-30

Method note for the report writer: repository facts (commits, tags, contributors, file contents) come from blobless git clones of https://github.com/boost-ext/di, https://github.com/qlibs/di, the vcpkg, conan-center-index and homebrew-core GitHub repos, and https://github.com/qdrvm/kagome, all taken on 2026-10-30. Nothing was built or run. Star, issue and PR counts come from github.com pages fetched on 2026-10-30. The following hosts were outside this session's allowed network and could not be fetched: vcpkg.io, conan.io, listarchives.boost.org, lists.preview.boost.org and two Reddit mirrors (lr.eu.psf.lt, redlib.hackliberty.org). For those sources only search-engine snippets were available, and they are labelled as such below.

## Key question 1: Per-library profiles (Boost.DI, and the sibling/successor qlibs/di)

### Takeaway
Boost.DI is a mature, feature-rich, header-only C++14 compile-time DI library. It is effectively a one-person project, and since 2023 it has been in low-activity "merge outside patches" mode: the last feature-bearing release was v1.3.0 (2023-05-24), v1.3.2 (2025-04-02) changed only the build system, and the 2026 compiler fixes are not in any tag. The only DI sibling found is Kris Jusiak's C++20 single-header qlibs/di (MIT, 2024); boost-ext/di has no C++17/20 branch. That library is tiny (41 stars), not packaged, and has had no commits since 2025-05-09. It is a different, more minimal design, not a drop-in successor.

### Cited Findings

#### Boost.DI profile

**1. Identity**
- Repo: https://github.com/boost-ext/di. The default branch is `cpp14`; the other branches are `cpp03`, `benchmarks` and `gh-pages`. No C++17/20 branch exists. — [boost-ext/di repo](https://github.com/boost-ext/di); branch list from git clone 2026-10-30.
- The GitHub description reads "C++14 Dependency Injection library", and the README tagline reads "Your C++14 one header only Dependency Injection library with no dependencies". — [README](https://github.com/boost-ext/di/blob/cpp14/README.md)
- Author and maintainer: Kris (Krzysztof) Jusiak. About 4,980 of 5,071 commits on `cpp14` are by Jusiak under various emails, including 345 commits made with a king.com address. The next largest contributor, Kanstantsin Chernik, has 46 commits, 41 of them with a bloomberg.net address. — [commit history](https://github.com/boost-ext/di/commits/cpp14) (git shortlog, 2026-10-30)
- License: Boost Software License 1.0. — [header licence notice](https://github.com/boost-ext/di/blob/cpp14/include/boost/di.hpp); [README badge](https://github.com/boost-ext/di/blob/cpp14/README.md)
- First commit was 2012-02-06 ("init"). The first tag, v0.1.0 (the C++03/C++11 version), is dated 2014-08-15, and v1.0.0 is dated 2016-02-22. — [tags](https://github.com/boost-ext/di/tags); [CHANGELOG](https://github.com/boost-ext/di/blob/cpp14/doc/CHANGELOG.md)
- The project was previously hosted as `boost-experimental/di`; Fruit's benchmark wiki still links there. The docs brand it "[Boost::ext].DI" and the namespace is `boost::ext::di` (an inline `ext` namespace). — [Fruit benchmarks wiki](https://github.com/google/fruit/wiki/benchmarks); [overview.md configuration table](https://github.com/boost-ext/di/blob/cpp14/doc/overview.md)

**2. Maintenance (as of 2026-10-30)**
- Latest tag: v1.3.2, dated 2025-04-02. It was preceded the same day by v1.3.1 and before that by v1.3.0 on 2023-05-24. Neither of the 2025 tags appears in the CHANGELOG, whose last entry is 1.3.0 (2023-05-23, "Bug Fixes" only). — [tags](https://github.com/boost-ext/di/tags); [CHANGELOG](https://github.com/boost-ext/di/blob/cpp14/doc/CHANGELOG.md)
- The v1.3.0 to v1.3.2 changes are build-system only. They raise the CMake minimum to 3.10, remove the Boost-headers CMake dependency, and delete `meson.build`, `test/Jamfile.v2` and `meta/libraries.json`. The header still says `#define BOOST_DI_VERSION 1'3'0` and uses namespace `v1_3_0`. — [compare v1.3.0...v1.3.2](https://github.com/boost-ext/di/compare/v1.3.0...v1.3.2); [di.hpp L11](https://github.com/boost-ext/di/blob/cpp14/include/boost/di.hpp#L11)
- The latest commit on `cpp14` is 2026-09-11, "fix warning `TU-local entity 'make_injector'`". It was authored by Igor Koznov and committed (merged) by Kris Jusiak. — [commit e35c8d9](https://github.com/boost-ext/di/commit/e35c8d94d82c2b0028ab47d0cca6554ca90a2afe); [PR #585](https://github.com/boost-ext/di/pull/585)
- Activity in the past 12 months (2025-10-30 to 2026-10-30): 5 commits, all from outside contributors (Igor Koznov 3, Michał Zawiślak 2), all merged by Jusiak. Activity in the past 24 months: 10 commits, adding Jusiak's own 5 commits, all made on 2025-04-02. No untagged commit has been released. — [commit history](https://github.com/boost-ext/di/commits/cpp14) (git rev-list/shortlog, 2026-10-30)
- The untagged 2026 commits are a fix for "error with clang 21" (PR #584, 2026-04-04), "add missing header" (`<type_traits>` in the mocks provider, PR #583), a CMake option for extensions (PR #578, authored 2025-11-28 and merged 2026-04-04), and the TU-local fix (PR #585). — [PR list](https://github.com/boost-ext/di/pulls?q=is%3Apr+sort%3Aupdated-desc); [commit 1c69ddc](https://github.com/boost-ext/di/commit/1c69ddc66975f082b0a0f7ab59f943863c86f016); [commit 3f13955](https://github.com/boost-ext/di/commit/3f1395580cb8584aa409454b0571737fe4c45a7e)
- Open issues: 86. Open PRs: 1 (#508 "Runtime provider additions", open since 2021-01-15). Closed PRs: 184. — [issues](https://github.com/boost-ext/di/issues); [PRs](https://github.com/boost-ext/di/pulls?q=is%3Apr+sort%3Aupdated-desc) (fetched 2026-10-30)
- Responsiveness is mixed. Recent issues have no maintainer replies, for example #581 "1.3.2 fails to compile tests" (2026-01-26, no comments). On #568 (Clang 18.1), Jusiak replied within a day in Sept 2024 but not after the reporter posted a reproduction. Outside PRs do get merged, with latency ranging from the same day (#585) to about 4 months (#578). — [#581](https://github.com/boost-ext/di/issues/581); [#568](https://github.com/boost-ext/di/issues/568); [PR list](https://github.com/boost-ext/di/pulls?q=is%3Apr+sort%3Aupdated-desc)
- At its merge in Sept 2026, PR #585 showed "2 of 4 checks passed". — [PR #585](https://github.com/boost-ext/di/pull/585)
- No archival, deprecation or maintenance-mode statement was found in the repo or README. The repo is not archived. — [boost-ext/di repo](https://github.com/boost-ext/di)
- The `cpp03` branch is explicitly unmaintained ("this branch is not maintained anymore"); its last commit is 2014-12-11. The docs site (`gh-pages`) was last deployed on 2021-06-14. — [FAQ](https://github.com/boost-ext/di/blob/cpp14/doc/FAQ.md); [cpp03 branch](https://github.com/boost-ext/di/tree/cpp03); branch tips from git, 2026-10-30

**3. Adoption** (details under Key question 7)
- About 1.3k stars, 149 forks and 64 watchers (github.com, 2026-10-30). — [repo](https://github.com/boost-ext/di)
- Packaged in vcpkg as `bext-di` 1.3.2 and in Conan Center as `di` (1.1.0, 1.2.0, 1.3.0, 1.3.2). Not in Homebrew core. — [vcpkg port](https://github.com/microsoft/vcpkg/blob/master/ports/bext-di/vcpkg.json); [CCI config.yml](https://github.com/conan-io/conan-center-index/blob/master/recipes/di/config.yml)
- Verified open-source dependent: Kagome, the C++ Polkadot host by Quadrivium/qdrvm, through a pinned 1.1.0 fork. — [Kagome application_injector.cpp](https://github.com/qdrvm/kagome/blob/master/core/injector/application_injector.cpp)

**4. Technical model** (details under Key question 3)
- Wiring is compile-time; bindings are types. Constructors are discovered automatically by taking the "unique constructor with the longest parameter list", up to `BOOST_DI_CFG_CTOR_LIMIT_SIZE`, which defaults to 10. Aggregates are supported. Macros such as `BOOST_DI_INJECT` are needed only to disambiguate. — [user_guide.md, Injections](https://github.com/boost-ext/di/blob/cpp14/doc/user_guide.md); [overview.md](https://github.com/boost-ext/di/blob/cpp14/doc/overview.md)
- Binding syntax, from the README quick start: `di::make_injector(di::bind<int>.to(42), di::bind<double>.to(87.0)); injector.create<example>();` — [README](https://github.com/boost-ext/di/blob/cpp14/README.md)
- Illustrative example, assembled from the documented syntax and **not compiled**:
  ```cpp
  #include <boost/di.hpp>
  namespace di = boost::di;
  struct ilogger { virtual ~ilogger() = default; virtual void log(const char*) = 0; };
  struct console_logger : ilogger { void log(const char*) override {} };
  struct app { app(std::shared_ptr<ilogger> log, int port) {} };   // ctor deduced, no macro
  int main() {
    auto injector = di::make_injector(
        di::bind<ilogger>.to<console_logger>(),   // interface -> implementation
        di::bind<int>.to(8080));                  // value (instance scope)
    auto a = injector.create<app>();              // shared_ptr param => singleton (deduced)
  }
  ```
  Syntax sources: [user_guide.md, Bindings](https://github.com/boost-ext/di/blob/cpp14/doc/user_guide.md); [README](https://github.com/boost-ext/di/blob/cpp14/README.md)
- Scopes: `unique`, `singleton`, `instance` and `deduce` (the default). The extensions add `scoped`, `session` and `shared` scopes, and custom scopes are supported. — [user_guide.md, Scopes](https://github.com/boost-ext/di/blob/cpp14/doc/user_guide.md); [extensions.md](https://github.com/boost-ext/di/blob/cpp14/doc/extensions.md)
- Interface-to-implementation, multi-interface, value, array/multi, named, override and lambda (runtime-conditional) bindings are all supported. — [user_guide.md, Bindings](https://github.com/boost-ext/di/blob/cpp14/doc/user_guide.md)
- Factories and assisted injection, lazy injection, contextual bindings, a runtime provider/injector, XML injection, a mocks provider and a UML dumper are provided as extensions, not in the core. — [extensions.md](https://github.com/boost-ext/di/blob/cpp14/doc/extensions.md); [CHANGELOG 1.1.0](https://github.com/boost-ext/di/blob/cpp14/doc/CHANGELOG.md)
- Modules are functions returning `di::make_injector(...)` (all types exposed) or `di::injector<T>` (one type exposed). — [benchmarks.md legend](https://github.com/boost-ext/di/blob/cpp14/doc/benchmarks.md); [user_guide.md, Modules](https://github.com/boost-ext/di/blob/cpp14/doc/user_guide.md)
- Thread safety: the docs say "[Boost].DI is thread safe." — [overview.md](https://github.com/boost-ext/di/blob/cpp14/doc/overview.md)
- RTTI and exceptions are not required. The docs say it is "not using exceptions internally and therefore might be compiled with `-fno-exceptions`", and the README's sample flags include `-fno-exceptions -fno-rtti`. — [overview.md](https://github.com/boost-ext/di/blob/cpp14/doc/overview.md); [README](https://github.com/boost-ext/di/blob/cpp14/README.md)
- Header-only. The single header `include/boost/di.hpp` is 137,124 bytes at HEAD; the author describes it as "One header - 3k LOC". Extensions are separate headers (20 under `extension/include`). — git ls-tree, 2026-10-30; [CppCon 2018 slides](https://github.com/boost-ext/di/blob/cpp14/doc/cppcon-2018/index.html)
- Third-party dependencies: none. The docs say "neither STL nor Boost" is required: STL headers are used via `__has_include` when present, and `boost::shared_ptr` is supported if available. — [overview.md](https://github.com/boost-ext/di/blob/cpp14/doc/overview.md); [di.hpp prologue](https://github.com/boost-ext/di/blob/cpp14/include/boost/di.hpp)
- Minimum standard is C++14; the header has `#error "boost-ext.di requires C++14 support (Clang-3.4+, GCC-5.1+, MSVC-2015+)"`. CMake defaults to `CMAKE_CXX_STANDARD 14`. — [di.hpp L8-9](https://github.com/boost-ext/di/blob/cpp14/include/boost/di.hpp#L8); [CMakeLists.txt](https://github.com/boost-ext/di/blob/cpp14/CMakeLists.txt)
- Current CI is GitHub Actions with three Debug jobs: "windows MinGW" g++ on windows-latest, "ubuntu g++" on ubuntu-latest and "macos clang++" on macos-latest. There is no MSVC job and no C++17/20/23 job. — [.github/workflows/build.yml](https://github.com/boost-ext/di/blob/cpp14/.github/workflows/build.yml)

**5. Diagnostics** (details under Key question 6)
- Missing bindings, ambiguous or duplicate bindings, and constructor-arity problems are compile-time errors with type-encoded names, for example `abstract_type<interface>::is_not_bound` with the hint "did you forget to add: 'di::bind<interface>.to<implementation>()'?". Cycles have no dedicated message. — [example/errors](https://github.com/boost-ext/di/blob/cpp14/example/errors/creatable_abstract_type_is_not_bound.cpp); [user_guide.md, Concepts](https://github.com/boost-ext/di/blob/cpp14/doc/user_guide.md)

**6. Performance** (details under Key question 5)
- The author claims "none run-time overhead". His own 2016-era benchmark shows Boost.DI compiling faster than Fruit. Fruit's 2020 benchmark wiki shows the opposite at scale: Clang full compile took 580 s against Fruit's 64 s at 1000 classes, and GCC ran out of memory. — [overview.md](https://github.com/boost-ext/di/blob/cpp14/doc/overview.md); [benchmarks.md](https://github.com/boost-ext/di/blob/cpp14/doc/benchmarks.md); [Fruit benchmarks wiki](https://github.com/google/fruit/wiki/benchmarks)

**7. Documentation and learning curve**
- The docs are extensive: Tutorial, User Guide (about 1,600 lines of markdown), Examples (110 example .cpp files in the repo), Extensions (20), FAQ, Benchmarks, "Try it online!", and seven conference-talk slide decks in-repo. — [docs index](https://github.com/boost-ext/di/blob/cpp14/doc/index.md); [user_guide.md](https://github.com/boost-ext/di/blob/cpp14/doc/user_guide.md); git ls-tree, 2026-10-30
- Parts of the docs are stale. Badges point to Travis/AppVeyor, the compiler list reads "Clang-3.4+ / GCC-5.2+ / MSVC-2015+", and the site was last deployed in 2021. — [docs index](https://github.com/boost-ext/di/blob/cpp14/doc/index.md); [overview.md](https://github.com/boost-ext/di/blob/cpp14/doc/overview.md)
- The author's own summary slide lists "(-) Steeper learning curve" as the con of DI libraries. — [C++Now 2019 slides](https://github.com/boost-ext/di/blob/cpp14/doc/cppnow-2019/index.html)

**8. Criticisms** (details under Key questions 3, 4, 5 and 7)
- These include compile-time and memory scaling, lifetime-deduction surprises, process-global singletons, dangling-reference reports, open MSVC issues, unanswered issues, and fixes that never reach a tag.

**9. Fit**: see Inferences below.

#### qlibs/di profile

**1. Identity**
- Repo: https://github.com/qlibs/di, described as "C++20 Dependency Injection library". The author is Kris Jusiak ("Copyright (c) 2024 Kris Jusiak"). The license is MIT. — [qlibs/di repo](https://github.com/qlibs/di); [header `di`](https://github.com/qlibs/di/blob/main/di)
- The first commit is 2024-08-09 and v1.0.0 is tagged 2024-08-26. — [commits](https://github.com/qlibs/di/commits/main); [tags](https://github.com/qlibs/di/tags) (git, 2026-10-30)

**2. Maintenance (as of 2026-10-30)**
- Latest tag: v1.0.6, dated 2025-04-27 (":bug: :fire: [provider_t]"). Latest commit: 2025-05-09 (README edits). — [tags](https://github.com/qlibs/di/tags); [commits](https://github.com/qlibs/di/commits/main)
- The repo has 116 commits in total, all by Kris Jusiak, which gives a bus factor of 1. There were 0 commits in the past 12 months and 13 in the past 24 months (2025-04-04 to 2025-05-09, adding the C++20 module `di.cppm` on 2025-04-19). — git log/shortlog of [qlibs/di](https://github.com/qlibs/di/commits/main), 2026-10-30
- There are 3 open issues. The repo contains only `.github/LICENSE`, `.github/README.md` (a symlink to the header), `di` and `di.cppm`, with no CI workflow; the "Build" badge links to a Godbolt snippet. — [qlibs/di repo](https://github.com/qlibs/di); git ls-tree, 2026-10-30
- No deprecation or archival notice was found. — [qlibs/di repo](https://github.com/qlibs/di)

**3. Adoption**
- 41 stars, 7 forks and 3 watchers (2026-10-30). — [qlibs/di repo](https://github.com/qlibs/di)
- Not in vcpkg or Conan Center: no `qlibs*` port or recipe exists in either index at HEAD on 2026-10-30. — [vcpkg ports](https://github.com/microsoft/vcpkg/tree/master/ports); [conan-center-index recipes](https://github.com/conan-io/conan-center-index/tree/master/recipes)
- No verified dependents were found.

**4. Technical model**
- Distribution: a single header `di` (34,828 bytes, 995 lines, including the embedded README) or the C++20 module `di.cppm`. It requires C++20 ("clang++13+, g++11+"); MSVC is not listed. It includes only `<cstdint>`, `<utility>`, `<type_traits>` and `<memory>`. — [header `di`](https://github.com/qlibs/di/blob/main/di)
- Wiring model: bindings are overload sets of concept-constrained generic lambdas (`di::overload{...}`) passed to `di::make<T>(...)`. Constructor parameters are deduced "by leveraging concepts" (constraint subsumption), for classes and aggregates. `ctor_traits` defaults to `N = 16` parameters. — [header `di`, FAQ/API](https://github.com/qlibs/di/blob/main/di)
- README example (verbatim): `auto a = di::make<aggregate1>(di::overload{ [](di::is<int> auto t) requires (t.name() == "i1") { return 4; }, [](di::is<int> auto t) requires (t.name() == "i2") { return 2; }, });` — [header `di`](https://github.com/qlibs/di/blob/main/di)
- Named injection (`t.name()`) is available only `#if defined(REFLECT)`, meaning it needs the author's separate reflect library. — [header `di`, API](https://github.com/qlibs/di/blob/main/di)
- Scopes are do-it-yourself: the README implements a singleton as a function-local `static` inside an overload for reference types. Assisted injection works by passing arguments to `di::make`. Policies are `static_assert`s inside overloads, for example "raw pointers are not allowed!". Test overrides are a priority overload that falls back to the production bindings. Logging and profiling work by intercepting `di::provider<T, Index, Parent>`. The README's "DIY - Dependency Injection Yourself" example rebuilds a Boost.DI-like `injector`/`bind` on top. — [header `di`](https://github.com/qlibs/di/blob/main/di)
- The header "verifies itself upon include" with `static_assert` tests; define `-DNTEST` to disable them. — [header `di`](https://github.com/qlibs/di/blob/main/di)
- Multibindings, thread-safety statements and RTTI/exception requirements: not documented. README examples use `typeid` in user code for context matching.

**5. Diagnostics**
- A missing binding yields a `di::error<int, ...>` type, according to the README's "errors" example. Quality on real code is unknown. — [header `di`](https://github.com/qlibs/di/blob/main/di)

**6. Performance**: no published benchmarks found.

**7. Documentation**: the README is embedded in the header (Overview, Examples, API, FAQ), with Godbolt links. — [header `di`](https://github.com/qlibs/di/blob/main/di)

**8. Criticisms**: no third-party commentary found beyond the Aug 2024 r/cpp thread (see Key question 7).

### Inferences
- **Boost.DI fit (2026):** it best suits greenfield or mid-size C++14 to C++20 codebases on GCC/Clang that want compile-time-checked wiring, header-only distribution and `-fno-exceptions -fno-rtti` compatibility, where the team accepts template-heavy code and can vendor or patch the header.
- **Boost.DI red flags:**
  - Bus factor of 1, with all merges by Jusiak.
  - No feature release since 2023, and recent fixes (Clang 21, C++20-module TU-local) only on an untagged branch, so vcpkg/Conan 1.3.2 users lack them.
  - No MSVC or C++20/23 CI.
  - Third-party evidence of severe compile-time and memory blow-up on large graphs.
  - Process-global singleton semantics (see Key question 3).
  - 86 open issues, including dangling-reference and lifetime bugs.
- **Versus Google Fruit:** in the only head-to-head with relatively modern compilers (Fruit's own wiki, 2020, Clang 10 and GCC 9), Fruit:
  - compiled about 2.6x to 9x faster;
  - used about 2.6x to 33x less compiler memory;
  - produced binaries about 30% (Clang) to 2-3x (GCC) smaller;
  - was "10-15x" faster per request (the wiki's own summary).

  Boost.DI had slightly faster startup (about 1-2 ms). Treat these as competitor-run numbers.
- **Versus hand-written wiring:** the same benchmark shows simple manual DI with interfaces compiling about 17x (100 classes) to 84x (1000 classes) faster than Boost.DI on Clang, with binaries about 9x smaller. Jusiak himself argues that "DI doesn't require a library/framework". The library's value is removing the "wiring mess" and making re-wiring cheap, not performance.
- **qlibs/di** is best treated as an experimental, personal-scale design exploration by the same author: C++20 only, no CI, no packaging, no MSVC claim, quiet since May 2025. It is not a maintained production successor, and migrating to it would be a rewrite of the wiring code, not an upgrade.

### Gaps
- Exact star count beyond "1.3k": not available without the GitHub API, which is blocked.
- Whether boost-ext/di has formal GitHub "Releases" or only tags: the repo page's Releases sidebar was not captured.
- qlibs/di thread-safety, RTTI and exception requirements: not documented.

## Key question 2: Is Boost.DI part of official Boost? History, branches, and maintenance status today

### Takeaway
Boost.DI is not and never has been part of the official Boost distribution; the "Boost" in its name is branding (`boost-ext`, "[Boost::ext].DI"). Jusiak sought Boost review at least as late as Feb 2021, but no formal review was found, and the 2025 cleanup deleted the Boost-integration scaffolding (Jamfile, `meta/libraries.json`). The library is not deprecated, but it is in minimal-maintenance mode. The author's newer DI work (qlibs/di, 2024) exists, but it has not been declared a successor and is itself dormant since May 2025.

### Cited Findings
- **Not an official Boost library:**
  - The README says: "**Disclaimer** `boost-ext.di` is not an official Boost library." — [README](https://github.com/boost-ext/di/blob/cpp14/README.md)
  - The same disclaimer appears in the CppCon 2018 and C++Now 2019 decks. — [CppCon 2018 slides](https://github.com/boost-ext/di/blob/cpp14/doc/cppcon-2018/index.html); [C++Now 2019 slides](https://github.com/boost-ext/di/blob/cpp14/doc/cppnow-2019/index.html)
- **Fruit's wiki on Boost inclusion:** Fruit's benchmark wiki (last edited 2020-05-02) says Boost.DI is "not actually part of Boost" and that, although the author has pushed for inclusion for years, it "has never been accepted" (wording as relayed by the fetch tool). — [Fruit benchmarks wiki](https://github.com/google/fruit/wiki/benchmarks)
- **Boost Trac ticket (2018):** ticket #13244, "Is there any plans to review and add BOOST::DI into main library", appears in Boost-bugs archives from May 2018 with status closed. The resolution text was not visible (search snippet only; the host is blocked). — [boost-bugs 2018/05 #13244](https://lists.boost.org/boost-bugs/2018/05/50537.php)
- **Call for review managers (Feb 2021):** Jusiak posted "[boost-ext] Looking for Review Manager/s" on the Boost developers list. Replies came from Richard Hodges, Edward Diener, Niall Douglas, Peter Dimov and others, plus a "DI questions" sub-thread by Andrzej Krzemienski. A search snippet relays the post as saying all boost-ext libraries except TE were "successfully deployed in production systems". This is an author claim seen only via snippet. All of these thread details come from search snippets, because the archive hosts could not be fetched. No outcome (review manager or review dates) was found. — [Boost list archive 2021/02](https://lists.boost.org/Archives/boost/2021/02/250845.php); [Google Groups mirror](https://groups.google.com/g/boost-developers-archive/c/n_6SsP7o2dg)
- **No review schedule entry found:** searches of Boost review-schedule material turned up no entry or review manager for a Jusiak DI library. — [boost.io upcoming reviews](https://boost.io/review/upcoming) (search result only)
- **Boost scaffolding removed in v1.3.2 (2025-04-02):** `test/Jamfile.v2` (111 lines) and `meta/libraries.json` (9 lines) were deleted, along with Meson files. — [compare v1.3.0...v1.3.2](https://github.com/boost-ext/di/compare/v1.3.0...v1.3.2)
- **Version timeline:**
  - 2012-02-06: first commit.
  - v0.1.0, 2014-08-15: C++03/C++11 version, Clang 3.2+.
  - v0.5.0, 2015-01-12: C++14 version, Clang 3.4+.
  - v1.0.0, 2016-02-22: MSVC-2015+, GCC-5+, libc++/libstdc++, macOS.
  - v1.0.1, 2016-05-04: simplified policy interface (breaking); contextual bindings and serialize extensions.
  - v1.0.2, 2018-01-04/05: templates/concepts injection; extensions shipped as headers; shared factory.
  - v1.1.0, 2019-01-07: Clang 5-7, GCC 7/8, MSVC-2017 preview; `is_creatable`; binding to constructor; runtime-provider and injector extensions.
  - v1.2.0, 2020-07-21/22: Clang 9/10, Xcode 11, GCC 9/10, MSVC-2019, Meson.
  - v1.3.0, 2023-05-23/24: bug fixes only.
  - v1.3.1 and v1.3.2, 2025-04-02: CMake/cleanup only.

  — [CHANGELOG](https://github.com/boost-ext/di/blob/cpp14/doc/CHANGELOG.md); [tags](https://github.com/boost-ext/di/tags)
- **cpp03 vs cpp14 branches:** the FAQ says the C++03 implementation (switching to C++11 where available) "was/is really slow to compile and hard to maintain". Return-type deduction (C++14) was needed for a clean `modules` interface, so the core was reimplemented in C++14, which "sped up compilation times dramatically". The `cpp03` branch "is not maintained anymore"; its last commit is 2014-12-11. — [FAQ](https://github.com/boost-ext/di/blob/cpp14/doc/FAQ.md); [cpp03 branch](https://github.com/boost-ext/di/tree/cpp03)
- **Maintenance today:** 5 commits in the 12 months to 2026-10-30, all outside patches merged by Jusiak. There have been no feature commits since v1.3.0 (2023), and the newest fixes are untagged. — [commit history](https://github.com/boost-ext/di/commits/cpp14)
- **Author on qlibs/di vs boost-ext.di (Aug 2024 r/cpp thread):** Jusiak said the new library is more general than boost-ext.di, requires C++20, supports contextual injection via concepts, and that "boost-ext.di syntax and functionality can be build on top of it". This is a paraphrase from a search snippet of a mirror of reddit.com/r/cpp/comments/1f2fpqr; the thread itself could not be fetched. — [r/cpp thread (mirror)](https://lr.eu.psf.lt/r/cpp/comments/1f2fpqr/c20_dependency_injection)
- **qlibs/di README framing:** the README does not call itself a successor; it lists "boost-ext.di" among "Similar projects" alongside google.fruit, kangaru, wallaroo, hypodermic and dingo. — [qlibs/di](https://github.com/qlibs/di)
- **Community perception (2025, search snippet):** an r/cpp_questions commenter called Boost.DI "an incredible piece of engineering but afaik it isn't maintained". — [r/cpp_questions thread (mirror)](https://redlib.hackliberty.org/r/cpp_questions/comments/1l4wbpp/dependency_injection_at_scale)

### Inferences
- The project is not abandoned, since the owner still merges small PRs (most recently 2026-09-11). It is in a de facto maintenance mode, with no roadmap, no releases cutting the merged fixes, and no issue triage.
- Removing the Jamfile and `meta/libraries.json` in 2025 suggests the Boost-submission ambition has been dropped. This is an inference from the diff, not a stated decision.
- "Superseded" is only half true. The author's attention moved to the C++20 qlibs/* libraries in 2024-2025 and he describes qlibs/di as more general. But qlibs/di is a different, lower-level API, it is also quiet, and boost-ext/di was never marked deprecated.

### Gaps
- The outcome of the Feb 2021 review-manager request and the resolution text of Trac #13244: the archive hosts are blocked in this session (listarchives.boost.org and lists.preview.boost.org must be allowed to verify).
- Whether Boost.DI was ever listed in the Boost Library Incubator or on the official review schedule: no source found.

## Key question 3: Technical model (injectors, constructor deduction, bindings, scopes, policies, providers, extensions) and lifetime-deduction surprises

### Takeaway
Boost.DI resolves the whole object graph at compile time from a type list of bindings. It deduces constructors without macros, using the longest unique constructor (10 parameters or fewer). It infers lifetime from how each consumer spells the parameter type: `T&` and `shared_ptr<T>` mean singleton, while `T`, `unique_ptr<T>` and `T*` mean a new instance. The singleton scope is a function-local `static`, so singletons are process-global, not per-injector. That, plus parameter-spelling-dependent lifetimes and some dangling-reference reports, is the main source of surprises.

### Cited Findings
- **Injector:**
  - `di::make_injector(bindings...)` builds an injector and `injector.create<T>()` creates an object graph. The README example creates an `example` taking an aggregate and `const ctor&`, binding only `int` and `double`. — [README](https://github.com/boost-ext/di/blob/cpp14/README.md)
  - The author's slides note "Full Object graph is being deduced (recursively)" and "Non-polymorphic types don't have to be bound". — [Bloomberg 2018 slides](https://github.com/boost-ext/di/blob/cpp14/doc/bloomberg-2018/index.html)
- **Constructor deduction:**
  - "[Boost].DI will deduce the best available constructor to be used for injection - unique constructor with the longest parameter list". To change this, mark the constructor with `BOOST_DI_INJECT`, `BOOST_DI_INJECT_TRAITS`, `di::ctor_traits` or `di::inject`. — [user_guide.md, Injections](https://github.com/boost-ext/di/blob/cpp14/doc/user_guide.md)
  - Automatic deduction "is limited to BOOST_DI_CFG_CTOR_LIMIT_SIZE, which by default is set to 10" (configurable 0-10). — [user_guide.md](https://github.com/boost-ext/di/blob/cpp14/doc/user_guide.md); [overview.md](https://github.com/boost-ext/di/blob/cpp14/doc/overview.md); [di.hpp L953](https://github.com/boost-ext/di/blob/cpp14/include/boost/di.hpp#L953)
  - "[Boost].DI is not able to automatically distinguish between ambiguous constructors with the same (longest) number of parameters." — [user_guide.md](https://github.com/boost-ext/di/blob/cpp14/doc/user_guide.md)
  - The approach is described as "Constructor parameters deduction without reflection"; the FAQ points to the C++Now 2016 slides for how it works. — [Bloomberg 2018 slides](https://github.com/boost-ext/di/blob/cpp14/doc/bloomberg-2018/index.html); [FAQ](https://github.com/boost-ext/di/blob/cpp14/doc/FAQ.md)
- **Bindings** (user-guide table):
  - `di::bind<I>.to<Impl>()`, and `di::bind<I1, I2, ...>.to<Impl>()` ("using one object").
  - `di::bind<T>.to(value)`.
  - `di::bind<int[]>.to({1, 2, ...})`.
  - `di::bind<Interface*[]>.to<Impl1, Impl2, ...>()` (multibinding).
  - `.in(scope)`, `.named(name)` and `[di::override]`.
  - Lambda bindings `di::bind<I>.to([](const auto& injector){...})` "depending on a run-time condition".

  — [user_guide.md, Bindings](https://github.com/boost-ext/di/blob/cpp14/doc/user_guide.md)
- **Templates/concepts can be bound:** `di::bind<class Greater>().to<hello>()`. — [FAQ](https://github.com/boost-ext/di/blob/cpp14/doc/FAQ.md)
- **Dynamic bindings:** the FAQ shows choosing `gui_view&` or `text_view&` at runtime inside a binding lambda. — [FAQ](https://github.com/boost-ext/di/blob/cpp14/doc/FAQ.md)
- **Named and annotated bindings:** the Annotations section documents `named`, used with `BOOST_DI_INJECT(T, (named = name) int ...)` and `.named(name)`. — [user_guide.md, Annotations](https://github.com/boost-ext/di/blob/cpp14/doc/user_guide.md); [test/ft/di_errors.cpp](https://github.com/boost-ext/di/blob/cpp14/test/ft/di_errors.cpp)
- **Deduce scope (the default when no scope is given):**

  | Type | Scope |
  |---|---|
  | `T` | unique |
  | `T&` | singleton |
  | `const T&` | unique (temporary) / singleton |
  | `T*` | unique (ownership transfer) |
  | `const T*` | unique (ownership transfer) |
  | `T&&` | unique |
  | `std::unique_ptr<T>` | unique |
  | `std::shared_ptr<T>`, `boost::shared_ptr<T>`, `std::weak_ptr<T>` | singleton |

  — [user_guide.md, Scopes](https://github.com/boost-ext/di/blob/cpp14/doc/user_guide.md)
- **Singleton scope:**
  - Described as "Scope representing shared value between all instances as well as threads".
  - It "will convert automatically between `std::shared_ptr` and `boost::shared_ptr`".
  - Implementation: `static auto object(provider.get(...))` and `static std::shared_ptr<T_> object{provider.get()};`, function-local statics inside `scopes::singleton::scope<TExpected, TGiven>::create`.

  — [user_guide.md](https://github.com/boost-ext/di/blob/cpp14/doc/user_guide.md); [di.hpp ~L1877, ~L1897](https://github.com/boost-ext/di/blob/cpp14/include/boost/di.hpp#L1877)
- **Lifetime diagram in the author's slides:** "Singleton scope" spans "Application life time", "Shared scope" spans "Injector life time", and "Shared scope / Instance scope aren't deduced by default". — [Bloomberg 2018 slides](https://github.com/boost-ext/di/blob/cpp14/doc/bloomberg-2018/index.html)
- **Issue #459:** "Singleton scope creates global singleton instead of per injector" (2020-02-20, v1.1.0, MSVC). Two injectors with `di::bind<int>.in(di::singleton)` returned the same object. Jusiak closed it as "Duplicate -> #431" without a linked code change. — [#459](https://github.com/boost-ext/di/issues/459)
- **Instance scope:** values passed via `bind<>.to(in)`. "[Boost].DI is not maintaining the life time of these objects, however, values and strings will be copied". `std::unique_ptr<T>` and `T*` cannot be bound as instances. — [user_guide.md](https://github.com/boost-ext/di/blob/cpp14/doc/user_guide.md)
- **Providers:** create objects; the default `stack_over_heap` "Creates objects on the stack whenever possible, otherwise on the heap". `heap` and custom providers can be set locally via `make_injector` or globally via `BOOST_DI_CFG`. — [user_guide.md, Providers](https://github.com/boost-ext/di/blob/cpp14/doc/user_guide.md)
- **Policies:** "Compile-time limitations for types / Run-time types visitor", for example the `constructible` policy. Config (`BOOST_DI_CFG`) customizes providers and policies. — [Bloomberg 2018 slides](https://github.com/boost-ext/di/blob/cpp14/doc/bloomberg-2018/index.html); [overview.md](https://github.com/boost-ext/di/blob/cpp14/doc/overview.md); [docs index](https://github.com/boost-ext/di/blob/cpp14/doc/index.md)
- **Extensions (20):** Injector, Constructor Bindings, Contextual Bindings, Assisted Injection, Extensible Injector, Concepts, Factory, Shared Factory, Lazy, Named Parameters, XML Injection, Serialize, Types Dumper, UML Dumper, Heap Provider, Mocks Provider, Runtime Provider, Scoped Scope, Session Scope and Shared Scope. — [extensions.md](https://github.com/boost-ext/di/blob/cpp14/doc/extensions.md)
- **Modules:** "all = all types exposed from module: auto configure(); exposed = one type exposed from module: di::injector<T> configure();" — [benchmarks.md](https://github.com/boost-ext/di/blob/cpp14/doc/benchmarks.md)
- **Lifetime and ownership bug reports (titles and dates from the issue tracker):**
  - Open:
    - #553 "Binding to LValue Reference becomes dangling" (2023-06-20)
    - #580 "Initializing value with lambda causes returning reference to a temporary" (2025-12-10)
    - #577 "di::extensions::factory recreates a singleton." (2025-11-19)
    - #575 "memory leak valgrind report" (2025-09-08)
    - #557 "enforce deduction of const std::shared_ptr<T> &" (2023-11-23)
    - #548 "Dynamic bindings don't work with explicit singleton scope" (2023-04-27)
    - #545 "Shared Scope does not destruct singletons in the correct order." (2023-02-07)
    - #527 "Shared scope behaves unexpectedly when the injector is a class member" (2021-08-24)
    - #460 "Extension shared_factory holds dangling reference to factory"
    - #474 "Vector data lost when passed to factory"
    - #461 "Runtime injector crashes on implementation with multiple interfaces"
    - #579 "Policies and contextual bindings are broken." (2025-12-03)

  — [issues list](https://github.com/boost-ext/di/issues?q=is%3Aissue+is%3Aopen+sort%3Acreated-desc); [issue search](https://github.com/boost-ext/di/issues?q=is%3Aissue+circular+OR+cycle+OR+msvc+OR+%22c%2B%2B20%22+OR+%22visual+studio%22)
- **Runtime vs compile-time inconsistency:** a search snippet of issue #528 says the runtime injector with the session extension creates objects with deduce scope, unlike the compile-time injector. — [#528](https://github.com/boost-ext/di/issues/528) (search snippet only)
- **Method injection:** not supported in core; #541 "Support for method injection?" (2022) is open. — [issues list](https://github.com/boost-ext/di/issues?q=is%3Aissue+is%3Aopen+sort%3Acreated-desc)

### Inferences
- **The same binding gives different lifetimes depending on how each consumer spells its parameter.** `di::bind<ilogger>.to<console_logger>()` yields one shared instance to classes taking `std::shared_ptr<ilogger>` or `ilogger&`, and a fresh instance to classes taking `std::unique_ptr<ilogger>` or `ilogger*`. A constructor-signature refactor can therefore silently change sharing semantics. This follows from the deduce table; it is not a documented warning.
- **Because singletons are function-local statics keyed by the binding types, they outlive injectors and are shared across all injectors in the process** (confirmed by #459 and the "Application life time" diagram). Consequences:
  - Unit tests that create a fresh injector per test case can leak state between tests unless they use the Shared Scope extension, which is per-injector.
  - Destruction follows static-destruction order.
  - Creation is thread-safe only to the extent of C++11 "magic statics".
- **`T*` parameters mean ownership transfer.** The injector news up the object and the consumer owns it, which is a leak risk for codebases where raw pointers mean "non-owning".
- **Assisted injection, factories and lazy injection are extensions with their own bug reports** (#577, #460, #474), so teams relying on them take on more risk than with the core.

### Gaps
- The content of issue #431 (the canonical "singleton is global" issue), and whether the maintainer called it by-design: not fetched.
- Whether the Shared Scope extension is the officially recommended per-injector alternative: implied by the slides' diagram, but not stated explicitly in the docs read.

## Key question 4: Current compilers (GCC 13-15, Clang 17-20, MSVC 17.x) and C++20/23 modes; open breakage reports

### Takeaway
Boost.DI generally works on current GCC and Clang in C++14 to C++20 code, but support is reactive. The two tagged-release problems with newer toolchains, the Clang/CMake C++20 module-scanning break and the TU-local warning when wrapped in a C++20 module, were fixed only on the untagged `cpp14` branch in 2026. CI covers only C++14 builds on GitHub-hosted MinGW g++, Ubuntu g++ and macOS clang++. MSVC is untested in CI and has old open bug reports. No source specifically documents MSVC 17.x (VS 2022) or C++23 status.

### Cited Findings
- **Documented support** is stale: "Clang-3.4+ / GCC-5.2+ / MSVC-2015+", linking to Travis CI and AppVeyor. — [overview.md](https://github.com/boost-ext/di/blob/cpp14/doc/overview.md)
- **CHANGELOG support additions** stop at 1.2.0 (2020): "Clang-9/Clang-10/XCode-11", "GCC-9/10" and "MSVC-2019". — [CHANGELOG](https://github.com/boost-ext/di/blob/cpp14/doc/CHANGELOG.md)
- **Current CI matrix:** windows-latest MinGW g++, ubuntu-latest g++ and macos-latest clang++, in Debug with tests and examples. CMake's default standard is C++14 unless the user sets `CMAKE_CXX_STANDARD`. There is no MSVC job and no standard-version matrix. — [build.yml](https://github.com/boost-ext/di/blob/cpp14/.github/workflows/build.yml); [CMakeLists.txt](https://github.com/boost-ext/di/blob/cpp14/CMakeLists.txt)
- **A sanitizer CI attempt was abandoned:** PR #520 "Add sanitizers to flags (Draft)" was closed on 2024-12-29. — [PR list](https://github.com/boost-ext/di/pulls?q=is%3Apr+sort%3Aupdated-desc)
- **Clang 18.1 and C++20 module scanning (#568, opened 2024-09-27, still open):**
  - Errors read "token is not a valid binary operator in a preprocessor subexpression" at `di.hpp:2104/2127` on `#if (defined(__CLANG__) && __CLANG__ >= 3'9)`.
  - The reporter traced it to CMake dependency scanning, "which newer CMake versions turn on by default for C++20 and above".
  - Jusiak initially could not reproduce it on Godbolt.

  — [#568](https://github.com/boost-ext/di/issues/568)
- **Same error with Clang modules (#572, 2025-03-03, open):** "'token is not a valid binary operator in a preprocessor subexpression' when using c++ modules with clang". — [issues list](https://github.com/boost-ext/di/issues?q=is%3Aissue+is%3Aopen+sort%3Acreated-desc)
- **Fix on the untagged branch (#584, 2026-04-04):** PR #584 "fix error with clang 21" changed both `3'9` occurrences to `39`. The v1.3.2 tag still contains `3'9` at lines 2104 and 2127; HEAD does not. — [commit 1c69ddc](https://github.com/boost-ext/di/commit/1c69ddc66975f082b0a0f7ab59f943863c86f016); git show v1.3.2 vs HEAD, 2026-10-30
- **C++20 module wrapping (#585, merged 2026-09-11):**
  - The warning reads "instantiation of 'make_injector<...>' triggers reference to TU-local entity 'make_injector' from other TU" `[-Wreference-tu-local-entity-in-other-tu]`, found "while wrapping library into C++20 module" (`export module boost_di;`).
  - The fix replaces `static auto make_injector = [](auto injector){...}` with a function template.
  - The compiler is not named; the warning style suggests Clang (inference by the fetch tool).

  — [PR #585](https://github.com/boost-ext/di/pull/585); [commit e35c8d9](https://github.com/boost-ext/di/commit/e35c8d94d82c2b0028ab47d0cca6554ca90a2afe)
- **GCC reports:**
  - #570 "new-delete-type-mismatch on GCC 14" (2024-11-05, open).
  - #559 "Factory with of object with shared_ptr parameter not compiling on gcc 13.2.1 and gcc 12.2.1" (2024-02-12, open).
  - #581 "1.3.2 fails to compile tests" (2026-01-26, open, no replies). With g++ `-std=gnu++14`, `test/ut/type_traits/ctor_traits.cpp` fails with "static assertion failed: fail".

  — [issues list](https://github.com/boost-ext/di/issues?q=is%3Aissue+is%3Aopen+sort%3Acreated-desc); [#581](https://github.com/boost-ext/di/issues/581)
- **Older MSVC reports:**
  - Open: #498 "MSVC 16.6.5 will not compile the simplest code using di.hpp!!" (2020-08-28), #468 "Compilation errors with MSVC 2019 with c++17 option enabled" (2020-05-18) and #462 "Multiple issues with VS 16.5.3" (2020-04-08).
  - Closed: #542 "Are you going to release a new version? Add support for C++20 with Visual Studio 2022 generator." (closed 2023-05-24, alongside v1.3) and #512 (MSVC release-build `shared_config`, 2021).
  - PR #513 "Fix issue with MSVC optimization" was merged in 2021-03.

  — [issue search](https://github.com/boost-ext/di/issues?q=is%3Aissue+circular+OR+cycle+OR+msvc+OR+%22c%2B%2B20%22+OR+%22visual+studio%22); [PR list](https://github.com/boost-ext/di/pulls?q=is%3Apr+sort%3Aupdated-desc)
- **Concepts TS:** #410 "test.injections_concepts: Fails compilation with -fconcepts" (2019, open). — [issue search](https://github.com/boost-ext/di/issues?q=is%3Aissue+circular+OR+cycle+OR+msvc+OR+%22c%2B%2B20%22+OR+%22visual+studio%22)
- **qlibs/di** claims "C++20 (clang++13+, g++11+)" and does not mention MSVC. It has no CI in the repo. — [header `di`](https://github.com/qlibs/di/blob/main/di)

### Inferences
- **For GCC 13-15 and Clang 17-20 in C++17/20 mode, the header is likely usable**, given Kagome's real use, the 2026 Clang 21 fix and active outside patchers. Teams should still:
  - pin to the `cpp14` HEAD commit rather than the v1.3.2 tag if they use Clang with CMake's C++20 module dependency scanning or wrap the header in a module. My understanding, not verified here, is that scanning is on by default for C++20 targets from CMake 3.28;
  - not expect the vcpkg/Conan 1.3.2 packages to contain these fixes.
- **MSVC 17.x is the biggest unknown:** no CI, no recent reports either way, and several unresolved 2020-era MSVC 2019 bugs.
- The macOS CI job uses Apple clang. `ubuntu-latest` currently implies a GCC 13-era compiler, but that is an inference about GitHub runner images, not stated in the repo.

### Gaps
- No source states results for GCC 15, Clang 19/20, MSVC 17.x or `-std=c++23` specifically.
- The CI run history (pass/fail per job) could not be viewed; the GitHub API is blocked. PR #585 showed 2 of 4 checks passing at merge.

## Key question 5: Published benchmarks (compile time, runtime overhead, binary size) vs Fruit and manual DI

### Takeaway
There are two published benchmark sets, and each was run by a library's own author: Boost.DI's (about 2016, Clang 3.x) and Fruit's (2020, Clang 10 and GCC 9). They reach opposite conclusions on compile time and binary size. Boost.DI's tiny no-interface, no-binding workload shows it near hand-written cost and far ahead of Fruit 2.0.2. Fruit's interface-heavy workload (90% of classes with 10 dependencies) shows Boost.DI compiling 2.6x to 9x slower than Fruit, needing up to 4.2 GB (Clang) or 7.5 GB (GCC) of compiler memory, and crashing GCC at 1000 classes. On runtime, both agree that object creation from a ready injector is cheap. No independent third-party benchmark was found.

### Cited Findings
- **Author claims:** "[Boost].DI has none run-time overhead and compiles faster than Java's DI frameworks". The Bloomberg 2018 slides add "No overhead (at least as fast as hand written)" and "Compiles faster than Java-Dagger2!". — [overview.md](https://github.com/boost-ext/di/blob/cpp14/doc/overview.md); [Bloomberg 2018 slides](https://github.com/boost-ext/di/blob/cpp14/doc/bloomberg-2018/index.html)
- **Author's method:** run-time performance tests "Compares generated assembler opcodes" (test/pt/di_run_time.cpp), and compilation-error tests "Validates error message". The run-time environment was an i7-4770 at 3.40 GHz with "clang++3.4 -O2 -fno-exceptions", using `gdb ... disassemble main`. `#include <boost/di.hpp>` took 0.050 s. — [Bloomberg 2018 slides](https://github.com/boost-ext/di/blob/cpp14/doc/bloomberg-2018/index.html); [benchmarks.md](https://github.com/boost-ext/di/blob/cpp14/doc/benchmarks.md)
- **Author-run comparison (benchmarks.md):** Boost.DI 1.0.1 vs Fruit 2.0.2, Guice 4.0, Dagger2 2.4 and Ninject 3.2. The workload was "Create a unique objects tree for each iteration", `BINDINGS: 0`, `ITERATIONS: 1024`, on "Clang-3.7 -O2 / Java8 / Mono4.2" on an i7-4770. Version 1.0.1 dates this to about May 2016.

  | Types | Compile time: baseline / Boost.DI / Fruit | Executable size: baseline / Boost.DI / Fruit | Execution time: baseline / Boost.DI / Fruit |
  |---|---|---|---|
  | 64 | 0.063 s / 0.376 s / 2.329 s ("FRUIT_NO_LOOP_CHECK") | 4.2K / 8.5K / 213K | 0.002 s / 0.002 s / 0.037 s |
  | 1024 | 0.402 s / 4.911 s / 1m9.601 s | 12K / 17K / 15M | 0.004 s / 0.004 s / 0.765 s |

  — [benchmarks.md](https://github.com/boost-ext/di/blob/cpp14/doc/benchmarks.md)
- **README per-example tables (about 2016-2017):** compilation time 0.102 s (Clang-3.8), 0.118 s (GCC-6) and 0.296 s (MSVC-2015); stripped binary size 6.2 kB, 6.2 kB and 105 kB. No methodology is given. — [README](https://github.com/boost-ext/di/blob/cpp14/README.md)
- **Fruit's benchmark wiki** (last edited 2020-05-02 by GitHub user poletti-marco):
  - Setup: Threadripper 1950X, Kubuntu 19.10, Clang 10.0.0 and GCC 9.2.1, `-O2 -DNDEBUG` (C++14 for Boost.DI). Codebases of 100, 250 and 1000 classes, 10% of classes with no dependencies and 90% with 10 dependencies, interfaces for all injected classes, 95% confidence intervals.
  - Library versions are not stated.

  | Measure (100 / 250 / 1000 classes) | Boost.DI | Fruit | Simple manual DI w/ interfaces |
  |---|---|---|---|
  | Full compile, Clang | 17 s / 58 s / 580 s | 6.6-6.7 s / 16 s / 64 s | 0.97 s / 1.9 s / 6.9 s |
  | Full compile, GCC | 18 s / 110 s / N/A ("GCC crashes (AFAICT due to it running out of memory)") | 5.1 s / 12 s / 48 s | — |
  | Incremental compile, Clang | 16 s / 57 s / 570-580 s | 3.9 s / 4 s / 6.1 s | — |
  | Compile memory, Clang | 343 MB / 772 MB / 4196 MB | 133 MB / 133 MB / 209 MB | — |
  | Compile memory, GCC | 572 MB / 1430 MB / 7534 MB | — | — |
  | Startup time, Clang | 5.7-5.8 ms / 6.2 ms / 8.4 ms | 7.3 ms / 6.6 ms / 9.2 ms | 5.4 ms / 6.2 ms / 6.5 ms |
  | Per-request (new injector each request), Clang | 47-50 µs / 150 µs / 710-740 µs | 2.5-2.6 µs / 8-8.1 µs / 85 µs | 1-1.1 µs / 2.8 µs / 13 µs |
  | Stripped executable, Clang | 576 KB / 1464 KB / 5664 KB | 390 KB / 947 KB / 3710 KB | 66 KB / 156 KB / 585 KB |
  | Stripped executable, GCC | 761 KB / 2050 KB / N/A | 390 KB / 957 KB / 3808 KB | — |

  — [Fruit benchmarks wiki](https://github.com/google/fruit/wiki/benchmarks)
- **Fruit wiki remarks:**
  - Boost.DI's incremental compile times are "huge".
  - A roughly 10-minute overhead at 1000 classes would significantly slow development.
  - Startup is "competitive", about 1-2 ms faster than Fruit.
  - Per request, Boost.DI "does not offer comparable functionality" to Fruit's `NormalizedComponent`, so injectors are rebuilt, making it "10-15x slower than Fruit in all cases".
  - Executables are about 30% larger than Fruit's with Clang and 2-3x larger with GCC.
  - Boost.DI is described as the second most popular C++ DI framework by GitHub stars, after Fruit (2020).

  — [Fruit benchmarks wiki](https://github.com/google/fruit/wiki/benchmarks)
- **qlibs/di:** no benchmarks published in its README. — [header `di`](https://github.com/qlibs/di/blob/main/di)

### Inferences
- The two datasets conflict because the workloads differ. The author's has zero bindings and no interfaces, so the optimizer can inline everything and Fruit's per-binding runtime structures dominate. Fruit's uses dense interface graphs, which stress Boost.DI's template recursion and type-list machinery. The two runs are also four years and many compiler versions apart (Clang 3.7 vs Clang 10). Each set was run by the competing library's author: Boost.DI's by Jusiak, and Fruit's wiki edited by Fruit's maintainer (inference from the editor name).
- The "no runtime overhead" claim holds narrowly, for creating a graph from a compile-time injector: Fruit's own data shows Boost.DI's startup at near manual-DI levels. It does not hold for build cost or binary size against hand-written wiring. Fruit's data shows simple manual DI with interfaces compiling about 17x (100 classes) to 84x (1000 classes) faster than Boost.DI on Clang, with binaries about 9x smaller.
- For large graphs (hundreds of bound interfaces) in one translation unit, compile memory is the practical risk, as GCC ran out of memory at 1000 classes. Splitting into exposed modules (`di::injector<T>`) is the documented mitigation, but no benchmark quantifying it with modern compilers was found.
- Any 2026 decision should re-measure: all published numbers are 6 to 10 years old, from compilers (Clang 3.7 to 10, GCC 9) that predate major template-instantiation performance changes.

### Gaps
- No independent (third-party) benchmark of Boost.DI vs Fruit vs manual DI was found.
- No benchmark with GCC 13+ or Clang 17+, and none for qlibs/di.
- The Fruit wiki does not state the Boost.DI and Fruit versions used.

## Key question 6: Compile-time diagnostics for missing or ambiguous bindings and cycles

### Takeaway
Missing bindings, duplicate bindings, constructor-arity ambiguity, too many constructor parameters and scope/type mismatches are all caught at compile time. The diagnostic is a `static_assert`-style "constraint not satisfied" plus a readable type-encoded reason and a fix hint, with adjustable verbosity (`BOOST_DI_CFG_DIAGNOSTICS_LEVEL` 0-2), and the library's test suite checks these messages. Dependency cycles have no dedicated diagnostic. The test suite expects pointer cycles to fail compilation, but a 2018 report shows a cycle compiling on MSVC and then hanging or producing null dependencies at runtime.

### Cited Findings
- **Design goals** include "Give short and intuitive error messages" and "Guarantee object creation at compile-time". The overview says "[Boost].DI is designed to give great diagnostic errors". — [docs index](https://github.com/boost-ext/di/blob/cpp14/doc/index.md); [overview.md](https://github.com/boost-ext/di/blob/cpp14/doc/overview.md)
- **Missing binding** (example, with `BOOST_DI_CFG_DIAGNOSTICS_LEVEL 1`): `di::make_injector().create<example>();` where `example(std::unique_ptr<interface>)` produces:

  "creatable constraint not satisfied", then "abstract_type<interface>::is_not_bound — type is not bound, did you forget to add: 'di::bind<interface>.to<implementation>()'?"

  — [creatable_abstract_type_is_not_bound.cpp](https://github.com/boost-ext/di/blob/cpp14/example/errors/creatable_abstract_type_is_not_bound.cpp)
- **Diagnostic levels:** "0 -> 'constraint not satisfied', 1 -> (0) + abstract type is not bound, 2 -> (1) + creation tree". Level 1 is the default. — [user_guide.md, Concepts](https://github.com/boost-ext/di/blob/cpp14/doc/user_guide.md); [di.hpp L23](https://github.com/boost-ext/di/blob/cpp14/include/boost/di.hpp#L23)
- **Duplicate (ambiguous) binding:** `di::make_injector(di::bind<int>.to(42), di::bind<int>.to(87))` produces `type<int>::is_bound_more_than_once`. The intended override uses `[di::override]`. — [boundable_type_is_bound_more_than_once.cpp](https://github.com/boost-ext/di/blob/cpp14/example/errors/boundable_type_is_bound_more_than_once.cpp)
- **Constructor/traits mismatch:** produces `type<T>::has_ambiguous_number_of_constructor_parameters::given<2>::expected<4>` with the hint "verify BOOST_DI_INJECT_TRAITS or di::ctor_traits". — [creatable_type_has_ambiguous_number_of_constructor_parameters.cpp](https://github.com/boost-ext/di/blob/cpp14/example/errors/creatable_type_has_ambiguous_number_of_constructor_parameters.cpp)
- **Other documented errors:**
  - `type<T>::has_to_many_constructor_parameters::max<Max>`
  - `scoped<TScope>::is_not_convertible_to<T>`
  - `scoped<instance>::is_not_convertible_to<T>`
  - `type<T>::is_abstract`
  - `type<T>::is_not_related_to`
  - `type<T>::has_disallowed_qualifiers`
  - `type<T>::is_neither_a_dependency_nor_an_injector`
  - `policy<TPolicy>::requires_<call_operator>`
  - `provider<TProvider>::requires_<get>`
  - `scope<TScope>::requires_<create>`

  — [user_guide.md, Concepts](https://github.com/boost-ext/di/blob/cpp14/doc/user_guide.md)
- **The message is generated by a `static_assert`:** `struct concept_check { static_assert(T::value, "constraint not satisfied"); };` — [di.hpp](https://github.com/boost-ext/di/blob/cpp14/include/boost/di.hpp)
- **Messages are verified by tests:** "Compilation-error tests ([Validates error message](https://github.com/boost-ext/di/blob/cpp14/test/pt/di_compile_time.cpp))". — [Bloomberg 2018 slides](https://github.com/boost-ext/di/blob/cpp14/doc/bloomberg-2018/index.html)
- **Messages vary by compiler:** issue #443 "Make errors test less dependent on compiler version" (2019) is still open. — [issue search](https://github.com/boost-ext/di/issues?q=is%3Aissue+circular+OR+cycle+OR+msvc+OR+%22c%2B%2B20%22+OR+%22visual+studio%22)
- **Cycles in the test suite:** `test/ft/di_errors.cpp` contains `circular_dependencies_simple` (`cd1(cd2*)` / `cd2(cd1*)`) and `circular_dependencies_complex` as `expect_compile_fail` tests. The header contains no "circular" or "cycle" diagnostic text. — [test/ft/di_errors.cpp](https://github.com/boost-ext/di/blob/cpp14/test/ft/di_errors.cpp); grep of [di.hpp](https://github.com/boost-ext/di/blob/cpp14/include/boost/di.hpp), 2026-10-30
- **Cycles in practice (#358, 2018, v1.0.2, VS2017 15.8.7):**
  - The reporter found "cycles are not detected/rejected": "I had the injector.create(...) hanging in _Init_thread_header", and "With -Zc:threadSafeInit- some object in the cycle was created with a null-dependency".
  - Collaborator Kanstantsin Chernik replied that "circular dependencies are code smell" and suggested the lazy extension or an extra abstraction layer such as `IMyLazyProvider`.
  - The issue was closed on 2018-11-29.

  — [#358](https://github.com/boost-ext/di/issues/358)
- **Missing-binding test cited in the docs:** benchmarks.md links a "Missing Bindings Error" comparison (`benchmark/bind_error/di.cpp` vs `fruit.cpp`, etc.). The doc's library table characterizes Boost.DI as "compile-time errors" and Fruit as "compile-time errors/exceptions". — [benchmarks.md](https://github.com/boost-ext/di/blob/cpp14/doc/benchmarks.md)
- **qlibs/di:** a missing binding yields `di::error<int, ...>`, and policies use user-written `static_assert`s. — [header `di`](https://github.com/qlibs/di/blob/main/di)

### Inferences
- Missing and duplicate binding diagnostics are a genuine strength compared with runtime-checked containers. The first error line is short and actionable, though in practice it sits inside a long template-instantiation backtrace; the 2018 Bloomberg slides themselves list "(-) Non-descriptive error messages" as a problem of template-based static wiring that concepts improve.
- **The #358 symptoms are consistent with the singleton implementation:** a cycle through singleton-deduced parameters (references or `shared_ptr`) re-enters the initialization of a function-local `static`. MSVC's thread-safe-statics guard then deadlocks; without the guard, a not-yet-constructed object is used. Pointer/unique cycles instead recurse at compile time. This is an inference linking #358 to the code at [di.hpp ~L1877](https://github.com/boost-ext/di/blob/cpp14/include/boost/di.hpp#L1877); it was not reproduced (nothing was built). Do not rely on Boost.DI to reject cycles at compile time in all scope combinations.

### Gaps
- No verbatim modern-compiler (GCC 13+, Clang 17+) error output was found. Nothing was compiled, per constraints.
- How cycles through `shared_ptr` or reference parameters behave on current GCC/Clang is unverified.

## Key question 7: Production use, packaging, community sentiment, and Kris Jusiak's talks

### Takeaway
Verified evidence of production-grade use is thin but real. The only confirmed open-source dependent found is Kagome (qdrvm/kagome, a C++ Polkadot node), which uses Boost.DI throughout its application injector via a pinned 1.1.0 fork. Packaging is good (vcpkg, Conan Center, Hunter, a Gentoo overlay, Salix OS), but not Homebrew. Sentiment is "impressive engineering, black magic, possibly unmaintained". Jusiak's talks argue that DI is "a 25-dollar term for a 5-cent concept": constructor injection matters more than any framework, and a library is justified only to remove the "wiring mess", preferably with compile-time errors.

### Cited Findings
- **Verified dependent: Kagome (Quadrivium/qdrvm):**
  - `core/injector/application_injector.cpp` contains `#include <boost/di.hpp>`, `#include <boost/di/extension/scopes/shared.hpp>`, and many `di::make_injector(...)` / `di::bind<runtime::ModuleFactory>.template to<runtime::wasm_edge::ModuleFactoryImpl>()` calls. — [Kagome application_injector.cpp](https://github.com/qdrvm/kagome/blob/master/core/injector/application_injector.cpp) (default branch HEAD 2025-06-18, read 2026-10-30)
  - The dependency is declared via Hunter as `hunter_add_package(Boost.DI)`, with the comment `# https://github.com/masterjedy/di`, pinned to `hunter_config(Boost.DI VERSION 1.1.0 ...)`. — [Kagome cmake/dependencies.cmake](https://github.com/qdrvm/kagome/blob/master/cmake/dependencies.cmake); [Kagome cmake/Hunter/config.cmake](https://github.com/qdrvm/kagome/blob/master/cmake/Hunter/config.cmake)
  - The related fork `qdrvm/boost-di` (default branch `cpp14`) was last committed on 2020-04-02 ("fix version error", with earlier "change namespace" and "make installable" commits). — [qdrvm/boost-di](https://github.com/qdrvm/boost-di)
- **Indirect or claimed use (not verified as production):**
  - A "Boost Your Design With Dependency Injection — Bloomberg, 2018" deck is in the repo, and a bloomberg.net-email contributor made 41 commits. — [Bloomberg 2018 slides](https://github.com/boost-ext/di/blob/cpp14/doc/bloomberg-2018/index.html); [commit history](https://github.com/boost-ext/di/commits/cpp14)
  - Jusiak's Feb 2021 Boost-list post reportedly claimed boost-ext libraries (except TE) were "successfully deployed in production systems" (search snippet only). — [Boost list 2021/02](https://lists.boost.org/Archives/boost/2021/02/250845.php)
  - The README has no "used by" list. — [README](https://github.com/boost-ext/di/blob/cpp14/README.md)
- **vcpkg:** port `bext-di`, version 1.3.2, "C++14 Dependency Injection Library.", with an optional `extensions` feature. Version history: 1.2.0 (port-versions 0 and 1), 1.3.0 (0 and 1), 1.3.2 (0). — [vcpkg ports/bext-di](https://github.com/microsoft/vcpkg/blob/master/ports/bext-di/vcpkg.json); [versions/b-/bext-di.json](https://github.com/microsoft/vcpkg/blob/master/versions/b-/bext-di.json) (vcpkg HEAD 2026-10-29)
- **Conan Center:** recipe `di` with versions 1.1.0, 1.2.0, 1.3.0 and 1.3.2, all sourcing from github.com/boost-ext/di archives. — [CCI recipes/di/config.yml](https://github.com/conan-io/conan-center-index/blob/master/recipes/di/config.yml); [conandata.yml](https://github.com/conan-io/conan-center-index/blob/master/recipes/di/all/conandata.yml) (CCI HEAD 2026-10-29)
- **Homebrew:** not packaged. Homebrew-core's `Formula/d/di.rb` is an unrelated "Advanced df-like disk information utility". — [homebrew-core di.rb](https://github.com/Homebrew/homebrew-core/blob/master/Formula/d/di.rb) (HEAD 2026-10-30)
- **Linux distros:**
  - Gentoo has it only in a third-party overlay listing (`dev-libs/boost-di` on gpo.zugaina.org). — [gpo.zugaina.org](https://gpo.zugaina.org/dev-libs/boost-di)
  - Salix OS (Slackware-based) ships `boost-di-1.0.1`. — [Salix package listing](https://backup.salixos.org/daily.4/repo/i486/extra-15.0/salix/development/boost-di-1.0.1-noarch-1salix15.0.txt)
  - Debian, Fedora and Arch official repos were not verified (Repology not reachable).
- **qlibs/di packaging:** none. — [vcpkg ports](https://github.com/microsoft/vcpkg/tree/master/ports); [CCI recipes](https://github.com/conan-io/conan-center-index/tree/master/recipes)
- **Community sentiment:**
  - r/cpp_questions, "Dependency injection at scale" (about 2025; search snippet of a mirror):
    - Boost.DI "is an interesting one, an incredible piece of engineering but afaik it isn't maintained", with warnings of "some subtle unexpected behaviours".
    - Another commenter: "we didn't use boost.di coz we didn't want to bring boost as a dependency". This is a misconception, since the library needs no Boost.
    - Another called it "some kind of awesome black magic".

    — [r/cpp_questions thread (mirror)](https://redlib.hackliberty.org/r/cpp_questions/comments/1l4wbpp/dependency_injection_at_scale)
  - r/cpp, Aug 27 2024: the qlibs/di announcement thread (also mirrored as "DIY - Dependency Injection Yourself"), where Jusiak contrasted it with boost-ext.di (see Key question 2). — [r/cpp (mirror)](https://lr.eu.psf.lt/r/cpp/comments/1f2fpqr/c20_dependency_injection); [Telegram r_cpp mirror](https://t.me/s/r_cpp/20550)
  - A Russian-language Habr post's author found Boost.DI's source hard to follow (search snippet). — [Habr 722940](https://habr.com/ru/post/722940)
  - A dou.ua article walks through kangaru and Boost.DI on a sample banking app (search snippet). — [dou.ua](https://dou.ua/lenta/articles/frameworks-for-dependencies/)
  - A 2016 independent blog post exists. — [Daniel Kraic, "Boost DI" (2016-05-27)](https://danielkraic.github.io/2016/05/27/boost-di/)
  - An isocpp.org blog item links a Boost.DI talk video. — [isocpp.org 2020-11](https://isocpp.org/blog/2020/11/dependency-injection-with-boost.di)
  - No Hacker News threads were found, and no notable Stack Overflow threads surfaced in searches.
- **Independent talk:** "Utah C++ Programmers: Dependency Injection with Boost.DI" was added to the docs via PR #530 (merged 2021-10-27, by GitHub user LegalizeAdulthood). — [docs index](https://github.com/boost-ext/di/blob/cpp14/doc/index.md); [PR list](https://github.com/boost-ext/di/pulls?q=is%3Apr+sort%3Aupdated-desc); [video](https://www.youtube.com/watch?v=zMTAc8WHIgo)
- **Jusiak's talks** (listed in the docs):
  - C++Now 2016, "C++14 Dependency Injection Library" ([video](https://www.youtube.com/watch?v=comZthFv3PM))
  - Meeting C++ 2016, "TDD/BDD and Dependency Injection" ([video](https://www.youtube.com/watch?v=T3uMcxhzRUE))
  - C++Now 2017, "Concepts driven design with Dependency Injection" ([video](https://www.youtube.com/watch?v=50VIYIHlUJE))
  - C++ London 2017 (Skills Matter), "Boost your design with C++14 dependency injection"
  - CppCon 2018, "[Boost].DI - Inject all the things!" ([video](https://www.youtube.com/watch?v=8HmjM3G8jhQ))
  - C++Now 2019, "Dependency Injection - a 25-dollar term for a 5-cent concept" ([video](https://www.youtube.com/watch?v=yVogS4NbL6U))

  — [docs index](https://github.com/boost-ext/di/blob/cpp14/doc/index.md)
- **C++Now 2019 arguments (slides):**
  - "DI doesn't imply using a library/framework".
  - "Whether DI is done right depends on what and how will be passed into constructors".
  - Wiring "Separates the creation logic from the business logic", with "No raw new/make_unique/etc... except in the wiring" and a single "Composition root".
  - Gotchas: singletons, "carrying dependencies", Law of Demeter, weak types.
  - "Writing a DI library ain't easy in C++": no reflection, value categories, east/west const, error reporting.
  - "Prefer compile- and link-time errors to run-time errors".
  - DI-library gotchas: "Consider avoiding global injector/containers", "Consider Service Locator to be antipattern", "Consider not coupling code to a specific DI framework".
  - Summary: DI libraries "(+) Promotes loosely coupled code, (+) Removes / Cleanup the wiring mess, (+) Simplify refactoring / Adding new features, (+) Makes testing / enforcing guidelines easier, (-) Steeper learning curve"; "DI is just a fancy term for using constructors (25 dollar concept...)", "DI can be easily misused", "DI library/framework helps with the wiring mess".

  — [C++Now 2019 slides](https://github.com/boost-ext/di/blob/cpp14/doc/cppnow-2019/index.html)
- **C++Now 2019 comparison of libraries:**
  - Hypodermic: header only, MIT, STL plus Boost dependencies, C++11, "Run-time".
  - Google.Fruit: "Library", Apache-2, C++11, "Semi compile-time".
  - [Boost].DI: "Header only (Single header)", Boost 1.0, "Dependencies: None", C++14, "Compile-time/Run-time".

  — [C++Now 2019 slides](https://github.com/boost-ext/di/blob/cpp14/doc/cppnow-2019/index.html)
- **CppCon 2018 claims:** "One header - 3k LOC" and "No 'if's, 'virtual's, 'exception's, macros, run-time errors", with side-by-side manual DI vs Boost.DI for values, abstractions and templates/concepts, and injecting mocks via GUnit. — [CppCon 2018 slides](https://github.com/boost-ext/di/blob/cpp14/doc/cppcon-2018/index.html)
- **Bloomberg 2018 quality claims:** CI with `-Wall -Wextra -Werror -pedantic -pedantic-errors`, clang static analysis and clang-tidy, Valgrind/Dr. Memory, "Unit tests (99% test code coverage)", "100+ examples / 15+ extensions". — [Bloomberg 2018 slides](https://github.com/boost-ext/di/blob/cpp14/doc/bloomberg-2018/index.html)

### Inferences
- **Kagome is a meaningful real-world signal** (a large, security-sensitive C++20-era node implementation), but it pins a 2019 fork (1.1.0) rather than tracking upstream. That suggests a vendor-and-freeze adoption pattern, which is probably the realistic way to use Boost.DI in production in 2026.
- **The 2018 quality claims describe the Travis/AppVeyor era.** Today's CI has no static analysis, sanitizer or coverage jobs and no MSVC (see Key question 4), so the claims should not be read as current.
- **Jusiak's own guidance cuts against heavy framework lock-in.** He recommends not coupling code to a DI framework and treats a library as optional. Teams can therefore get most of the design benefit with hand-written constructor injection and adopt the library only where wiring churn hurts.

### Gaps
- No verified commercial production users; the King and Bloomberg links are indirect.
- GitHub "Used by" or dependents counts are unavailable (API blocked).
- Debian, Fedora and Arch packaging status is unverified (Repology, vcpkg.io and conan.io are blocked; vcpkg and Conan facts come from their GitHub repos instead).
- The full text of the Reddit threads could not be read (mirrors blocked); only search snippets were available.
