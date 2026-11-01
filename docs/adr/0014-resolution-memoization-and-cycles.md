# ADR-0014: Resolution: memoization, depth per hop and cycles in deduced mode

- **Status:** Accepted (owner, 2026-10-31). The MSVC amendment to items 2 and 3, for MSVC before 19.50 (an assumed boundary), is provisional, awaiting owner review (T1.13, 2026-10-31).
- **Plan:** §7.3, §7.4 (E007), §8.1 C1, C5, C7; NFR-04, NFR-05, NFR-09; risks K1, K3. Plan [§4](../plan.md#4-architecture-decisions) has no ADR for this decision; this one is new.
- **Spikes:** [S2 memoization and cycles](../spikes/S2-memoization-cycles.md), whose recommendation the owner accepted on 2026-10-31, and its MSVC addendum (T1.13).
- **Amends:** plan §7.3 (the cycle option and the depth budget), §8.1 C7 and, provisionally, NFR-09.

## Context

Plan §7.3 memoizes resolution per (key, request form, consumer lifetime), and keeps the path and the depth out of the key on the normal pass, because a path in the type multiplies instantiations by the number of paths (qlibs/di, R9). It budgets at most 4 instantiation levels per hop, so that chains of at least 200 hops fit the default compiler limits (NFR-09, §8.1 C7).

It calls cycles in deduced mode "the main open design problem". In the explicit and reflection modes the graph is known, so a `consteval` depth-first search over dense type indices reports E007 with the full cycle. In deduced mode, parameter types are discovered only by instantiating conversion operators (ADR-0003), and §7.3 feared a cycle would hit the compiler's recursion limits first. It offered three options:

1. **Depth-bounded resolution:** a depth counter in the key, the default if it costs at most 1.2×.
2. **Opt-in checking:** option 1 behind `options{.cycle_check = true}`.
3. **No check on the normal pass:** rely on explicit or reflection mode in a test build (`di::testing::validate`), and document the compiler's messages.

Spike S2 measured three strategies on S1's injector stub, with identical creation code: memoization (§7.3's key), a depth counter in the key, and path tracking. It used T0.3's graph generator at N = 25 to 1,000 with fan-in 1–3 and 10, on gcc-13 and clang-18, checked every program with the equivalence oracle, swept `-ftemplate-depth` over chains of 50–500 hops on four compilers, and compiled four kinds of cycle (S2 §2).

## Decision

1. **Memoize per (key, request form, consumer lifetime), with no path or depth on the normal pass.** This confirms §7.3 and §8.1 C1 (S2 §4.1). It is the only strategy that met NFR-04 and NFR-05 at every size measured. Compile time / peak memory against hand-wiring, gcc-13 and clang-18, fan-in 1–3 and 10 (S2 §3.2):

   | Strategy | N = 25 | N = 100 | N = 250 | N = 1,000 (fan-in 1–3) |
   |---|---|---|---|---|
   | memoization | 0.89–1.03× / 0.94–1.04× | 0.85–1.04× / 0.92–1.06× | 0.76–1.10× / 0.91–1.09× | 0.62–0.72× / 0.88–1.10× |
   | depth counter | 0.94–1.25× / 0.99–1.21× | 1.20–2.41× / 1.22–3.59× | 1.64–3.49× / 1.97–5.53×; out of memory on clang-18 with fan-in 10 | not run |
   | path tracking | 4.88–13.67× / 5.92–13.25× with fan-in 1–3; out of memory with fan-in 10 | out of memory | not attempted | not attempted |

   Memoization's recursion depth is that of a depth-first search, not the longest path: 14 hops at N = 100 and 66 at N = 1,000 on the research graph (S2 §3.1).

