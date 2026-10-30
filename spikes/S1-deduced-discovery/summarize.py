#!/usr/bin/env python3
"""Pivot the outputs of run.sh into the tables quoted in docs/spikes/S1-deduced-discovery.md.

usage: summarize.py OUT_DIR CONFIG...

Reads OUT_DIR/forms-<config>.txt (ROW lines from forms.cpp), OUT_DIR/diag/*.txt
(first error line of each failing cell) and OUT_DIR/bench/*.tsv, and prints
Markdown to stdout.
"""
import re
import statistics
import sys
from pathlib import Path

ROW = re.compile(r"^ROW\|([^|]*)\|([^|]*)\|([^|]*)\|([^|]*)\|([^|]*)\|([^|]*)\|(\d+)\|(\d+)$")
SHORT_OP = {"operator T()": "T()", "operator T&()": "T&()", "operator const T&()": "const T&()",
            "operator T&&()": "T&&()", "-": "-"}


def classify(line):
    """Map a compiler's first error line to a short reason."""
    low = line.lower()
    if "ambiguous" in low:
        return "ambiguous"
    if "deleted" in low:
        return "deleted"
    if "invalid user-defined conversion" in low:
        return "picks T&() for an rvalue ref"
    if "no viable conversion" in low or "no matching" in low or "no known conversion" in low:
        return "no viable conversion"
    if "cannot bind" in low or "binds to" in low or "bind" in low:
        return "cannot bind"
    return line.strip()[:60] if line.strip() else "?"


def read_forms(out, config):
    rows = {}
    order = []
    path = out / f"forms-{config}.txt"
    if not path.exists():
        return rows, order
    for line in path.read_text().splitlines():
        m = ROW.match(line)
        if m:
            key = (m.group(1), m.group(2))
            rows[key] = m.groups()[:6]
            if key not in order:
                order.append(key)
    return rows, order


def read_diag(out, config, set_name, form):
    safe = re.sub(r"[^A-Za-z0-9]+", "_", f"{config}-{set_name}-{form}")
    path = out / "diag" / f"{safe}.txt"
    return classify(path.read_text()) if path.exists() else "not diagnosed"


def cell(out, config, row):
    set_name, form, status, oper, request, verdict = row
    if status == "resolves":
        mark = "" if verdict.startswith("exact") else " (differs)"
        return f"`{SHORT_OP.get(oper, oper)}` → `{request}`{mark}"
    return f"✗ {read_diag(out, config, set_name, form)}"


def forms_tables(out, configs):
    data = {c: read_forms(out, c)[0] for c in configs}
    order = next((read_forms(out, c)[1] for c in configs if read_forms(out, c)[1]), [])
    sets = []
    for s, _ in order:
        if s not in sets:
            sets.append(s)
    forms = [f for s, f in order if s == sets[0]] if sets else []

    print("### Recommended set (`min`): form by compiler\n")
    print("| Parameter | " + " | ".join(configs) + " |")
    print("|---|" + "---|" * len(configs))
    for form in forms:
        cells = [cell(out, c, data[c][("min", form)]) if ("min", form) in data[c] else "missing" for c in configs]
        print(f"| `{form}` | " + " | ".join(cells) + " |")

    print("\n### Every operator set: form by set (all five configurations agree unless a cell says otherwise)\n")
    print("| Parameter | " + " | ".join(f"`{s}`" for s in sets) + " |")
    print("|---|" + "---|" * len(sets))
    for form in forms:
        cells = []
        for s in sets:
            per = {c: cell(out, c, data[c][(s, form)]) for c in configs if (s, form) in data[c]}
            values = list(dict.fromkeys(per.values()))
            if len(values) == 1:
                cells.append(values[0])
            else:
                cells.append("; ".join(f"{c}: {v}" for c, v in per.items()))
        print(f"| `{form}` | " + " | ".join(cells) + " |")


def bench_table(out, configs):
    bench = out / "bench"
    if not bench.exists():
        return
    print("\n### Arity probing cost (front end only, `-fsyntax-only`; median of the runs)\n")
    variants = []
    results = {}
    for c in configs:
        path = bench / f"{c}.tsv"
        if not path.exists():
            continue
        for line in path.read_text().splitlines():
            parts = line.split("\t")
            if len(parts) != 4 or parts[0] == "variant":
                continue
            variant, run, seconds, kib = parts
            if variant not in variants:
                variants.append(variant)
            results.setdefault((c, variant), []).append((float(seconds), int(kib)))
    print("| Variant | " + " | ".join(f"{c} s (MB)" for c in configs) + " |")
    print("|---|" + "---|" * len(configs))
    medians = {}
    for v in variants:
        cells = []
        for c in configs:
            runs = results.get((c, v))
            if not runs:
                cells.append("-")
                continue
            t = statistics.median(r[0] for r in runs)
            m = statistics.median(r[1] for r in runs) / 1024
            medians[(c, v)] = t
            cells.append(f"{t:.3f} ({m:.0f})")
        print(f"| {v} | " + " | ".join(cells) + " |")
    print("\nCost per class above the baseline of the same size, in milliseconds:\n")
    print("| Variant | " + " | ".join(configs) + " |")
    print("|---|" + "---|" * len(configs))
    for v in variants:
        size = 1000 if v.endswith("-1000") else 100
        base = "baseline-1000" if size == 1000 else "baseline"
        if v == base:
            continue
        cells = []
        for c in configs:
            if (c, v) in medians and (c, base) in medians:
                cells.append(f"{(medians[(c, v)] - medians[(c, base)]) * 1000 / size:.2f}")
            else:
                cells.append("-")
        print(f"| {v} | " + " | ".join(cells) + " |")


def main():
    out = Path(sys.argv[1])
    configs = sys.argv[2:]
    forms_tables(out, configs)
    bench_table(out, configs)


if __name__ == "__main__":
    main()
