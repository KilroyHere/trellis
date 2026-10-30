# DI experiment results

- Date (UTC): 2026-10-30T04:34:27Z
- Host: Linux vm 6.18.44-fc-v185 #1 SMP PREEMPT_DYNAMIC @0 x86_64 x86_64
- CPU: Intel(R) Xeon(R) Processor @ 2.10GHz (4 logical CPUs)
- Compilers: g++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0 / Ubuntu clang version 18.1.3 (1ubuntu1); cmake version 3.28.3; Boost headers 1_83
- Flags: -O2 -Wall -Wextra, single translation unit
- Framework versions:

```
# name tag commit commit-date
boost_di v1.3.2 20b6454c889f23c1e7c9eb5136cd7e3d6f6d718d 2025-04-02
fruit v3.7.1 8149131119475ccaae3c1ae386fe9f2c679317f0 2023-01-21
kangaru v4.3.2 ab498eb9db0de74ffebc46f395b9a31093017412 2024-04-29
dingo v0.2.0 ccafcb6cbde54fb2819a3da8243106642f3b1711 2026-04-02
hypodermic v2.5.3 0e0d85d70aa2f2391dfd84f8af4a3863d4fb1611 2021-09-21
qlibs_di v1.0.6 bdafce9baa19dfc503d6acd5f5abf91f503fa5ef 2025-04-27
```

## Compile matrix (scenario program)

