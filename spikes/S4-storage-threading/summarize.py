#!/usr/bin/env python3
"""Summarize the S4 Google Benchmark JSON files into Markdown tables.

usage: summarize.py OUT_DIR CONFIG...

Reads OUT_DIR/bench-<config>.json for each config. Every benchmark ran with repetitions; for
each one this prints the median and the min-max range of nanoseconds per slot access (wall time
divided by the `batch` counter; for 8-thread runs Google Benchmark already divides wall time by
the accesses of all threads, so those numbers are amortized over the threads). Ratios compare
each design's median with design C (atomic state + atomic wait) in the same run, which cancels
most of the noise from other processes on the machine.
"""
import json
import statistics
import sys
from collections import defaultdict
from pathlib import Path

UNIT = {"ns": 1.0, "us": 1e3, "ms": 1e6, "s": 1e9}
DESIGNS = ["eager", "none", "once", "cv", "cv_nodetect", "wait", "wait_nodetect", "wait_spin"]
LABEL = {"eager": "eager", "none": "none", "once": "A once", "cv": "B cv",
         "cv_nodetect": "B cv, no R004", "wait": "C wait", "wait_nodetect": "C wait, no R004",
         "wait_spin": "C+spin"}
REFERENCE = "wait"


def load(path):
    data = json.loads(path.read_text())
    samples = defaultdict(list)
    for b in data["benchmarks"]:
        if b.get("run_type") != "iteration" or b.get("error_occurred"):
            continue
        kind, storage, design = b["run_name"].split("/")[:3]
        per_access = b["real_time"] * UNIT[b["time_unit"]] / b.get("batch", 1.0)
        samples[(kind, storage, design)].append(per_access)
    return samples


def stats(values):
    return statistics.median(values), min(values), max(values), len(values)


def fmt(ns):
    return f"{ns:.3g}" if ns < 100 else f"{ns:.0f}"


def rows(samples):
    groups = defaultdict(dict)
    for (kind, storage, design), values in samples.items():
        groups[(kind, storage)][design] = stats(values)
    return groups


ORDER = ["warm", "warm8_same", "warm8_distinct", "share", "share8_same", "cold", "cold8_same",
         "cold8_same_spin1us", "cold8_distinct"]


def sort_key(item):
    (kind, storage), _ = item
    return (ORDER.index(kind) if kind in ORDER else 99, storage)


def main(argv):
    out = Path(argv[1])
    configs = argv[2:]
    per_config = {}
    for config in configs:
        path = out / f"bench-{config}.json"
        if path.exists():
            per_config[config] = rows(load(path))
    if not per_config:
        print("no benchmark results found")
        return 1

    for config, groups in per_config.items():
        print(f"\n### {config}: ns per access, median [min-max] of the repetitions\n")
        print("| benchmark | " + " | ".join(LABEL[d] for d in DESIGNS) + " |")
        print("|---|" + "---|" * len(DESIGNS))
        for (kind, storage), cells in sorted(groups.items(), key=sort_key):
            line = [f"{kind}/{storage}"]
            for d in DESIGNS:
                if d in cells:
                    med, lo, hi, n = cells[d]
                    line.append(f"{fmt(med)} [{fmt(lo)}-{fmt(hi)}]")
                else:
                    line.append("")
            print("| " + " | ".join(line) + " |")

    print(f"\n### All compilers: median ns per access (ratio to {LABEL[REFERENCE]} in the same run)\n")
    names = list(per_config)
    print("| benchmark | design | " + " | ".join(names) + " |")
    print("|---|---|" + "---|" * len(names))
    keys = sorted({k for g in per_config.values() for k in g}, key=lambda k: sort_key((k, None)))
    for key in keys:
        for d in DESIGNS:
            line = []
            present = False
            for config in names:
                cells = per_config[config].get(key, {})
                if d not in cells:
                    line.append("")
                    continue
                present = True
                med = cells[d][0]
                ref = cells.get(REFERENCE)
                ratio = f" ({med / ref[0]:.2f}x)" if ref and d != REFERENCE else ""
                line.append(fmt(med) + ratio)
            if present:
                print(f"| {key[0]}/{key[1]} | {LABEL[d]} | " + " | ".join(line) + " |")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
