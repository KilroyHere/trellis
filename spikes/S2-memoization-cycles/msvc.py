#!/usr/bin/env python3
"""Spike S2 on MSVC (task T1.13): the chain-depth probe and the cycle cases, compiled with cl.
Run it from a Visual Studio developer prompt; .github/workflows/msvc-probe.yml does that on
GitHub's windows-latest runner.

  python msvc.py chains OUT [--max-hops N] [--configs "NAME;..."] [--jobs N]
  python msvc.py cycles OUT [--variants NAME,...]

chains   For each of S2's depth configurations (s2.py's DEPTH_CONFIGS, sources from
         s2.chain_source, so the very chains S2 measured on GCC and Clang), the longest chain
         C_K -> ... -> C_0 that compiles at cl's default limits: doubling from 200 hops (NFR-09)
         up to --max-hops (default 1024), then bisection. Where cl has /templateDepth:N (VS 2026),
         also S2's levels per hop: the smallest /templateDepth that compiles chains of 50 and 100
         hops. Before that: the builtins ADR-0014 relies on, and cl's nesting limit for a class
         template and a variable template, the counterpart of GCC's -ftemplate-depth=900 and
         Clang's 1024.
cycles   cycles.cpp's four cases under the memo strategy (the default: no cycle check) and the
         depth strategy (the opt-in check) with bounds of 100 hops (as in S2), 150 and 200
         (ADR-0014's bounds before and from MSVC 19.50).

Every compile is a full compile (/c, the flags of msvc.py in spikes/S1-deduced-discovery, plus
/bigobj only if cl asks for it with C1128), under a 300 s timeout. The chains and cycles get a
forced include that maps the missing __is_same builtin to std::is_same_v (COMPAT_HEADER). Results
are GitHub annotations, at most ten per step. Each step's results are compared with the recorded
MSVC results (results/msvc.json; MSVC_PROBE_S2_RECORD overrides it): exit status 0 when they are
unchanged, 1 when MSVC's behaviour changed.
"""
import argparse
import json
import os
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

msvc = s2.variants.load("s1_msvc", S1_DIR / "msvc.py")  # cl, annotations, errors, the record

RECORD = Path(os.environ.get("MSVC_PROBE_S2_RECORD") or HERE / "results" / "msvc.json")
TIMEOUT_S = 300
NFR09 = 200
INCLUDES = [f"/I{HERE}", f"/I{S1_DIR}"]
# cl has no __is_same builtin (the first run of this probe found it), and stub.hpp's on_path uses
# one at namespace scope, so stub.hpp doesn't compile at all. The fallback ADR-0014 names,
# std::is_same_v, comes in through a forced include, so every spike file stays unchanged. on_path
# serves the path strategy only: the memo and depth strategies never instantiate it. The header
# first includes every standard header the spike sources use, so the macro can't reach into the
# standard library (libstdc++, for one, uses __is_same itself).
COMPAT_HEADER = """// Forced include for spike S2 on MSVC (msvc.py): cl lacks the __is_same builtin.
#pragma once
#include <concepts>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <memory>
#include <new>
#include <tuple>
#include <type_traits>
#include <utility>
#if defined(_MSC_VER) && !defined(__clang__)
#define __is_same(A, B) std::is_same_v<A, B>
#endif
"""
TEMPLATE_DEPTH_HOPS = (50, 100)  # chain lengths for the /templateDepth sweep (levels per hop)
# S2's configurations, most important first: the lean memo stub decides NFR-09.
CONFIGS = ["memo value", "memo shared_ptr", "memo value, check only", "memo shared_ptr, check only",
           "depth value", "s1-stub value"]
