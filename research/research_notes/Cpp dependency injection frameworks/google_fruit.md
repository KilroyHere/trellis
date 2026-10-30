# Google Fruit (github.com/google/fruit): profile as of 2026-10-30

Method note: I took repository facts from a blobless `git clone` of https://github.com/google/fruit.git and of its wiki (https://github.com/google/fruit.wiki.git), both made on 2026-10-30. Commit counts and dates below come from `git log` on `master`. Issue, PR, star and CI numbers come from github.com pages fetched on 2026-10-30. Nothing was built or run. Some hosts were unreachable from this session: reddit.com and stackoverflow.com (the search tool refused them), and repology.org, repos.ecosyste.ms, habr.com, tracker.debian.org and pistack.xyz (DNS blocked). Facts from those sites rest on search-engine snippets only and are marked as such.

## Q1. Per-library profile for Fruit (fields 1 to 9, at a glance; details in Q2 to Q11)

### Takeaway
Fruit is a mature, Apache-2.0, Guice-inspired C++11 DI library. Its components are type-checked at compile time, but it is a compiled (not header-only) library whose injector is built at runtime. It has one main author (Marco Poletti). As of 2026-10-30 it is in low-activity maintenance mode: the last release was v3.7.1 on 2023-01-21, the maintainer's last commit was on 2025-05-11, and in the past 12 months there were 2 commits, both from outside contributors. Linux CI passes, but Windows CI fails and macOS CI does not run.

