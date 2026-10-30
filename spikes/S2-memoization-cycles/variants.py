"""Spike S2: the stub's wirings as variants of benchmarks/compile/gen_graph.py.

A variant emits, for one generated graph, a named binding map and walk_roots(), which builds two
injectors, creates Root twice from the first and once from the second, and walks them: the same
contract as gen_graph.ManualVariant, so run.py's equivalence oracle checks every variant's wiring.
"""
import importlib.util
import sys
from pathlib import Path

sys.dont_write_bytecode = True  # keep the source tree free of __pycache__

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
COMPILE_BENCH = ROOT / "benchmarks" / "compile"

STRATEGIES = {"memo": 0, "depth": 1, "path": 2}


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class StubVariant:
    """Wiring through spike S2's stub (stub.hpp) with one check strategy."""

    include_dirs = ("spikes/S2-memoization-cycles", "spikes/S1-deduced-discovery")

    def __init__(self, strategy, max_depth=None):
        self.strategy = strategy
        self.max_depth = max_depth
        self.name = f"s2-{strategy}"

    def includes(self, graph):
        lines = [f"#define S2_STRATEGY {STRATEGIES[self.strategy]}"]
        if self.max_depth is not None:
            lines.append(f"#define S2_MAX_DEPTH {self.max_depth}")
        return lines + ['#include "stub.hpp"']

    def wiring(self, graph):
        bindings = []
        for comp in graph.components:
            lifetime = "singleton" if comp.lifetime == "singleton" else "unique"
            bindings.append(f"s2::{lifetime}<{comp.type}, C{comp.index}>")
        lines = ["struct bindings : s2::map<"]
        lines += [f"    {b}," for b in bindings[:-1]] + [f"    {bindings[-1]}> {{}};"]
        lines += ["inline void walk_roots(gen::walker& w) {",
                  "  s2::injector<bindings> first;",
                  "  s2::injector<bindings> second;",
                  "  const Root a = first.create<Root>();",
                  "  const Root b = first.create<Root>();",
                  "  const Root c = second.create<Root>();",
                  "  w.edge(a);",
                  "  w.edge(b);",
                  "  w.edge(c);",
                  "}"]
        return lines


def register(gen_graph, max_depth=None):
    """Add the three stub variants to gen_graph.VARIANTS (the module object run.py loaded)."""
    for strategy in STRATEGIES:
        variant = StubVariant(strategy, max_depth)
        gen_graph.VARIANTS[variant.name] = variant
    return gen_graph
