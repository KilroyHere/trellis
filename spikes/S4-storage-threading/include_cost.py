#!/usr/bin/env python3
"""Measure what each design's headers add to a translation unit.

usage: include_cost.py OUT_DIR COMPILER...

The baseline is the set of headers a DI core includes anyway. Design C needs nothing beyond
<atomic> (C++20 atomic wait lives there); design A needs <mutex>; design B needs <mutex> and
<condition_variable>. Prints preprocessed line counts (deterministic) and the median of five
-fsyntax-only wall times.
"""
import statistics
import subprocess
import sys
import time
from pathlib import Path

BASE = ["array", "atomic", "cstddef", "cstdint", "memory", "new", "tuple", "type_traits", "utility"]
SETS = {
    "baseline (C: atomic wait)": BASE,
    "+ <mutex> (A: call_once)": BASE + ["mutex"],
    "+ <mutex>, <condition_variable> (B)": BASE + ["mutex", "condition_variable"],
}


def main(argv):
    out = Path(argv[1])
    out.mkdir(parents=True, exist_ok=True)
    print("| compiler | headers | preprocessed lines | -fsyntax-only, median of 5 (s) |")
    print("|---|---|---|---|")
    for compiler in argv[2:]:
        for index, (name, headers) in enumerate(SETS.items()):
            source = out / f"include_set_{index}.cpp"
            source.write_text("".join(f"#include <{h}>\n" for h in headers) + "int main() {}\n")
            pre = subprocess.run([compiler, "-std=c++20", "-E", "-P", str(source)],
                                 capture_output=True, text=True, check=True).stdout
            lines = sum(1 for line in pre.splitlines() if line.strip())
            times = []
            for _ in range(5):
                start = time.perf_counter()
                subprocess.run([compiler, "-std=c++20", "-fsyntax-only", str(source)], check=True)
                times.append(time.perf_counter() - start)
            print(f"| {compiler} | {name} | {lines} | {statistics.median(times):.3f} |")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