2. **Cycles in deduced mode: option 3 by default, option 2 as an opt-in, and option 1 rejected** (S2 §4.2).
   - **Default: no cycle check on the normal pass.** Under memoization a cycle is never silently accepted, never crashes a compiler and never reaches a recursion limit: the recursion re-enters a node whose value is still being computed, and every compiler stops there with a hard error (S2 §3.4). The error catalog documents the signatures under E007 (deduced mode), and points to the explicit or reflection route (`di::testing::validate`) and to the opt-in check:
     - GCC 13 and 14: `satisfaction of atomic constraint '…' depends on itself`, or `the value of '…' is not usable in a constant expression` for a class that depends on itself;
     - Clang 18 and 20: `substitution into constraint expression resulted in a non-constant expression`, or `satisfaction of constraint '…' depends on itself` for a cycle below the root;
     - then the root's own TRL-E002. Across S1's stub and the memoized stub, that is 2–6 errors, 20–217 lines and 3–24 KB, and the classes on the cycle appear only in the instantiation notes.
   - **Opt-in: `options{.cycle_check = true}`** switches deduced mode to the depth-bounded check, for a debugging or CI build. It reports exactly one error, TRL-E007, on all four compilers, in 11–48 lines and 1–5 KB. MSVC also reports exactly one, cl 19.51 at every bound measured and cl 19.44 at bounds up to 150 (see below), but prints its whole instantiation context, 8 lines per hop: 824 lines and 62–63 KB at a bound of 100 hops, 1,224 at 150, and on cl 19.51 1,624 at 200 (T1.13). It needs:
     - **a relaxed second pass on failure,** which treats the failing nodes as resolvable without an error. If that pass succeeds, the cycle was the only cause, and `create()` adds nothing. Without it, Clang adds the root's E002, because it marks a class invalid after a failed `static_assert`;
     - **a bound of 200 hops,** NFR-09's own figure, which is below every limit a valid chain hits: Clang's check stops at 251 hops, GCC's creation code at 221 and MSVC 19.51's check at 245, so a chain that passes the check also compiles. The message says "a cycle, or a chain longer than 200 hops".
     - **On MSVC before 19.50, an assumed boundary (item 3), a bound of 150 hops** (provisional, T1.13). The check stops there at 161 hops (cl 19.44). The four cycle cases confirm it on cl 19.44: a bound of 200 ends every case in the fatal C1202, without TRL-E007, while a bound of 150 gives exactly one TRL-E007. On cl 19.51 both bounds give exactly one TRL-E007. The bound is NFR-09's figure for that compiler (item 3).

     It costs 1.2–2.4× the compile time at N = 100, and 1.6–3.5× or out of memory at N = 250, only when enabled. It is a tool for finding a cycle, not a mode to build a large graph in.
   - **Path tracking only where the graph is known.** Naming the cycle needs the path, and a path-tracked check is exponential on graphs that share dependencies: 4.9–13.7× the compile time at N = 25, and out of memory under 4 GB from N = 20–50. The explicit and reflection modes keep §7.3's `consteval` depth-first search over dense indices, which finds the cycle without instantiating a path per node.