LEAN = ("memo value", "memo shared_ptr")
LIMIT_CODES = {  # what stopped a compile, by the first matching error code
    "C2999": "C2999 template depth limit", "C1202": "C1202 recursion limit",
    "C1063": "C1063 compiler stack overflow", "C1060": "C1060 out of heap space",
    "C1001": "C1001 internal compiler error", "C1004": "C1004 internal compiler error",
    "C1128": "C1128 object section limit", "C1054": "C1054 nesting limit",
    "C1026": "C1026 parser stack overflow",
}
# cycles.cpp's strategies: the default (memo) and the opt-in check at three bounds.
CYCLE_VARIANTS = {"memo": ["/DSTUB=2", "/DS2_STRATEGY=0"],
                  "depth-100": ["/DSTUB=2", "/DS2_STRATEGY=1", "/DS2_MAX_DEPTH=100"],
                  "depth-150": ["/DSTUB=2", "/DS2_STRATEGY=1", "/DS2_MAX_DEPTH=150"],
                  "depth-200": ["/DSTUB=2", "/DS2_STRATEGY=1", "/DS2_MAX_DEPTH=200"]}
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


class TimedOut(Exception):
    """A compile hit TIMEOUT_S; its argument is the size."""


class Probe:
    """Compiles generated sources of size n in its own directory, at cl's default limits
    (self.cache, keyed by n) or with /templateDepth:D (self.depth_cache, keyed "n@D")."""

    def __init__(self, name, work, source_of, flags=()):
        self.name, self.work, self.source_of, self.flags = name, work, source_of, list(flags)
        self.work.mkdir(parents=True, exist_ok=True)
        self.cache, self.depth_cache = {}, {}

    def _compile(self, n, extra):
        src = self.work / f"{self.name}-{n}.cpp"
        src.write_text(self.source_of(n))
        obj = self.work / f"{self.name}-{n}.obj"
        args = [*INCLUDES, *self.flags, *extra, "/c", str(src), f"/Fo{obj}"]
        code, out, secs, timed_out = msvc.cl(args, self.work, TIMEOUT_S)
        bigobj = False
        if code != 0 and re.search(r"\bC1128\b", out):  # the object format's section limit
            code, out, secs, timed_out = msvc.cl(["/bigobj", *args], self.work, TIMEOUT_S)
            bigobj = True
        src.unlink()
        obj.unlink(missing_ok=True)
        return {"ok": code == 0, "kind": classify(code, out, timed_out),
                "first_error": msvc.first_error(out), "seconds": round(secs, 1), "bigobj": bigobj}

    def compiles(self, n):
        if n not in self.cache:
            self.cache[n] = self._compile(n, [])
        return self.cache[n]["ok"]

    def compiles_at_depth(self, n, depth):
        key = f"{n}@{depth}"
        if key not in self.depth_cache:
            self.depth_cache[key] = self._compile(n, [f"/templateDepth:{depth}"])
        return self.depth_cache[key]["ok"]

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

    def min_depth(self, n):
        """The smallest /templateDepth that compiles size n, assuming 1 to 16 levels per step;
        None if even the largest fails."""
        lo, hi = n, 16 * n + 128
        if not self.compiles_at_depth(n, hi):
            return None
        while lo < hi:  # hi compiles; the answer is in [lo, hi]
            mid = (lo + hi) // 2
            if self.compiles_at_depth(n, mid):
                hi = mid
            else:
                lo = mid + 1
        return hi


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
    """'N (what stops it at the next size)' for a search result."""
    if failing is None:
        return f">= {n} (the largest tried)"
    f = probe.cache[failing]
    # A known limit's code already says what stopped it; otherwise quote the first error.
    known = f["kind"] == "timeout" or f["kind"] in LIMIT_CODES.values() or "0xC00000FD" in f["kind"]
    err = "" if known or not f["first_error"] else ": " + msvc.error_message(f["first_error"], 110)
    return f"{n} ({f['kind']} at {failing}{err})"


