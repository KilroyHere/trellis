# ADR-0014: Resolution: memoization, depth per hop and cycles in deduced mode

- **Status:** Accepted (owner, 2026-10-31)
- **Plan:** §7.3, §7.4 (E007), §8.1 C1, C5, C7; NFR-04, NFR-05, NFR-09; risks K1, K3. Plan [§4](../plan.md#4-architecture-decisions) has no ADR for this decision; this one is new.
- **Spikes:** [S2 memoization and cycles](../spikes/S2-memoization-cycles.md), whose recommendation the owner accepted on 2026-10-31.
- **Amends:** plan §7.3 (the cycle option and the depth budget) and §8.1 C7.

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
   - **Opt-in: `options{.cycle_check = true}`** switches deduced mode to the depth-bounded check, for a debugging or CI build. It reports exactly one error, TRL-E007, on all four compilers, in 11–48 lines and 1–5 KB. It needs:
     - **a relaxed second pass on failure,** which treats the failing nodes as resolvable without an error. If that pass succeeds, the cycle was the only cause, and `create()` adds nothing. Without it, Clang adds the root's E002, because it marks a class invalid after a failed `static_assert`;
     - **a bound of 200 hops,** NFR-09's own figure, which is below every limit a valid chain hits: Clang's check stops at 251 hops and GCC's creation code at 221, so a chain that passes the check also compiles. The message says "a cycle, or a chain longer than 200 hops".

     It costs 1.2–2.4× the compile time at N = 100, and 1.6–3.5× or out of memory at N = 250, only when enabled. It is a tool for finding a cycle, not a mode to build a large graph in.
   - **Path tracking only where the graph is known.** Naming the cycle needs the path, and a path-tracked check is exponential on graphs that share dependencies: 4.9–13.7× the compile time at N = 25, and out of memory under 4 GB from N = 20–50. The explicit and reflection modes keep §7.3's `consteval` depth-first search over dense indices, which finds the cycle without instantiating a path per node.

3. **NFR-09 stays at ≥ 200 hops at the default limits, and §8.1 C7's budget covers the creation code as well as the check** (S2 §3.3, §4.3). Levels per hop / the longest chain that compiles at the default limits:

   | Configuration | gcc-13, gcc-14 | clang-18, clang-20 |
   |---|---|---|
   | S1's stub as written, by value | 6 / 148 hops | 8 / 125 hops |
   | lean memoized stub, by value | 3 / 298 | 4 / 251 |
   | lean memoized stub, `shared_ptr<I>` to singletons | 4 / 221 | 4 / 251 |
   | lean memoized stub, check only | 2 / 445 | 4 / 251 |

   - The check costs 2 levels per hop on GCC and 4 on Clang. The creation code nests too, because a function template's body is instantiated where it is first used: 3 levels per hop by value and 4 through a singleton. Whichever is larger sets the limit.
   - **Phase 1 keeps the lean per-hop shape:** the node is a variable template partial specialization initialized from `__is_constructible`; the per-node checks are separate atoms of one constraint, so `&&` and `||` short-circuit; no `constexpr` function call stays open across a hop; and the creation code nests at most 4 levels per hop.
   - **CI gets a test:** a 200-hop chain in each request form, at the default limits, on every compiler.
   - **In deduced mode the real size limit is chain depth, not N.** The generated graphs pass at N = 1,000 because their depth-first stack is 35–66 hops, but a graph whose classes list their longest-chain dependency first would need 900–1,000. The documentation of deduced mode says so.

4. **Smaller rules for Phase 1** (S2 §4.4):
   - failure passes (the relaxed pass, and ADR-0006's explain pass) run only on failure, in the same `if constexpr` chain in `create()`;
   - the hop uses the builtins `__is_constructible`, and `__is_same` for path tracking, where the `std` traits would instantiate class templates; both get `config.hpp` fallbacks for MSVC;
   - the injector names a binding map rather than taking the binding list (ADR-0010, open point);
   - no single fold over more than 256 operands, which is Clang's limit (ADR-0010; inferred for binding lists, untested).

## Alternatives

- **Option 1, a depth counter by default:** fails §7.3's own condition of at most 1.2× (item 1). Its node count is about N²/2: 4,278 nodes against memoization's 100 at N = 100, and 480,163 against 1,000 at N = 1,000 (S2 §3.1). It also limits graph shape: the check recurses along the longest path, so a graph whose longest path exceeds about 250 hops on Clang (445 on GCC) doesn't compile at the default limits, whatever its size, and a bound below the compiler's limit rejects such a graph with a false E007. NFR-04's 1,000-component graphs have longest paths of 900–1,000 hops. Its message can't name the cycle.
- **Path tracking by default:** names the cycle, but runs out of memory from N = 20–50.
- **Option 3 alone, without the opt-in:** a cycle in deduced mode would get only the compiler's messages until the user moves the graph to the explicit or reflection route.
- **Warming the memo in binding order,** checking every binding's node in declaration order before the root: tried by hand, and it didn't help. The creation code still nests 3–4 levels per hop, and Clang refuses a fold over more than 256 bindings. Lifting NFR-09's limit would need the creation code instantiated in binding order as well, a possible Phase 1 experiment (S2 §5).

## Consequences

- **ADR-0006's "one error per root cause" has an exception** under the default: a cycle in deduced mode gives the compiler's 2–6 errors, without the ID on the first line. With `cycle_check` on, or in the explicit and reflection modes, it is one TRL-E007.
- **Deduced mode is size-limited by chain depth** (item 3), which its documentation must state.
- **MSVC risk** (S2 §5, untested): high for NFR-09, medium for cycles. MSVC has no `-ftemplate-depth`; its template recursion limit is fixed (error C1202) and reported to be lower than GCC's and Clang's. A chain test on MSVC in CI should come first in Phase 1, together with S1's tests. How MSVC reports a re-entered constraint satisfaction is unknown, so the catalog needs an MSVC entry. `__is_same` may be missing there, and `std::is_same_v` is the fallback.
- **Not measured** (S2 §5): request forms other than `shared_ptr` were checked for correctness but not timed; graphs where one class has consumers of different lifetimes; aggregates, scoped lifetimes, factories and `lazy`; Apple Clang; option 2 at N = 1,000, which can't compile there at the default limits; and fan-in 10 at N of 500 or more.
- **Work this creates:** Phase 2 implements `cycle_check` and the error catalog's E007 page (plan §12.2: "E007 cycles (per S2)"). No task carries the 200-hop chain test or the MSVC chain test yet.

## History

- **2026-10-30, plan draft 1:** §7.3 posed the three options and named spike S2 to choose between them.
- **2026-10-30, spike S2 (T0.7):** measured the strategies and recommended this decision (S2 §4), which then waited for the owner (docs/PROGRESS.md, "Waiting for owner").
- **2026-10-31, T0.8:** recorded as this new ADR rather than inside ADR-0003, because it concerns resolution rather than discovery, and its status then differed from ADR-0003's. Plan §7.3 and §8.1 C7 amended, each marked provisional.
- **2026-10-31, owner:** accepted S2's recommendation: no cycle check on the normal pass, plus the opt-in, depth-bounded `cycle_check`.
- **2026-10-31, T0.8 review:** status set to Accepted (owner, 2026-10-31). The plan markers in §7.3 and §8.1 C7 now carry that date and no longer say provisional.
