# Spike S2 results (full run)

Date 2026-10-30T22:23:49Z; 4 vCPU (Intel(R) Xeon(R) Processor @ 2.80GHz); load average at start 2.72, 2.32, 1.84.

- gcc-13: g++-13 (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0
- gcc-14: g++-14 (Ubuntu 14.2.0-4ubuntu2~24.04.1) 14.2.0
- clang-18: Ubuntu clang version 18.1.3 (1ubuntu1)
- clang-20: Ubuntu clang version 20.1.2 (0ubuntu1~24.04.3)

## stats

Nodes each strategy instantiates, and recursion depths in hops. The memo check's stack is the depth of its depth-first search, with the parameters in the generated order (k-1 last) and reversed (k-1 first); the depth and path strategies recurse along every path, so they reach the longest path.

| fan-in | N | edges | memo: nodes | depth: (node, depth) pairs | path: (node, path) pairs | longest path | memo stack | memo stack, reversed parameters |
|---|---|---|---|---|---|---|---|---|
| 1-3 | 25 | 69 | 25 | 253 | 3.51e+04 | 25 | 7 | 25 |
| 1-3 | 50 | 144 | 50 | 1,080 | 3.27e+07 | 50 | 11 | 50 |
| 1-3 | 100 | 294 | 100 | 4,278 | 2.88e+10 | 100 | 14 | 100 |
| 1-3 | 250 | 743 | 250 | 28,514 | 3.15e+16 | 250 | 35 | 250 |
| 1-3 | 500 | 1493 | 500 | 118,984 | 6.64e+24 | 500 | 48 | 500 |
| 1-3 | 1000 | 2993 | 1000 | 480,163 | 2.16e+35 | 1000 | 66 | 1000 |
| 10 | 25 | 195 | 25 | 282 | 1.3e+06 | 24 | 4 | 24 |
| 10 | 50 | 436 | 50 | 1,162 | 7.29e+09 | 46 | 6 | 46 |
| 10 | 100 | 901 | 100 | 4,736 | 7.97e+14 | 91 | 8 | 91 |
| 10 | 250 | 2251 | 250 | 29,953 | 1.18e+24 | 226 | 16 | 226 |
| 10 | 500 | 4501 | 500 | 121,621 | 9e+35 | 451 | 21 | 451 |
| 10 | 1000 | 9001 | 1000 | 488,108 | 3.61e+50 | 901 | 35 | 901 |


## cycles

Depth strategy built with S2_MAX_DEPTH=100. Errors counts lines with `error:`; lines and bytes are the whole output (-fsyntax-only, NFR-10 warnings, no -Werror).

| compiler | variant | case | errors | lines | bytes | kind | first error |
|---|---|---|---|---|---|---|---|
| gcc-13 | s1-stub | 1: A -> B -> A | 4 | 69 | 12486 | other error | `the value of 'injector<singleton<A>, singleton<B> >::can_resolve<A&>' is not usable in a constant expression` |
| gcc-13 | s1-stub | 2: A -> B -> C -> D -> E -> A | 4 | 78 | 16581 | other error | `the value of 'injector<singleton<A>, singleton<B>, singleton<C>, singleton<D>, singleton<E> >::can_resolve<A&>' is not usable in a constant ` |
| gcc-13 | s1-stub | 3: A -> A (A takes A*) | 2 | 20 | 3230 | other error | `'consteval bool constructible_from() [with T = A; Ctx = injector<singleton<A> >]' called in a constant expression before its definition is c` |
| gcc-13 | s1-stub | 4: App -> X -> A -> B -> A | 3 | 46 | 9563 | other error, depends on itself | `satisfaction of atomic constraint '!(can_resolve<Ctx, T&>) [with Ctx = Ctx; T = T]' depends on itself` |
| gcc-13 | memo | 1: A -> B -> A | 4 | 36 | 5967 | other error, depends on itself | `satisfaction of atomic constraint 'ctor_ok<Map, typename binding_of<Map, typename request<R>::key, decltype((find_binding<typename request<R` |
| gcc-13 | memo | 2: A -> B -> C -> D -> E -> A | 4 | 36 | 5967 | other error, depends on itself | `satisfaction of atomic constraint 'ctor_ok<Map, typename binding_of<Map, typename request<R>::key, decltype((find_binding<typename request<R` |
| gcc-13 | memo | 3: A -> A (A takes A*) | 4 | 50 | 5396 | other error | `the value of 'ctor_ok<app, A, life::singleton, integer_sequence<long unsigned int, 0> >' is not usable in a constant expression` |
| gcc-13 | memo | 4: App -> X -> A -> B -> A | 3 | 29 | 3959 | other error, depends on itself | `satisfaction of atomic constraint '!(can_take<Map, T&, L, next_state<State> >) [with Map = Map; T = T; L = L; State = State]' depends on its` |
| gcc-13 | depth | 1: A -> B -> A | 1 | 11 | 1045 | TRL-E007 | `static assertion failed: TRL-E007: dependency chain longer than S2_MAX_DEPTH hops: a dependency cycle?` |
| gcc-13 | depth | 2: A -> B -> C -> D -> E -> A | 1 | 11 | 1045 | TRL-E007 | `static assertion failed: TRL-E007: dependency chain longer than S2_MAX_DEPTH hops: a dependency cycle?` |
| gcc-13 | depth | 3: A -> A (A takes A*) | 1 | 11 | 1037 | TRL-E007 | `static assertion failed: TRL-E007: dependency chain longer than S2_MAX_DEPTH hops: a dependency cycle?` |
| gcc-13 | depth | 4: App -> X -> A -> B -> A | 1 | 11 | 1055 | TRL-E007 | `static assertion failed: TRL-E007: dependency chain longer than S2_MAX_DEPTH hops: a dependency cycle?` |
| gcc-13 | path | 1: A -> B -> A | 1 | 11 | 1065 | TRL-E007 | `static assertion failed: TRL-E007: dependency cycle (the path<...> argument lists it)` |
| gcc-13 | path | 2: A -> B -> C -> D -> E -> A | 1 | 11 | 1074 | TRL-E007 | `static assertion failed: TRL-E007: dependency cycle (the path<...> argument lists it)` |
| gcc-13 | path | 3: A -> A (A takes A*) | 1 | 11 | 1043 | TRL-E007 | `static assertion failed: TRL-E007: dependency cycle (the path<...> argument lists it)` |
| gcc-13 | path | 4: App -> X -> A -> B -> A | 1 | 11 | 1085 | TRL-E007 | `static assertion failed: TRL-E007: dependency cycle (the path<...> argument lists it)` |
| gcc-14 | s1-stub | 1: A -> B -> A | 4 | 137 | 17556 | other error | `the value of 'injector<singleton<A>, singleton<B> >::can_resolve<A&>' is not usable in a constant expression` |
| gcc-14 | s1-stub | 2: A -> B -> C -> D -> E -> A | 4 | 137 | 19305 | other error | `the value of 'injector<singleton<A>, singleton<B>, singleton<C>, singleton<D>, singleton<E> >::can_resolve<A&>' is not usable in a constant ` |
| gcc-14 | s1-stub | 3: A -> A (A takes A*) | 2 | 38 | 4476 | other error | `'consteval bool constructible_from() [with T = A; Ctx = injector<singleton<A> >]' called in a constant expression before its definition is c` |
| gcc-14 | s1-stub | 4: App -> X -> A -> B -> A | 3 | 88 | 11978 | other error, depends on itself | `satisfaction of atomic constraint '!(can_resolve<Ctx, T&>) [with Ctx = Ctx; T = T]' depends on itself` |
| gcc-14 | memo | 1: A -> B -> A | 4 | 97 | 10355 | other error, depends on itself | `satisfaction of atomic constraint 'ctor_ok<Map, typename binding_of<Map, typename request<R>::key, decltype((find_binding<typename request<R` |
| gcc-14 | memo | 2: A -> B -> C -> D -> E -> A | 4 | 148 | 16052 | other error, depends on itself | `satisfaction of atomic constraint 'ctor_ok<Map, typename binding_of<Map, typename request<R>::key, decltype((find_binding<typename request<R` |
| gcc-14 | memo | 3: A -> A (A takes A*) | 4 | 82 | 7517 | other error | `the value of 'ctor_ok<app, A, life::singleton, integer_sequence<long unsigned int, 0> >' is not usable in a constant expression` |
| gcc-14 | memo | 4: App -> X -> A -> B -> A | 3 | 97 | 10154 | other error, depends on itself | `satisfaction of atomic constraint '!(can_take<Map, T&, L, next_state<State> >) [with Map = Map; T = T; L = L; State = State]' depends on its` |
| gcc-14 | depth | 1: A -> B -> A | 1 | 48 | 4969 | TRL-E007 | `static assertion failed: TRL-E007: dependency chain longer than S2_MAX_DEPTH hops: a dependency cycle?` |
| gcc-14 | depth | 2: A -> B -> C -> D -> E -> A | 1 | 48 | 4969 | TRL-E007 | `static assertion failed: TRL-E007: dependency chain longer than S2_MAX_DEPTH hops: a dependency cycle?` |
| gcc-14 | depth | 3: A -> A (A takes A*) | 1 | 48 | 4945 | TRL-E007 | `static assertion failed: TRL-E007: dependency chain longer than S2_MAX_DEPTH hops: a dependency cycle?` |
| gcc-14 | depth | 4: App -> X -> A -> B -> A | 1 | 48 | 4979 | TRL-E007 | `static assertion failed: TRL-E007: dependency chain longer than S2_MAX_DEPTH hops: a dependency cycle?` |
| gcc-14 | path | 1: A -> B -> A | 1 | 31 | 2887 | TRL-E007 | `static assertion failed: TRL-E007: dependency cycle (the path<...> argument lists it)` |
| gcc-14 | path | 2: A -> B -> C -> D -> E -> A | 1 | 48 | 4906 | TRL-E007 | `static assertion failed: TRL-E007: dependency cycle (the path<...> argument lists it)` |
| gcc-14 | path | 3: A -> A (A takes A*) | 1 | 23 | 1938 | TRL-E007 | `static assertion failed: TRL-E007: dependency cycle (the path<...> argument lists it)` |
| gcc-14 | path | 4: App -> X -> A -> B -> A | 1 | 47 | 4813 | TRL-E007 | `static assertion failed: TRL-E007: dependency cycle (the path<...> argument lists it)` |
| clang-18 | s1-stub | 1: A -> B -> A | 3 | 91 | 10220 | other error | `substitution into constraint expression resulted in a non-constant expression` |
| clang-18 | s1-stub | 2: A -> B -> C -> D -> E -> A | 3 | 91 | 11246 | other error | `substitution into constraint expression resulted in a non-constant expression` |
| clang-18 | s1-stub | 3: A -> A (A takes A*) | 6 | 217 | 23788 | other error | `constexpr variable 'can_resolve<A *>' must be initialized by a constant expression` |
| clang-18 | s1-stub | 4: App -> X -> A -> B -> A | 2 | 44 | 5255 | other error, depends on itself | `satisfaction of constraint 'Ctx::template can_resolve<R>' depends on itself` |
| clang-18 | memo | 1: A -> B -> A | 3 | 88 | 7801 | other error | `substitution into constraint expression resulted in a non-constant expression` |
| clang-18 | memo | 2: A -> B -> C -> D -> E -> A | 3 | 88 | 7803 | other error | `substitution into constraint expression resulted in a non-constant expression` |
| clang-18 | memo | 3: A -> A (A takes A*) | 5 | 160 | 13915 | other error | `substitution into constraint expression resulted in a non-constant expression` |
| clang-18 | memo | 4: App -> X -> A -> B -> A | 2 | 47 | 4159 | other error, depends on itself | `satisfaction of constraint 'ctor_ok<Map, typename binding_t<Map, R>::impl, binding_t<Map, R>::lifetime, arity_seq<typename binding_t<Map, R>` |
| clang-18 | depth | 1: A -> B -> A | 1 | 41 | 3530 | TRL-E007 | `static assertion failed due to requirement 'sizeof(app) == 0': TRL-E007: dependency chain longer than S2_MAX_DEPTH hops: a dependency cycle?` |
| clang-18 | depth | 2: A -> B -> C -> D -> E -> A | 1 | 41 | 3530 | TRL-E007 | `static assertion failed due to requirement 'sizeof(app) == 0': TRL-E007: dependency chain longer than S2_MAX_DEPTH hops: a dependency cycle?` |
| clang-18 | depth | 3: A -> A (A takes A*) | 1 | 41 | 3524 | TRL-E007 | `static assertion failed due to requirement 'sizeof(app) == 0': TRL-E007: dependency chain longer than S2_MAX_DEPTH hops: a dependency cycle?` |
| clang-18 | depth | 4: App -> X -> A -> B -> A | 1 | 41 | 3538 | TRL-E007 | `static assertion failed due to requirement 'sizeof(app) == 0': TRL-E007: dependency chain longer than S2_MAX_DEPTH hops: a dependency cycle?` |
| clang-18 | path | 1: A -> B -> A | 1 | 41 | 3540 | TRL-E007 | `static assertion failed due to requirement 'sizeof(path<false, A, B, A>) == 0': TRL-E007: dependency cycle (the path<...> argument lists it)` |
| clang-18 | path | 2: A -> B -> C -> D -> E -> A | 1 | 41 | 3577 | TRL-E007 | `static assertion failed due to requirement 'sizeof(path<false, A, B, C, D, E, A>) == 0': TRL-E007: dependency cycle (the path<...> argument ` |
| clang-18 | path | 3: A -> A (A takes A*) | 1 | 41 | 3521 | TRL-E007 | `static assertion failed due to requirement 'sizeof(path<false, A, A>) == 0': TRL-E007: dependency cycle (the path<...> argument lists it)` |
| clang-18 | path | 4: App -> X -> A -> B -> A | 1 | 41 | 3581 | TRL-E007 | `static assertion failed due to requirement 'sizeof(path<false, App, X, A, B, A>) == 0': TRL-E007: dependency cycle (the path<...> argument l` |
| clang-20 | s1-stub | 1: A -> B -> A | 3 | 91 | 10192 | other error | `substitution into constraint expression resulted in a non-constant expression` |
| clang-20 | s1-stub | 2: A -> B -> C -> D -> E -> A | 3 | 91 | 11218 | other error | `substitution into constraint expression resulted in a non-constant expression` |
| clang-20 | s1-stub | 3: A -> A (A takes A*) | 6 | 217 | 23718 | other error | `constexpr variable 'can_resolve<A *>' must be initialized by a constant expression` |
| clang-20 | s1-stub | 4: App -> X -> A -> B -> A | 2 | 44 | 5241 | other error, depends on itself | `satisfaction of constraint 'Ctx::template can_resolve<R>' depends on itself` |
| clang-20 | memo | 1: A -> B -> A | 3 | 88 | 7801 | other error | `substitution into constraint expression resulted in a non-constant expression` |
| clang-20 | memo | 2: A -> B -> C -> D -> E -> A | 3 | 88 | 7803 | other error | `substitution into constraint expression resulted in a non-constant expression` |
| clang-20 | memo | 3: A -> A (A takes A*) | 5 | 160 | 13915 | other error | `substitution into constraint expression resulted in a non-constant expression` |
| clang-20 | memo | 4: App -> X -> A -> B -> A | 2 | 47 | 4159 | other error, depends on itself | `satisfaction of constraint 'ctor_ok<Map, typename binding_t<Map, R>::impl, binding_t<Map, R>::lifetime, arity_seq<typename binding_t<Map, R>` |
| clang-20 | depth | 1: A -> B -> A | 1 | 41 | 3530 | TRL-E007 | `static assertion failed due to requirement 'sizeof(app) == 0': TRL-E007: dependency chain longer than S2_MAX_DEPTH hops: a dependency cycle?` |
| clang-20 | depth | 2: A -> B -> C -> D -> E -> A | 1 | 41 | 3530 | TRL-E007 | `static assertion failed due to requirement 'sizeof(app) == 0': TRL-E007: dependency chain longer than S2_MAX_DEPTH hops: a dependency cycle?` |
| clang-20 | depth | 3: A -> A (A takes A*) | 1 | 41 | 3524 | TRL-E007 | `static assertion failed due to requirement 'sizeof(app) == 0': TRL-E007: dependency chain longer than S2_MAX_DEPTH hops: a dependency cycle?` |
| clang-20 | depth | 4: App -> X -> A -> B -> A | 1 | 41 | 3538 | TRL-E007 | `static assertion failed due to requirement 'sizeof(app) == 0': TRL-E007: dependency chain longer than S2_MAX_DEPTH hops: a dependency cycle?` |
| clang-20 | path | 1: A -> B -> A | 1 | 41 | 3540 | TRL-E007 | `static assertion failed due to requirement 'sizeof(path<false, A, B, A>) == 0': TRL-E007: dependency cycle (the path<...> argument lists it)` |
| clang-20 | path | 2: A -> B -> C -> D -> E -> A | 1 | 41 | 3577 | TRL-E007 | `static assertion failed due to requirement 'sizeof(path<false, A, B, C, D, E, A>) == 0': TRL-E007: dependency cycle (the path<...> argument ` |
| clang-20 | path | 3: A -> A (A takes A*) | 1 | 41 | 3521 | TRL-E007 | `static assertion failed due to requirement 'sizeof(path<false, A, A>) == 0': TRL-E007: dependency cycle (the path<...> argument lists it)` |
| clang-20 | path | 4: App -> X -> A -> B -> A | 1 | 41 | 3581 | TRL-E007 | `static assertion failed due to requirement 'sizeof(path<false, App, X, A, B, A>) == 0': TRL-E007: dependency cycle (the path<...> argument l` |


## depth

Smallest -ftemplate-depth that compiles a chain of K hops; the slope is the instantiation levels per hop. The longest chain is at the compiler's default limits (-ftemplate-depth=900 on GCC, 1024 on Clang).

| compiler | config | 50 hops | 100 hops | 250 hops | 500 hops | levels/hop | longest chain at default limits | what stops it |
|---|---|---|---|---|---|---|---|---|
| gcc-13 | s1-stub value | 312 | 612 | 1512 | 3012 | 6 | 148 | template depth |
| gcc-13 | memo value | 154 | 304 | 754 | 1504 | 3 | 298 | template depth |
| gcc-13 | memo shared_ptr | 215 | 415 | 1015 | 2015 | 4 | 221 | template depth |
| gcc-13 | depth value | 154 | 304 | 754 | 1504 | 3 | 298 | template depth |
| gcc-13 | path value | 154 | 304 | 754 | 1504 | 3 | 298 | template depth |
| gcc-13 | memo value, check only | 110 | 210 | 510 | 1010 | 2 | 445 | template depth |
| gcc-13 | memo shared_ptr, check only | 110 | 210 | 510 | 1010 | 2 | 445 | template depth |
| gcc-14 | s1-stub value | 312 | 612 | 1512 | 3012 | 6 | 148 | template depth |
| gcc-14 | memo value | 155 | 305 | 755 | 1505 | 3 | 298 | template depth |
| gcc-14 | memo shared_ptr | 215 | 415 | 1015 | 2015 | 4 | 221 | template depth |
| gcc-14 | depth value | 155 | 305 | 755 | 1505 | 3 | 298 | template depth |
| gcc-14 | path value | 155 | 305 | 755 | 1505 | 3 | 298 | template depth |
| gcc-14 | memo value, check only | 110 | 210 | 510 | 1010 | 2 | 445 | template depth |
| gcc-14 | memo shared_ptr, check only | 110 | 210 | 510 | 1010 | 2 | 445 | template depth |
| clang-18 | s1-stub value | 418 | 818 | 2018 | 4018 | 8 | 125 | template depth |
| clang-18 | memo value | 218 | 418 | 1018 | 2018 | 4 | 251 | template depth |
| clang-18 | memo shared_ptr | 218 | 418 | 1018 | 2018 | 4 | 251 | template depth |
| clang-18 | depth value | 218 | 418 | 1018 | 2018 | 4 | 251 | template depth |
| clang-18 | path value | 218 | 418 | 1018 | - | 4 | 251 | template depth |
| clang-18 | memo value, check only | 217 | 417 | 1017 | 2017 | 4 | 251 | template depth |
| clang-18 | memo shared_ptr, check only | 217 | 417 | 1017 | 2017 | 4 | 251 | template depth |
| clang-20 | s1-stub value | 418 | 818 | 2018 | 4018 | 8 | 125 | template depth |
| clang-20 | memo value | 218 | 418 | 1018 | 2018 | 4 | 251 | template depth |
| clang-20 | memo shared_ptr | 218 | 418 | 1018 | 2018 | 4 | 251 | template depth |
| clang-20 | depth value | 218 | 418 | 1018 | 2018 | 4 | 251 | template depth |
| clang-20 | path value | 218 | 418 | 1018 | - | 4 | 251 | template depth |
| clang-20 | memo value, check only | 217 | 417 | 1017 | 2017 | 4 | 251 | template depth |
| clang-20 | memo shared_ptr, check only | 217 | 417 | 1017 | 2017 | 4 | 251 | template depth |

Not measured:
- clang-18 path value: 500 hops: other error at -ftemplate-depth=8128: spikes/S2-memoization-cycles/stub.hpp:217:79: fatal error: instantiating fold expression with 257 a
- clang-20 path value: 500 hops: other error at -ftemplate-depth=8128: spikes/S2-memoization-cycles/stub.hpp:217:79: fatal error: instantiating fold expression with 257 a


## scaling

Median compile time (s) and peak memory (MB) of `-std=c++20 -O2 -c`, and in brackets the ratio to hand-wiring (manual) in the same run: time×, memory×. Every compiled program was linked, run and printed the equivalence oracle's line.

| compiler | fan-in | N | manual | memo | depth | path |
|---|---|---|---|---|---|---|
| clang-18 | 1-3 | 10 |  |  |  | 1.35 s, 141 MB (3 runs) |
| clang-18 | 1-3 | 15 |  |  |  | 2.77 s, 203 MB (3 runs) |
| clang-18 | 1-3 | 20 |  |  |  | 12.09 s, 783 MB |
| clang-18 | 1-3 | 25 | 2.52 s, 146 MB (3 runs) | 2.31 s, 151 MB [0.92×, 1.04×] (3 runs) | 2.51 s, 166 MB [1.00×, 1.14×] (3 runs) | 34.44 s, 1934 MB [13.67×, 13.25×] |
| clang-18 | 1-3 | 50 | 4.01 s, 180 MB (3 runs) | 4.22 s, 189 MB [1.05×, 1.05×] (3 runs) | 5.38 s, 260 MB [1.34×, 1.45×] (3 runs) | **memory cap** after 67 s, 3698 MB |
| clang-18 | 1-3 | 100 | 7.60 s, 248 MB (3 runs) | 7.05 s, 264 MB [0.93×, 1.06×] (3 runs) | 14.30 s, 561 MB [1.88×, 2.26×] | **not attempted** |
| clang-18 | 1-3 | 250 | 21.00 s, 454 MB | 18.46 s, 494 MB [0.88×, 1.09×] | 73.23 s, 2510 MB [3.49×, 5.53×] |  |
| clang-18 | 1-3 | 500 | 45.03 s, 796 MB | 39.64 s, 875 MB [0.88×, 1.10×] |  |  |
| clang-18 | 1-3 | 1000 | 124.09 s, 1482 MB | 88.88 s, 1631 MB [0.72×, 1.10×] |  |  |
| clang-18 | 10 | 10 |  |  |  | 1.98 s, 152 MB (3 runs) |
| clang-18 | 10 | 15 |  |  |  | 12.92 s, 708 MB |
| clang-18 | 10 | 20 |  |  |  | **memory cap** after 75 s, 3692 MB |
| clang-18 | 10 | 25 | 2.89 s, 153 MB (3 runs) | 2.97 s, 154 MB [1.03×, 1.01×] (3 runs) | 3.62 s, 185 MB [1.25×, 1.21×] (3 runs) | **not attempted** |
| clang-18 | 10 | 50 | 5.42 s, 196 MB (3 runs) | 5.66 s, 196 MB [1.05×, 1.00×] (3 runs) | 9.56 s, 360 MB [1.76×, 1.84×] (3 runs) | **not attempted** |
| clang-18 | 10 | 100 | 12.58 s, 279 MB | 13.09 s, 278 MB [1.04×, 1.00×] | 30.26 s, 1002 MB [2.41×, 3.59×] | **not attempted** |
| clang-18 | 10 | 250 | 28.43 s, 528 MB | 31.23 s, 524 MB [1.10×, 0.99×] | **memory cap** after 94 s, 3692 MB |  |
| gcc-13 | 1-3 | 10 |  |  |  | 1.38 s, 130 MB (3 runs) |
| gcc-13 | 1-3 | 15 |  |  |  | 2.06 s, 175 MB (3 runs) |
| gcc-13 | 1-3 | 20 |  |  |  | 5.98 s, 467 MB (3 runs) |
| gcc-13 | 1-3 | 25 | 2.60 s, 180 MB (3 runs) | 2.31 s, 173 MB [0.89×, 0.96×] (3 runs) | 2.45 s, 179 MB [0.94×, 0.99×] (3 runs) | 12.68 s, 1068 MB [4.88×, 5.92×] |
| gcc-13 | 1-3 | 50 | 4.02 s, 255 MB (3 runs) | 3.54 s, 238 MB [0.88×, 0.93×] (3 runs) | 3.67 s, 270 MB [0.91×, 1.06×] (3 runs) | **memory cap** after 51 s, 3881 MB |
| gcc-13 | 1-3 | 100 | 7.59 s, 383 MB (3 runs) | 6.46 s, 353 MB [0.85×, 0.92×] (3 runs) | 9.13 s, 466 MB [1.20×, 1.22×] (3 runs) | **memory cap** after 43 s, 3881 MB |
| gcc-13 | 1-3 | 250 | 21.21 s, 814 MB | 16.20 s, 739 MB [0.76×, 0.91×] | 34.88 s, 1601 MB [1.64×, 1.97×] |  |
| gcc-13 | 1-3 | 500 | 44.27 s, 1564 MB | 43.20 s, 1415 MB [0.98×, 0.90×] |  |  |
| gcc-13 | 1-3 | 1000 | 179.48 s, 3107 MB | 110.43 s, 2738 MB [0.62×, 0.88×] |  |  |
| gcc-13 | 10 | 10 |  |  |  | 2.02 s, 146 MB (3 runs) |
| gcc-13 | 10 | 15 |  |  |  | 6.12 s, 417 MB (3 runs) |
| gcc-13 | 10 | 20 |  |  |  | 38.83 s, 2551 MB |
| gcc-13 | 10 | 25 | 2.80 s, 191 MB (3 runs) | 2.59 s, 180 MB [0.92×, 0.94×] (3 runs) | 2.72 s, 193 MB [0.97×, 1.01×] (3 runs) | **memory cap** after 54 s, 3881 MB |
| gcc-13 | 10 | 50 | 4.85 s, 286 MB (3 runs) | 4.54 s, 269 MB [0.93×, 0.94×] (3 runs) | 5.69 s, 315 MB [1.17×, 1.10×] (3 runs) | **not attempted** |
| gcc-13 | 10 | 100 | 11.01 s, 439 MB | 10.07 s, 409 MB [0.91×, 0.93×] | 15.31 s, 632 MB [1.39×, 1.44×] | **not attempted** |
| gcc-13 | 10 | 250 | 33.46 s, 896 MB | 26.45 s, 867 MB [0.79×, 0.97×] | 68.01 s, 2426 MB [2.03×, 2.71×] |  |

