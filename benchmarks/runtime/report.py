#!/usr/bin/env python3
"""Summarize runtime benchmark results (Google Benchmark JSON) as a table.

  report.py [--baseline manual] [--out SUMMARY.json] RESULTS.json...

Each RESULTS file is trellis_runtime_bench's --benchmark_out for one preset, named <preset>.json
(scripts/bench-runtime.sh writes build/bench/runtime/<preset>.json); the compiler column is the
preset name without "-release". For every benchmark, case/variant, the table gives the median,
minimum and maximum time per operation over the repetitions (real time, ns), and for variants
other than the baseline the ratio of medians against the baseline with the same compiler: NFR-01
(resolve) and NFR-02 (graph) are such ratios. --out also writes the rows as JSON.
"""
import argparse
import json
import statistics
import sys
from pathlib import Path

TO_NS = {"ns": 1.0, "us": 1e3, "ms": 1e6, "s": 1e9}


def rows_of(path):
    data = json.loads(Path(path).read_text(encoding="utf-8"))
    compiler = Path(path).stem
    if compiler.endswith("-release"):  # not str.removesuffix: Python 3.8 is supported
        compiler = compiler[:-len("-release")]
    samples = {}
    for bench in data.get("benchmarks", []):
        if bench.get("run_type", "iteration") != "iteration" or bench.get("error_occurred"):
            continue
        name = bench.get("run_name", bench["name"])
        samples.setdefault(name, []).append(
            (bench["real_time"] * TO_NS[bench.get("time_unit", "ns")], bench["iterations"]))
    rows = []
    for name, values in samples.items():
        case, _, variant = name.partition("/")
        times = [t for t, _ in values]
        rows.append({"case": case, "variant": variant or "-", "compiler": compiler,
                     "median_ns": round(statistics.median(times), 2), "min_ns": round(min(times), 2),
                     "max_ns": round(max(times), 2), "repetitions": len(values),
                     "iterations": values[0][1]})
    return rows


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--baseline", default="manual")
    parser.add_argument("--out", type=Path, help="also write the rows to this JSON file")
    parser.add_argument("results", nargs="+")
    args = parser.parse_args(argv)
    try:
        rows = [row for path in args.results for row in rows_of(path)]
    except (OSError, ValueError, KeyError) as error:
        print(f"error: cannot read the results: {error}", file=sys.stderr)
        return 2
    if not rows:
        print("error: the results hold no benchmark runs", file=sys.stderr)
        return 2
    base = {(r["case"], r["compiler"]): r["median_ns"] for r in rows if r["variant"] == args.baseline}
    for r in rows:
        reference = base.get((r["case"], r["compiler"]))
        if r["variant"] != args.baseline and reference:
            r["ratio_to_baseline"] = round(r["median_ns"] / reference, 3)
    rows.sort(key=lambda r: (r["case"], r["compiler"], r["variant"] != args.baseline, r["variant"]))
    header = ["case", "variant", "compiler", "median ns", "min ns", "max ns", "reps x iters",
              f"vs {args.baseline}"]
    table = [[r["case"], r["variant"], r["compiler"], f"{r['median_ns']:.1f}", f"{r['min_ns']:.1f}",
              f"{r['max_ns']:.1f}", f"{r['repetitions']} x {r['iterations']}",
              f"{r['ratio_to_baseline']:.2f}x" if "ratio_to_baseline" in r else "-"] for r in rows]
    widths = [max(len(line[i]) for line in [header] + table) for i in range(len(header))]
    for line in [header] + table:
        print("  ".join(cell.ljust(width) for cell, width in zip(line, widths)).rstrip())
    if args.out:
        args.out.write_text(json.dumps(rows, indent=1) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
