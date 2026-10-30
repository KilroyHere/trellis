#!/usr/bin/env python3
"""Spike S2 (memoization and cycles): rebuild the numbers in docs/spikes/S2-memoization-cycles.md.

  s2.py [--quick] [--out DIR] [--only stats,cycles,depth,scaling]

Sections, each written to DIR (default spikes/S2-memoization-cycles/out/, git-ignored):
  stats    the generated graphs' shape: nodes, edges, (node, depth) pairs, root-to-node paths,
           the longest path and the memoized DFS's stack depth. These are exact counts of the
           nodes each strategy instantiates (stats.md).
  cycles   cycles.cpp under S1's stub and the three strategies: what each compiler reports
           (cycles.md, logs/cycles/).
  depth    chains of K hops: the smallest -ftemplate-depth that compiles each chain, so the
           instantiation levels per hop, and the deepest chain at the default limits (depth.md).
  scaling  generated graphs (benchmarks/compile/gen_graph.py) wired by hand and by each
           strategy, compiled and checked by benchmarks/compile/run.py's measure(): compile time
           and peak memory, as ratios against hand-wiring in the same run (scaling.md).

Every compile runs alone, never through ccache, in a new process group under a 600 s timeout
and a 3906 MiB (4,000,000 KiB) address-space cap; a compile that hits a cap is a result.
Exit status: 0 when every expectation in check() holds, 1 otherwise.
"""
import argparse
import json
import re
import statistics
import sys
import time
from pathlib import Path
from types import SimpleNamespace

sys.dont_write_bytecode = True  # keep the source tree free of __pycache__

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import variants  # noqa: E402

ROOT = variants.ROOT
bench = variants.load("s2_bench_run", variants.COMPILE_BENCH / "run.py")
gen_graph = variants.register(bench.gen_graph)

TIMEOUT_S = 600
MEMORY_MB = 3906  # 4,000,000 KiB, the coordinator's ulimit -v
S1_DIR = ROOT / "spikes" / "S1-deduced-discovery"
INCLUDES = [f"-I{HERE}", f"-I{S1_DIR}"]
DEFAULT_DEPTH = {"gcc": 900, "clang": 1024}  # -ftemplate-depth defaults
ALL_COMPILERS = ["gcc-13", "gcc-14", "clang-18", "clang-20"]
MAIN_COMPILERS = ["gcc-13", "clang-18"]


def family(compiler):
    return "gcc" if compiler.startswith("gcc") else "clang"


def compile_once(compiler_path, args, cwd):
    out = bench.run_command([compiler_path, *args], TIMEOUT_S, MEMORY_MB, cwd=cwd,
                            env=bench.child_environment())
    return out


def classify(out):
    """What a failed compile hit, from its output."""
    text = out.stderr + out.stdout
    if out.timed_out:
        return "timeout"
    if bench.MEMORY_ERROR.search(text):
        return "memory cap"
    if "TRL-E007" in text:
        return "TRL-E007"
    if re.search(r"template instantiation depth exceeds|recursive template instantiation exceeded", text):
        return "template depth"
    if re.search(r"constexpr evaluation depth exceeds|constexpr evaluation exceeded maximum depth", text):
        return "constexpr depth"
    if re.search(r"internal compiler error|Segmentation fault|PLEASE submit a bug report", text):
        return "compiler crash"
    if out.returncode and out.returncode < 0:
        return f"signal {-out.returncode}"
    return "other error"


def first_error(text):
    """The first error line, with paths inside the repository made relative."""
    for line in text.splitlines():
        if re.search(r"\berror\b", line) and "errors generated" not in line and "error generated" not in line:
            return line.strip().replace(f"{ROOT}/", "")
    return ""


# ---- stats -------------------------------------------------------------------------------

