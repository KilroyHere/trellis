#!/usr/bin/env python3
"""Spike S2 on MSVC (task T1.13): the chain-depth probe and the cycle cases, compiled with cl.
Run it from a Visual Studio developer prompt; .github/workflows/msvc-probe.yml does that on
GitHub's windows-latest runner.

  python msvc.py chains OUT [--max-hops N] [--configs "NAME;..."] [--jobs N]
  python msvc.py cycles OUT

chains   For each of S2's depth configurations (s2.py's DEPTH_CONFIGS, sources from
         s2.chain_source, so the very chains S2 measured on GCC and Clang), the longest chain
         C_K -> ... -> C_0 that compiles at MSVC's default limits: doubling from 200 hops (NFR-09)
         up to --max-hops (default 1024), then bisection. cl has no -ftemplate-depth, so S2's
         levels per hop can't be measured directly. Instead two calibration programs find cl's own
         nesting limit for a class template and for a variable template, the counterpart of
         GCC's -ftemplate-depth=900 and Clang's 1024. Also checks the builtins ADR-0014 relies on.
cycles   cycles.cpp's four cases under the memo strategy (the default: no cycle check) and the
         depth strategy (the opt-in check, bound 100 as in S2): what cl reports.

Every compile is a full compile (/c, the flags of msvc.py in spikes/S1-deduced-discovery, plus
/bigobj only if cl asks for it with C1128), under a 300 s timeout. Results are GitHub annotations,
at most ten per step. Exit status: 1 if the lean memo stub compiles fewer than 200 hops in either
request form (NFR-09), or if a cycle compiles; 0 otherwise.
"""
import argparse
import json
import re
import sys
import threading
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

sys.dont_write_bytecode = True  # keep the source tree free of __pycache__

HERE = Path(__file__).resolve().parent
S1_DIR = HERE.parent / "S1-deduced-discovery"
sys.path.insert(0, str(HERE))
import s2  # noqa: E402  spike S2's driver: chain_source() and the configurations

msvc = s2.variants.load("s1_msvc", S1_DIR / "msvc.py")  # cl, annotations, error parsing

TIMEOUT_S = 300
NFR09 = 200
INCLUDES = [f"/I{HERE}", f"/I{S1_DIR}"]
# S2's configurations, most important first: the lean memo stub decides NFR-09.
CONFIGS = ["memo value", "memo shared_ptr", "memo value, check only", "memo shared_ptr, check only",
           "depth value", "s1-stub value"]
LEAN = ("memo value", "memo shared_ptr")
LIMIT_CODES = {  # what stopped a compile, by the first matching error code
    "C1202": "C1202 recursion limit", "C1063": "C1063 compiler stack overflow",
    "C1060": "C1060 out of heap space", "C1001": "C1001 internal compiler error",
    "C1004": "C1004 internal compiler error", "C1128": "C1128 object section limit",
    "C1054": "C1054 nesting limit", "C1026": "C1026 parser stack overflow",
}
print_lock = threading.Lock()


def annotate(level, title, message):
    with print_lock:
        msvc.annotate(level, title, message)


def classify(code, out, timed_out):
    if timed_out:
        return "timeout"
    if code == 0:
        return "ok"
    for c, kind in LIMIT_CODES.items():
        if re.search(rf"\b{c}\b", out):
            return kind
    if code in (0xC00000FD, -1073741571):
        return "stack overflow (0xC00000FD)"
    if "TRL-E007" in out:
        return "TRL-E007"
    m = re.search(r"\berror ([A-Z]+\d+)", out)
    return f"error {m.group(1)}" if m else f"exit {code}"