def template_depth_supported(work):
    """Whether this cl takes /templateDepth:N. VS 2026's does; an older one ignores an unknown
    option with command-line warning D9002 and still succeeds, so the output decides."""
    work.mkdir(parents=True, exist_ok=True)
    src = work / "template-depth.cpp"
    src.write_text("int main() { return 0; }\n")
    code, out, _, _ = msvc.cl(["/templateDepth:2000", "/c", str(src), f"/Fo{work / 'template-depth.obj'}"],
                              work, 60)
    return code == 0 and not re.search(r"\bD9002\b|unknown option", out)


def compat_flags(out):
    out.mkdir(parents=True, exist_ok=True)
    header = out / "msvc_compat.hpp"
    header.write_text(COMPAT_HEADER)
    return [f"/FI{header}"]


def check_against_record(title, out, name, summary, select):
    """Save the summary, then compare it with the record's matching part: select(record data)
    picks what this step measured. Returns the step's exit status."""
    (out / f"{name}-summary.json").write_text(json.dumps(summary, indent=1, sort_keys=True) + "\n")
    msvc.dump(f"{name}: summary in the record's format", json.dumps(summary, indent=1, sort_keys=True))
    version_key, data = msvc.record(RECORD)
    with print_lock:
        return msvc.verdict(title, version_key, select(data) if version_key else None, summary)


def chains(out, max_hops, configs, jobs):
    out.mkdir(parents=True, exist_ok=True)
    summary = {"builtins": {}, "calibration": {}, "chains": {}}
    details = {}
    for name, src in BUILTINS.items():
        p = Probe(name.strip("_"), out / "builtins", lambda n, s=src: s + "int main() {}\n")
        p.compiles(1)
        summary["builtins"][name] = p.cache[1]["ok"]
        details[name] = p.cache[1]
    annotate("notice", "S2 builtins on cl",
             "; ".join(f"{k}: {'yes' if v['ok'] else 'no (' + msvc.error_message(v['first_error'], 80) + ')'}"
                       for k, v in ((k, details[k]) for k in BUILTINS)))
    parts = []
    for name, template in CALIBRATIONS.items():
        p = Probe(name.replace(" ", "-"), out / "calibration",
                  lambda n, t=template: t.format(n=n) + "int main() {}\n")
        n, failing = p.longest(256, 65536)
        summary["calibration"][name] = {"levels": n,
                                        "stopped_by": p.cache[failing]["kind"] if failing else None}
        parts.append(f"{name}: {outcome_text(p, n, failing)}")
    depth_ok = template_depth_supported(out / "calibration")
    summary["template_depth_option"] = depth_ok
    annotate("notice", "S2 cl nesting limit (calibration)",
             "deepest nesting that compiles: " + "; ".join(parts) +
             f". /templateDepth:N {'accepted' if depth_ok else 'not accepted'}."
             " GCC's default -ftemplate-depth is 900, Clang's 1024")
    flags = compat_flags(out)

    def run_config(config):
        stub, strategy, form = s2.DEPTH_CONFIGS[config]
        p = Probe(config.replace(" ", "-").replace(",", ""), out / "chains",
                  lambda n: s2.chain_source(stub, strategy, form, n), flags)
        n, failing = p.longest(NFR09, max_hops)
        at_n = p.cache.get(n, {})
        lean = config in LEAN
        text = (f"longest chain at cl's default limits: {outcome_text(p, n, failing)}"
                + (f"; {at_n['seconds']} s at {n}" if at_n else "")
                + ("; needed /bigobj" if any(c["bigobj"] for c in p.cache.values()) else "")
                + (f"; NFR-09 (>= {NFR09}) {'met' if n >= NFR09 else 'NOT met'}" if lean else ""))
        points = {}
        if depth_ok and n > 0:  # S2's method: the smallest depth that compiles K hops, two Ks
            for k in TEMPLATE_DEPTH_HOPS:
                d = p.min_depth(k)
                if d is None:
                    break
                points[str(k)] = d
        if len(points) == 2:
            (k1, d1), (k2, d2) = ((int(k), d) for k, d in points.items())
            slope = (d2 - d1) / (k2 - k1)
            text += (f"; levels per hop {slope:g} (smallest /templateDepth {d1} at {k1} hops, "
                     f"{d2} at {k2})")
        summary["chains"][config] = {"longest": n,
                                     "stopped_by": p.cache[failing]["kind"] if failing else None,
                                     "min_template_depth": points}
        details[config] = {"cache": p.cache, "depth_cache": p.depth_cache}
        annotate("notice", f"S2 chain {config}", text)

    with ThreadPoolExecutor(max_workers=jobs) as pool:
        list(pool.map(run_config, configs))
    (out / "chains.json").write_text(json.dumps(details, indent=1, default=str) + "\n")
    msvc.dump("S2 chains: every compile", json.dumps(details, indent=1, default=str))

    def select(data):
        part = {k: data[k] for k in ("builtins", "calibration", "template_depth_option") if k in data}
        part["chains"] = {c: v for c, v in data.get("chains", {}).items() if c in configs}
        return part

    return check_against_record("S2 chains against the MSVC record", out, "chains", summary, select)


