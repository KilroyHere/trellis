# C++ dependency injection frameworks: hands-on experiments

One small, equivalent program per framework plus a hand-written (manual) DI
baseline, and scripts that compile, measure and benchmark them with GCC and
Clang in C++17 and C++20 mode.

| Folder | Framework | Pinned version |
|---|---|---|
| `manual/` | hand-written composition root | n/a |
| `boost_di/` | [boost-ext/di](https://github.com/boost-ext/di) (Boost.DI) | `v1.3.2` = `20b6454c889f23c1e7c9eb5136cd7e3d6f6d718d` |
| `fruit/` | [google/fruit](https://github.com/google/fruit) | `v3.7.1` = `8149131119475ccaae3c1ae386fe9f2c679317f0` |
| `kangaru/` | [gracicot/kangaru](https://github.com/gracicot/kangaru) | `v4.3.2` = `ab498eb9db0de74ffebc46f395b9a31093017412` |
| `dingo/` | [romanpauk/dingo](https://github.com/romanpauk/dingo) | `v0.2.0` = `ccafcb6cbde54fb2819a3da8243106642f3b1711` |
| `hypodermic/` | [ybainier/Hypodermic](https://github.com/ybainier/Hypodermic) | `v2.5.3` = `0e0d85d70aa2f2391dfd84f8af4a3863d4fb1611` |
| `qlibs_di/` | [qlibs/di](https://github.com/qlibs/di) (Kris Jusiak, C++20) | `v1.0.6` = `bdafce9baa19dfc503d6acd5f5abf91f503fa5ef` |

`common/domain.hpp` holds the domain classes shared by every program and
`common/bench.hpp` the micro-benchmark harness. `scripts/` holds the tooling.
No third-party code is stored here; `scripts/fetch.sh` downloads it.

## Requirements

* Linux, `g++` (tested 13.3.0) and `clang++` (tested 18.1.3), `cmake` (3.28),
  `git`, `python3` >= 3.9, `strip`; optional `ninja` and `taskset`.
* Boost headers for Hypodermic only (signals2, algorithm/string, range). On
  Debian/Ubuntu: `apt-get install libboost-dev` (tested 1.83). Nothing else
  needs Boost: Fruit is built with `-DFRUIT_USES_BOOST=False`.

## Rerun everything

```sh
cd experiments
scripts/run.sh /tmp/di-deps /tmp/di-build results          # full run (~50 min on 4 vCPUs, scaling ~30 min)
# quick smoke run:
scripts/run.sh /tmp/di-deps /tmp/di-build /tmp/di-results --runs 1 \
    --scaling-n 10,25 --bench-iters 20000 --bench-repeats 3
```

`run.sh <deps-dir> <build-dir> [results-dir] [measure.py options]` runs, in order:

1. `scripts/fetch.sh <deps-dir>`: clones every framework at its pinned tag into
   its own new, empty `<deps-dir>/<name>` (`git clone --depth 1 --branch <tag>`,
   no submodules), fails if the tag no longer resolves to the pinned commit,
   and writes `<deps-dir>/VERSIONS.txt`.
2. `scripts/prepare_deps.sh <deps-dir> <build-dir>`:
   * Fruit is a compiled library. It is configured out-of-tree,
     `cmake -S <deps>/fruit -B <build>/fruit-<cc>-<std> -DCMAKE_BUILD_TYPE=Release
     -DBUILD_SHARED_LIBS=OFF -DFRUIT_USES_BOOST=False
     -DFRUIT_TESTS_USE_PRECOMPILED_HEADERS=OFF`, and only the `fruit` target is
     built (Fruit has no option to skip its tests directory, and its `all`
     target builds a test PCH). It is built 4 times: gcc/clang x 17/20.
     Fruit's CMakeLists appends a hard-coded `-std=c++11` after CMake's
     `-std=gnu++NN`, so the library itself is always compiled as C++11
     (the user program is compiled in the requested mode).
   * Kangaru is header-only but includes `kangaru/detail/config.hpp`, which
     its CMake configure step generates from `cmake/config/config.hpp.in`.
     To avoid running Kangaru's own CMake scripts, `prepare_deps.sh` performs
     the same `#cmakedefine` substitution itself (reverse destruction ON, the
     other options OFF) into `<build>/kangaru-config/generated/include`. The
     result is byte-identical to what
     `cmake -DKANGARU_REVERSE_DESTRUCTION=ON` produces.
3. `python3 -I scripts/measure.py --deps D --build B --results R [options]`
   runs these phases (default `--only matrix,sizes,diagnostics,bench,scaling`;
   `multitu` and `extras` are implemented but were **not exercised** in the
   2026-10-30 run, so treat them as untested; restrict with
   `--frameworks boost_di,fruit,...`; results of earlier phases already in
   `R/results.json` are kept, and scaling records are merged by
   framework/N/compiler and saved after every compile):
   * **matrix**: every framework x {g++, clang++} x {C++17, C++20}:
     `-O2 -Wall -Wextra -c` of the single translation unit; records exit
     status, every warning (with file and flag), compile wall time (one
     warm-up compile, then the median of `--runs` compiles, default 5), the
     compiler's peak RSS (`wait4`), the preprocessed size (`-E -P` lines),
     the binary size after `strip --strip-all`, and the program's output.
   * **diagnostics**: compiles each program with `-DDI_MISSING_BINDING`
     (the IRepository binding is removed) in C++20; records whether the error
     is reported at compile, link or run time, the length of the compiler
     output (lines/bytes) and the runtime message. When the compile only warns,
     it is repeated with `-Werror`. Full outputs land in
     `R/logs/diagnostics/`.
   * **bench**: builds each program with `-O2 -DNDEBUG -DDI_BENCH` (C++20) and
     runs it pinned to the last CPU (`taskset`), `--bench-iters` iterations
     (default 200000) x `--bench-repeats` (default 7) after a 10 % warm-up;
     reports median/min/max ns per operation.
   * **scaling**: `scripts/gen_scaling.py` generates a DAG of N
     interface/implementation pairs (1-3 `shared_ptr` constructor dependencies
     each) for every framework, compiles it (C++20 `-O2 -c`, median of
     `--scaling-runs`, default 3, one run if a compile exceeds 60 s), links it
     and checks that the printed checksum matches the manual wiring.
     `--scaling-n` (default `10,25,50,100`), `--scaling-timeout` (default
     300 s; the whole process group is killed), `--mem-limit-gb`
     (address-space limit per compile via `RLIMIT_AS`, like
     `prlimit --as`, default 6 GiB, so a runaway compile fails instead of
     waking the OOM killer; hitting either cap is recorded as a result) and
     `--scaling-cap` (default `fruit_flat=10,qlibs_di=10`: largest N per
     framework, because those two blow up, see "Notes on the 2026-10-30 run").
     `fruit` uses one component function per class (installing its
     dependencies' components); `fruit_flat` puts all bindings in one
     component.
   * **multitu**: the same graph split the way Fruit is meant to be used, one
     translation unit per component plus `main.cpp`, compiled in parallel with
     `nproc` jobs (like `make -j4`); manual and Fruit only (the other
     frameworks need every concrete constructor visible where the graph is
     wired). `--multitu-n` (default `50,100`).
   * **sizes**: adds `.text`/data/bss sizes (`size`) of the matrix binaries
     (stripped file sizes are quantised by 4 KiB segment alignment).
   * **extras**: (a) compile time of a TU that includes only the Boost headers
     Hypodermic uses, to attribute Hypodermic's compile time; (b) Fruit's 10
     library sources compiled directly in C++17/C++20 with `-Wall -Wextra`
     (Fruit's CMake always compiles them as C++11).

Outputs: `R/results.json` (all raw data), `R/summary.md` (tables),
`R/measure.log`, `R/logs/` (compiler/runtime outputs). Objects and binaries
stay in `<build-dir>`. Nothing inside `<deps-dir>` is executed: sources and
include paths are passed to the compilers as arguments.

## The scenario (`common/domain.hpp`)

| Type | Binding | Lifetime |
|---|---|---|
| `ILogger` -> `CountingLogger` | interface | shared (one per container) |
| `IRepository` -> `InMemoryRepository` | interface | shared |
| `IClock` -> `SteadyClock` | interface | unique (new per resolution) |
| `Config` | instance, by reference | external |
| `Service(shared_ptr<ILogger>, shared_ptr<IRepository>, unique_ptr<IClock>, const Config&)` | concrete | unique |
| `App(unique_ptr<Service>, shared_ptr<ILogger>)` | root | resolved twice |

Every program resolves two `App` objects and `check_wiring()` verifies the
semantics (same logger/repository in both, different Service/clock, Config is
the caller's object). Expected output:
`<name>: wiring_errors=0 handled=6 log_lines=8 repo_size=6`.

Each `main.cpp` has three build modes: default (the scenario),
`-DDI_MISSING_BINDING` (diagnostics) and `-DDI_BENCH` (micro-benchmark with two
cases: `graph` = create the container, register, resolve App, destroy; and
`resolve` = resolve App from an existing container). `boost_di/main.cpp` also
accepts `-DDI_BOOST_SHARED_EXT` (see below).

### Where the programs cannot be identical

* **Boost.DI**: closest to the scenario. Its core `di::singleton` scope stores
  objects in function-local statics, so "shared" means one per process, not
  per injector; creating a new injector does not create new singletons. The
  benchmark therefore also runs a variant with the extension scope
  `di::extension::shared` (per-injector, needs
  `-I<deps>/boost_di/extension/include`).
* **Fruit**: objects are owned by the injector (one per injector). Fruit
  cannot inject `std::unique_ptr<T>` parameters; per-resolution objects are
  `std::function<std::unique_ptr<T>()>` factories. Leaf types use
  `registerConstructor<>()` (no `INJECT` macros, to keep the domain shared)
  and `Service`/`App` are registered with `registerFactory()` and a small
  lambda. The root is obtained as a factory and called twice. Fruit's
  `shared_ptr<ILogger>` does not own the object, so the injector must
  outlive the Apps. The benchmark adds `graph_normalized` (an injector built
  from a pre-built `NormalizedComponent`, Fruit's recommended per-request
  pattern).
* **Kangaru**: every type needs an explicit service-definition struct with a
  `kgr::dependency<...>` list. Only single services can override an
  interface, so the unique clock is defined on `SteadyClock` itself (its
  `unique_ptr<SteadyClock>` converts to `unique_ptr<IClock>`).
* **dingo**: every resolved type must be registered, including `Service`
  and `App`.
* **Hypodermic**: auto-wires only `std::shared_ptr<T>` constructor arguments
  and hands out `std::shared_ptr<T>`; it cannot provide `unique_ptr` or
  `const&` parameters, so `App` is registered with an instance factory that
  builds `Service` and the clock itself and asks the container only for
  `ILogger`, `IRepository` and `Config`.
* **qlibs/di**: a building block, not a container. Bindings are an overload
  set of lambdas consulted for each constructor parameter; scopes are DIY
  (here: a `Scope` struct captured by reference, one per "container"). Its
  generic `std::unique_ptr` path returns a `std::shared_ptr` (see
  `ctor_traits<std::unique_ptr<T>>` in the header), so `unique_ptr`
  bindings are written explicitly. C++20 only.

### Compiling a program by hand

```sh
D=/tmp/di-deps B=/tmp/di-build E=$PWD   # E = this experiments/ folder
g++ -std=c++20 -O2 -Wall -Wextra -I$E/common $E/manual/main.cpp -o manual
g++ -std=c++20 -O2 -Wall -Wextra -I$E/common -I$D/boost_di/include $E/boost_di/main.cpp -o boost_di
g++ -std=c++20 -O2 -Wall -Wextra -I$E/common -I$D/fruit/include -I$B/fruit-gcc-20/include \
    $E/fruit/main.cpp $B/fruit-gcc-20/src/libfruit.a -o fruit
g++ -std=c++20 -O2 -Wall -Wextra -I$E/common -I$D/kangaru/include \
    -I$B/kangaru-config/generated/include $E/kangaru/main.cpp -o kangaru
g++ -std=c++20 -O2 -Wall -Wextra -I$E/common -I$D/dingo/include $E/dingo/main.cpp -o dingo
g++ -std=c++20 -O2 -Wall -Wextra -I$E/common -I$D/hypodermic $E/hypodermic/main.cpp -o hypodermic
g++ -std=c++20 -O2 -Wall -Wextra -I$E/common -I$D/qlibs_di $E/qlibs_di/main.cpp -o qlibs_di
# add -DDI_MISSING_BINDING or -DDI_BENCH (and -DNDEBUG) for the other modes
```

## Notes on the 2026-10-30 run (`results/`)

* The main run (`scripts/run.sh` with the defaults of that time: scaling
  timeout 600 s, no memory limit) completed matrix, diagnostics and bench,
  then the container restarted during the scaling phase, while compiling
  `qlibs_di` N=25 with Clang (it had reached ~5.1 GB RSS after 102 s; that
  compile has no result). The scaling records for N=10 and N=25 were
  recovered from `results/measure.log` (median of up to 3 runs; binary
  sizes were not logged) and are marked as recovered in `results.json`.
* Scaling blow-ups found in the main run: `fruit_flat` at N=25 was killed by
  the container's memory-cgroup OOM killer at about 13.9 GB RSS with both
  compilers (`results/logs/scaling/oom_kills_dmesg.txt`); `qlibs_di` at N=25
  took 518 s and 10.8 GB with GCC (2.75 s at N=10). Both were therefore not
  run at N=50 or N=100 (recorded as `skipped`), and the defaults now cap
  them at N=10 (`--scaling-cap`).
* After the restart, N=50 and N=100 were run one framework at a time, one
  compile at a time, with `--only scaling --scaling-timeout 300
  --mem-limit-gb 5.6` (about 6 GB); no compile hit either cap. Hypodermic
  was run with `--scaling-runs 1` to save time. `--only sizes` was then run
  to add `.text` sizes. `multitu` and `extras` were not run.
* `results/exploratory/bench_20k_x3.json` is an earlier, shorter benchmark
  run (20,000 iterations x 3) kept to show run-to-run noise.

## Caveats

* The machine is a shared 4-vCPU KVM guest; compile times are wall-clock
  medians and benchmark numbers can move by 10-30 % between runs. Compare
  ratios, not absolute values.
* Compile time is measured for one translation unit. Fruit is designed to
  spread components across translation units, so a single TU is its worst
  case; Fruit's library build (about 5 s with `-j4`) is not included.
* Binary sizes include the static `libfruit.a` for Fruit.
* The benchmark measures construction cost only, with `bench::do_not_optimize`
  (an `asm volatile` memory barrier on the resolved object) as the sink.