def graph_stats(n, fan_in):
    graph = gen_graph.make_graph(n, fan_in, "shared_ptr")
    comps = graph.components
    paths = [0] * n
    depths = [set() for _ in range(n)]
    for e in graph.root:
        paths[e.target] += 1
        depths[e.target].add(1)
    for k in range(n - 1, -1, -1):  # consumers have larger indices than their dependencies
        for e in comps[k].deps:
            paths[e.target] += paths[k]
            depths[e.target] |= {d + 1 for d in depths[k]}
    def dfs_depth(order):
        """The memoized check's recursion depth in hops: a depth-first search that visits each
        node once, taking the parameters in the given order (each node's first visit happens
        inside the first consumer that reaches it)."""
        visited, deepest = [False] * n, 0

        def visit(k, d):
            nonlocal deepest
            visited[k] = True
            deepest = max(deepest, d)
            for e in order(comps[k].deps):
                if not visited[e.target]:
                    visit(e.target, d + 1)

        for e in graph.root:
            if not visited[e.target]:
                visit(e.target, 1)
        return deepest

    limit = sys.getrecursionlimit()
    sys.setrecursionlimit(max(limit, 4 * n + 100))
    in_order = dfs_depth(lambda deps: deps)  # gen_graph sorts parameters by index: k-1 comes last
    reversed_order = dfs_depth(lambda deps: tuple(reversed(deps)))  # k-1 first
    sys.setrecursionlimit(limit)
    return {"n": n, "fan_in": fan_in, "edges": sum(len(c.deps) for c in comps) + len(graph.root),
            "memo_nodes": n, "depth_nodes": sum(len(s) for s in depths), "path_nodes": sum(paths),
            "longest_path": max(max(s) for s in depths if s), "memo_stack": in_order,
            "memo_stack_reversed": reversed_order}


def stats(sizes, out):
    rows = [graph_stats(n, f) for f in ("1-3", "10") for n in sizes]
    lines = ["Nodes each strategy instantiates, and recursion depths in hops. The memo check's stack is the "
             "depth of its depth-first search, with the parameters in the generated order (k-1 last) and "
             "reversed (k-1 first); the depth and path strategies recurse along every path, so they reach "
             "the longest path.", "",
             "| fan-in | N | edges | memo: nodes | depth: (node, depth) pairs | path: (node, path) pairs "
             "| longest path | memo stack | memo stack, reversed parameters |", "|---|---|---|---|---|---|---|---|---|"]
    for r in rows:
        lines.append(f"| {r['fan_in']} | {r['n']} | {r['edges']} | {r['memo_nodes']} | {r['depth_nodes']:,} | "
                     f"{r['path_nodes']:.3g} | {r['longest_path']} | {r['memo_stack']} | "
                     f"{r['memo_stack_reversed']} |")
    (out / "stats.md").write_text("\n".join(lines) + "\n")
    (out / "stats.json").write_text(json.dumps(rows, indent=1) + "\n")
    return rows


# ---- cycles ------------------------------------------------------------------------------

CYCLE_CASES = {1: "A -> B -> A", 2: "A -> B -> C -> D -> E -> A", 3: "A -> A (A takes A*)",
               4: "App -> X -> A -> B -> A"}
CYCLE_VARIANTS = {  # name -> defines
    "s1-stub": ["-DSTUB=1"],
    "memo": ["-DSTUB=2", "-DS2_STRATEGY=0"],
    "depth": ["-DSTUB=2", "-DS2_STRATEGY=1"],  # with S2_MAX_DEPTH from cycle_bound()
    "path": ["-DSTUB=2", "-DS2_STRATEGY=2"],
}
WARNINGS = ["-Wall", "-Wextra", "-Wpedantic", "-Wshadow", "-Wconversion"]  # NFR-10, no -Werror


