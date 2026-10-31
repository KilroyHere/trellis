# Spike S2: memoization and cycles

> **Decision:** accepted by the owner on 2026-10-31; recorded in [ADR-0014](../adr/0014-resolution-memoization-and-cycles.md). The status line below is as the spike left it.

Status: **waiting for owner sign-off** (task T0.7). Code: `spikes/S2-memoization-cycles/`. Reproduce every number with `bash spikes/S2-memoization-cycles/run.sh --large` from the repository root. It takes about an hour on the 4-vCPU container; `--quick` takes about six minutes and is the verify command. It exits non-zero if an expectation below stops holding. Outputs go to `out/`, which is ignored by version control. `results/` holds a committed snapshot of the final full run, which was stopped twice to adjust the plan and resumed, keeping the records already measured.

## 1. Question

From plan §12.1 S2 and §7.3: how do per-key memoization and depth-bounded checking scale at compile time (N = 100 and 1,000, fan-in 3 and 10), what is the instantiation depth per dependency hop, and how should deduced mode handle dependency cycles? Concretely:

1. Compile time and peak memory of three resolution strategies on the S1 stub, at N = 25, 50, 100 and 250 with fan-in 3 and 10, on gcc-13 and clang-18, as ratios against hand-wiring (NFR-04, NFR-05): (a) memoization per (key, request form, consumer lifetime); (b) depth-bounded resolution, a depth counter in the key; (c) path-tracked resolution. N = 500 and 1,000 for the leading strategy only.
2. Instantiation depth per hop, from `-ftemplate-depth` sweeps on chains of 50 to 500 hops, and the deepest chain each strategy compiles at the default limits: confirm or revise NFR-09 (at least 200 hops).
3. What each compiler reports for a cycle under each strategy, and which of §7.3's three options to take.

Decides the §7.3 option and NFR-09.

## 2. Method

- **Compilers:** GCC 13.3.0 and 14.2.0, each with its own libstdc++; Clang 18.1.3 and 20.1.2 with GCC 14's libstdc++. All in `-std=c++20`, called directly, never through ccache. `results/env.json` records the exact versions and the machine. MSVC is not available (see §5). The scaling sweep uses gcc-13 and clang-18, as the task asks; the cycle and depth measurements use all four.
- **Caps:** every compile runs alone, in a new process group, under a 600 s timeout and a 4,000,000 KiB address-space cap (`ulimit -v` around the whole run, and RLIMIT_AS per compile through `benchmarks/compile/run.py`). A compile that hits a cap is recorded as a result. The machine (4 vCPU) was shared with another agent's builds, at a load average of about 1 to 2.5.
- **The stub** (`stub.hpp`, throwaway): S1's injector stub, extended so it can wire `gen_graph.py`'s graphs.
  - Discovery is S1's, unchanged: `deduce.hpp`'s context-free `probe_arg` and `ctor_info<T>`, memoized per class.
  - Interface bindings `singleton<I, C>` and `unique<I, C>` in an inheritance-based map (plan §8.1 C2). The user names the map (`struct app : s2::map<...> {}`), so instantiated names stay short (C5); taking the binding list itself as `injector<Bindings...>` would put all N bindings into the name of every node and creation-code symbol (not measured).
  - The request forms `gen_graph.py` generates, with plan §6.3's rules: `T` (unique), `T&`, `const T&`, `T*`, `const T*` (singleton), `unique_ptr<T>` (unique), `shared_ptr<T>` (singleton, or unique when the consumer is unique: the captive rule, E006, is why the consumer lifetime is in the key).
  - **Two passes.** The *check* is S1's operator set with deleted twins (`check_arg`); the recursion runs through the operators' constraints. `S2_STRATEGY` chooses what the recursion carries, and so what the compiler memoizes:
    - **memo** (0): nothing. One node `ctor_ok<Map, Impl, Lifetime, Seq>` per (implementation, lifetime) and one satisfaction of `can_take<Map, Request, ConsumerLifetime>` per (request, consumer lifetime): plan §7.3's key.
    - **depth** (1): the hop count, so one node per (implementation, lifetime, depth). Above `S2_MAX_DEPTH` hops it raises TRL-E007.
    - **path** (2): the list of classes on the path, so one node per (implementation, lifetime, path). A class already on the path raises TRL-E007, and the path type names the cycle.

    The *creation* code uses `make_arg`, the same two operator shapes without constraints, so overload resolution picks the same operator as the check did. It is the same code under every strategy, so the differences measured are the check's.
  - **The hop is lean on purpose:** the node is a variable template partial specialization on the arity's `index_sequence`, initialized directly with `__is_constructible(Impl, check_arg...)`; the per-node checks are separate atoms of one constraint, so `&&` and `||` short-circuit satisfaction; and no constexpr function call stays open across a hop. `self_check`, a consteval function, returns before the recursion starts.
  - **Cycle reports** (depth and path): the check raises TRL-E007 where it finds the cycle and counts the node as unresolvable. On failure, `create()` runs a *relaxed* pass that treats the same nodes as resolvable without an error; if it passes, the cycle was the only cause and `create()` adds nothing. Without it, Clang added the root's E002: after a failed `static_assert` it marks the class invalid, so "report, then succeed" does not work there. The relaxed pass runs only on failure, like S3's explain pass.