3. **NFR-09 stays at ≥ 200 hops at the default limits, and §8.1 C7's budget covers the creation code as well as the check** (S2 §3.3, §4.3). On MSVC before 19.50 the figure is ≥ 150 hops (provisional, T1.13; the boundary is assumed, as the MSVC bullet below explains). Levels per hop / the longest chain that compiles at the default limits:

   | Configuration | gcc-13, gcc-14 | clang-18, clang-20 | cl 19.51 (VS 2026) | cl 19.44 (VS 2022 17.14) |
   |---|---|---|---|---|
   | S1's stub as written, by value | 6 / 148 hops | 8 / 125 hops | 10 / 98 hops | 60 hops |
   | lean memoized stub, by value | 3 / 298 | 4 / 251 | 4 / 245 | 161 |
   | lean memoized stub, `shared_ptr<I>` to singletons | 4 / 221 | 4 / 251 | 4 / 245 | 161 |
   | lean memoized stub, check only | 2 / 445 | 4 / 251 | 4 / 245 | 161 |

   - The check costs 2 levels per hop on GCC and 4 on Clang and on MSVC 19.51. The creation code nests too on GCC and Clang, because a function template's body is instantiated where it is first used: 3 levels per hop by value and 4 through a singleton. Whichever is larger sets the limit. On MSVC the creation code adds nothing measurable: every configuration stops at the same length, consistent with cl instantiating function bodies at the end of the translation unit.
   - **Phase 1 keeps the lean per-hop shape:** the node is a variable template partial specialization initialized from `__is_constructible`; the per-node checks are separate atoms of one constraint, so `&&` and `||` short-circuit; no `constexpr` function call stays open across a hop; and the creation code nests at most 4 levels per hop.
   - **MSVC** (S2's MSVC addendum, T1.13). The amendment for MSVC before 19.50 is provisional, awaiting the owner.
     - cl 19.51 (Visual Studio 2026) meets NFR-09: 245 hops at its default `/templateDepth` of 1,000, a switch that raises the limit as `-ftemplate-depth` does.
     - cl 19.44 (Visual Studio 2022 17.14) has a fixed recursion limit of about 500 levels and no switch, and reaches 161 hops in every configuration. Versions 19.38 to 19.43 weren't available, and presumably behave the same.
     - The check sets that limit, and it already has the lean shape. So NFR-09 is ≥ 200 hops on GCC, Clang and MSVC ≥ 19.50, and ≥ 150 hops on MSVC 19.38 to 19.4x, the rest of NFR-13's range. 150 leaves a margin below the 161 measured.
     - **The boundary at 19.50 is assumed, not measured.** 19.44 is the newest version measured without `/templateDepth`, and 19.51 the first measured with it. 19.50 is Visual Studio 2026's first toolset, and the runner image doesn't have it: it has 14.29, 14.44 and 14.51 (T1.13's third run).
     - The alternative is raising NFR-13's MSVC minimum to 19.50, or to 19.51 until 19.50 is checked (Alternatives).
   - **CI gets a test:** a 200-hop chain in each request form, at the default limits, on every compiler; 150 hops on MSVC before 19.50, where CI tests one. CI's `windows-latest` runs cl 19.51.
   - **In deduced mode the real size limit is chain depth, not N.** The generated graphs pass at N = 1,000 because their depth-first stack is 35–66 hops, but a graph whose classes list their longest-chain dependency first would need 900–1,000. The documentation of deduced mode says so.

4. **Smaller rules for Phase 1** (S2 §4.4):
   - failure passes (the relaxed pass, and ADR-0006's explain pass) run only on failure, in the same `if constexpr` chain in `create()`;
   - the hop uses the builtins `__is_constructible`, and `__is_same` for path tracking, where the `std` traits would instantiate class templates. MSVC has `__is_constructible` but not `__is_same` (T1.13), so `__is_same` gets a `config.hpp` fallback, `std::is_same_v`, there; S2's stub doesn't compile on MSVC without it;
   - the injector names a binding map rather than taking the binding list (ADR-0010, open point);
   - no single fold over more than 256 operands, which is Clang's limit (ADR-0010; inferred for binding lists, untested).

## Alternatives

- **Option 1, a depth counter by default:** fails §7.3's own condition of at most 1.2× (item 1). Its node count is about N²/2: 4,278 nodes against memoization's 100 at N = 100, and 480,163 against 1,000 at N = 1,000 (S2 §3.1). It also limits graph shape: the check recurses along the longest path, so a graph whose longest path exceeds about 250 hops on Clang (445 on GCC) doesn't compile at the default limits, whatever its size, and a bound below the compiler's limit rejects such a graph with a false E007. NFR-04's 1,000-component graphs have longest paths of 900–1,000 hops. Its message can't name the cycle.
- **Path tracking by default:** names the cycle, but runs out of memory from N = 20–50.
- **Option 3 alone, without the opt-in:** a cycle in deduced mode would get only the compiler's messages until the user moves the graph to the explicit or reflection route.
- **Warming the memo in binding order,** checking every binding's node in declaration order before the root: tried by hand, and it didn't help. The creation code still nests 3–4 levels per hop, and Clang refuses a fold over more than 256 bindings. Lifting NFR-09's limit would need the creation code instantiated in binding order as well, a possible Phase 1 experiment (S2 §5).
- **Raising NFR-13's MSVC minimum to 19.50 (Visual Studio 2026), instead of qualifying NFR-09 for MSVC before 19.50** (T1.13). This keeps one figure, 200 hops, for every compiler, but drops Visual Studio 2022, which reached 161 hops (cl 19.44). It depends on the assumed boundary: 19.51 is the first version measured with `/templateDepth`, so 19.51 is the safe minimum until 19.50 is checked. The provisional amendment keeps Visual Studio 2022 supported, with a lower, documented chain limit. The owner chooses.

## Consequences

- **ADR-0006's "one error per root cause" has an exception** under the default: a cycle in deduced mode gives the compiler's 2–6 errors, without the ID on the first line. With `cycle_check` on, or in the explicit and reflection modes, it is one TRL-E007.
- **Deduced mode is size-limited by chain depth** (item 3), which its documentation must state.
- **MSVC** (S2 §5 rated the risk high for NFR-09 and medium for cycles; T1.13 measured it, in S2's MSVC addendum):
  - **NFR-09:** met on cl 19.51 (245 hops), not on cl 19.44 (161 hops), as item 3 records.
  - **Cycles:** under memoization a cycle fails promptly on both versions, never silently and never with a crash. The first error is C7608, "atomic constraint should be a constant expression", then C2131 and the root's TRL-E002: 3–5 errors, 23–57 lines. That is the catalog's MSVC entry.
  - **The opt-in check:** exactly one TRL-E007, but 824 lines (item 2).
  - **Builtins:** `__is_same` is missing (item 4).
- **Not measured** (S2 §5): request forms other than `shared_ptr` were checked for correctness but not timed; graphs where one class has consumers of different lifetimes; aggregates, scoped lifetimes, factories and `lazy`; Apple Clang; option 2 at N = 1,000, which can't compile there at the default limits; and fan-in 10 at N of 500 or more.
- **Work this creates:** Phase 2 implements `cycle_check` and the error catalog's E007 page (plan §12.2: "E007 cycles (per S2)"). Task T1.11 carries the 200-hop chain test and the MSVC chain test (150 hops on MSVC before 19.50, if CI tests one; `platforms.yml` runs the image's latest toolset, cl 19.51).

## History

- **2026-10-30, plan draft 1:** §7.3 posed the three options and named spike S2 to choose between them.
- **2026-10-30, spike S2 (T0.7):** measured the strategies and recommended this decision (S2 §4), which then waited for the owner (docs/PROGRESS.md, "Waiting for owner").
- **2026-10-31, T0.8:** recorded as this new ADR rather than inside ADR-0003, because it concerns resolution rather than discovery, and its status then differed from ADR-0003's. Plan §7.3 and §8.1 C7 amended, each marked provisional.
- **2026-10-31, owner:** accepted S2's recommendation: no cycle check on the normal pass, plus the opt-in, depth-bounded `cycle_check`.
- **2026-10-31, T0.8 review:** status set to Accepted (owner, 2026-10-31). The plan markers in §7.3 and §8.1 C7 now carry that date and no longer say provisional.
- **2026-10-31, T1.13 (MSVC check):** S2's chain probe and cycle cases ran on cl 19.51 (Visual Studio 2026) and cl 19.44 (Visual Studio 2022 17.14) in GitHub Actions (S2's MSVC addendum).
  - cl 19.51 meets NFR-09 with 245 hops. cl 19.44, whose recursion limit is fixed, reaches 161.
  - Items 2 and 3 are therefore amended for MSVC before 19.50: NFR-09 there is ≥ 150 hops, and so is the opt-in check's bound. This is provisional, awaiting the owner, who may choose instead to raise NFR-13's MSVC minimum to 19.50 (Alternatives). Plan NFR-09 and §7.3 are marked provisional.
  - Item 4 corrected: MSVC has `__is_constructible`; only `__is_same` needs the fallback.
  - The cycle results and the opt-in check's output size on MSVC added.
  - After review, a third run tested the cycle check at bounds of 150 and 200 hops. On cl 19.44, 150 gives TRL-E007 and 200 ends in C1202; on cl 19.51 both give TRL-E007. The boundary at 19.50 is marked as assumed: the runner has no 14.50 toolset, so only 19.44 and 19.51 were measured.
- **2026-11-01, T1.5 (the engine):** `create<T>()` keeps item 1's key and item 3's lean shape ([docs/internals/resolution-and-storage.md](../internals/resolution-and-storage.md)). The check is a variable template per (injector, consumer lifetime, request) whose value is the request's node, evaluated with the `__is_constructible` builtin (item 4); everything else about a request is worked out before the recursion. The creation code nests three function bodies per hop, also through a singleton, whose slot gets a function pointer rather than the creation code. Measured on chains through `create<C0>()`: 3N + 13 levels on gcc-13 and 4N + 15 on clang-18 for N hops, the same by value, through `std::unique_ptr`, and through singletons by reference or `std::shared_ptr`; 2N + 14 to 2N + 23 for declared constructors. The deepest chain at the default limits is 295 hops on gcc-13 and gcc-14 and 252 on clang-18 and clang-20, so NFR-09 holds on GCC and Clang. With `std::is_constructible_v` instead of the builtin, the check costs a level more per hop (about 222 and 202 hops). MSVC gets the builtin through `config.hpp`, untested; T1.11 tests the chain, on MSVC too.