### Cited Findings
- **1. Identity.** Repo https://github.com/google/fruit. Author and main maintainer: Marco Poletti (GitHub `poletti-marco`). License: Apache 2.0. First commit 2014-06-21; first tag v0.9.0 on 2014-06-21; v1.0.0 on 2014-11-09 — [git tags](https://github.com/google/fruit/tags); [COPYING](https://github.com/google/fruit/blob/master/COPYING); [wiki Home](https://github.com/google/fruit/wiki)
- **2. Maintenance (2026-10-30).** Latest release/tag v3.7.1 (2023-01-21). Latest commit on `master`: 2026-04-11 ("Modernize bazel config", Keith Smiley). Commits: 2 in the past 12 months, 17 in the past 24 months. 32 open issues; 1 open PR (#133, from Apr 2021). No archival or deprecation notice — [commits](https://github.com/google/fruit/commits/master); [issues](https://github.com/google/fruit/issues); [pulls](https://github.com/google/fruit/pulls)
- **3. Adoption (2026-10-30).** About 1.9k stars, 201 forks, 83 watchers. Packaged in vcpkg (3.7.1), Conan Center (3.4.0 to 3.7.1), Homebrew (3.7.1), Debian/Ubuntu as `google-fruit` 3.7.1-1 (per search snippets), and the Arch AUR (`google-fruit` 3.7.1-1, per search snippet). Verified production user: Google's Android Cuttlefish host tools — [GitHub repo page](https://github.com/google/fruit); [vcpkg port](https://github.com/microsoft/vcpkg/blob/master/ports/fruit/vcpkg.json); [Conan Center config](https://github.com/conan-io/conan-center-index/blob/master/recipes/fruit/config.yml); [Homebrew formula](https://github.com/Homebrew/homebrew-core/blob/master/Formula/f/fruit.rb); [Cuttlefish run_cvd](https://github.com/google/android-cuttlefish/blob/HEAD/base/cvd/cuttlefish/host/commands/run_cvd/main.cc)
- **4. Technical model.** Hybrid: compile-time type checking of `fruit::Component<fruit::Required<...>, Provided...>`, with non-templated binding storage that is "normalized" at runtime when an `Injector` is constructed. Constructors are discovered through an explicit `INJECT(...)` macro, an `using Inject = C(Args...)` typedef, or `registerConstructor<C(Args...)>()`. Other bindings: `bind<I, C>()`, `registerProvider(lambda)`, `bindInstance(obj)`, `registerFactory` with `fruit::Assisted<>`/`ASSISTED()`, `fruit::Annotated<Tag, T>`, `addMultibinding<I, C>()`, `install(getXComponent, args...)`, `NormalizedComponent`, `fruit::Provider<T>` and `replace(...).with(...)`. The only lifetime is one instance per injector; there are no scopes. C++11 minimum. Compiled library (link `-lfruit`). Boost is optional and needed only when building Fruit itself. RTTI is optional. Fruit throws no exceptions; runtime injection errors print to stderr and call `exit(1)` — [wiki Home](https://github.com/google/fruit/wiki); [Quick reference](https://github.com/google/fruit/wiki/quick-reference); [Install](https://github.com/google/fruit/wiki/install); [binding_normalization.cpp](https://github.com/google/fruit/blob/master/src/binding_normalization.cpp)
- **5. Diagnostics.** Caught at compile time: missing bindings (`NoBindingFoundError`), dependency loops (`SelfLoopError`) and duplicate bindings visible in component types (`TypeAlreadyBoundError`, `DuplicateTypesInComponentError`). `injection_errors.h` defines 43 named error types. Caught at runtime (fatal): inconsistent bindings hidden in different components, components that `install()` each other, a provider returning `nullptr`, and `replace()` placed after `install()` — [injection_errors.h](https://github.com/google/fruit/blob/master/include/fruit/impl/injection_errors.h); [wiki Home](https://github.com/google/fruit/wiki)
- **6. Performance.** The author's benchmarks (wiki last edited 2020-05-02; Threadripper 1950X, Clang 10 / GCC 9.2.1) put Fruit at roughly 4 to 5 s of extra cold compile time for 100 classes compared with hand-written DI. Startup overhead is at most 3.4 ms. Per-request injector creation takes 2.5 to 100 µs with `NormalizedComponent`. The binary grows by about 3 to 4 KB per injected class. In these numbers Fruit beats Boost.DI on compile time, compile memory and per-request time, and loses on startup by 1 to 2 ms. Boost.DI's author published opposite compile-time and size results in 2016, against the older Fruit 2.0.2 — [Benchmarks](https://github.com/google/fruit/wiki/benchmarks); [Boost.DI benchmarks.md](https://github.com/boost-ext/di/blob/cpp14/doc/benchmarks.md)
- **7. Documentation.** A GitHub wiki: Home, FAQ, Install, a roughly 46 KB Quick reference, and 7 tutorial pages, plus 7 example directories. Most tutorial pages were last edited on 2017-08-06; the wiki's last edit was 2020-10-31 — [wiki](https://github.com/google/fruit/wiki); [examples](https://github.com/google/fruit/tree/master/examples)
- **8. Criticisms.** The macro is intrusive on constructors, though a typedef or `registerConstructor` avoids it. Other complaints: no scopes or child injectors (#74, open since 2018); `registerProvider` accepts only captureless lambdas; component-function arguments must be hashable and equality-comparable; runtime errors call `exit(1)`; template-heavy error output; long-open MSVC, Bazel-on-Windows and `unique_ptr` factory issues; no release since Jan 2023 — [Quick reference](https://github.com/google/fruit/wiki/quick-reference); [issues](https://github.com/google/fruit/issues)
- **9. Fit.** See Q11. In short: a reasonable fit for large, modular, interface-heavy C++ codebases built with GCC/Clang on Linux, by teams willing to pin or patch master. Red flags are the bus factor of 1, the stale releases, decayed Windows/macOS CI, and fatal (non-recoverable) runtime errors.

### Inferences
- Every profile field can be filled from primary sources except verified third-party production users beyond Google's Cuttlefish and first-hand community reports; see Q8.

### Gaps
- Exact star count (GitHub shows only "1.9k"), the "Used by" dependents count (not rendered) and the current GitHub contributors count (not rendered) are unknown.

## Q2. History: who wrote it, what "not an official Google product" means, and release history

### Takeaway
Fruit was written almost entirely by Marco Poletti (not "Poletto"). He has 1,312 of 1,352 commits (about 97%), under two author identities. The code carries "Copyright 2014 Google Inc." headers and sits in the `google` GitHub org. The project explicitly says Google does not support it. Releases ran from 0.9.0 (June 2014) through 3.7.1 (January 2023); since then there has been no release.

### Cited Findings
- The author's name is **Marco Poletti**. `git shortlog` (2026-10-30) shows 1,300 commits by "Marco Poletti <poletti.marco@gmail.com>" plus 12 by "poletti-marco" (same email), out of 1,352 total. The next contributors are tt4g (22) and Timo Lange (4); everyone else has 1 or 2 — [commit history](https://github.com/google/fruit/commits/master)
- The wiki (2017 text) says: "Currently Marco Poletti ... is the only regular developer of this project." It lists occasional contributors Jason Mealler and Maxwell Koo — [wiki Home](https://github.com/google/fruit/wiki)
- The FAQ says the project "was started in May 2014, Fruit 1.0.0 was released in November 2014 and Fruit 3.0.0 was released in August 2017." The first git commit is dated 2014-06-21 — [FAQ](https://github.com/google/fruit/wiki/faq); [first commit, via commit history](https://github.com/google/fruit/commits/master)
- The name comes from Guice (pronounced "juice"): "If you'd like some Guice but don't want to wait (for run-time errors), have some Fruit." — [FAQ](https://github.com/google/fruit/wiki/faq)
- Source headers read "Copyright 2014 Google Inc. All rights reserved." under Apache 2.0 — [include/fruit/fruit.h](https://github.com/google/fruit/blob/master/include/fruit/fruit.h)
- Disclaimer: "This project is not an official Google project. It is not supported by Google and Google specifically disclaims all warranties as to its quality, merchantability, or fitness for a particular purpose." — [wiki Home](https://github.com/google/fruit/wiki)
- Google's open-source release guide (via search summary) requires non-product projects to carry an "not an officially supported Google product" notice. It explains that such projects "may not be staffed like larger, supported products ... and support and/or new releases may be limited." — [opensource.google: Releasing and Publicizing](https://opensource.google/docs/releasing/publishing/) (search-result summary; page not fetched)
- In 2015 a user asked in issue #5 how Google uses Fruit internally. The thread never answered that question — [issue #5](https://github.com/google/fruit/issues/5)
- Release and tag history from `git tag --sort=-creatordate`:
  - 3.x: v3.7.1 2023-01-21; v3.7.0 2022-12-03; v3.6.0 2020-07-12; v3.5.0 2020-04-05; v3.4.0 2018-09-12; v3.3.0 2018-09-09; v3.2.0 2018-07-24; v3.1.1 2018-01-13; v3.1.0 2017-11-19; v3.0.0 2017-08-06.
  - 2.x: v2.1.0 2017-08-06 (a migration release on a separate `2.1.x` branch); v2.0.4 2016-05-25; v2.0.0 2016-03-27.
  - 1.x and 0.9.x: v1.0.2 2015-02-28; v1.0.0 2014-11-09; v0.9.0 2014-06-21.
  — [tags](https://github.com/google/fruit/tags)
- Release-note highlights:
  - 3.0.0: `install()`/`Injector`/`NormalizedComponent` take component functions plus arguments; installed components are de-duplicated; "GCC <5.0.0 is no longer supported"; Windows support for MSVC and MinGW; "large performance and executable-size improvements".
  - 3.1.0: compile-time errors for missing virtual destructors.
  - 3.5.0: requires CMake 3.2.
  - 3.6.0: "MSVC 2015 is no longer supported (only MSVC 2017 or 2019 are)".
  - 3.7.0: movable-only factory arguments, a move to GitHub Actions, CI with GCC 12 / Clang 15 / MSVC 2022.
  - 3.7.1: `T&`/`T&&` assisted parameters; "Fruit is now again compatible with Visual Studio 2022."
  — [releases](https://github.com/google/fruit/releases)
- Commits per calendar year (git, `master`): 2014: 261; 2015: 146; 2016: 275; 2017: 277; 2018: 81; 2019: 76; 2020: 112; 2021: 62; 2022: 27; 2023: 11; 2024: 7; 2025: 16; 2026 (to 10-30): 1 — [commit history](https://github.com/google/fruit/commits/master)

### Inferences
- In practice, "not an official Google product" here means: Google owns the copyright (the code was presumably written under Google's IP terms) and hosts it in its org, but no Google team staffs it, there is no SLA, and maintenance depends on one individual working under a personal email.
- Development peaked in 2014 to 2017. From 2018 onward the project has been incremental, and since 2023 almost entirely compatibility upkeep.

### Gaps
- Marco Poletti's current employer, and whether he was a Google employee when he wrote Fruit, are not confirmed by any source fetched (the Google copyright suggests he was).
- I found no conference talk by Marco Poletti about Fruit (search returned none).

## Q3. Maintenance status as of 2026-10-30

### Takeaway
The project is not archived or deprecated. It is in reactive, low-activity maintenance:
- The maintainer still answers and closes issues (most recently in January 2026).
- His last commit was 2025-05-11.
- The last 12 months saw only 2 small outside commits (an Android C++23 include fix and a Bazel bzlmod migration).
- 26 commits sit unreleased on master since v3.7.1, including CMake 4.0 and C++23 fixes, so downstream packagers carry patches.
- Scheduled CI shows Linux green, Windows failing and macOS cancelled every month from June to October 2026.

### Cited Findings
- Latest commit on `master` (default branch): `092cea3`, 2026-04-11, Keith Smiley, "Modernize bazel config". Its message: "This makes this repo build with bzlmod ... This works inside google but not with bazel." It adds `MODULE.bazel` and removes `WORKSPACE` files — [commit 092cea3](https://github.com/google/fruit/commit/092cea3f1408ff20e26334db203a923c581506ba)
- The previous commit is `45cc058`, 2025-12-27, Sharjeel Khan (google.com address): "Add missing <type_traits> include for C++23 in Android ... In Android, we need the added <type_traits> to build with C++23." — [commit 45cc058](https://github.com/google/fruit/commit/45cc0583a170bc29a41e02e10911b1dee699c60c)
- Activity windows (git, `master`):
  - Since 2025-10-30: 2 commits (Keith Smiley 1, Sharjeel Khan 1; none by the maintainer).
  - Since 2024-10-30: 17 commits (Marco Poletti 14, plus 1 each from Keith Smiley, Patrick O'Connell and Sharjeel Khan).
  - Marco Poletti's last commit: 2025-05-11.
  — [commit history](https://github.com/google/fruit/commits/master)
- The 2025 maintainer commits are compatibility and correctness work:
  - CI updated to Ubuntu 24.10, Clang 18/19 and GCC 14; C++23 fixes for Clang 19 (issue #164), all on 2025-02-01.
  - `cmake_minimum_required(VERSION 3.2...4.0)` for CMake 4.0 (2025-04-06).
  - "Allow binding a type to itself if the annotations are different. Fixes #167" (2025-05-10).
  - A compile-time failure when no arguments are passed to a `get*Component` function that takes arguments (2025-05-11).
  — [commit history](https://github.com/google/fruit/commits/master); [CMakeLists.txt](https://github.com/google/fruit/blob/master/CMakeLists.txt)
- `master` has 26 commits after v3.7.1 (`git rev-list --count v3.7.1..HEAD`), and `CMakeLists.txt` still says `project(Fruit VERSION 3.7.1 ...)` — [CMakeLists.txt](https://github.com/google/fruit/blob/master/CMakeLists.txt)
- Homebrew's formula for 3.7.1 applies a backported upstream commit, "Update cmake_minimum_required for compatibility with CMake ... Remove on next release." — [Homebrew fruit.rb](https://github.com/Homebrew/homebrew-core/blob/master/Formula/f/fruit.rb)
- Issues (2026-10-30): 32 open; the unfiltered issue search shows 140 results (so about 108 closed). The newest issue is #172 ("C++23 build errors in Android", closed 2025-12-28). Open issues stretch back to 2017 (#50, #51). PRs: 1 open (#133 "Update README.md", opened 2021-04-09) and 31 closed — [issues](https://github.com/google/fruit/issues?q=is%3Aissue%20sort%3Acreated-desc); [pulls](https://github.com/google/fruit/pulls)
- Responsiveness, issue #171 ("Abnormal exit occurs in Qt"): opened 2025-12-23. poletti-marco replied the same day ("traveling ... suggested Valgrind ... same compiler settings, including debug vs. release"), followed up on 2026-01-19, and closed it on 2026-01-25 "due to lack of response" — [issue #171](https://github.com/google/fruit/issues/171)
- Recent issues closed quickly with fixes: #164 (C++23 build errors) closed 2025-02-03 after commits on 2025-02-01; #166 ("Compilation fails with cmake 4.0") closed 2025-04-06, the same day as the fix commit; #167 and #168 closed 2025-05-11/12 alongside fixes; #170 (non-owning provider feature request) closed 2025-05-18 — [issue list](https://github.com/google/fruit/issues?q=is%3Aissue%20sort%3Acreated-desc); [commit history](https://github.com/google/fruit/commits/master)
- Long-open issues include:
  - #122 (Bazel on Windows/MSVC, 2020, 25 comments)
  - #126 (`registerFactory` with `Assisted<std::unique_ptr<...>>`, 2020, 22 comments)
  - #138 ("Fruit does not build in Visual Studio Versions 2019, 2022 ... with CMake", 2022)
  - #152 (factories for classes with multiple constructors, 2023)
  - #163 (FindFruit after installing via vcpkg, Oct 2024)
  - #169 ("fruit Usage inquiries", May 2025)
  — [open issues](https://github.com/google/fruit/issues)
- CI: all three workflows (Linux, OS X, Windows) run on push, on PR and on a monthly cron `'13 8 1 * *'` — [test-on-linux.yml](https://github.com/google/fruit/blob/master/.github/workflows/test-on-linux.yml)
- On the Actions page (fetched 2026-10-30), every monthly run from 2026-06-01 to 2026-10-01 shows "Test on Linux: Success", "Test on OS X: Cancelled", "Test on Windows: Failure" — [GitHub Actions](https://github.com/google/fruit/actions)
- The OS X workflow targets runners `macos-10.15`, `macos-11` and `macos-12`. The Windows workflow targets `windows-2019` and `windows-2022` (MSVC 2019/2022) and adds a MinGW job on `windows-2022` — [test-on-osx.yml](https://github.com/google/fruit/blob/master/.github/workflows/test-on-osx.yml); [test-on-windows.yml](https://github.com/google/fruit/blob/master/.github/workflows/test-on-windows.yml)
- Open Hub rates the project "Very Low Activity" (search snippet; snapshot date unknown) — [Open Hub](https://openhub.net/p/fruit)

### Inferences
- Bus factor is 1. About 97% of all commits, and 14 of the 17 commits in the last 24 months, are Marco Poletti's. The two commits in the last 12 months came from people with their own downstream needs (Android/Google, Bazel).
- The status is best described as "maintenance mode, not abandoned". New-compiler and build-tool breakages have been fixed within days when reported (Feb, Apr and May 2025). But there have been no feature additions and no release in more than 3.5 years. The scheduled CI failures on Windows have persisted for at least 5 months.
- The macOS runs are probably "Cancelled" because GitHub has retired those runner labels (macos-10.15/11/12). The Windows failure may involve the `windows-2019` image. Both points come from general knowledge of GitHub's runner retirements and were not verified in this session. Either way, macOS and Windows regressions on master currently go undetected.
- Anyone consuming packaged 3.7.1 (vcpkg, Conan, Homebrew, distros) misses the CMake 4.0, C++23 and GCC 14 fixes unless the packager patches them in, as Homebrew does. Google's Cuttlefish pins a master commit instead (see Q8).

### Gaps
- I could not see who closed each issue other than #171, nor the median time to first response across all issues.
- The cause of the Windows CI failures was not inspected (the logs were not fetched).
- No explicit statement on maintenance mode, a roadmap or a next release exists in the repo or wiki.

## Q4. Technical model

### Takeaway
Fruit follows Guice's model. Components are functions that return `fruit::Component<fruit::Required<Deps...>, Provides...>`, built with a fluent builder (`createComponent().bind<>()...`) and composed with `install()`. Template metaprogramming checks each component's provided and required sets and its loops at compile time. At runtime an `Injector` normalizes all bindings into non-templated hash-map storage and lazily creates exactly one instance per type per injector. There are no scopes. Request-like lifetimes are handled by creating cheap per-request injectors from a pre-computed `NormalizedComponent`.

### Cited Findings
- Core description: "It uses C++ metaprogramming together with some C++11 features to detect most injection problems at compile-time. It allows to split the implementation code in 'components' (aka modules) that can be assembled to form other components. From a component with no requirements it's then possible to create an injector." — [README](https://github.com/google/fruit/blob/master/README.md)
- The FAQ says "the storage that backs injectors and components is *not* templated." Kris Jusiak (Boost.DI's author) classifies Fruit's approach as "Semi compile-time" in his C++Now 2019 slides — [FAQ](https://github.com/google/fruit/wiki/faq); [C++Now 2019 slides, boost-ext/di repo](https://github.com/boost-ext/di/blob/cpp14/doc/cppnow-2019/index.html)
- Example (adapted from `examples/hello_world/main.cpp`; `Writer`/`StdoutWriter` are defined the same way, with `INJECT(StdoutWriter()) = default;`) — [examples/hello_world/main.cpp](https://github.com/google/fruit/blob/master/examples/hello_world/main.cpp):
  ```cpp
  class Greeter { public: virtual void greet() = 0; };
  class GreeterImpl : public Greeter {
    Writer* writer;
  public:
    INJECT(GreeterImpl(Writer* writer)) : writer(writer) {}   // marks the ctor used for injection
    void greet() override { writer->write("Hello world!\n"); }
  };
  fruit::Component<Greeter> getGreeterComponent() {            // a component is a function
    return fruit::createComponent()
        .bind<Writer, StdoutWriter>()
        .bind<Greeter, GreeterImpl>();
  }
  int main() {
    fruit::Injector<Greeter> injector(getGreeterComponent);      // bindings normalized at runtime
    injector.get<Greeter*>()->greet();
  }
  ```
- **Constructor discovery is explicit, never by arity:**
  - `INJECT(Signature)` expands to `using Inject = Signature;` plus helper alias templates, followed by the signature.
  - `ASSISTED(...)` and `ANNOTATED(Annotation, ...)` are macro markers used inside it.
  — [include/fruit/macro.h](https://github.com/google/fruit/blob/master/include/fruit/macro.h)
- Alternatives to the macro: an `Inject` typedef (`using Inject = C(Args...)`) and `registerConstructor<...>()`. Fruit is "Not intrusive. You can bind interfaces and classes without modifying them (e.g. from a third-party library that doesn't use Fruit)." — [Quick reference](https://github.com/google/fruit/wiki/quick-reference); [wiki Home](https://github.com/google/fruit/wiki)
- "Even 0-argument constructors must be explicitly registered ... Fruit never uses a constructor unless that constructor has been explicitly marked ... to avoid undesired constructor injection of value types; e.g. if `std::vector<SomeType>` is not bound ... Fruit reports an error ... rather than silently constructing an empty vector." — [Quick reference](https://github.com/google/fruit/wiki/quick-reference)
- **Component types:** `fruit::Component<fruit::Required<T1, T2, T3>, U1, U2>` exposes U1 and U2 and requires T1 to T3. Requirements are satisfied by bindings or by installed components. Types with an `Inject` typedef that are needed but not declared are auto-bound ("automatic binding"). Types that are only required need just a forward declaration — [Quick reference](https://github.com/google/fruit/wiki/quick-reference)
- **Interface bindings:** `bind<Interface, Impl>()` (Impl must derive from Interface). It also binds factories: `std::function<std::unique_ptr<Interface>(T1..Tn)>` binds to `std::function<std::unique_ptr<Impl>(T1..Tn)>` — [Quick reference](https://github.com/google/fruit/wiki/quick-reference)
- **Providers:** `registerProvider([](DbConnectionPool* pool) { ... return db; })`. If the lambda returns a pointer, "Fruit will take ownership ... and will call delete ... when the injector is destroyed". A returned value is moved into the injector. "`registerProvider()` supports only lambdas with no captured variables (aka stateless lambdas). Plain functions are not supported." — [Quick reference](https://github.com/google/fruit/wiki/quick-reference)
- **Instances:** `bindInstance(obj)` (annotatable) does not copy or take ownership: "The caller ... must ensure that the provided reference is valid for the entire lifetime of the component and of any components or injectors that install this component." — [Quick reference](https://github.com/google/fruit/wiki/quick-reference)
- **Factories and assisted injection:** `.registerFactory<std::unique_ptr<MyClass>(Foo*, fruit::Assisted<int>)>(lambda)` exposes `std::function<std::unique_ptr<MyClass>(int)>`. The shorthand is `INJECT(MyClass(Foo* foo, ASSISTED(int) n));`. Memoization, if wanted, goes in the lambda. A no-argument `std::function<std::unique_ptr<MyClass>()>` factory comes from binding the implementation — [Quick reference](https://github.com/google/fruit/wiki/quick-reference)
- **Annotated (named) bindings:** `fruit::Annotated<Tag, T>`, for example `bind<fruit::Annotated<Cached, Greeter>, CachedGreeterImpl>()`, retrieved with `injector.get<fruit::Annotated<Cached, Greeter*>>()` — [Tutorial: Testing](https://github.com/google/fruit/wiki/tutorial:-testing)
- **Multibindings:**
  - `addMultibinding<Listener, AListener>()`, retrieved with `injector.getMultibindings<Listener>()`, which returns `const std::vector<Listener*>&`.
  - They "do **not** contribute to a Component type" and are not de-duplicated.
  - They "can depend on normal bindings ... but they can't depend on other multibindings".
  - They can also serve as optional injection (0 or more bindings).
  - Issue #51 ("Allow injection of multibinding providers") has been open since 2017.
  — [Quick reference](https://github.com/google/fruit/wiki/quick-reference); [issues](https://github.com/google/fruit/issues)
- **Composition:**
  - `install(getXComponent, args...)`; installed components are de-duplicated (3.0.0).
  - `installComponentFunctions(...)` installs a pack expansion.
  - Templated and parametrized components exist. Argument types must be "Copy-constructible ... Equality-comparable ... Hashable (`std::hash` must be defined)".
  - Issue #99 ("Passing reference parameters to get*Component functions doesn't work") is open.
  — [Quick reference](https://github.com/google/fruit/wiki/quick-reference); [releases](https://github.com/google/fruit/releases); [issues](https://github.com/google/fruit/issues)
- **Injector API:**
  - `fruit::Injector<T1..Tn>` cannot have requirements. Only declared types are directly gettable.
  - `get<T>()`, `get<T*>()`, `get<T&>()`, `get<const T*>()`, `get<const T&>()`, `get<std::shared_ptr<T>>()` and `get<fruit::Provider<T>>()` are allowed, plus conversion operators (`MyClass* myClass(injector);`).
  - Issue #71 asks to "Clarify ownership of injected shared_ptr objects" (open since 2018).
  — [Quick reference](https://github.com/google/fruit/wiki/quick-reference); [issues](https://github.com/google/fruit/issues)
- **Lifetimes and scopes:**
  - "Unlike Guice, Fruit does **not** have injection scopes."
  - Injection scopes were "implemented and then removed, replaced by the use of `NormalizedComponent`".
  - `fruit::Provider<T>::get()` returns "the same `Bar` instance" on every call within an injector.
  - Issue #74 "Support child injectors" has been an open enhancement since 2018.
  — [Quick reference](https://github.com/google/fruit/wiki/quick-reference); [wiki Home: Rejected features](https://github.com/google/fruit/wiki); [issue list](https://github.com/google/fruit/issues)
- **NormalizedComponent:**
  - It pre-computes the binding normalization once and is thread-safe and shareable.
  - Building an `Injector` from it plus a small extra component (for example `bindInstance` of per-request data) "is very cheap, on the order of 2 us".
  - Nested NormalizedComponents emulate outer, middle and inner scopes.
  — [Quick reference](https://github.com/google/fruit/wiki/quick-reference); [Tutorial: Server](https://github.com/google/fruit/wiki/tutorial:-server)
- **Lazy injection:** depending on `fruit::Provider<Bar>` delays building Bar and its subgraph until `get()`. Each `get()` contains an "is_already_initialized" branch — [Quick reference](https://github.com/google/fruit/wiki/quick-reference)
- **Test replacement:** `.replace(getKeyValueStorageComponent).with(getFakeKeyValueStorageComponent)` works at any nesting depth. It "must be before the `install()` ... otherwise Fruit will report the wrong order as a run-time error". The replaced and replacement components must return the same component type — [Tutorial: Testing](https://github.com/google/fruit/wiki/tutorial:-testing); [Quick reference](https://github.com/google/fruit/wiki/quick-reference)
- **Runtime configuration:** "Conditional injection based on runtime conditions ... flags passed to the executable or an XML file loaded at runtime." A process can build a new injector with a new configuration, for example a web server reconfiguring itself without stopping. Multibindings "can be useful for e.g. plugin loading/hooks" — [wiki Home](https://github.com/google/fruit/wiki)
- **Thread safety:**
  - "No internal static data. This allows the creation of separate injectors ... (even concurrently)."
  - A shared injector needs locking unless `eagerlyInjectAll()` has been called, after which it "can be shared by multiple threads with no locking". Even then, `Provider<Foo>::get()` is not thread-safe unless Foo has already been injected.
  - "Fruit is not multithreaded, even though it's reentrant and thread-safe."
  — [wiki Home](https://github.com/google/fruit/wiki); [Quick reference](https://github.com/google/fruit/wiki/quick-reference); [Benchmarks](https://github.com/google/fruit/wiki/benchmarks)
- **RTTI and exceptions:**
  - RTTI is optional and used only to print type names in errors: "Fruit never calls `typeid` on an object, and never calls it at all at runtime." The build detects `FRUIT_HAS_TYPEID` and `FRUIT_HAS_CXA_DEMANGLE`.
  - Client code built without RTTI must link a Fruit built without RTTI; "the pre-packaged Fruit binaries have RTTI enabled".
  - Fruit's `src/` and `include/` contain no `throw` (v3.7.1). The author's benchmarks include "no exceptions/RTTI" builds.
  — [FAQ](https://github.com/google/fruit/wiki/faq); [configuration/CMakeLists.txt](https://github.com/google/fruit/blob/master/configuration/CMakeLists.txt); [Benchmarks](https://github.com/google/fruit/wiki/benchmarks)
- **Header-only or compiled:** "No code generation. Just include `fruit/fruit.h` and link with the `fruit` library." CMake option `BUILD_SHARED_LIBS` defaults to ON — [wiki Home](https://github.com/google/fruit/wiki); [CMakeLists.txt](https://github.com/google/fruit/blob/master/CMakeLists.txt)
- **Modularity benefit claimed:** a component's header includes only the interfaces it exposes, so changing bindings recompiles only that component. "Helps with binary compatibility" — [wiki Home](https://github.com/google/fruit/wiki)

### Inferences
- Lifetimes besides per-injector singletons:
  - Factory-produced objects: a new `std::unique_ptr` per call, owned by the caller.
  - `bindInstance` objects: owned externally.
  - Per-request or per-scope injectors built from `NormalizedComponent`, which approximate request scope.
  - Nothing like Guice's custom scopes, prototype/transient bindings or child injectors.
- Runtime-condition wiring and the per-request-injector pattern are things a purely compile-time container cannot easily do. They are the main payoff of Fruit's hybrid design.

### Gaps
- I did not verify the destruction order of injector-owned objects, or what happens if a user constructor throws during injection.
- The exact names of the multibinding provider and instance APIs in 3.x were not checked line by line (the Home page text mentions `registerMultibindingProvider`, which may be an older name).

## Q5. What is checked at compile time vs at runtime, and the quality of diagnostics

### Takeaway
Missing bindings, dependency cycles and duplicate bindings between visible component types are all compile-time errors, reported through named `static_assert` error types. Some errors are runtime-only, by design: inconsistent bindings hidden inside different components, mutual `install()` loops, `nullptr` from providers, and mis-ordered `replace()`. These are fatal; Fruit prints "Fatal injection error: ..." and calls `exit(1)`. The compile-time messages carry a readable one-line cause, but it sits inside long template-instantiation noise, and the docs advise reading only the first error.

### Cited Findings
- Compile-time checks, examples: "Checking that all required types are bound (implicitly or explicitly)" and "Checking that there are no dependency loops in the bound types." — [wiki Home](https://github.com/google/fruit/wiki)
- "The only injection errors that can't be detected at compile time (and will be detected at run-time) are:" multiple inconsistent bindings for a type in different components; "two `Component` functions `install()` each other"; a provider lambda returning `nullptr`; and `replace(...).with(...)` after the replaced component is installed — [wiki Home](https://github.com/google/fruit/wiki)
- "Compile-time detection of multiple inconsistent bindings" is a rejected feature, "because it would interfere with ... conditional injection, binary compatibility, few includes" — [wiki Home](https://github.com/google/fruit/wiki)
- `injection_errors.h` defines 43 named error structs. Examples: `NoBindingFoundError`, `NoBindingFoundForAbstractClassError`, `SelfLoopError`, `TypeAlreadyBoundError`, `DuplicateTypesInComponentError`, `InjectorWithRequirementsError`, `NotABaseClassOfError`, `LambdaWithCapturesError`, `CannotConstructAbstractClassError`, `InterfaceBindingToSelfError`, `TypesInInjectorNotProvidedError`, `ProviderReturningPointerToAbstractClassWithNoVirtualDestructorError` and `IncorrectArgTypePassedToInstallComponentFuntionsError` — [injection_errors.h](https://github.com/google/fruit/blob/master/include/fruit/impl/injection_errors.h)
- Message texts (verbatim) — [injection_errors.h](https://github.com/google/fruit/blob/master/include/fruit/impl/injection_errors.h):
  - `NoBindingFoundError`: "No explicit binding nor C::Inject definition was found for T."
  - `SelfLoopError`: "Found a loop in the dependencies! The types in TypesInLoop all depend on the next, and the last one depends on the first."
  - `TypeAlreadyBoundError`: "Trying to bind C but it is already bound."
  - `DuplicateTypesInComponentError`: "The installed component provides some types that are already provided by the current component."
  - `InterfaceBindingToSelfError`: "The type C was bound to itself, with the same annotations (if any). If this was intentional, to "tell Fruit to inject the type C", this binding is unnecessary, just remove it."
- Component-level cycle example: installing `getT1Component` (`Component<Required<T2>, T1>`) and `getT2Component` (`Component<Required<T1>, T2>`) together "produces a (compile time) dependency loop error" — [Quick reference](https://github.com/google/fruit/wiki/quick-reference)
- Real compiler output, GCC 7 on a missing binding (excerpt from the tutorial):
  ```
  /usr/local/include/fruit/impl/injection_errors.h: In instantiation of 'struct fruit::impl::NoBindingFoundError<X>':
  /usr/local/include/fruit/impl/component.defn.h:59:55:   required from 'fruit::Component<Types>::Component(fruit::PartialComponent<Bindings ...>) [with Bindings = {}; Params = {X}]'
  main.cpp:8:35:   required from here
  /usr/local/include/fruit/impl/injection_errors.h:33:3: error: static assertion failed: No explicit binding nor C::Inject definition was found for T.
  ```
  Three more follow-on errors come after it, such as "no type named 'Result' in 'fruit::impl::meta::OpForComponent<>::ConvertTo<fruit::impl::meta::Comp<...>>'". The tutorial says: "you should only look at the first error. Different compilers have varying amount of success in reporting useful errors after the first one." It also says "these error messages might be confusing at first" — [Tutorial: Errors](https://github.com/google/fruit/wiki/tutorial:-errors)
- By default Fruit hides its internals in error traces. The debug switch `FRUIT_DEEP_TEMPLATE_INSTANTIATION_STACKTRACES_FOR_ERRORS` gives "deep template instantiation stacktraces ... instead of the user-friendly default that hides Fruit internals" — [CMakeLists.txt](https://github.com/google/fruit/blob/master/CMakeLists.txt)
- Runtime error text (verbatim start): "Fatal injection error: the type ... was provided more than once, with different bindings. This was not caught at compile time because at least one of the involved components bound this type but didn't expose it in the component signature." Another: "Fatal injection error: unable to replace (using .replace(...).with(...)) the component function at ... because the former component function was installed before the .replace(...).with(...)." Both are followed by `exit(1)` — [src/binding_normalization.cpp](https://github.com/google/fruit/blob/master/src/binding_normalization.cpp)
- `InjectorStorage::fatal()` prints `"Fatal injection error: " << error` and calls `exit(1)`. Fruit 2.0.2 used the same exit(1) mechanism — [src/injector_storage.cpp](https://github.com/google/fruit/blob/master/src/injector_storage.cpp); [v2.0.2 tree](https://github.com/google/fruit/tree/v2.0.2/src)
- Error checks are regression-tested. The Python test suite uses `expect_compile_error(...)` and `expect_success(...)` helpers. A 2025-02-01 commit was needed to "Update checks on compiler error messages to work with GCC 14." — [tests/test_bind_interface.py](https://github.com/google/fruit/blob/master/tests/test_bind_interface.py); [commit history](https://github.com/google/fruit/commits/master)
- Users still struggle with the errors: issue #159 "Understanding Compiler error messages in Fruit" (Apr 2024, 6 comments, closed) — [issue list](https://github.com/google/fruit/issues?q=is%3Aissue%20sort%3Acreated-desc)
- Boost.DI's comparison table (2016) lists Fruit's errors as "compile-time errors/exceptions" — [Boost.DI benchmarks.md](https://github.com/boost-ext/di/blob/cpp14/doc/benchmarks.md). This contradicts Fruit's source: no `throw`, and fatal errors use `exit(1)`, also in 2.0.2 — [src/injector_storage.cpp](https://github.com/google/fruit/blob/master/src/injector_storage.cpp)
- Fruit's author compares the libraries in issue #5: "Fruit (and Boost.DI) give you the guarantee that certain classes of errors are always detected at compile-time". He names missing bindings (both libraries) and binding dependency cycles (Fruit only, by his account) — [issue #5](https://github.com/google/fruit/issues/5)

### Inferences
- Summary:
  - **Missing bindings:** compile time.
  - **Cycles:** compile time for bindings visible to the type system. Mutual component installation is caught at runtime.
  - **Duplicate or ambiguous bindings:** compile time when the duplicate types are visible in component signatures. Runtime (fatal) when one binding is hidden behind a component boundary.
- Because runtime errors call `exit(1)` rather than throwing, they cannot be caught, logged by the host or turned into test failures without a death test. This matters for long-running servers that rebuild injectors on reconfiguration, and for plugin hosts.
- The diagnostics are better than raw template errors (named error type plus a plain-English `static_assert` text). They still demand that users know to find the first error and read the `T`/`C` placeholders in the message as the instantiated type.

### Gaps
- I have no Clang or MSVC sample outputs; the wiki shows only GCC 7. Error-message quality relative to Boost.DI was not tested hands-on.

## Q6. Requirements, build system, packaging and platforms

### Takeaway
Fruit needs C++11 or later, both for itself and for client code; CI covers up to C++23 on Clang 19. It builds with CMake (3.2 to 4.0 on master) or Bazel (bzlmod on master since April 2026). It is a compiled library, shared by default. Boost is needed only to build it, and only optionally. It is widely packaged: vcpkg, Conan Center, Homebrew, Debian/Ubuntu (per search snippets), AUR, FreeBSD ports and the author's own OBS repositories. Every package is still 3.7.1.

### Cited Findings
- C++ standard: `if ("${CMAKE_CXX_STANDARD}" STREQUAL "") set(CMAKE_CXX_STANDARD 11)` — [CMakeLists.txt](https://github.com/google/fruit/blob/master/CMakeLists.txt). Fruit "uses many C++11 features ... **This is not just to build Fruit itself, but also to build any code that uses Fruit.**" — [Install](https://github.com/google/fruit/wiki/install)
- CMake: `cmake_minimum_required(VERSION 3.2...4.0)` on master. 3.5.0 raised the minimum to CMake 3.2. A `CMAKE_BUILD_TYPE` of Debug, Release, RelWithDebInfo or MinSizeRel is mandatory — [CMakeLists.txt](https://github.com/google/fruit/blob/master/CMakeLists.txt); [releases](https://github.com/google/fruit/releases)
- Bazel: supported via `extras/bazel_root`. Master moved to bzlmod (`MODULE.bazel`) on 2026-04-11. "Building Fruit with Bazel on Windows (with MSVC) doesn't currently work" (#122) — [Install](https://github.com/google/fruit/wiki/install); [commit 092cea3](https://github.com/google/fruit/commit/092cea3f1408ff20e26334db203a923c581506ba)
- The Bazel Central Registry has no `fruit` module: `modules/fruit/metadata.json` returned HTTP 404 on 2026-10-30 — [BCR modules](https://github.com/bazelbuild/bazel-central-registry/tree/main/modules)
- Boost: `FRUIT_USES_BOOST` defaults to TRUE, for `boost::unordered_set`/`unordered_map`. "If this is false, Fruit will use std::unordered_set and std::unordered_map instead (however this causes injection to be a bit slower)." — [CMakeLists.txt](https://github.com/google/fruit/blob/master/CMakeLists.txt). "The Boost library is only required when building Fruit itself and not when building code that uses Fruit." — [Install](https://github.com/google/fruit/wiki/install)
- Supported compilers per the Install page (last edited 2020-08-16): GCC ≥5.0.0 on Linux/OS X; MinGW GCC ≥5; Clang ≥3.5 with libstdc++ or libc++; "MSVC 2015 or later". The Intel compiler is known not to work because of compiler bugs — [Install](https://github.com/google/fruit/wiki/install). This conflicts with the 3.6.0 release note "MSVC 2015 is no longer supported (only MSVC 2017 or 2019 are)" — [releases](https://github.com/google/fruit/releases)
- The in-repo Conan recipe still lists minimums gcc 5, clang 3.5, apple-clang 7.3 and "Visual Studio" 14 (MSVC 2015). It uses the legacy Conan 1 API (`from conans import ConanFile`) — [conanfile.py](https://github.com/google/fruit/blob/master/conanfile.py)
- CI matrix on master (workflow files) — [test-on-linux.yml](https://github.com/google/fruit/blob/master/.github/workflows/test-on-linux.yml); [test-on-osx.yml](https://github.com/google/fruit/blob/master/.github/workflows/test-on-osx.yml); [test-on-windows.yml](https://github.com/google/fruit/blob/master/.github/workflows/test-on-windows.yml):
  - **Linux:** Docker images `polettimarco/fruit-basesystem` (Ubuntu 20.04, 22.04, 24.04, 24.10). GCC 7, 9, 10, 11, 12 and 14; Clang 6.0, 10, 11, 13, 14, 16 and 19 with libstdc++. Clang 19 also runs at `cxx_standard: 23`. Release, Debug and DebugAsanUbsan variants.
  - **OS X:** clang-default, 11, 12 and 14 with libc++, plus gcc 9 and 11, on `macos-10.15`, `macos-11` and `macos-12`.
  - **Windows:** MSVC 2019 (v16) and 2022 (v17), some jobs in C++17. Boost on and off, shared and static builds, and a MinGW job.
- The CI results as of Oct 2026 (Linux success, macOS cancelled, Windows failure) are covered in Q3 — [GitHub Actions](https://github.com/google/fruit/actions)
- Officially recognized compiler IDs: GNU, Clang, AppleClang and MSVC; others trigger "Compiler not officially supported" — [CMakeLists.txt](https://github.com/google/fruit/blob/master/CMakeLists.txt)
- vcpkg: port `fruit` version 3.7.1, Apache-2.0 — [vcpkg ports/fruit/vcpkg.json](https://github.com/microsoft/vcpkg/blob/master/ports/fruit/vcpkg.json). Fruit's docs: "The vcpkg configuration is maintained by Microsoft and other contributors, but not by the maintainers of Fruit" (labelled "not officially supported") — [Install](https://github.com/google/fruit/wiki/install). Issue #163 (open): FindFruit fails after installing via vcpkg.json — [issues](https://github.com/google/fruit/issues)
- Conan Center: recipe `fruit` with versions 3.4.0, 3.5.0, 3.6.0, 3.7.0 and 3.7.1 — [conan-center-index recipes/fruit/config.yml](https://github.com/conan-io/conan-center-index/blob/master/recipes/fruit/config.yml); [conan.io/center/recipes/fruit](https://conan.io/center/recipes/fruit)
- Homebrew: formula `fruit` 3.7.1, built with `-DFRUIT_USES_BOOST=False`, with bottles and a backported CMake-compatibility patch — [Homebrew fruit.rb](https://github.com/Homebrew/homebrew-core/blob/master/Formula/f/fruit.rb)
- Debian/Ubuntu (search snippets only; the pages could not be fetched): Debian source package `google-fruit` 3.7.1-1 with development package `libgoogle-fruit-dev`; Ubuntu lists `libgoogle-fruit-dev` in universe for the release codenamed "stonking" — [Debian control 3.7.1-1](https://tracker.debian.org/media/packages/g/google-fruit/control-3.7.1-1); [packages.ubuntu.com libgoogle-fruit-dev](https://packages.ubuntu.com/stonking/libgoogle-fruit-dev)
- Arch AUR: `google-fruit` 3.7.1-1, no maintainer field, 0 votes (search snippet) — [AUR google-fruit](https://aur.archlinux.org/packages/google-fruit)
- FreeBSD ports `devel/fruit` (search result) — [FreshPorts](https://freshports.org/devel/fruit). Slackware SlackBuild `google-fruit` for 14.2 (search result) — [SlackBuilds](https://slackbuilds.org/repository/14.2/libraries/google-fruit/)
- The author's own prebuilt packages "for Fedora, openSUSE, Ubuntu, Debian and various other distributions" live on openSUSE Build Service project `home:poletti_marco`, package `libfruit` — [Install](https://github.com/google/fruit/wiki/install); [OBS debian.control](https://build.opensuse.org/projects/home:poletti_marco/packages/libfruit/files/debian.control?expand=1)

### Inferences
- Fruit's toolchain baseline (C++11, GCC 5+, Clang 3.5+) is unusually permissive for 2026. Effectively tested coverage today, though, is Linux with GCC 7 to 14 and Clang 6 to 19. MSVC and macOS support should be treated as "worked at 3.7.1, currently not verified by CI".
- Being a compiled library adds packaging and ABI concerns that header-only libraries don't have. RTTI settings, debug/release and compiler settings must match between Fruit and client binaries (see the FAQ RTTI note and the maintainer's advice in #171).

### Gaps
- Fedora's official repositories: no evidence found (only the author's OBS repos). The exact Debian suites and Ubuntu releases carrying `google-fruit`, and the runtime library package name, could not be verified because tracker.debian.org and packages.ubuntu.com could not be fetched directly.
- I did not check whether the vcpkg and Conan recipes carry the CMake 4.0 patch.

## Q7. Performance: compile time, startup, per-request injection, binary size (vs Boost.DI and hand-written DI)

### Takeaway
The only comprehensive benchmarks are author-run and dated: Fruit's own wiki (2020) and Boost.DI's docs (2016, against the old Fruit 2.0.2), and they disagree sharply on compile time and binary size.
- **Fruit's numbers:** moderate fixed costs over hand-written wiring (about 4 to 5 s extra cold compile for 100 classes, at most 3.4 ms startup, about 3 to 4 KB per class). Much better compile time, compile memory and per-request time than Boost.DI; Boost.DI starts up slightly faster.
- **Boost.DI's 2016 numbers:** Fruit much slower to compile and much larger.
- No independent benchmark was found.

### Cited Findings
- **Fruit wiki benchmarks: who, when, what.**
  - Wiki page authored by poletti-marco, last edited 2020-05-02 (wiki git history).
  - "AMD Ryzen Threadripper 1950X @3.4GHz (32 virtual cores) ... 32 GB RAM". Kubuntu 19.10, Clang 10.0.0 and GCC 9.2.1, `-std=c++11 -O2 -DNDEBUG` (C++14 for Boost.DI).
  - 95% confidence intervals; at least 3 and up to 20 runs.
  - Synthetic codebases of 100, 250 and 1000 classes: 10% have no dependencies, 90% have 10 dependencies each, all behind interfaces.
  - Compared against Boost.DI and three hand-written "Simple DI" variants: concrete classes on the stack; interfaces on the stack; interfaces with new/delete.
  — [Benchmarks](https://github.com/google/fruit/wiki/benchmarks)
- The generator and runner scripts ship in the repo (`fruit_source_generator.py`, `boost_di_source_generator.py`, `no_di_library_source_generator.py`, `run_benchmarks.py`) — [extras/benchmark](https://github.com/google/fruit/tree/master/extras/benchmark)
- **Cold compile, Clang (100 / 250 / 1000 classes):**
  - Fruit: 6.6–6.7 s / 16 s / 64 s
  - Boost.DI: 17 s / 58 s / 580 s
  - Simple DI: 0.93 s / 1.9 s / 8.3 s
  - Simple DI with interfaces, new/delete: 2.5 s / 5.8 s / 25 s
  - With GCC, Fruit took 5.1 s / 12 s / 48 s. Boost.DI took 18 s / 110 s, and the 1000-class build was N/A because "GCC crashes (AFAICT due to it running out of memory)".
  — [Benchmarks](https://github.com/google/fruit/wiki/benchmarks)
- **Incremental compile, touching 5 files, Clang:**
  - Fruit: 3.9 s / 4 s / 6.1 s
  - Boost.DI: 16 s / 57 s / 570–580 s
  - Simple DI: 0.84 s / 1.8 s / 8 s
  - Simple DI with interfaces, new/delete: 2.2 s / 4.6 s / 20 s
  - Author's takeaway: "in larger codebases the incremental compilation time is *lower* with Fruit even when compared with some 'no DI framework' approaches".
  — [Benchmarks](https://github.com/google/fruit/wiki/benchmarks)
- **Peak compile memory, Clang:**
  - Fruit: 133 / 133 / 209 MB
  - Boost.DI: 343 / 772 / 4196 MB
  - Simple DI: 89 / 94 / 114 MB
  - With GCC, Boost.DI peaked at 7534 MB for 1000 classes.
  — [Benchmarks](https://github.com/google/fruit/wiki/benchmarks)
- **Startup (create the injector, inject everything, print, exit), Clang:**
  - Fruit: 7.3 / 6.6 / 9.2 ms
  - Boost.DI: 5.7–5.8 / 6.2 / 8.4 ms
  - Simple DI: 5.4 / 6.8 / 7.1 ms
  - Takeaway: "The Fruit startup time overhead is at most 3.4ms ... Boost.DI is actually competitive here, about 1-2ms faster than Fruit."
  — [Benchmarks](https://github.com/google/fruit/wiki/benchmarks)
- **Per-request injector creation plus injecting all classes** (Fruit via `NormalizedComponent`; Boost.DI rebuilds from scratch, since it "does not offer comparable functionality"), Clang:
  - Fruit: 2.5–2.6 µs / 8–8.1 µs / 85 µs
  - Boost.DI: 47–50 µs / 150 µs / 710–740 µs
  - Simple DI: 0.81–0.83 µs / 2.2 µs / 8.2–8.3 µs
  - Simple DI with interfaces, new/delete: 2.2 µs / 5.5 µs / 47 µs
  - With GCC, Fruit took 2.9–3 µs / 12 µs / 99–100 µs.
  - "Boost.DI is 10-15x slower than Fruit in all cases."
  — [Benchmarks](https://github.com/google/fruit/wiki/benchmarks)
- **Stripped executable size, Clang:**
  - Fruit: 390 / 947 / 3710 KB
  - Boost.DI: 576 / 1464 / 5664 KB
  - Simple DI: 30 / 54 / 195 KB
  - Simple DI with interfaces, new/delete: 70 / 166 / 625 KB
  - Without exceptions and RTTI, Fruit was 322 / 791 / 3125 KB (Clang) and 302 / 732 / 2832 KB (GCC).
  - Takeaways: "around 3-4KB per injected class". Boost.DI's overhead is "about 30% higher than Fruit with Clang, and about 2-3x with GCC".
  — [Benchmarks](https://github.com/google/fruit/wiki/benchmarks)
- Older FAQ figures (2017 text): with 100 interfaces, 100 classes and 900 edges, "the injection time (creation of the injector + injection of all the 100 classes) is around 70 us (when using GCC)". With `NormalizedComponent`, "pay only 2 us". A comparable codebase "takes around `400 KB`". Up to 4 extra symbols are emitted per binding — [FAQ](https://github.com/google/fruit/wiki/faq)
- **Counter-benchmark by Boost.DI's author.**
  - The doc was written 2016-03-17 and updated 2016-05-04 (git history of the file). It used Fruit 2.0.2, Boost.DI 1.0.1, Clang 3.7 -O2, on an "Intel(R) Core(TM) i7-4770 CPU @ 3.40GHz".
  - Workload: "Create a unique objects tree for each iteration", 0 bindings, 1024 iterations.
  - Compile time: Fruit 2.329 s vs Boost.DI 0.376 s vs a 0.063 s manual baseline at 64 types; 1m9.601 s vs 4.911 s vs 0.402 s at 1024 types. Fruit was measured "/ FRUIT_NO_LOOP_CHECK", i.e. with loop checking disabled.
  - Executable size: Fruit 213K vs Boost.DI 8.5K at 64 types; 15M vs 17K at 1024 types.
  - Execution time: Fruit 0.037 s vs 0.002 s at 64 types; 0.765 s vs 0.004 s at 1024 types.
  — [Boost.DI doc/benchmarks.md](https://github.com/boost-ext/di/blob/cpp14/doc/benchmarks.md)
- Fruit 3.0.0 (Aug 2017), released after the Boost.DI benchmark, announced "large performance and executable-size improvements". 3.4.0 made Injector and NormalizedComponent construction faster — [releases](https://github.com/google/fruit/releases)

### Inferences
- **Against the closest hand-written equivalent** ("interfaces + new/delete") in Fruit's own data, Fruit costs about 2.6 to 2.7× the cold compile time (Clang: 6.6 vs 2.5 s at 100 classes; 64 vs 25 s at 1000). It improves incremental compiles at scale (6.1 vs 20 s at 1000). It adds 1 to 3 ms at startup. It is up to about 1.8× slower per request (85 vs 47 µs at 1000). It makes binaries about 5.6 to 5.9× larger (390 vs 70 KB at 100 classes; 3710 vs 625 KB at 1000).
- **Against plain stack-allocated concrete wiring** ("Simple DI"), Fruit's binaries are about 11.5 to 19× larger: Clang 390/30, 947/54 and 3710/195 KB; GCC 390/34 to 3808/253 KB. Per-request time is about 3 to 10× slower with Clang and about 7 to 21× slower with GCC.
- **The two benchmark sets are not comparable.**
  - They differ in Fruit version (2.0.2 vs about 3.5), workload (single TU and 0 bindings vs multi-TU interface graphs) and toolchain (Clang 3.7 vs Clang 10 / GCC 9).
  - Each was run by a competing library's author on a workload of their choosing.
  - The fair conclusion: Fruit's per-type compile cost is concentrated in component TUs and scales roughly linearly. Boost.DI's cost depends heavily on how much of the graph a single TU instantiates.
  - Neither result should be treated as current in 2026. Both predate current compilers, and Boost.DI has changed since.
- Fruit's binary-size and per-request overheads look irrelevant for typical servers and desktop apps. They could matter for code-size-constrained embedded targets.

### Gaps
- I found no independent (third-party) benchmark of Fruit, and no benchmark on GCC 13+/Clang 17+/MSVC 2022 or with C++20/23.
- I found no published per-`get()` latency figure separate from injector creation.
- The Boost.DI version used in Fruit's 2020 benchmarks is not stated on the wiki page.

## Q8. Adoption and community sentiment

### Takeaway
Adoption evidence is modest but includes one solid, verified production user inside Google: the Android Cuttlefish virtual-device host tools, which pin a 2025 master commit. AOSP also carries an `external/google-fruit` project. Beyond that I found only claims and mentions (a C++Now 2019 talk by Boost.DI's author, small blogs) and about 1.9k GitHub stars. First-hand production reports from r/cpp, Stack Overflow or Hacker News could not be retrieved or were not found.

### Cited Findings
- GitHub metrics on 2026-10-30: "1.9k" stars, 201 forks, 83 watching; About text: "Fruit, a dependency injection framework for C++" — [GitHub repo page](https://github.com/google/fruit)
- **Verified user, Android Cuttlefish** (`google/android-cuttlefish`, HEAD dated 2026-10-30):
  - `base/cvd/cuttlefish/host/commands/run_cvd/main.cc` includes `fruit/component.h` and `fruit/injector.h`, defines `fruit::Component<> runCvdComponent(...)` returning `fruit::createComponent()...`, and builds `fruit::Injector<> injector(runCvdComponent, config, &environment, &instance);`.
  - `assemble_cvd.cc` likewise builds `fruit::Injector<> injector(FlagsComponent, &system_image_dir);`.
  — [run_cvd/main.cc](https://github.com/google/android-cuttlefish/blob/HEAD/base/cvd/cuttlefish/host/commands/run_cvd/main.cc); [assemble_cvd.cc](https://github.com/google/android-cuttlefish/blob/HEAD/base/cvd/cuttlefish/host/commands/assemble_cvd/assemble_cvd.cc)
- Cuttlefish pulls Fruit via Bazel `git_repository(name = "fruit", commit = "0af6ffd113ad963dbd2a23e8cbabba13493c015f",  # 2025-02-02, remote = "https://github.com/google/fruit.git")`, i.e. a master commit, not the 3.7.1 release — [fruit.MODULE.bazel](https://github.com/google/android-cuttlefish/blob/HEAD/base/cvd/build_external/fruit/fruit.MODULE.bazel)
- AOSP hosts `platform/external/google-fruit`, with Android branches and tags such as `android10-mainline-release`, `master-cuttlefish-testing-release` and `aml_*` (from search results) — [android.googlesource.com external/google-fruit](https://android.googlesource.com/platform/external/google-fruit/)
- A Google engineer committed the Android C++23 fix ("In Android, we need the added <type_traits> to build with C++23", 2025-12-27) — [commit 45cc058](https://github.com/google/fruit/commit/45cc0583a170bc29a41e02e10911b1dee699c60c)
- The 2026 bzlmod commit says the old Bazel rule "works inside google but not with bazel" — [commit 092cea3](https://github.com/google/fruit/commit/092cea3f1408ff20e26334db203a923c581506ba)
- Popularity claims by the author. The FAQ (2017 text): "Fruit is the most popular dependency injection library for C++ (at least, it's the first result in a Google search ... and it's the top C++ dependency injection library ... by number of stars on Github)". The Benchmarks page (2020): Boost.DI is "the 2nd most popular DI framework, behind Fruit" by stars — [FAQ](https://github.com/google/fruit/wiki/faq); [Benchmarks](https://github.com/google/fruit/wiki/benchmarks)
- Talks: Kris Jusiak's C++Now 2019 talk "Dependency Injection - a 25-dollar term for a 5-cent concept" has a "Google.Fruit" section. It describes Fruit as a library (not header-only) under Apache-2 with dependencies "STL, [Boost]", C++11, "Semi compile-time", and shows the `INJECT` and `bind<>` wiring — [C++Now 2019 slides](https://github.com/boost-ext/di/blob/cpp14/doc/cppnow-2019/index.html)
- The issue tracker shows Qt desktop users: #146 "Fruit with QT" (closed 2023) and #171 (a Qt/MinGW plugin system, Dec 2025). There is a stream of how-to questions from individual developers, for example IrisPeter's #138, #143, #144, #152, #154, #156, #159 and #160 in 2022 to 2024 — [issue list](https://github.com/google/fruit/issues?q=is%3Aissue%20sort%3Acreated-desc)
- Third-party commentary (search snippets only; pages could not be fetched):
  - A Russian Habr article on writing a C++ DI container criticizes Fruit for requiring macros on the injected class's constructor — [habr.com/ru/post/722940](https://habr.com/ru/post/722940)
  - A June 2026 comparison blog recommends Fruit "for large teams with explicit contracts". The search tool flagged other claims in it as questionable — [pistack.xyz](https://www.pistack.xyz/posts/2026-06-22-cpp-dependency-injection-containers-boost-di-google-fruit-kangaru/)
- A search for Hacker News discussion of Fruit returned no Fruit-specific thread — [HN search results, e.g. "The problem with dependency injection frameworks"](https://news.ycombinator.com/item?id=34298153)

### Inferences
- Verified: Google uses Fruit in at least one shipped product's tooling (Cuttlefish host binaries), and it builds within the Android tree. This is the strongest signal of real-world use. It also explains the recent outside commits (Android C++23, Bazel).
- Claimed, not verified: broad industry use. The "most popular C++ DI library" claim dates from about 2017 and is not re-verified for 2026 (comparative star counts are another researcher's scope).
- Community sentiment, as far as it is visible: respected for compile-time safety and its Guice-like structure; criticized for macros and complexity. Little evidence of active public discussion in recent years.

### Gaps
- r/cpp and Stack Overflow could not be searched (the tool refused those domains), so I have no first-hand production reports or sentiment threads. GitHub's "Used by"/dependents page returned HTTP 403 to curl and was not rendered by WebFetch. No company case studies were found.

## Q9. Documentation, learning curve and testing story

### Takeaway
The docs are thorough but frozen: a good tutorial series and a detailed quick reference, mostly unchanged since 2017 to 2020. The learning curve is moderate to steep (component type algebra, macros, annotations, assisted injection, normalized components, metaprogramming error output). Testing support is a strong point: `replace().with()`, per-test injectors with `NormalizedComponent`, and a large compile-error regression suite inside Fruit itself.

### Cited Findings
- Wiki pages: Home, FAQ, Install, Quick reference (about 46 KB), and tutorials (Getting started, Simple system, Errors, Assisted injection, Annotated injection, Server, Testing). There are 7 example directories (annotated_injection, hello_world, multibindings, scaling_doubles, server, simple_injection, testing) — [wiki](https://github.com/google/fruit/wiki); [examples](https://github.com/google/fruit/tree/master/examples)
- Wiki edit dates (wiki git history): Home, FAQ and most tutorials 2017-08-06; Server and Testing tutorials 2018-08-27; Benchmarks 2020-05-02; Install 2020-08-16; Quick reference 2020-10-31, the last wiki commit — [wiki history](https://github.com/google/fruit/wiki/_history)
- Documentation issues still open: #137 "[Docs] Possible outdated documentation" (2021), #117 "Missing documentation how to build without RTTI" (2020), #71 shared_ptr ownership docs (2018), #50 "Document const bindings" (2017), #157 "Examples miss virtual dtor" (2023) — [open issues](https://github.com/google/fruit/issues)
- The testing tutorial walks through a fake `KeyValueStorage` swapped in with `.replace(getKeyValueStorageComponent).with(getFakeKeyValueStorageComponent)` and googletest tests. It notes "Fruit doesn't have any specific support for googletest, any other testing framework would also work". It suggests a `static fruit::NormalizedComponent` so each test gets a fresh injector cheaply — [Tutorial: Testing](https://github.com/google/fruit/wiki/tutorial:-testing)
- Fruit's own test suite has 48 `test_*.py` pytest modules plus `meta`, `data_structures` and `util` C++ tests. They use `expect_compile_error`/`expect_success` helpers to assert that specific compile errors occur. There are a Valgrind option and ASan/UBSan CI configurations, plus Coverity Scan and CII Best Practices badges in the README — [tests/](https://github.com/google/fruit/tree/master/tests); [CMakeLists.txt](https://github.com/google/fruit/blob/master/CMakeLists.txt); [README](https://github.com/google/fruit/blob/master/README.md)

### Inferences
- For a team new to DI, the Guice-style concepts plus C++ template diagnostics mean a real ramp-up. The 2022 to 2024 issue history shows a single user filing repeated how-to issues on factories, lifetimes and error messages. Teams with Guice or Dagger experience will find the model familiar.
- Testing is one of Fruit's clearest advantages over hand-written wiring, where swapping a deep dependency in a test usually means threading parameters or writing a parallel composition root.

### Gaps
- No API reference generated from header docs (Doxygen site) was found. Whether the docs cover C++17/20 idioms was not assessed.

## Q10. Criticisms, pitfalls, bugs and limitations

### Takeaway
The main limitations fall into four groups:
- **Design:** constructor macros or typedefs; no scopes or child injectors; providers only from captureless lambdas; hashable component arguments; runtime errors that kill the process.
- **Packaging and ABI:** a compiled library with RTTI and build-setting coupling.
- **Usability:** template-heavy errors.
- **Project health:** single maintainer, no release since 2023, decayed Windows/macOS CI, long-open MSVC and factory issues.

### Cited Findings
- **Macros:** `INJECT(...)` defines `Inject` typedefs inside the class, and "make sure you don't define another typedef/field/method with the same name if you use the INJECT macro" — [include/fruit/macro.h](https://github.com/google/fruit/blob/master/include/fruit/macro.h). A Habr article criticizes the constructor macro (search snippet) — [habr.com/ru/post/722940](https://habr.com/ru/post/722940)
- **No scopes:** "Unlike Guice, Fruit does **not** have injection scopes." Scopes were removed in favour of NormalizedComponent. Child injectors (#74) are an open enhancement since 2018 — [Quick reference](https://github.com/google/fruit/wiki/quick-reference); [wiki Home](https://github.com/google/fruit/wiki); [issues](https://github.com/google/fruit/issues)
- **Provider restrictions:** "`registerProvider()` supports only lambdas with no captured variables (aka stateless lambdas). Plain functions are not supported." `LambdaWithCapturesError` and `NonTriviallyCopyableLambdaError` enforce this at compile time — [Quick reference](https://github.com/google/fruit/wiki/quick-reference); [injection_errors.h](https://github.com/google/fruit/blob/master/include/fruit/impl/injection_errors.h)
- Issue #170 "Feature request: non-owning provider" (2025) and issue #158 "bindInstance with copy" (2024, 13 comments) show ownership-model friction — [issue list](https://github.com/google/fruit/issues?q=is%3Aissue%20sort%3Acreated-desc)
- **Component arguments** must be copy-constructible, move-constructible, assignable, equality-comparable and hashable. Reference parameters don't work (#99, open) — [Quick reference](https://github.com/google/fruit/wiki/quick-reference); [issues](https://github.com/google/fruit/issues)
- **Fatal runtime errors:** `exit(1)` after "Fatal injection error: ..." (see Q5). The "multiple inconsistent bindings" class of error is detected only at runtime, by design — [src/binding_normalization.cpp](https://github.com/google/fruit/blob/master/src/binding_normalization.cpp); [wiki Home](https://github.com/google/fruit/wiki)
- Misuse of `createComponent()` that is never converted to a `Component` is not reported. The abort code for it in `src/component.cpp` is commented out under `// TODO: reimplement this check somehow.`, and issue #75 "Report misuse of fruit::createComponent() at runtime" is open — [src/component.cpp](https://github.com/google/fruit/blob/master/src/component.cpp); [issues](https://github.com/google/fruit/issues)
- **Factories:**
  - #126: `registerFactory` with `Assisted<std::unique_ptr<Qux>>` fails with "use of deleted function 'std::unique_ptr<_Tp, _Dp>::unique_ptr(const std::unique_ptr<_Tp, _Dp>&)'". The maintainer agreed "this should be supported"; still open, with a `shared_ptr` workaround. Version 3.7.0 later allowed movable-only factory arguments.
  - #147: "Factories with reference types broken in fruit 3.7.0", fixed in 3.7.1.
  - #152: factories for multiple constructors, open.
  — [issue #126](https://github.com/google/fruit/issues/126); [releases](https://github.com/google/fruit/releases); [issue list](https://github.com/google/fruit/issues?q=is%3Aissue%20sort%3Acreated-desc)
- **MSVC and Windows friction:** #138 (VS 2019/2022 CMake build, open since 2022), #145 (VS 2022 with C++20, closed 2022), #156 (VS2022 Preview, closed 2024), #122 (Bazel on Windows, open). 3.7.1 had to restore VS 2022 compatibility after a VS update broke it — [issue list](https://github.com/google/fruit/issues?q=is%3Aissue%20sort%3Acreated-desc); [releases](https://github.com/google/fruit/releases)
- **RTTI and ABI coupling:** "If you disable RTTI when building code that uses Fruit, you'll need to link against a Fruit build that also has RTTI disabled." In #171 (a Qt DLL plugin crash) the maintainer advised checking that "Fruit, the main binary, and the DLL use the same compiler settings, including debug vs. release mode" — [FAQ](https://github.com/google/fruit/wiki/faq); [issue #171](https://github.com/google/fruit/issues/171)
- **Binary size:** 390 KB vs 30 KB for hand-written stack-based DI at 100 classes (Clang, author's numbers) — [Benchmarks](https://github.com/google/fruit/wiki/benchmarks)
- **Compile time:** about 7 to 8× the cold compile of plain hand-written concrete wiring at 100 and 1000 classes (6.6 vs 0.93 s; 64 vs 8.3 s, Clang; author's numbers) — [Benchmarks](https://github.com/google/fruit/wiki/benchmarks)
- **Multibindings** can't depend on other multibindings and do not appear in component types, so they are invisible to compile-time checking of provided types — [Quick reference](https://github.com/google/fruit/wiki/quick-reference)
- **Boost.DI author's characterization:** "Semi compile-time" (C++Now 2019) — [slides](https://github.com/boost-ext/di/blob/cpp14/doc/cppnow-2019/index.html)

### Inferences
- The `exit(1)` policy is the most production-relevant design flaw. In services that build injectors dynamically (per request or on hot reconfiguration, a scenario the docs advertise), a latent inconsistent-binding bug becomes a process crash rather than a handled error.
- Project-health risks (Q3) now outweigh most technical criticisms when choosing Fruit for a new long-lived codebase.

### Gaps
- No CVEs or security advisories were found or searched. Stack Overflow and Reddit pitfall threads could not be accessed.

## Q11. Fit for production C++ in 2026, and comparison with Boost.DI and hand-written wiring

### Takeaway
Fruit remains a credible, battle-tested choice for large, interface-heavy, modular C++ codebases on Linux with GCC/Clang. It suits teams that want Guice-style components, compile-time detection of missing bindings and cycles, cheap per-request injectors, and test-time component replacement, and that can pin or patch master. It is a poor fit for MSVC-first or macOS-first shops, header-only or zero-dependency requirements, code-size-constrained targets, projects needing scopes, child injectors or recoverable DI errors, and organizations that need an actively developed dependency with more than one maintainer.

### Cited Findings
- **Design goals:** reduced `#include`s and recompilation through component boundaries; binary compatibility; conditional runtime wiring; per-request injectors; multibindings for plugins and handlers — [wiki Home](https://github.com/google/fruit/wiki)
- **Fruit vs Boost.DI.**
  - Fruit's author: both detect missing bindings at compile time; Fruit also detects binding cycles — [issue #5](https://github.com/google/fruit/issues/5)
  - Fruit is a compiled C++11 library with optional Boost. Boost.DI is described as "Header only (Single header) ... Dependencies: None ... std: C++14" — [C++Now 2019 slides](https://github.com/boost-ext/di/blob/cpp14/doc/cppnow-2019/index.html)
  - Fruit's benchmarks favour Fruit on compile time, compile memory, per-request time and size; Boost.DI's 2016 benchmarks favour Boost.DI (see Q7) — [Benchmarks](https://github.com/google/fruit/wiki/benchmarks); [Boost.DI benchmarks.md](https://github.com/boost-ext/di/blob/cpp14/doc/benchmarks.md)
  - Fruit's wiki notes Boost.DI "is *not* actually part of Boost" — [Benchmarks](https://github.com/google/fruit/wiki/benchmarks)
- **Fruit vs hand-written wiring:** the author frames the three "Simple DI" codebases as "successive steps towards the Fruit model". Fruit pays roughly 4 to 5 s extra cold compile at 100 classes, gains on incremental builds at 1000 classes, adds at most 3.4 ms startup and about 3 to 4 KB per class — [Benchmarks](https://github.com/google/fruit/wiki/benchmarks)
- **Real-world precedent:** Google's Cuttlefish uses Fruit for top-level wiring of host tools, pinning a 2025 master commit — [fruit.MODULE.bazel](https://github.com/google/android-cuttlefish/blob/HEAD/base/cvd/build_external/fruit/fruit.MODULE.bazel)

### Inferences
- **Suits:**
  - Large server or tooling binaries with many interfaces and modules, especially those already using virtual interfaces and heap allocation, where Fruit's runtime overhead is "negligible" by its FAQ.
  - Teams that value component-level contracts (`Component<Required<...>, ...>`) and fast incremental builds.
  - Test suites that need deep fake substitution.
  - Request-scoped state via per-request injectors.
- **Red flags for 2026:**
  - Bus factor of 1.
  - No release since 2023-01-21, so packagers carry patches.
  - Windows CI failing and macOS CI not running (Jun to Oct 2026).
  - Fatal `exit(1)` runtime errors.
  - No scopes or child injectors; captureless-lambda providers only.
  - Constructor macros or typedefs (intrusive unless `registerConstructor` is used).
  - Compiled-library ABI and RTTI coupling.
  - Template-heavy diagnostics.
  - Benchmarks are author-run and 6 to 10 years old.
- **Risk mitigation if adopting:**
  - Vendor or pin a known-good master commit, as Cuttlefish does, and build it in-tree with your exact flags.
  - Run your own CI on MSVC and macOS if you target them.
  - Keep all bindings visible in component signatures to keep errors at compile time.
  - Wrap injector construction so that a fatal error surfaces in testing, not in production.
- **Versus hand-written wiring:** hand-written composition roots remain the zero-dependency, zero-overhead baseline, with the fastest compiles and smallest binaries in Fruit's own data. Fruit earns its keep only when the dependency graph and the test-substitution needs are large enough that manual wiring becomes a maintenance burden.
- **Versus Boost.DI:** choose on header-only vs compiled, C++14 vs C++11, automatic vs explicit constructor registration, and maintenance health. Boost.DI's own status is covered by another researcher. The published performance comparisons are contradictory and dated, so a team should benchmark on its own codebase and toolchain.

### Gaps
- There is no current (2024 to 2026) independent head-to-head of Fruit, Boost.DI and hand-written DI on modern compilers. A hands-on test (compile time and error output on GCC 14 / Clang 19 / MSVC 2022) would close this, but per the constraints nothing was built or run here.