def cycles(compilers, out, bound):
    logs = out / "logs" / "cycles"
    logs.mkdir(parents=True, exist_ok=True)
    rows = []
    for compiler in compilers:
        _, path = bench.resolve_compiler(compiler)
        for variant, defines in CYCLE_VARIANTS.items():
            for case in CYCLE_CASES:
                extra = [f"-DS2_MAX_DEPTH={bound}"] if variant == "depth" else []
                args = ["-std=c++20", "-fsyntax-only", *WARNINGS, *INCLUDES, *defines, *extra,
                        f"-DCASE={case}", "cycles.cpp"]
                o = compile_once(path, args, HERE)
                text = o.stderr + o.stdout
                (logs / f"{compiler}-{variant}-case{case}.log").write_text(text)
                errors = [ln for ln in text.splitlines() if re.search(r"(^|: )(fatal )?error: ", ln)]
                row = {"compiler": compiler, "variant": variant, "case": case, "exit": o.returncode,
                       "seconds": round(o.seconds, 2), "peak_mb": round(o.peak_rss_kb / 1024, 1),
                       "errors": len(errors), "lines": len(text.splitlines()), "bytes": len(text.encode()),
                       "first_error": first_error(text), "kind": classify(o) if o.returncode else "compiled",
                       "depends_on_itself": "depends on itself" in text,
                       "non_constant": bool(re.search(r"not usable in a constant expression|"
                                                      r"non-constant expression", text))}
                rows.append(row)
                print(f"cycles {compiler} {variant} case{case}: {row['kind']}, {row['errors']} error(s), "
                      f"{row['lines']} lines: {row['first_error'][:150]}", flush=True)
    (out / "cycles.json").write_text(json.dumps(rows, indent=1) + "\n")
    return rows


def short_error(line):
    """The message of an error line, without the file and position."""
    line = re.sub(r"^.*?(fatal )?error: ", "", line)
    line = re.sub(r"s2::|s1::|std::", "", line)
    return line[:140]


def cycles_md(rows, bound):
    lines = [f"Depth strategy built with S2_MAX_DEPTH={bound}. Errors counts lines with `error:`; "
             "lines and bytes are the whole output (-fsyntax-only, NFR-10 warnings, no -Werror).", "",
             "| compiler | variant | case | errors | lines | bytes | kind | first error |",
             "|---|---|---|---|---|---|---|---|"]
    for r in rows:
        lines.append(f"| {r['compiler']} | {r['variant']} | {r['case']}: {CYCLE_CASES[r['case']]} | {r['errors']} | "
                     f"{r['lines']} | {r['bytes']} | {r['kind']}{', depends on itself' if r['depends_on_itself'] else ''} "
                     f"| `{short_error(r['first_error'])}` |")
    return "\n".join(lines) + "\n"


# ---- depth per hop -----------------------------------------------------------------------

DEPTH_CONFIGS = {  # name -> (stub, strategy, form)
    "s1-stub value": ("s1", None, "value"),
    "memo value": ("s2", 0, "value"),
    "memo shared_ptr": ("s2", 0, "shared_ptr"),
    "depth value": ("s2", 1, "value"),
    "path value": ("s2", 2, "value"),
    "memo value, check only": ("check", 0, "value"),
    "memo shared_ptr, check only": ("check", 0, "shared_ptr"),
}


def chain_source(stub, strategy, form, hops):
    """A chain C<hops> -> ... -> C0 of `hops` dependency edges, created from C<hops>. Stub
    "check" only asserts that the root is resolvable: the check pass without the creation code."""
    lines = [f"// Generated by spikes/S2-memoization-cycles/s2.py: a chain of {hops} hops.", "#include <memory>"]
    if stub == "s1":
        lines.append('#include "injector_stub.hpp"')
    else:
        lines += [f"#define S2_STRATEGY {strategy}", "#define S2_MAX_DEPTH 1000000", '#include "stub.hpp"']
    if form == "value":
        lines.append("struct C0 { C0() {} };")
        lines += [f"struct C{i} {{ explicit C{i}(C{i - 1}) {{}} }};" for i in range(1, hops + 1)]
        if stub == "s1":
            lines.append("using injector_t = s1::injector<>;")
        else:
            lines += ["struct app : s2::map<> {};", "using injector_t = s2::injector<app>;"]
        root = f"C{hops}"
    else:
        lines += ["struct I0 { virtual ~I0() = default; };", "struct C0 final : I0 { C0() {} };"]
        for i in range(1, hops + 1):
            lines += [f"struct I{i} {{ virtual ~I{i}() = default; }};",
                      f"struct C{i} final : I{i} {{ explicit C{i}(std::shared_ptr<I{i - 1}>) {{}} }};"]
        bindings = ", ".join(f"s2::singleton<I{i}, C{i}>" for i in range(hops + 1))
        lines += [f"struct app : s2::map<{bindings}> {{}};", "using injector_t = s2::injector<app>;"]
        root = f"std::shared_ptr<I{hops}>"
    if stub == "check":
        lines += [f"static_assert(injector_t::resolvable<{root}>);", "int main() {}"]
    else:
        lines += ["int main() {", "  injector_t inj;", f"  {root} r = inj.create<{root}>();", "  (void)r;", "}"]
    return "\n".join(lines) + "\n"