| framework | compiler | std | compiles | warnings (-Wall -Wextra) | compile s (median) | min-max | runs | peak RSS MB | stripped bytes | .text bytes | preprocessed lines | run output |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| manual | gcc | c++17 | yes | 0 | 0.49 | 0.48-0.51 | 5 | 86.7 | 22832 | 9966 | 28924 | manual: wiring_errors=0 handled=6 log_lines=8 repo_size=6 |
| manual | gcc | c++20 | yes | 0 | 1.20 | 1.15-1.38 | 5 | 164.5 | 18736 | 10037 | 64923 | manual: wiring_errors=0 handled=6 log_lines=8 repo_size=6 |
| manual | clang | c++17 | yes | 0 | 0.49 | 0.47-0.57 | 5 | 119.2 | 14744 | 8451 | 27286 | manual: wiring_errors=0 handled=6 log_lines=8 repo_size=6 |
| manual | clang | c++20 | yes | 0 | 1.41 | 1.35-1.50 | 5 | 151.9 | 14744 | 8451 | 63353 | manual: wiring_errors=0 handled=6 log_lines=8 repo_size=6 |
| boost_di | gcc | c++17 | yes | 0 | 0.74 | 0.72-0.86 | 5 | 128.6 | 22832 | 11271 | 51420 | boost_di: wiring_errors=0 handled=6 log_lines=8 repo_size=6 |
| boost_di | gcc | c++20 | yes | 0 | 1.47 | 1.37-1.55 | 5 | 206.2 | 22832 | 11345 | 82893 | boost_di: wiring_errors=0 handled=6 log_lines=8 repo_size=6 |
| boost_di | clang | c++17 | yes | 0 | 0.86 | 0.83-0.94 | 5 | 140.7 | 22992 | 10333 | 48844 | boost_di: wiring_errors=0 handled=6 log_lines=8 repo_size=6 |
| boost_di | clang | c++20 | yes | 0 | 1.64 | 1.60-1.76 | 5 | 171.3 | 22992 | 10333 | 81118 | boost_di: wiring_errors=0 handled=6 log_lines=8 repo_size=6 |
| fruit | gcc | c++17 | yes | 0 | 1.44 | 1.34-1.47 | 5 | 180.2 | 145712 | 129029 | 48033 | fruit: wiring_errors=0 handled=6 log_lines=8 repo_size=6 |
| fruit | gcc | c++20 | yes | 0 | 2.20 | 2.14-2.24 | 5 | 264.8 | 145712 | 130313 | 81331 | fruit: wiring_errors=0 handled=6 log_lines=8 repo_size=6 |
| fruit | clang | c++17 | yes | 0 | 1.57 | 1.56-1.63 | 5 | 161.7 | 133800 | 123313 | 45685 | fruit: wiring_errors=0 handled=6 log_lines=8 repo_size=6 |
| fruit | clang | c++20 | yes | 0 | 2.43 | 2.23-2.63 | 5 | 207.4 | 133800 | 123313 | 79600 | fruit: wiring_errors=0 handled=6 log_lines=8 repo_size=6 |
| kangaru | gcc | c++17 | yes | 3 | 1.39 | 1.35-1.53 | 5 | 173.4 | 35120 | 22937 | 48515 | kangaru: wiring_errors=0 handled=6 log_lines=8 repo_size=6 |
| kangaru | gcc | c++20 | yes | 3 | 2.06 | 2.00-2.17 | 5 | 248.9 | 35120 | 22695 | 76699 | kangaru: wiring_errors=0 handled=6 log_lines=8 repo_size=6 |
| kangaru | clang | c++17 | yes | 3 | 1.91 | 1.83-2.00 | 5 | 149.3 | 35304 | 21422 | 46277 | kangaru: wiring_errors=0 handled=6 log_lines=8 repo_size=6 |
| kangaru | clang | c++20 | yes | 3 | 2.71 | 2.65-2.92 | 5 | 190.3 | 35304 | 21422 | 75118 | kangaru: wiring_errors=0 handled=6 log_lines=8 repo_size=6 |
| dingo | gcc | c++17 | yes | 0 | 2.97 | 2.83-3.01 | 5 | 227.9 | 117112 | 108077 | 53248 | dingo: wiring_errors=0 handled=6 log_lines=8 repo_size=6 |
| dingo | gcc | c++20 | yes | 0 | 3.54 | 3.45-3.64 | 5 | 305.3 | 117112 | 105775 | 85246 | dingo: wiring_errors=0 handled=6 log_lines=8 repo_size=6 |
| dingo | clang | c++17 | yes | 0 | 2.87 | 2.84-3.03 | 5 | 172.8 | 113224 | 101621 | 50918 | dingo: wiring_errors=0 handled=6 log_lines=8 repo_size=6 |
| dingo | clang | c++20 | yes | 0 | 3.80 | 3.73-3.82 | 5 | 215.2 | 121416 | 105730 | 83570 | dingo: wiring_errors=0 handled=6 log_lines=8 repo_size=6 |
| hypodermic | gcc | c++17 | yes | 0 | 8.25 | 8.16-8.47 | 5 | 526.3 | 317792 | 294515 | 115854 | hypodermic: wiring_errors=0 handled=6 log_lines=8 repo_size=6 |
| hypodermic | gcc | c++20 | yes | 0 | 9.23 | 8.63-9.44 | 5 | 522.0 | 317792 | 294972 | 139137 | hypodermic: wiring_errors=0 handled=6 log_lines=8 repo_size=6 |
| hypodermic | clang | c++17 | yes | 0 | 7.66 | 7.55-7.87 | 5 | 322.0 | 326536 | 304361 | 121912 | hypodermic: wiring_errors=0 handled=6 log_lines=8 repo_size=6 |
| hypodermic | clang | c++20 | yes | 0 | 8.13 | 8.06-8.62 | 5 | 354.1 | 326520 | 303414 | 143728 | hypodermic: wiring_errors=0 handled=6 log_lines=8 repo_size=6 |
| qlibs_di | gcc | c++17 | NO | 0 | - | --- | - | - | - | - | - | /tmp/claude-0/-home-user/2d27a639-9f98-5872-8a89-4b383c577f29/scratchpad/deps/qlibs_di/di:705:28: error: ‘concept’ does  |
| qlibs_di | gcc | c++20 | yes | 0 | 1.21 | 1.14-1.29 | 5 | 179.6 | 18736 | 10837 | 65194 | qlibs_di: wiring_errors=0 handled=6 log_lines=8 repo_size=6 |
| qlibs_di | clang | c++17 | NO | 0 | - | --- | - | - | - | - | - | /tmp/claude-0/-home-user/2d27a639-9f98-5872-8a89-4b383c577f29/scratchpad/deps/qlibs_di/di:705:28: error: unknown type na |
| qlibs_di | clang | c++20 | yes | 0 | 1.50 | 1.39-1.53 | 5 | 163.8 | 18840 | 9687 | 63623 | qlibs_di: wiring_errors=0 handled=6 log_lines=8 repo_size=6 |

