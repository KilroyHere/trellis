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