class Probe:
    """Compiles generated sources in its own directory and caches the outcome per size."""

    def __init__(self, name, work, source_of):
        self.name, self.work, self.source_of = name, work, source_of
        self.work.mkdir(parents=True, exist_ok=True)
        self.cache = {}

    def compiles(self, n):
        if n not in self.cache:
            src = self.work / f"{self.name}-{n}.cpp"
            src.write_text(self.source_of(n))
            obj = self.work / f"{self.name}-{n}.obj"
            args = [*INCLUDES, "/c", str(src), f"/Fo{obj}"]
            code, out, secs, timed_out = msvc.cl(args, self.work, TIMEOUT_S)
            bigobj = False
            if code != 0 and re.search(r"\bC1128\b", out):  # the object format's section limit
                code, out, secs, timed_out = msvc.cl(["/bigobj", *args], self.work, TIMEOUT_S)
                bigobj = True
            self.cache[n] = {"ok": code == 0, "kind": classify(code, out, timed_out),
                             "first_error": msvc.first_error(out), "seconds": round(secs, 1),
                             "bigobj": bigobj}
            src.unlink()
            obj.unlink(missing_ok=True)
        return self.cache[n]["ok"]

    def longest(self, start, maximum):
        """The largest n in [1, maximum] that compiles, assuming every smaller one does: doubling
        from start, then bisection. Returns (n, the smallest n known to fail, or None). A compile
        that times out ends the search, so a slow configuration can't take the job's whole time."""
        best = 0

        def ok(n):
            nonlocal best
            good = self.compiles(n)
            if self.cache[n]["kind"] == "timeout":
                raise TimedOut(n)
            if good:
                best = max(best, n)
            return good

        try:
            if ok(start):
                lo, hi = start, None
                while hi is None:
                    if lo >= maximum:
                        return lo, None
                    nxt = min(2 * lo, maximum)
                    if ok(nxt):
                        lo = nxt
                    else:
                        hi = nxt
            else:
                if not ok(1):
                    return 0, 1
                lo, hi = 1, start
            while hi - lo > 1:
                mid = (lo + hi) // 2
                if ok(mid):
                    lo = mid
                else:
                    hi = mid
            return lo, hi
        except TimedOut as stop:
            return best, stop.args[0]


class TimedOut(Exception):
    """A compile hit TIMEOUT_S; its argument is the size."""


CALIBRATIONS = {  # name -> source with N levels of nesting
    "class template": "template <int N> struct nest {{ static constexpr int value = nest<N - 1>::value + 1; }};\n"
                      "template <> struct nest<0> {{ static constexpr int value = 0; }};\n"
                      "static_assert(nest<{n}>::value == {n});\n",
    "variable template": "template <int N> constexpr int nest = nest<N - 1> + 1;\n"
                         "template <> constexpr int nest<0> = 0;\n"
                         "static_assert(nest<{n}> == {n});\n",
}
BUILTINS = {"__is_constructible": "static_assert(__is_constructible(int, int));\n",
            "__is_same": "static_assert(__is_same(int, int) && !__is_same(int, long));\n"}


def outcome_text(probe, n, failing):
    """'N hops (what stops N+1)' for a search result."""
    if failing is None:
        return f">= {n} (the largest tried)"
    f = probe.cache[failing]
    # A known limit's code already says what stopped it; otherwise quote the first error.
    known = f["kind"] == "timeout" or f["kind"] in LIMIT_CODES.values() or "0xC00000FD" in f["kind"]
    err = "" if known or not f["first_error"] else ": " + msvc.error_message(f["first_error"], 110)
    return f"{n} ({f['kind']} at {failing}{err})"