### Distinct warnings

- kangaru gcc c++17: `<deps>/kangaru/include/kangaru/container.hpp`: unused variable ‘service’ [-Wunused-variable]
- kangaru gcc c++20: `<deps>/kangaru/include/kangaru/container.hpp`: unused variable ‘service’ [-Wunused-variable]
- kangaru clang c++17: `<deps>/kangaru/include/kangaru/container.hpp`: unused variable 'service' [-Wunused-variable]
- kangaru clang c++20: `<deps>/kangaru/include/kangaru/container.hpp`: unused variable 'service' [-Wunused-variable]

## Missing binding (IRepository binding removed, C++20)

| framework | compiler | detected | compile rc | stderr lines | stderr bytes | error lines | link/run stderr bytes | runtime rc | first error / runtime message |
|---|---|---|---|---|---|---|---|---|---|
| manual | gcc | compile time | 1 | 4 | 417 | 1 | - | - | /home/user/cpp-di-research/experiments/manual/main.cpp:27:50: error: ‘repository’ was not declared in this scope |
| manual | clang | compile time | 1 | 4 | 291 | 2 | - | - | /home/user/cpp-di-research/experiments/manual/main.cpp:27:50: error: use of undeclared identifier 'repository' |
| boost_di | gcc | link time (compile only warned) | 0 | 18 | 3553 | 2 | 1757 | - | link: main.cpp:(.text._ZNK5boost3ext2di6v1_3_04core8injectorINS2_6configENS3_4poolINS2_3aux9type_listIJEEEEEJNS3_10dependencyINS2_6scopes8instanceEN4demo6ConfigERSF_NS2_7no_nameEvNS3_4noneEEENSB_INSC_9singletonENSE_7ILog |
| boost_di | clang | compile time | 1 | 37 | 3118 | 4 | - | - | /tmp/claude-0/-home-user/2d27a639-9f98-5872-8a89-4b383c577f29/scratchpad/deps/boost_di/include/boost/di.hpp:1169:2: error: inline function 'boost::di::concepts::abstract_type<demo::IRepository>::is_not_bound::error' is n |
| fruit | gcc | compile time | 1 | 22 | 9595 | 4 | - | - | /tmp/claude-0/-home-user/2d27a639-9f98-5872-8a89-4b383c577f29/scratchpad/deps/fruit/include/fruit/impl/injection_errors.h: In instantiation of ‘struct fruit::impl::NoBindingFoundForAbstractClassError<demo::IRepository, d |
| fruit | clang | compile time | 1 | 30 | 5324 | 4 | - | - | /tmp/claude-0/-home-user/2d27a639-9f98-5872-8a89-4b383c577f29/scratchpad/deps/fruit/include/fruit/impl/injection_errors.h:38:17: error: static assertion failed due to requirement 'AlwaysFalse<demo::IRepository>::value':  |
| kangaru | gcc | runtime (compiles, with warnings) | 0 | 24 | 10185 | 0 | 137 | -6 |   what():  No instance found for the requested abstract service |
| kangaru | clang | runtime (compiles, with warnings) | 0 | 58 | 7717 | 0 | 137 | -6 |   what():  No instance found for the requested abstract service |
| dingo | gcc | runtime (compiles) | 0 | 0 | 0 | 0 | 184 | -6 |   what():  type not found: std::shared_ptr<demo::IRepository> (required by demo::App -> demo::Service) |
| dingo | clang | runtime (compiles) | 0 | 0 | 0 | 0 | 184 | -6 |   what():  type not found: std::shared_ptr<demo::IRepository> (required by demo::App -> demo::Service) |
| hypodermic | gcc | runtime (compiles) | 0 | 0 | 0 | 0 | 515 | -6 |   what():  Unable to activate instance of type demo.App: Unable to resolve demo.IRepository - Throw location: resolveIfTypeCanBeRegistered in /tmp/claude-0/-home-user/2d27a639-9f98-5872-8a89-4b383c577f29/scratchpad/deps/ |
| hypodermic | clang | runtime (compiles) | 0 | 0 | 0 | 0 | 515 | -6 |   what():  Unable to activate instance of type demo.App: Unable to resolve demo.IRepository - Throw location: resolveIfTypeCanBeRegistered in /tmp/claude-0/-home-user/2d27a639-9f98-5872-8a89-4b383c577f29/scratchpad/deps/ |
| qlibs_di | gcc | compile time | 1 | 64 | 30388 | 5 | - | - | /tmp/claude-0/-home-user/2d27a639-9f98-5872-8a89-4b383c577f29/scratchpad/deps/qlibs_di/di:836:24: error: cannot allocate an object of abstract type ‘demo::IRepository’ |
| qlibs_di | clang | compile time | 1 | 81 | 13722 | 3 | - | - | /tmp/claude-0/-home-user/2d27a639-9f98-5872-8a89-4b383c577f29/scratchpad/deps/qlibs_di/di:850:30: error: return type 'decltype(di::make<typename std::remove_cvref_t<decltype(t)>::value_type>(t))' (aka 'demo::IRepository' |

## Runtime (C++20, -O2 -DNDEBUG, ns per operation)

`graph` = create container/injector + register bindings + resolve App + destroy everything; `resolve` = resolve App from an existing container (shared objects already built). Fruit `graph_normalized` = injector from a pre-built NormalizedComponent.

| framework | compiler | case | median ns | min ns | max ns | iters x repeats |
|---|---|---|---|---|---|---|
| manual | gcc | graph | 58.2 | 56.8 | 58.5 | 200000 x 7 |
| manual | gcc | resolve | 26.6 | 26.2 | 31.6 | 200000 x 7 |
| manual | clang | graph | 52.6 | 52.1 | 53.0 | 200000 x 7 |
| manual | clang | resolve | 26.5 | 26.1 | 26.7 | 200000 x 7 |
| boost_di | gcc | graph | 27.4 | 26.9 | 29.6 | 200000 x 7 |
| boost_di | gcc | resolve | 29.0 | 28.6 | 29.8 | 200000 x 7 |
| boost_di | clang | graph | 35.9 | 35.7 | 36.2 | 200000 x 7 |
| boost_di | clang | resolve | 35.7 | 35.6 | 35.9 | 200000 x 7 |
| fruit | gcc | graph | 1529.2 | 1467.1 | 1585.9 | 200000 x 7 |
| fruit | gcc | graph_normalized | 663.5 | 662.4 | 755.6 | 200000 x 7 |
| fruit | gcc | resolve | 101.9 | 101.4 | 105.5 | 200000 x 7 |
| fruit | clang | graph | 1516.6 | 1456.3 | 1588.5 | 200000 x 7 |
| fruit | clang | graph_normalized | 736.0 | 648.0 | 917.6 | 200000 x 7 |
| fruit | clang | resolve | 104.0 | 103.4 | 107.5 | 200000 x 7 |
| kangaru | gcc | graph | 579.0 | 568.7 | 588.5 | 200000 x 7 |
| kangaru | gcc | resolve | 39.8 | 39.5 | 41.0 | 200000 x 7 |
| kangaru | clang | graph | 551.2 | 540.0 | 570.8 | 200000 x 7 |
| kangaru | clang | resolve | 40.8 | 40.1 | 41.9 | 200000 x 7 |
| dingo | gcc | graph | 1064.0 | 1025.7 | 1214.1 | 200000 x 7 |
| dingo | gcc | resolve | 273.4 | 268.8 | 298.6 | 200000 x 7 |
| dingo | clang | graph | 1199.6 | 1156.2 | 1397.1 | 200000 x 7 |
| dingo | clang | resolve | 326.2 | 321.7 | 567.5 | 200000 x 7 |
| hypodermic | gcc | graph | 7845.0 | 7005.5 | 8074.8 | 200000 x 7 |
| hypodermic | gcc | resolve | 656.5 | 649.2 | 674.5 | 200000 x 7 |
| hypodermic | clang | graph | 7619.2 | 7196.0 | 8106.2 | 200000 x 7 |
| hypodermic | clang | resolve | 598.4 | 585.4 | 638.4 | 200000 x 7 |
| qlibs_di | gcc | graph | 55.6 | 52.6 | 60.8 | 200000 x 7 |
| qlibs_di | gcc | resolve | 27.3 | 25.6 | 30.6 | 200000 x 7 |
| qlibs_di | clang | graph | 58.9 | 55.2 | 62.3 | 200000 x 7 |
| qlibs_di | clang | resolve | 27.6 | 27.2 | 32.9 | 200000 x 7 |
| boost_di_shared_ext | gcc | graph | 163.8 | 152.0 | 176.0 | 200000 x 7 |
| boost_di_shared_ext | gcc | resolve | 32.6 | 31.7 | 37.3 | 200000 x 7 |
| boost_di_shared_ext | clang | graph | 179.3 | 175.5 | 197.3 | 200000 x 7 |
| boost_di_shared_ext | clang | resolve | 41.0 | 39.5 | 51.1 | 200000 x 7 |

## Scaling (generated DAG of N shared components, C++20 -O2, compile only)

| framework | N | compiler | compile s (median) | runs | peak RSS MB | stripped bytes | checksum ok | note |
|---|---|---|---|---|---|---|---|---|
| manual | 10 | clang | 0.90 | 3 | 133.4 | - | True | [recovered from measure.log after the container restart; main run, median of up to 3 runs, no binary size] |
| manual | 10 | gcc | 1.06 | 3 | 121.8 | - | True | [recovered from measure.log after the container restart; main run, median of up to 3 runs, no binary size] |
| manual | 25 | clang | 1.64 | 3 | 143.8 | - | True | [recovered from measure.log after the container restart; main run, median of up to 3 runs, no binary size] |
| manual | 25 | gcc | 2.13 | 3 | 173.4 | - | True | [recovered from measure.log after the container restart; main run, median of up to 3 runs, no binary size] |
| manual | 50 | clang | 2.18 | 3 | 171.6 | 145792 | True |  |
| manual | 50 | gcc | 2.51 | 3 | 244.4 | 133424 | True |  |
| manual | 100 | clang | 4.23 | 3 | 231.8 | 289152 | True |  |
| manual | 100 | gcc | 5.51 | 3 | 362.9 | 264496 | True |  |
| boost_di | 10 | clang | 1.33 | 3 | 150.8 | - | True | [recovered from measure.log after the container restart; main run, median of up to 3 runs, no binary size] |
| boost_di | 10 | gcc | 1.40 | 3 | 169.6 | - | True | [recovered from measure.log after the container restart; main run, median of up to 3 runs, no binary size] |
| boost_di | 25 | clang | 2.38 | 3 | 180.6 | - | True | [recovered from measure.log after the container restart; main run, median of up to 3 runs, no binary size] |
| boost_di | 25 | gcc | 2.66 | 3 | 235.7 | - | True | [recovered from measure.log after the container restart; main run, median of up to 3 runs, no binary size] |
| boost_di | 50 | clang | 3.29 | 3 | 231.3 | 178616 | True |  |
| boost_di | 50 | gcc | 3.72 | 3 | 316.1 | 162096 | True |  |
| boost_di | 100 | clang | 6.30 | 3 | 341.1 | 346552 | True |  |
| boost_di | 100 | gcc | 6.84 | 3 | 487.4 | 313648 | True |  |
| fruit | 10 | clang | 2.59 | 3 | 231.2 | - | True | [recovered from measure.log after the container restart; main run, median of up to 3 runs, no binary size] |
| fruit | 10 | gcc | 2.46 | 3 | 279.7 | - | True | [recovered from measure.log after the container restart; main run, median of up to 3 runs, no binary size] |
| fruit | 25 | clang | 5.85 | 3 | 364.5 | - | True | [recovered from measure.log after the container restart; main run, median of up to 3 runs, no binary size] |
| fruit | 25 | gcc | 4.85 | 3 | 400.2 | - | True | [recovered from measure.log after the container restart; main run, median of up to 3 runs, no binary size] |
| fruit | 50 | clang | 8.09 | 3 | 589.2 | 330392 | True |  |
| fruit | 50 | gcc | 9.47 | 3 | 625.5 | 334128 | True |  |
| fruit | 100 | clang | 16.25 | 3 | 1039.3 | 551576 | True |  |
| fruit | 100 | gcc | 18.06 | 3 | 1070.6 | 551216 | True |  |
| fruit_flat | 10 | clang | 5.55 | 3 | 470.5 | - | True | [recovered from measure.log after the container restart; main run, median of up to 3 runs, no binary size] |
| fruit_flat | 10 | gcc | 4.02 | 3 | 486.2 | - | True | [recovered from measure.log after the container restart; main run, median of up to 3 runs, no binary size] |
| fruit_flat | 25 | clang | - | - | - | - | - | rc=-9 - OOM-killed by the container's memory cgroup at ~13.9 GB RSS (logs/scaling/oom_kills_dmesg.txt) [recovered from measure.log after the container restart; main run, median of up to 3 runs, no binary size] |
| fruit_flat | 25 | gcc | - | - | - | - | - | rc=1 - OOM-killed by the container's memory cgroup at ~13.9 GB RSS (logs/scaling/oom_kills_dmesg.txt) [recovered from measure.log after the container restart; main run, median of up to 3 runs, no binary size] |
| fruit_flat | 50 | - | - | - | - | - | - | skipped: above --scaling-cap: N=25 was OOM-killed at ~13.9 GB RSS with GCC and Clang (2026-10-30) |
| fruit_flat | 100 | - | - | - | - | - | - | skipped: above --scaling-cap: N=25 was OOM-killed at ~13.9 GB RSS with GCC and Clang (2026-10-30) |
| kangaru | 10 | clang | 3.58 | 3 | 195.8 | - | True | [recovered from measure.log after the container restart; main run, median of up to 3 runs, no binary size] |
| kangaru | 10 | gcc | 3.27 | 3 | 273.6 | - | True | [recovered from measure.log after the container restart; main run, median of up to 3 runs, no binary size] |
| kangaru | 25 | clang | 7.69 | 3 | 280.1 | - | True | [recovered from measure.log after the container restart; main run, median of up to 3 runs, no binary size] |
| kangaru | 25 | gcc | 7.39 | 3 | 414.8 | - | True | [recovered from measure.log after the container restart; main run, median of up to 3 runs, no binary size] |
| kangaru | 50 | clang | 10.48 | 3 | 422.7 | 465352 | True |  |
| kangaru | 50 | gcc | 11.02 | 3 | 668.3 | 342320 | True |  |
| kangaru | 100 | clang | 21.64 | 3 | 708.9 | 911816 | True |  |
| kangaru | 100 | gcc | 22.50 | 3 | 1145.4 | 657712 | True |  |
| dingo | 10 | clang | 4.92 | 3 | 221.8 | - | True | [recovered from measure.log after the container restart; main run, median of up to 3 runs, no binary size] |
| dingo | 10 | gcc | 4.62 | 3 | 312.4 | - | True | [recovered from measure.log after the container restart; main run, median of up to 3 runs, no binary size] |
| dingo | 25 | clang | 9.64 | 3 | 324.0 | - | True | [recovered from measure.log after the container restart; main run, median of up to 3 runs, no binary size] |
| dingo | 25 | gcc | 9.71 | 3 | 439.9 | - | True | [recovered from measure.log after the container restart; main run, median of up to 3 runs, no binary size] |
| dingo | 50 | clang | 14.08 | 3 | 495.6 | 883256 | True |  |
| dingo | 50 | gcc | 14.13 | 3 | 624.5 | 727416 | True |  |
| dingo | 100 | clang | 27.72 | 3 | 840.3 | 1722936 | True |  |
| dingo | 100 | gcc | 26.85 | 3 | 1200.4 | 1411448 | True |  |
| hypodermic | 10 | clang | 12.29 | 3 | 394.4 | - | True | [recovered from measure.log after the container restart; main run, median of up to 3 runs, no binary size] |
| hypodermic | 10 | gcc | 13.46 | 3 | 664.6 | - | True | [recovered from measure.log after the container restart; main run, median of up to 3 runs, no binary size] |
| hypodermic | 25 | clang | 21.03 | 3 | 527.9 | - | True | [recovered from measure.log after the container restart; main run, median of up to 3 runs, no binary size] |
| hypodermic | 25 | gcc | 23.99 | 3 | 899.7 | - | True | [recovered from measure.log after the container restart; main run, median of up to 3 runs, no binary size] |
| hypodermic | 50 | clang | 29.01 | 1 | 755.1 | 2493296 | True |  |
| hypodermic | 50 | gcc | 36.97 | 1 | 1531.1 | 2455904 | True |  |
| hypodermic | 100 | clang | 54.89 | 1 | 1197.4 | 4766576 | True |  |
| hypodermic | 100 | gcc | 74.97 | 1 | 2625.6 | 4733280 | True |  |
| qlibs_di | 10 | clang | 6.58 | 3 | 177.6 | - | True | [recovered from measure.log after the container restart; main run, median of up to 3 runs, no binary size] |
| qlibs_di | 10 | gcc | 2.75 | 3 | 229.7 | - | True | [recovered from measure.log after the container restart; main run, median of up to 3 runs, no binary size] |
| qlibs_di | 25 | clang | - | - | - | - | - | did not finish: the container restarted while this compile was running (ps showed ~5.1 GB RSS after 102 s) [main run, interrupted] |
| qlibs_di | 25 | gcc | 518.08 | 1 | 10807.2 | - | True | [recovered from measure.log after the container restart; main run, median of up to 3 runs, no binary size] |
| qlibs_di | 50 | - | - | - | - | - | - | skipped: above --scaling-cap: N=25 took 518 s and 10.8 GB RSS with GCC (2026-10-30) |
| qlibs_di | 100 | - | - | - | - | - | - | skipped: above --scaling-cap: N=25 took 518 s and 10.8 GB RSS with GCC (2026-10-30) |

## Header sizes (files / lines in the pinned source trees)

| tree | files | lines |
|---|---|---|
| boost_di | 41 | 7776 |
| fruit | 90 | 15462 |
| fruit (library sources) | 10 | 1026 |
| kangaru | 46 | 6698 |
| dingo | 51 | 8062 |
| hypodermic | 95 | 4854 |
| qlibs_di | 1 | 1003 |