- **Correctness:** every scaling program is linked, run and checked by T0.3's equivalence oracle (the identity line `gen_graph.py` predicts). Before the sweep, all three strategies also passed the oracle on gcc-13 and clang-18 for shared_ptr, reference and mixed graphs (mixed covers all seven request forms, both lifetimes, interfaces and concrete classes, and the captive rule).
- **Scaling** (`s2.py scaling`): `gen_graph.py --forms shared_ptr`, the research setting of NFR-04 and NFR-05, with fan-in 1–3 (the research graph, seed 12345) and fan-in 10 (Fruit's setting). Each program is measured by `run.py`'s `measure()`: `-std=c++20 -O2 -c`, median wall time of up to 3 compiles (a program whose first compile takes over 10 s gets one), peak RSS of the compiler's process tree from `wait4`. The hand-wired baseline is T0.3's `ManualVariant`, measured in the same run. A strategy that hits a cap at some N is not attempted at larger N for that compiler and fan-in.
- **Node counts** (`s2.py stats`): computed from the same graphs, they are exactly the nodes each strategy instantiates: N for memo; the number of distinct (node, distance from the root) pairs for depth; the number of root-to-node paths for path. The memo check visits parameters in order, each node once, so its recursion depth is the depth of that depth-first search, not the longest path.
- **Depth per hop** (`s2.py depth`): chains `C_K(C_{K-1}) … C_1(C_0)` of K = 50, 100, 250 and 500 hops, created from `C_K`, with `-fsyntax-only`. For each chain, a bisection finds the smallest `-ftemplate-depth` that compiles it; the slope over K is the levels per hop. (After two bisections, the line through them predicts the others, and two compiles confirm each prediction.) A second search finds the longest chain that compiles at the default limits (`-ftemplate-depth` 900 on GCC, 1024 on Clang, everything else default), trying up to 2,048 hops. Seven configurations:
  - S1's `injector_stub.hpp` as written (by value; it has no interfaces);
  - memo by value, and by `shared_ptr<I>` to singletons (the scaling graphs' form);
  - depth and path by value, both built with an unbounded `S2_MAX_DEPTH`, so that only the compiler's limits apply;
  - memo by value and by `shared_ptr<I>`, *check only*: `static_assert(injector::resolvable<Root>)` without `create()`, so without the creation code.
- **Cycles** (`cycles.cpp`): singletons requested by reference, in four cases: A → B → A; A → B → C → D → E → A; A → A (A takes `A*`); and App → X → A → B → A, below a root. Each is compiled with `-fsyntax-only` and the NFR-10 warning set (no `-Werror`), under S1's stub, memo, depth (`S2_MAX_DEPTH=100`) and path. Recorded: the first error line, the number of `error:` lines, and the size of the whole output.

## 3. Results

### 3.1 What each strategy instantiates

Exact counts for the generated graphs (`results/stats.md`). Recursion depths are in hops.

| fan-in | N | memo: nodes | depth: (node, depth) pairs | path: (node, path) pairs | longest path | memo's stack | memo's stack, parameters reversed |
|---|---|---|---|---|---|---|---|
| 1–3 | 25 | 25 | 253 | 3.5 × 10⁴ | 25 | 7 | 25 |
| 1–3 | 50 | 50 | 1,080 | 3.3 × 10⁷ | 50 | 11 | 50 |
| 1–3 | 100 | 100 | 4,278 | 2.9 × 10¹⁰ | 100 | 14 | 100 |
| 1–3 | 250 | 250 | 28,514 | 3.2 × 10¹⁶ | 250 | 35 | 250 |
| 1–3 | 1,000 | 1,000 | 480,163 | 2.2 × 10³⁵ | 1,000 | 66 | 1,000 |
| 10 | 25 | 25 | 282 | 1.3 × 10⁶ | 24 | 4 | 24 |
| 10 | 100 | 100 | 4,736 | 8.0 × 10¹⁴ | 91 | 8 | 91 |
| 10 | 250 | 250 | 29,953 | 1.2 × 10²⁴ | 226 | 16 | 226 |
| 10 | 1,000 | 1,000 | 488,108 | 3.6 × 10⁵⁰ | 901 | 35 | 901 |

- **Depth** instantiates about N²/2 nodes on these graphs, because component *k* depends on *k* − 1 and so is reached at almost every distance from the root: 43 times memo's count at N = 100, 114 times at 250 and 480 times at 1,000. Its recursion follows every distinct depth, so it reaches the longest path, which is about N.
- **Path** is exponential: the number of root-to-node paths. It passes 10⁷ at N = 50 with fan-in 1–3, and 10⁶ at N = 25 with fan-in 10.
- **Memo** recurses only the first time it meets a node, so its stack is the depth of a depth-first search: 14 hops at N = 100 and 66 at 1,000. That depends on parameter order. `gen_graph.py` lists dependencies by index, so the long chain through *k* − 1 comes last; with each class's longest-chain dependency first, the stack is the longest path.

### 3.2 Compile time and memory against hand-wiring

Hand-wired: median compile time and peak memory of `-std=c++20 -O2 -c`. Strategies: compile time ÷ hand-wired, then peak memory ÷ hand-wired, same compiler, same run. `results/scaling.md` has the absolute numbers. Every program that compiled also linked, ran and printed the oracle's line.

| fan-in | N | compiler | hand-wired | memo | depth | path |
|---|---|---|---|---|---|---|
| 1–3 | 25 | gcc-13 | 2.6 s, 180 MB | 0.89× / 0.96× | 0.94× / 0.99× | 4.88× / 5.92× |
| 1–3 | 25 | clang-18 | 2.5 s, 146 MB | 0.92× / 1.04× | 1.00× / 1.14× | 13.67× / 13.25× |
| 1–3 | 50 | gcc-13 | 4.0 s, 255 MB | 0.88× / 0.93× | 0.91× / 1.06× | **out of memory** (4 GB cap) after 51 s |
| 1–3 | 50 | clang-18 | 4.0 s, 180 MB | 1.05× / 1.05× | 1.34× / 1.45× | **out of memory** after 67 s |
| 1–3 | 100 | gcc-13 | 7.6 s, 383 MB | 0.85× / 0.92× | 1.20× / 1.22× | **out of memory** after 43 s |
| 1–3 | 100 | clang-18 | 7.6 s, 248 MB | 0.93× / 1.06× | 1.88× / 2.26× | — |
| 1–3 | 250 | gcc-13 | 21.2 s, 814 MB | 0.76× / 0.91× | 1.64× / 1.97× | — |
| 1–3 | 250 | clang-18 | 21.0 s, 454 MB | 0.88× / 1.09× | 3.49× / 5.53× | — |
| 1–3 | 500 | gcc-13 | 44.3 s, 1,564 MB | 0.98× / 0.90× | not run | not run |
| 1–3 | 500 | clang-18 | 45.0 s, 796 MB | 0.88× / 1.10× | not run | not run |
| 1–3 | 1,000 | gcc-13 | 179.5 s, 3,107 MB | 0.62× / 0.88× | not run | not run |
| 1–3 | 1,000 | clang-18 | 124.1 s, 1,482 MB | 0.72× / 1.10× | not run | not run |
| 10 | 25 | gcc-13 | 2.8 s, 191 MB | 0.92× / 0.94× | 0.97× / 1.01× | **out of memory** after 54 s |
| 10 | 25 | clang-18 | 2.9 s, 153 MB | 1.03× / 1.01× | 1.25× / 1.21× | — |
| 10 | 50 | gcc-13 | 4.9 s, 286 MB | 0.93× / 0.94× | 1.17× / 1.10× | — |
| 10 | 50 | clang-18 | 5.4 s, 196 MB | 1.05× / 1.00× | 1.76× / 1.84× | — |
| 10 | 100 | gcc-13 | 11.0 s, 439 MB | 0.91× / 0.93× | 1.39× / 1.44× | — |
| 10 | 100 | clang-18 | 12.6 s, 279 MB | 1.04× / 1.00× | 2.41× / 3.59× | — |
| 10 | 250 | gcc-13 | 33.5 s, 896 MB | 0.79× / 0.97× | 2.03× / 2.71× | — |
| 10 | 250 | clang-18 | 28.4 s, 528 MB | 1.10× / 0.99× | **out of memory** after 94 s | — |

"—": not attempted, because a smaller N already hit the cap. N = 500 and 1,000 were run for the leading strategy only, with fan-in 1–3 (NFR-04's research setting): fan-in 10's hand-wired N = 1,000 alone takes over 300 s per compile on gcc-13 (`run.py`), and the time went to the depth sweep instead. Path at smaller sizes, gcc-13 / clang-18: fan-in 1–3, N = 10, 15, 20: 1.4 / 1.4 s, 2.1 / 2.8 s, 6.0 / 12.1 s and 130 / 141, 175 / 203, 467 / 783 MB; fan-in 10, N = 10, 15, 20: 2.0 / 2.0 s, 6.1 / 12.9 s, 38.8 s / out of memory, and 146 / 152, 417 / 708, 2,551 MB.

Against the targets: NFR-04 allows 1.3× the hand-wired time at N = 100 and 1.5× at 1,000, with linear growth; NFR-05 allows 1.5× the memory at 100 and no layout-dependent blow-up; §7.3 makes option 1 the default only if it costs at most 1.2×.

- **Memo** stays within 10% of the hand-wired baseline, or below it, at every size and on both compilers: 0.76–1.10× the time and 0.91–1.09× the memory for N = 25 to 250. NFR-04 and NFR-05 hold with a wide margin at N = 100 (at most 1.04× and 1.06×). At N = 1,000 memo costs 0.62× (gcc-13) and 0.72× (clang-18) the hand-wired time and 0.88× and 1.10× its memory, within NFR-04's 1.5×. From N = 100 to 1,000 its time grows 17× (gcc-13) and 12.6× (clang-18), less than the baseline's 24× and 16×, which is itself superlinear, as T0.3 found. All of it fits under the 4 GB cap (2.7 GB at most). The baseline's numbers match T0.3's (gcc-13 at N = 100: 7.6 s and 383 MB here, 7.4 s and 383 MB there). Why memo can come out below 1.0× was not measured separately. A likely cause: the hand-wired code builds every singleton in one large constructor that `-O2` optimizes as a whole, while the stub's creation code is many small functions, one per (request, class), and the check costs little next to code generation.
- **Depth** grows with N, faster on Clang and with fan-in 10. At N = 100 it costs 1.20× / 1.22× (gcc-13) and 1.88× / 2.26× (clang-18) with fan-in 1–3, and 1.39× / 1.44× and 2.41× / 3.59× with fan-in 10. At N = 250 it reaches 2.03× / 2.71× on gcc-13 and 3.49× / 5.53× on clang-18, and runs out of memory on clang-18 with fan-in 10. So §7.3's 1.2× holds only on the smallest graphs (N ≤ 100 on gcc-13 with fan-in 1–3; N = 25 on clang-18). At N = 100, NFR-04 fails in three of the four cells and NFR-05 in two. The node count is quadratic (§3.1), so the ratios keep rising with N.
- **Path** explodes, as predicted: 4.9× / 5.9× (gcc-13) and 13.7× / 13.3× (clang-18) already at N = 25 with fan-in 1–3, and out of memory under the cap at N = 50 on both compilers (and at N = 100, tried once on gcc-13). With fan-in 10 it runs out of memory at N = 20 (clang-18) and 25 (gcc-13). From N = 15 on, its memory grows 2.3 to 6 times for every 5 more components.

### 3.3 Instantiation depth per hop (NFR-09)

Levels per hop (the slope of the smallest `-ftemplate-depth` that compiles a chain, over K = 50, 100, 250 and 500 hops; exactly linear in every cell), and the longest chain that compiles at the default limits. `results/depth.md` has the depth needed at each K.

Each cell: levels per hop / **longest chain at the default limits**, in hops.

| Configuration | gcc-13 | gcc-14 | clang-18 | clang-20 |
|---|---|---|---|---|
| S1's stub as written, by value | 6 / **148** | 6 / **148** | 8 / **125** | 8 / **125** |
| memo, by value | 3 / **298** | 3 / **298** | 4 / **251** | 4 / **251** |
| memo, `shared_ptr<I>` to singletons | 4 / **221** | 4 / **221** | 4 / **251** | 4 / **251** |
| depth, by value | 3 / **298** | 3 / **298** | 4 / **251** | 4 / **251** |
| path, by value | 3 / **298** | 3 / **298** | 4 / **251** | 4 / **251** |
| memo, by value, check only | 2 / **445** | 2 / **445** | 4 / **251** | 4 / **251** |
| memo, `shared_ptr<I>`, check only | 2 / **445** | 2 / **445** | 4 / **251** | 4 / **251** |

- **Every cell is exactly linear** from 50 to 500 hops, with intercepts of 4 to 18 levels, and every limit hit at the default settings is `-ftemplate-depth`: never `-fconstexpr-depth`, and no compiler crashed. GCC compiled the 500-hop chains at `-ftemplate-depth=3012` without running out of stack.
- **The check costs 2 levels per hop on GCC and 4 on Clang** (the check-only rows). **The creation code nests too:** both compilers instantiate a function template's body where it is first used, inside the body that uses it, so `construct → operator T → resolve` costs 3 levels per hop by value and 4 through a singleton's `instance()`. That is what limits GCC's chains (221 and 298 hops against the check's 445). On Clang the check alone already costs 4, and the creation code adds nothing beyond it.
- **Depth and path cost the same per hop as memo.** Their problem is the path the recursion takes (§3.1), not the cost of a hop.
- **S1's stub as written** costs 6 levels per hop on GCC and 8 on Clang, and reaches only 148 and 125 hops, below NFR-09's 200. Its hop runs through a variable template, a consteval function, `resolvable_seq`'s lambda and `std::is_constructible_v` (which instantiates `std::is_constructible`); the lean stub replaces all of them with one variable template and the builtin. Which construct costs how many levels was not measured one by one.
- **Clang also limits fold expressions to 256 operands** (`-fbracket-depth=256`): at 500 hops, the path strategy's fold over its path failed with `instantiating fold expression with 257 arguments exceeded expression nesting limit of 256`, even with a high template depth. The same limit would apply to any fold over a binding list, such as S1's `is_singleton` (inferred, not tested), so Trellis must not fold over all bindings of a module; the inheritance-based map (C2) does not.

### 3.4 Cycles

No strategy compiles a cycle, and no compiler crashes or recurses into its depth limit. Ranges over the four cases (`results/cycles.md` has every cell and `results/cycles.json` the first error lines):

| Strategy | Compiler | Errors | Lines (bytes) | First error |
|---|---|---|---|---|
| S1's stub | gcc-13, gcc-14 | 2–4 | 20–78, 38–137 (3–19 KB) | `the value of '…::can_resolve<A&>' is not usable in a constant expression`. A → A: `'consteval bool s1::constructible_from() […]' called in a constant expression before its definition is complete`. Below a root: `satisfaction of atomic constraint '…' depends on itself` |
| S1's stub | clang-18, clang-20 | 2–6 | 44–217 (5–24 KB) | `substitution into constraint expression resulted in a non-constant expression`. A → A: `constexpr variable 'can_resolve<A *>' must be initialized by a constant expression`. Below a root: `satisfaction of constraint 'Ctx::template can_resolve<R>' depends on itself` |
| memo | gcc-13, gcc-14 | 3–4 | 29–50, 82–148 (4–16 KB) | `satisfaction of atomic constraint 'ctor_ok<…>' depends on itself`. A → A: `the value of 's2::ctor_ok<app, A, …>' is not usable in a constant expression` |
| memo | clang-18, clang-20 | 2–5 | 47–160 (4–14 KB) | `substitution into constraint expression resulted in a non-constant expression`. Below a root: `satisfaction of constraint 'ctor_ok<…>' depends on itself` |
| depth (bound 100 hops) | all four | **1** | 11 (gcc-13), 48 (gcc-14), 41 (Clang); 1–5 KB | `static assertion failed: TRL-E007: dependency chain longer than S2_MAX_DEPTH hops: a dependency cycle?` |
| path | all four | **1** | 11, 23–48, 41; 1–5 KB | `static assertion failed …: TRL-E007: dependency cycle`. Clang's first error line contains the cycle: `sizeof(s2::path<false, A, B, C, D, E, A>) == 0`. GCC names it on the line above: `In instantiation of 'struct s2::cycle_found<s2::path<false, A, B, C, D, E, A> >'` |

- **Without a check (S1's stub, memo)** the failure is prompt and clean in one sense: it is a hard error at the first node that the recursion re-enters, whose value is still being computed, and it costs no more than a normal compile. It is not catchable as an error ID, though: the first line is the compiler's own, its wording depends on the compiler and on where the cycle closes, and the root's TRL-E002 follows it as a cascade. The classes on the cycle appear only in the instantiation notes. The plan feared "recursion-limit noise"; memoization prevents it, because no node is ever entered twice on one path.
- **Depth** gives exactly one error, with the ID on the first line, on all four compilers, but cannot say which classes form the cycle (Clang's notes show the request at the bound).
- **Path** gives exactly one error, with the ID, and names the cycle, the best diagnostic of the three, on all four compilers.
- Both need the relaxed pass of §2: before it was added, Clang printed the root's TRL-E002 after the TRL-E007 (2 errors).

## 4. Recommendation

1. **Confirm plan §7.3's memoization key and §8.1 C1.** Memoize per (key, request form, consumer lifetime), with no path or depth on the normal pass. On the S1 design it is the only strategy that stays within NFR-04 and NFR-05 at every size measured (§3.2), and its recursion depth is the depth of a depth-first search, not the longest path (§3.1).

2. **Cycles in deduced mode: take option 3 as the default, keep option 2 as an opt-in, and reject option 1.**
   - **Reject option 1** (a depth counter in the key by default). It fails §7.3's own condition ("make it the default if it costs ≤ 1.2×"): 1.20–2.41× the hand-wired compile time and 1.22–3.59× its memory at N = 100, 1.64–3.49× and 1.97–5.53× at N = 250, and out of memory under 4 GB on clang-18 at N = 250 with fan-in 10, against memo's 0.76–1.10× on the same graphs. It also puts a hard limit on graph *shape*: the check recurses along the longest path, at the same cost per hop as the memo check (§3.3), so a graph whose longest path exceeds about 250 hops on Clang (445 on GCC) does not compile at the default limits, whatever its size. With the bound below the compiler's limit, as a clean error needs, the bound itself rejects such graphs with a false E007. NFR-04's 1,000-component graphs have 900- to 1,000-hop longest paths. Finally, its message cannot name the cycle, only "longer than L hops".
   - **Option 3: no cycle detection on the normal pass of deduced mode.** Under memoization a cycle is never silently accepted, never crashes a compiler and never runs into a recursion limit: the recursion re-enters a node whose value is still being computed, and every compiler stops there with a hard error (§3.4). Document these signatures in the error catalog (E007, deduced mode), and point to the explicit or reflection route (`di::testing::validate`) and to option 2:
     - GCC 13 and 14: `satisfaction of atomic constraint '…' depends on itself`, or `the value of '…' is not usable in a constant expression` for a class that depends on itself.
     - Clang 18 and 20: `substitution into constraint expression resulted in a non-constant expression`, or `satisfaction of constraint '…' depends on itself` for a cycle below the root.
     - Then the root's own TRL-E002. In total 2 to 6 errors, 20 to 217 lines, 3 to 24 KB; the classes on the cycle appear only in the instantiation notes.
   - **Option 2: `options{.cycle_check = true}`** switches deduced mode to the depth-bounded check for a debugging or CI build. It then reports exactly one error, TRL-E007, on all four compilers. Two requirements come out of the spike:
     - the relaxed second pass on failure (§2), or Clang adds the root's E002;
     - a bound below every limit a valid chain can hit: Clang's check stops at 251 hops and GCC's creation code at 221, so a bound of 200 hops, NFR-09's own figure, leaves a margin on every compiler measured. A chain that passes the check then also compiles. The message must say "a cycle, or a chain longer than 200 hops".

     Its cost applies only when enabled: 1.2–2.4× the compile time at N = 100, and 1.6–3.5× or out of memory at N = 250. It is a tool for finding a cycle, not a mode to build a large graph in.
   - **Path tracking only where the graph is known.** Naming the cycle needs the path, and the path-tracked check is exponential on graphs that share dependencies: 4.9–13.7× the compile time at N = 25, and out of memory under 4 GB from N = 20–50. Keep the path for explicit and reflection modes, where §7.3's consteval depth-first search over dense indices finds the cycle without instantiating a path per node.

3. **NFR-09: confirm "≥ 200 hops at default limits", and make plan §8.1 C7 a tested budget for the creation code too.**
   - The lean check of §2 costs 2 levels per hop on GCC and 4 on Clang. The creation code nests as well, at 3 levels per hop by value and 4 through a singleton's `shared_ptr`, and whichever is larger sets the limit (§3.3).
   - Measured at the default limits: 221 hops (singletons by `shared_ptr`) and 298 (by value) on gcc-13 and gcc-14, and 251 on clang-18 and clang-20. NFR-09 holds on every compiler for the lean stub, with 10 to 50% to spare.
   - S1's stub, as written, costs 6 (GCC) and 8 (Clang) levels per hop and reaches only 148 and 125 hops (§3.3). Phase 1 must keep the lean shape: the node as a variable template partial specialization initialized from `__is_constructible`, constraint-level `&&`/`||`, no constexpr call open across a hop, and the creation code at most 4 levels per hop.
   - Add a CI test: a 200-hop chain in each request form, at the default limits, on every compiler. MSVC has no `-ftemplate-depth` and is the main risk (§5).
   - Why the generated graphs pass at N = 1,000 although NFR-09 is only 200 hops: their depth-first stack is 35 to 66 hops. A graph whose classes take their longest-chain dependency first would need the full 900 to 1,000 hops, so NFR-09, not N, is deduced mode's real size limit. This belongs in the documentation of deduced mode.

4. **Smaller amendments for Phase 1.**
   - **C5: the injector takes a named binding map** (`struct app : trellis::module<...> {}`), not the binding list, which would put all N bindings into every node's and every creation-code symbol's name.
   - **Failure passes run only on failure.** The relaxed pass costs nothing on success, like S3's explain pass, and the same `if constexpr` chain in `create()` can host both.
   - **Builtins:** the lean hop uses `__is_constructible` (and the path strategy `__is_same`) where the `std` traits would instantiate class templates. Both builtins need `config.hpp` fallbacks for MSVC; how many levels the builtin saves on its own was not measured.

**Cost of the alternatives.** Option 1 by default: 1.2–2.4× the compile time at N = 100 and up to 3.5× (or out of memory) at 250, rising quadratically, and no graph with a longest path over about 250 hops (Clang) or 445 (GCC). Path tracking by default: out of memory from N = 20–50. Option 3 alone, without option 2: a cycle in deduced mode gets only the compiler's message, until the user moves the graph to the explicit or reflection route.

## 5. Limits and open questions

- **MSVC risk (untested here; high for NFR-09, medium for cycles).**
  - MSVC has no `-ftemplate-depth`; its recursion limit for templates is fixed (error C1202, "recursive type or function dependency context too complex") and reported to be lower than GCC's and Clang's. A chain test on MSVC in CI (platforms.yml) should come first in Phase 1, together with S1's tests.
  - How MSVC reports a re-entered constraint satisfaction or a variable template used in its own initializer is unknown, so option 3's documented signatures need an MSVC entry.
  - `__is_constructible` exists on MSVC; `__is_same` may not, and `std::is_same_v` in the fold is the portable fallback (one more instantiation per path element, path strategy only).
- **Scaling profile.** Only `shared_ptr` graphs were measured (the research and NFR-04 setting). The reference and mixed form sets were checked for correctness, not timed. The consumer lifetime in the key multiplies nodes only where one class is requested from consumers of different lifetimes; these graphs have one lifetime.
- **Noise.** The machine was shared, so expect ±10%. Programs whose first compile took more than 10 s were compiled once, not three times. The ratios that decide anything here (memo within 10% of hand-wiring or below it; depth at 1.2–2.4× at N = 100 and growing; path out of memory) are well outside the noise.
- **Not tested:** aggregates, scoped lifetimes, factories, `lazy`, MSVC and Apple Clang. Option 2's cost at N = 1,000 was not measured: it cannot compile there at the default limits.
- **Warming the memo in binding order** (checking every binding's node in declaration order before the root, so that a graph listed dependencies-first never recurses more than a hop) was tried by hand and did not help: the creation code still nests 3–4 levels per hop (§3.3), and Clang refuses a fold over more than 256 bindings (`-fbracket-depth`). Lifting NFR-09's limit would need the creation code instantiated in binding order as well. This is a possible Phase 1 experiment, not part of this spike's numbers.
- **Explain pass.** S3's explain pass needs each class's parameter list, which deduced mode cannot enumerate. In deduced mode it can name the consumer whose construction failed, but not the parameter; the deleted twin names the request in the compiler's note (S1).

## Addendum: MSVC (2026-10-31, task T1.13)

S2 couldn't test MSVC, and rated the risk high for NFR-09 and medium for cycles (§5). Task T1.13 ran the chain-depth probe and the cycle cases on GitHub's `windows-latest` runner. The compilers, flags and runs are those of S1's addendum (docs/spikes/S1-deduced-discovery.md, "Addendum: MSVC"): cl 19.51.36260 (Visual Studio 2026, the runner's latest) and cl 19.44.35229 (Visual Studio 2022 17.14), with `/std:c++20 /permissive- /W4 /EHsc`. The numbers come from [run 41235251688](https://github.com/KilroyHere/trellis/actions/runs/41235251688). The opt-in check at bounds of 150 and 200 hops comes from [run 41239761408](https://github.com/KilroyHere/trellis/actions/runs/41239761408), which also repeated everything else unchanged.

**Method.**

- **The chains.** `msvc.py`, next to `s2.py`, takes its chains from `s2.chain_source()`, so they are the ones measured in §3.3. It uses §3.3's configurations except `path value`, which ADR-0014 rejects for deduced mode. Every compile is a full compile (`/c`), so the creation code is instantiated as in a real build.
- **The longest chain at cl's default limits:** doubling from 200 hops, up to 1,024, then bisection.
- **Levels per hop.** cl 19.44 has no switch for its recursion limit. cl 19.51 has a new one, `/templateDepth:N` (default 1,000; error C2999). Where it exists, the levels per hop come from §2's method: the smallest depth that compiles chains of 50 and 100 hops. Through a cl-to-clang test shim, the same code reproduces §3.3's clang-18 figures exactly: 4 levels and 251 hops for the memo stub, 8 and 125 for S1's stub.
- **Calibration.** Two programs, a class template and a variable template that nest N levels, measure cl's raw nesting limit: the counterpart of GCC's `-ftemplate-depth=900` and Clang's 1,024.
- **cl has no `__is_same` builtin** (error C2144 on both versions). `stub.hpp`'s `on_path` uses one at namespace scope, so `stub.hpp` doesn't compile on MSVC at all, in any strategy; the probe's first run found this. The probe maps it to `std::is_same_v`, the fallback ADR-0014 names, through a forced include (`/FI`), so the spike files stay unchanged. Only the path strategy uses `on_path`, so the memo and depth numbers are unaffected. `__is_constructible` exists on both versions.
- **The record.** `results/msvc.json` holds these results per cl version, and every run of the probe is compared with it. A green run means MSVC behaves as recorded; a red one lists what changed.

**Results: NFR-09 holds on cl 19.51, with 245 hops, and fails on cl 19.44, with 161.**

| | cl 19.51 (VS 2026) | cl 19.44 (VS 2022 17.14) |
|---|---|---|
| Recursion limit | `/templateDepth`, default 1,000: the class-template calibration compiles 999 levels, the variable template 1,000 | Fixed, with no switch: 499 and 500 levels; C1202, "recursive type or function dependency context too complex" |

Each cell gives levels per hop / **the longest chain at the default limits**, in hops. The GCC and Clang columns are §3.3's.

| Configuration | gcc-13, gcc-14 | clang-18, clang-20 | cl 19.51 | cl 19.44 |
|---|---|---|---|---|
| S1's stub as written, by value | 6 / **148** | 8 / **125** | 10 / **98** | **60** |
| memo, by value | 3 / **298** | 4 / **251** | 4 / **245** | **161** |
| memo, `shared_ptr<I>` to singletons | 4 / **221** | 4 / **251** | 4 / **245** | **161** |
| depth, by value | 3 / **298** | 4 / **251** | 4 / **245** | **161** |
| memo, by value, check only | 2 / **445** | 4 / **251** | 4 / **245** | **161** |
| memo, `shared_ptr<I>`, check only | 2 / **445** | 4 / **251** | 4 / **245** | **161** |

- **cl 19.51 meets NFR-09** with 245 hops, between GCC's 221 and Clang's 251. The limit is C2999, "maximum template instantiation depth of 1000 exceeded".
  - The lean stub's hop costs 4 levels: the smallest `/templateDepth` is 217 at 50 hops and 417 at 100, in every lean configuration. That fit predicts the default limit exactly: (1,000 − 17) / 4 = 245.
  - S1's stub as written costs 10 levels per hop (516 and 1,016) and reaches only 98 hops, which confirms §4.3's call for the lean shape.
  - The switch raises the limit too: by the same fit, `/templateDepth:817` would reach 200 hops and `/templateDepth:2048` about 500. These were not measured.
- **cl 19.44 doesn't meet NFR-09.** Every lean configuration stops at 161 hops, with C1202, and the switch that would raise the limit doesn't exist.
  - The levels per hop can't be measured there. Its 500-level limit over 161 hops suggests about 3, so 19.44 counts these levels differently from 19.51.
  - Versions 19.38 to 19.43, the rest of NFR-13's MSVC range, weren't on the runner. They presumably behave like 19.44.
- **The creation code adds nothing on MSVC.** With or without it, by value or through a singleton's `shared_ptr`, every lean configuration stops at the same length, and the check alone sets the limit. This is consistent with cl instantiating function bodies at the end of the translation unit, rather than where they are first used, which is what made GCC's creation code the limit (§3.3).
- **The depth strategy** stops where memo does: 245 hops on 19.51 and 161 on 19.44. So the opt-in `cycle_check`'s bound of 200 (ADR-0014) is below 19.51's limit, but above 19.44's. The cycle cases confirm it (below): with a bound of 200, every cycle on 19.44 ends in C1202, without TRL-E007, while a bound of 150 gives TRL-E007 on both versions.

**Cycles.** Both versions give the same results, except with a bound of 200:

| Strategy | Errors | Lines (bytes) | First error |
|---|---|---|---|
| memo | 3–5 | 23–57 (2–5 KB) | `C7608: atomic constraint should be a constant expression`, at `can_take`. Then C2131, "expression did not evaluate to a constant", and the root's TRL-E002. |
| depth (bound 100) | **1** | 824 (62–63 KB) | `C2338: static assertion failed: 'TRL-E007: dependency chain longer than S2_MAX_DEPTH hops: a dependency cycle?'` |
| depth (bound 150) | **1** | 1,224 (93–94 KB) | the same TRL-E007 |
| depth (bound 200), cl 19.51 | **1** | 1,624 (123–125 KB) | the same TRL-E007 |
| depth (bound 200), cl 19.44 | 1 | 2 | `C1202: recursive type or function dependency context too complex`, with no ID |

- **Option 3's premise holds on MSVC.** No cycle compiles, none crashes cl, and none runs into the recursion limit: the memoized recursion stops at the re-entered node, as on GCC and Clang (§3.4). The E007 catalog's MSVC entry is C7608.
- **The opt-in check gives exactly one error, TRL-E007, as on GCC and Clang,** as long as its bound stays below cl's limit.
  - cl prints its whole instantiation context with that error: 309 note lines at a bound of 100, three per hop (the concept, the variable template and the member function being instantiated).
  - With their `with [...]` blocks that is 8 lines of output per hop: 824 lines (62–63 KB) at a bound of 100, against 11–48 lines and 1–5 KB on GCC and Clang.
- **Above cl's limit the check no longer reports.** At a bound of 200 on 19.44, whose limit is 161 hops, every case ends in the fatal C1202 instead. That is why ADR-0014's amendment bounds the check at 150 there, and 150 does give TRL-E007 on 19.44.

**What this changes.** NFR-09's "≥ 200 hops at the default limits" holds on the latest MSVC, so T1.13's question gets a yes. It can't hold on Visual Studio 2022, which NFR-13 supports (MSVC ≥ 19.38). The chain is limited by the check, which already has the lean shape, and 19.44 offers no switch. So either NFR-09 is qualified for older MSVC, or NFR-13's MSVC minimum moves up.

**The boundary at 19.50 is assumed, not measured.** 19.44 is the newest version measured without `/templateDepth`, and 19.51 the first measured with it. 19.50, Visual Studio 2026's first toolset, isn't on the runner image, which has 14.29, 14.44 and 14.51 (run 41239761408 listed them, and its 14.50 job found none). Until 19.50 is checked, 19.51 is the only minimum known to reach 200 hops.

ADR-0014 now carries a provisional amendment, awaiting the owner. NFR-09 becomes ≥ 200 hops on GCC, Clang and MSVC ≥ 19.50, and ≥ 150 hops on MSVC 19.38 to 19.4x. On MSVC before 19.50, the opt-in check's bound drops to 150, so it stays below the 161 measured there. Raising NFR-13's minimum is the recorded alternative.

**Not measured on MSVC:** compile time and memory against hand-wiring (§3.2), request forms other than by value and `shared_ptr`, the path strategy, and clang-cl. Versions 19.38 to 19.43 and 19.45 to 19.50 weren't on the runner either, so the boundary at 19.50 is assumed.