MAX_CHAIN = 2048  # the longest chain the default-limit search tries


class ChainProbe:
    def __init__(self, compiler, path, config, work):
        self.compiler, self.path, self.work = compiler, path, work
        self.stub, self.strategy, self.form = DEPTH_CONFIGS[config]
        self.name = config.replace(" ", "-")
        self.cache = {}

    def compiles(self, hops, template_depth=None):
        key = (hops, template_depth)
        if key not in self.cache:
            src = self.work / f"chain-{self.name}-{hops}.cpp"
            if not src.exists():
                src.write_text(chain_source(self.stub, self.strategy, self.form, hops))
            args = ["-std=c++20", "-fsyntax-only", *INCLUDES]
            if template_depth is not None:
                args.append(f"-ftemplate-depth={template_depth}")
            o = compile_once(self.path, [*args, src.name], self.work)
            self.cache[key] = (o.ok, "" if o.ok else classify(o), first_error(o.stderr + o.stdout),
                               round(o.seconds, 2))
        return self.cache[key]

    def min_depth(self, hops, lo=None, hi=None):
        """The smallest -ftemplate-depth that compiles the chain, or None (and why) if even hi
        fails. The default bracket assumes 1 to 16 levels per hop."""
        lo = lo or hops
        hi = hi or 16 * hops + 128
        ok, why, msg, _ = self.compiles(hops, hi)
        if not ok:
            return None, f"{why} at -ftemplate-depth={hi}: {msg[:160]}"
        if self.compiles(hops, lo)[0]:  # the answer is at most lo
            hi, lo = lo, 1
        else:
            lo += 1
        while lo < hi:  # invariant: hi compiles, and the answer is in [lo, hi]
            mid = (lo + hi) // 2
            if self.compiles(hops, mid)[0]:
                hi = mid
            else:
                lo = mid + 1
        return hi, ""

    def max_default(self, guess=None, lo=1, hi=MAX_CHAIN):
        """The longest chain that compiles at the default limits: the guess if it compiles and
        guess + 1 doesn't, else a bisection over the hops."""
        if guess and self.compiles(guess)[0] and not self.compiles(guess + 1)[0]:
            return guess
        if not self.compiles(lo)[0]:
            return 0
        if self.compiles(hi)[0]:
            return hi
        while hi - lo > 1:  # invariant: lo compiles, hi doesn't
            mid = (lo + hi) // 2
            if self.compiles(mid)[0]:
                lo = mid
            else:
                hi = mid
        return lo


QUICK_DEPTH_CONFIGS = ("s1-stub value", "memo value", "memo shared_ptr", "memo shared_ptr, check only")