def cycles(out, variants):
    work = out / "cycles"
    work.mkdir(parents=True, exist_ok=True)
    flags = compat_flags(out)
    summary = {}
    for variant in variants:
        summary[variant] = {}
        for case, shape in s2.CYCLE_CASES.items():
            obj = work / f"{variant}-{case}.obj"
            code, text, secs, timed_out = msvc.cl([*INCLUDES, *flags, *CYCLE_VARIANTS[variant], f"/DCASE={case}",
                                                   "/c", str(HERE / "cycles.cpp"), f"/Fo{obj}"], work, TIMEOUT_S)
            msvc.dump(f"cycles.cpp, {variant}, case {case}: output", text)
            kind = classify(code, text, timed_out)
            errors = msvc.error_lines(text)
            lines = len(text.splitlines())
            summary[variant][str(case)] = {"kind": kind, "errors": len(errors)}
            line = (f"{len(errors)} error{'s' if len(errors) != 1 else ''}, {lines} lines, "
                    f"{len(text.encode()) / 1024:.0f} KB ({kind}); first: "
                    f"{msvc.error_message(msvc.first_error(text), 150)}")
            annotate("notice", f"S2 cycle {variant} case {case}", f"{shape}: " + ("COMPILES" if kind == "ok" else line))
    name = "cycles-" + "-".join(variants)
    return check_against_record(f"S2 cycles ({', '.join(variants)}) against the MSVC record", out, name,
                                {"cycles": summary},
                                lambda data: {"cycles": {v: data.get("cycles", {}).get(v, "(not recorded)")
                                                         for v in variants}})


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("section", choices=["chains", "cycles"])
    parser.add_argument("out", type=Path)
    parser.add_argument("--max-hops", type=int, default=1024)
    parser.add_argument("--configs", default=";".join(CONFIGS),
                        help="separated by semicolons (the names contain commas), from: " + "; ".join(CONFIGS))
    parser.add_argument("--jobs", type=int, default=2, help="configurations compiled at once (default 2)")
    parser.add_argument("--variants", default=",".join(CYCLE_VARIANTS),
                        help="cycle strategies, from: " + ", ".join(CYCLE_VARIANTS))
    args = parser.parse_args(argv)
    out = args.out.resolve()
    if args.section == "chains":
        wanted = [c.strip() for c in args.configs.split(";") if c.strip()]
        unknown = sorted(set(wanted) - set(CONFIGS))
        if unknown or not wanted:
            parser.error(f"unknown configuration(s): {'; '.join(unknown) or args.configs}")
        return chains(out, args.max_hops, [c for c in CONFIGS if c in wanted], max(1, args.jobs))
    variants = [v.strip() for v in args.variants.split(",") if v.strip()]
    unknown = sorted(set(variants) - set(CYCLE_VARIANTS))
    if unknown or not variants:
        parser.error(f"unknown variant(s): {', '.join(unknown) or args.variants}")
    return cycles(out, variants)


if __name__ == "__main__":
    sys.exit(main())