def chains(out, max_hops, configs, jobs):
    out.mkdir(parents=True, exist_ok=True)
    results = {"builtins": {}, "calibration": {}, "chains": {}}
    for name, src in BUILTINS.items():
        p = Probe(name.strip("_"), out / "builtins", lambda n, s=src: s + "int main() {}\n")
        p.compiles(1)
        results["builtins"][name] = p.cache[1]
    annotate("notice", "S2 builtins on cl",
             "; ".join(f"{k}: {'yes' if v['ok'] else 'no (' + msvc.error_message(v['first_error'], 80) + ')'}"
                       for k, v in results["builtins"].items()))
    parts = []
    for name, template in CALIBRATIONS.items():
        p = Probe(name.replace(" ", "-"), out / "calibration",
                  lambda n, t=template: t.format(n=n) + "int main() {}\n")
        n, failing = p.longest(256, 65536)
        results["calibration"][name] = {"levels": n, "failing": failing,
                                        "at_failing": p.cache.get(failing)}
        parts.append(f"{name}: {outcome_text(p, n, failing)}")
    annotate("notice", "S2 cl nesting limit (calibration)",
             "deepest nesting that compiles: " + "; ".join(parts) +
             ". GCC's default -ftemplate-depth is 900, Clang's 1024")
    status = 0

    def run_config(config):
        stub, strategy, form = s2.DEPTH_CONFIGS[config]
        p = Probe(config.replace(" ", "-").replace(",", ""), out / "chains",
                  lambda n: s2.chain_source(stub, strategy, form, n))
        n, failing = p.longest(NFR09, max_hops)
        at_n = p.cache.get(n, {})
        results["chains"][config] = {"longest": n, "failing": failing, "cache": p.cache}
        lean = config in LEAN
        text = (f"longest chain at cl's default limits: {outcome_text(p, n, failing)}"
                + (f"; {at_n['seconds']} s at {n}" if at_n else "")
                + ("; needed /bigobj" if any(c["bigobj"] for c in p.cache.values()) else "")
                + (f"; NFR-09 (>= {NFR09}) {'met' if n >= NFR09 else 'NOT met'}" if lean else ""))
        annotate("warning" if lean and n < NFR09 else "notice", f"S2 chain {config}", text)
        return lean and n < NFR09

    with ThreadPoolExecutor(max_workers=jobs) as pool:
        for failed in pool.map(run_config, configs):
            status |= 1 if failed else 0
    (out / "chains.json").write_text(json.dumps(results, indent=1, default=str) + "\n")
    msvc.dump("S2 chains: every compile", json.dumps(results, indent=1, default=str))
    return status


CYCLE_VARIANTS = {"memo": ["/DSTUB=2", "/DS2_STRATEGY=0"],
                  "depth": ["/DSTUB=2", "/DS2_STRATEGY=1", "/DS2_MAX_DEPTH=100"]}


def cycles(out):
    work = out / "cycles"
    work.mkdir(parents=True, exist_ok=True)
    rows, status = [], 0
    for variant, defines in CYCLE_VARIANTS.items():
        for case, shape in s2.CYCLE_CASES.items():
            obj = work / f"{variant}-{case}.obj"
            code, text, secs, timed_out = msvc.cl([*INCLUDES, *defines, f"/DCASE={case}", "/c",
                                                   str(HERE / "cycles.cpp"), f"/Fo{obj}"], work, TIMEOUT_S)
            msvc.dump(f"cycles.cpp, {variant}, case {case}: output", text)
            kind = classify(code, text, timed_out)
            errors = msvc.error_lines(text)
            row = {"variant": variant, "case": case, "shape": shape, "kind": kind, "errors": len(errors),
                   "lines": len(text.splitlines()), "bytes": len(text.encode()), "seconds": round(secs, 1),
                   "first_error": msvc.first_error(text)}
            rows.append(row)
            bad = kind == "ok" or "internal compiler error" in kind or "stack overflow" in kind
            status |= 1 if bad else 0
            summary = (f"{len(errors)} error{'s' if len(errors) != 1 else ''}, {row['lines']} lines, "
                       f"{row['bytes'] / 1024:.0f} KB ({kind}); first: "
                       f"{msvc.error_message(row['first_error'], 150)}")
            annotate("warning" if bad else "notice", f"S2 cycle {variant} case {case}",
                     f"{shape}: " + ("COMPILES" if kind == "ok" else summary))
    (out / "cycles.json").write_text(json.dumps(rows, indent=1) + "\n")
    return status


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("section", choices=["chains", "cycles"])
    parser.add_argument("out", type=Path)
    parser.add_argument("--max-hops", type=int, default=1024)
    parser.add_argument("--configs", default=";".join(CONFIGS),
                        help="separated by semicolons (the names contain commas), from: " + "; ".join(CONFIGS))
    parser.add_argument("--jobs", type=int, default=2, help="configurations compiled at once (default 2)")
    args = parser.parse_args(argv)
    out = args.out.resolve()
    if args.section == "chains":
        wanted = [c.strip() for c in args.configs.split(";") if c.strip()]
        unknown = sorted(set(wanted) - set(CONFIGS))
        if unknown or not wanted:
            parser.error(f"unknown configuration(s): {'; '.join(unknown) or args.configs}")
        return chains(out, args.max_hops, [c for c in CONFIGS if c in wanted], max(1, args.jobs))
    return cycles(out)


if __name__ == "__main__":
    sys.exit(main())