def depth(compilers, hop_list, out, configs=tuple(DEPTH_CONFIGS)):
    rows = []
    for compiler in compilers:
        _, path = bench.resolve_compiler(compiler)
        work = out / "work" / "depth" / compiler
        work.mkdir(parents=True, exist_ok=True)
        for config in configs:
            probe = ChainProbe(compiler, path, config, work)
            points, notes = {}, []
            for hops in hop_list:
                if len(points) >= 2:  # predict from the first two points, then confirm
                    (k1, d1), (k2, d2) = list(points.items())[:2]
                    slope = (d2 - d1) / (k2 - k1)
                    guess = round(d1 + slope * (hops - k1))
                    if probe.compiles(hops, guess)[0] and not probe.compiles(hops, guess - 1)[0]:
                        points[hops] = guess
                        continue
                if len(points) == 1:  # the intercept is small: bracket the proportional guess
                    (k1, d1), = points.items()
                    d, why = probe.min_depth(hops, lo=max(1, d1 * hops // k1 - 160), hi=d1 * hops // k1 + 64)
                    if d is None:
                        d, why = probe.min_depth(hops)
                else:
                    d, why = probe.min_depth(hops)
                if d is None:
                    notes.append(f"{hops} hops: {why}")
                    break
                points[hops] = d
            fit = None
            if len(points) >= 2:
                ks, ds = list(points), list(points.values())
                slope = (ds[-1] - ds[0]) / (ks[-1] - ks[0])
                fit = {"levels_per_hop": round(slope, 3), "intercept": round(ds[0] - slope * ks[0], 1),
                       "linear": all(abs(d - (ds[0] + slope * (k - ks[0]))) <= 1 for k, d in points.items())}
            guess = None
            if fit and fit["levels_per_hop"] > 0.5:  # the template-depth limit, if that is what binds
                guess = int((DEFAULT_DEPTH[family(compiler)] - fit["intercept"]) // fit["levels_per_hop"])
            longest = probe.max_default(guess)
            why, msg = "", ""
            if longest < MAX_CHAIN:
                _, why, msg, _ = probe.compiles(longest + 1)
            row = {"compiler": compiler, "config": config, "min_template_depth": points, "fit": fit,
                   "notes": notes, "max_hops_default": longest, "beyond_default": why,
                   "beyond_default_error": msg, "default_template_depth": DEFAULT_DEPTH[family(compiler)],
                   "compiles": len(probe.cache)}
            rows.append(row)
            print(f"depth {compiler} {config}: min depth {points}, fit {fit}, longest chain at default limits "
                  f"{longest} hops ({why} at {longest + 1}){'; ' + '; '.join(notes) if notes else ''}", flush=True)
            (out / "depth.json").write_text(json.dumps(rows, indent=1) + "\n")
    return rows


def depth_md(rows):
    hop_list = sorted({int(k) for r in rows for k in r["min_template_depth"]})
    head = "| compiler | config | " + " | ".join(f"{k} hops" for k in hop_list) + \
           " | levels/hop | longest chain at default limits | what stops it |"
    lines = ["Smallest -ftemplate-depth that compiles a chain of K hops; the slope is the instantiation "
             "levels per hop. The longest chain is at the compiler's default limits "
             "(-ftemplate-depth=900 on GCC, 1024 on Clang).", "", head,
             "|---|---|" + "---|" * len(hop_list) + "---|---|---|"]
    for r in rows:
        cells = [str(r["min_template_depth"].get(k, r["min_template_depth"].get(str(k), "-"))) for k in hop_list]
        fit = r["fit"]
        slope = f"{fit['levels_per_hop']:g}" + ("" if fit["linear"] else " (not linear)") if fit else "-"
        longest = r["max_hops_default"]
        shown = f"≥ {longest} (the longest tried)" if longest >= MAX_CHAIN else str(longest)
        lines.append(f"| {r['compiler']} | {r['config']} | " + " | ".join(cells) +
                     f" | {slope} | {shown} | {r['beyond_default'] or '-'} |")
    notes = [f"- {r['compiler']} {r['config']}: {'; '.join(r['notes'])}" for r in rows if r["notes"]]
    if notes:
        lines += ["", "Not measured:"] + notes
    return "\n".join(lines) + "\n"


# ---- scaling ------------------------------------------------------------------------------

def scaling(compilers, plan, out, runs):
    """plan: list of (fan_in, n, variant, extra_flags, label)."""
    work = out / "work" / "scaling"
    (work / "src").mkdir(parents=True, exist_ok=True)
    args = SimpleNamespace(runs=runs, timeout=TIMEOUT_S, memory_mb=MEMORY_MB)
    path_file = out / "scaling.json"
    records = json.loads(path_file.read_text()) if path_file.exists() else []
    done = {(r["compiler"], r["fan_in"], r["n"], r["label"]) for r in records}
    resolved = [bench.resolve_compiler(c) for c in compilers]
    warm = set()
    # (compiler, fan_in, label) that already hit a cap at a smaller N, in this run or the one it resumes
    capped = {(r["compiler"], r["fan_in"], r["label"]) for r in records
              if r.get("kind") in ("timeout", "memory cap")}
    # Each program is compiled by every compiler before the next one, so that an interrupted
    # run has comparable data for both.
    for fan_in, n, variant, extra, label in plan:
        graph = gen_graph.make_graph(n, fan_in, "shared_ptr")
        source = work / "src" / f"graph-{variant}-{graph.name}{'-' + str(len(extra)) if extra else ''}.cpp"
        source.write_text(gen_graph.program([graph], variant))
        for name, path in resolved:
            cwork = work / name
            cwork.mkdir(parents=True, exist_ok=True)
            if (name, fan_in, n, label) in done:
                continue
            record = {"program": "graph", "variant": variant, "label": label, "n": n, "fan_in": fan_in,
                      "forms": "shared_ptr", "compiler": name, "flags": extra}
            if (name, fan_in, label) in capped:
                record["error"] = "not attempted: a smaller N already hit a cap"
                records.append(record)
                continue
            if name not in warm:  # one untimed compile loads the compiler and headers into the page cache
                bench.run_command([path, *bench.GRAPH_FLAGS, "-c", source, "-o", cwork / "warm-up.o"],
                                  TIMEOUT_S, MEMORY_MB, cwd=cwork, env=bench.child_environment())
                warm.add(name)
            started = time.time()
            bench.measure(record, path, source, [*bench.GRAPH_FLAGS, *extra],
                          gen_graph.variant_of(variant).include_dirs, gen_graph.identity(graph), cwork, args,
                          every_run=False)
            record["wall_s_total"] = round(time.time() - started, 1)
            if "error" in record and "compile:" in record["error"]:
                log = Path(ROOT / record["error"].split("(log ")[-1].rstrip(")")) if "(log " in record["error"] else None
                text = log.read_text() if log and log.exists() else ""
                record["kind"] = ("timeout" if "timed out" in record["error"] else
                                  "memory cap" if "out of memory" in record["error"] or bench.MEMORY_ERROR.search(text)
                                  else classify(SimpleNamespace(stderr=text, stdout="", timed_out=False, returncode=1)))
                if record["kind"] in ("timeout", "memory cap"):
                    capped.add((name, fan_in, label))
            records.append(record)
            path_file.write_text(json.dumps(records, indent=1) + "\n")
            if bench.measured(record):
                print(f"scaling {name} fan-in {fan_in} N={n} {label}: {record['compile_s']['median']:.2f} s, "
                      f"{record['peak_rss_mb']:.0f} MB, output ok", flush=True)
            else:
                print(f"scaling {name} fan-in {fan_in} N={n} {label}: FAILED ({record.get('kind', '?')}): "
                      f"{record['error'][:200]}", flush=True)
    path_file.write_text(json.dumps(records, indent=1) + "\n")
    return records


def scaling_md(records):
    labels = []
    for r in records:
        if r["label"] not in labels:
            labels.append(r["label"])
    base = {(r["compiler"], r["fan_in"], r["n"]): r for r in records if r["label"] == "manual" and bench.measured(r)}
    head = "| compiler | fan-in | N | " + " | ".join(labels) + " |"
    lines = ["Median compile time (s) and peak memory (MB) of `-std=c++20 -O2 -c`, and in brackets the ratio "
             "to hand-wiring (manual) in the same run: time×, memory×. Every compiled program was linked, run and "
             "printed the equivalence oracle's line.", "", head, "|---|---|---|" + "---|" * len(labels)]
    keys = sorted({(r["compiler"], r["fan_in"], r["n"]) for r in records},
                  key=lambda k: (k[0], k[1], k[2]))
    for key in keys:
        cells = []
        for label in labels:
            r = next((x for x in records if (x["compiler"], x["fan_in"], x["n"]) == key and x["label"] == label), None)
            if r is None:
                cells.append("")
            elif bench.measured(r):
                c = f"{r['compile_s']['median']:.2f} s, {r['peak_rss_mb']:.0f} MB"
                b = base.get(key)
                if b is not None and label != "manual":
                    c += (f" [{r['compile_s']['median'] / b['compile_s']['median']:.2f}×, "
                          f"{r['peak_rss_mb'] / b['peak_rss_mb']:.2f}×]")
                if r["compile_s"]["runs"] > 1:
                    c += f" ({r['compile_s']['runs']} runs)"
                cells.append(c)
            else:
                kind = r.get("kind") or ("not attempted" if "not attempted" in r.get("error", "") else "failed")
                after = f" after {r['failed_after_s']:.0f} s" if r.get("failed_after_s") else ""
                at = f", {r['peak_rss_mb_at_failure']:.0f} MB" if r.get("peak_rss_mb_at_failure") else ""
                cells.append(f"**{kind}**{after}{at}")
        lines.append(f"| {key[0]} | {key[1]} | {key[2]} | " + " | ".join(cells) + " |")
    return "\n".join(lines) + "\n"


def scaling_plan(quick):
    plan = []
    if quick:
        for n in (25, 50):
            plan += [("1-3", n, "manual", [], "manual"), ("1-3", n, "s2-memo", [], "memo"),
                     ("1-3", n, "s2-depth", [], "depth")]
        plan += [("1-3", n, "s2-path", [], "path") for n in (10, 15, 20)]
        return plan
    for fan_in in ("1-3", "10"):
        for n in (25, 50, 100, 250):
            plan += [(fan_in, n, "manual", [], "manual"), (fan_in, n, "s2-memo", [], "memo"),
                     (fan_in, n, "s2-depth", [], "depth")]
        plan += [(fan_in, n, "s2-path", [], "path") for n in (10, 15, 20, 25, 50, 100)]
    return plan


def scaling_plan_large():
    """N = 500 and 1000 for the leading strategy (memo) and its baseline, fan-in 1-3 (NFR-04's
    research setting). Fan-in 10 stops at 250: its hand-wired N = 1000 alone takes over 300 s per
    compile with gcc-13 (run.py)."""
    plan = []
    for n in (500, 1000):
        plan += [("1-3", n, "manual", [], "manual"), ("1-3", n, "s2-memo", [], "memo")]
    return plan


# ---- checks -------------------------------------------------------------------------------

NFR09_HOPS = {}  # config -> the fewest hops the write-up claims, where that is below 200

def check(results):
    """The claims the write-up rests on. Returns a list of failures."""
    fails = []
    for r in results.get("cycles", []):
        tag = f"cycles {r['compiler']} {r['variant']} case{r['case']}"
        if r["exit"] == 0:
            fails.append(f"{tag}: a cycle compiled")
        elif r["variant"] in ("depth", "path"):
            if "TRL-E007" not in r["first_error"] or r["errors"] != 1:
                fails.append(f"{tag}: expected exactly one error, TRL-E007; got {r['errors']}: {r['first_error'][:120]}")
        elif r["variant"] == "memo" and r["case"] != 3 and "TRL-E007" in r["first_error"]:
            fails.append(f"{tag}: memo has no cycle check, but reported TRL-E007")
    for r in results.get("depth", []):
        tag = f"depth {r['compiler']} {r['config']}"
        if r["fit"] is None:
            fails.append(f"{tag}: fewer than two chain lengths measured ({'; '.join(r['notes'])})")
        elif not r["fit"]["linear"]:
            fails.append(f"{tag}: the depth needed is not linear in the hops: {r['min_template_depth']}")
        # NFR-09: S1's stub as written misses 200 hops; the lean memo check meets it.
        if r["config"] == "s1-stub value" and r["max_hops_default"] >= 200:
            fails.append(f"{tag}: S1's stub now reaches {r['max_hops_default']} hops (the write-up says < 200)")
        if r["config"].startswith("memo") and r["max_hops_default"] < NFR09_HOPS.get(r["config"], 200):
            fails.append(f"{tag}: only {r['max_hops_default']} hops at the default limits (NFR-09: 200)")
    for r in results.get("scaling", []):
        tag = f"scaling {r['compiler']} fan-in {r['fan_in']} N={r['n']} {r['label']}"
        if r["label"] in ("manual", "memo") and not bench.measured(r):
            hit_cap = r.get("kind") in ("timeout", "memory cap")
            if not (hit_cap and r["n"] >= 500):  # N = 500 and 1000 are measured within the caps only
                fails.append(f"{tag}: {r.get('error', 'not measured')[:160]}")
        if "error" in r and "output:" in r["error"]:
            fails.append(f"{tag}: wrong wiring: {r['error'][:160]}")
    return fails


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--quick", action="store_true")
    parser.add_argument("--out", type=Path, default=HERE / "out")
    parser.add_argument("--only", default="stats,cycles,depth,scaling")
    parser.add_argument("--large", action="store_true", help="also N = 500 and 1000 (memo) and depth at N=250 "
                                                             "with a raised limit")
    parser.add_argument("--depth-bound", type=int, default=100,
                        help="S2_MAX_DEPTH for the depth strategy's cycle cases (default 100)")
    args = parser.parse_args(argv)
    bench.exit_on_termination_signals()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    sections = set(args.only.split(","))
    compilers = MAIN_COMPILERS if args.quick else ALL_COMPILERS
    results = {}
    env = bench.environment([bench.resolve_compiler(c) for c in ALL_COMPILERS])
    (out / "env.json").write_text(json.dumps(env, indent=1) + "\n")
    started = time.time()
    if "stats" in sections:
        results["stats"] = stats((25, 50, 100, 250, 500, 1000), out)
    if "cycles" in sections:
        results["cycles"] = cycles(compilers, out, args.depth_bound)
        (out / "cycles.md").write_text(cycles_md(results["cycles"], args.depth_bound))
    runs = 1 if args.quick else 3
    bench.LONG_COMPILE_S = 10  # a program whose first compile takes over 10 s gets only that one
    if "scaling" in sections:
        results["scaling"] = scaling(MAIN_COMPILERS, scaling_plan(args.quick), out, runs)
        (out / "scaling.md").write_text(scaling_md(results["scaling"]))
    if "depth" in sections:
        hop_list = [50, 100] if args.quick else [50, 100, 250, 500]
        configs = QUICK_DEPTH_CONFIGS if args.quick else tuple(DEPTH_CONFIGS)
        results["depth"] = depth(compilers, hop_list, out, configs)
        (out / "depth.md").write_text(depth_md(results["depth"]))
    if "scaling" in sections and args.large:
        results["scaling"] = scaling(MAIN_COMPILERS, scaling_plan_large(), out, runs)
        (out / "scaling.md").write_text(scaling_md(results["scaling"]))
    summary = [f"# Spike S2 results ({'quick' if args.quick else 'full'} run)", "",
               f"Date {env['date_utc']}; {env['nproc']} vCPU ({env['cpu']}); load average at start "
               f"{', '.join(f'{x:.2f}' for x in env['loadavg_at_start'])}.", ""]
    summary += [f"- {name}: {info['version']}" for name, info in env["compilers"].items()]
    for name in ("stats", "cycles", "depth", "scaling"):  # this run's, or a resumed run's
        if (out / f"{name}.md").exists():
            summary += ["", f"## {name}", "", (out / f"{name}.md").read_text()]
    (out / "summary.md").write_text("\n".join(summary) + "\n")
    fails = check(results)
    for f in fails:
        print(f"FAIL: {f}")
    print(f"S2: {'all checks passed' if not fails else f'{len(fails)} check(s) failed'} "
          f"({time.time() - started:.0f} s); tables in {bench.relative(out / 'summary.md')}")
    return 1 if fails else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("interrupted", file=sys.stderr)
        sys.exit(130)
