#!/usr/bin/env python3
"""Property tests: random dependency graphs must share objects exactly as their lifetimes say.

  check_sharing.py --config build/<preset>/trellis_compiler.json [--seeds SEEDS] [--shard I/N]
                   [--variants manual,trellis] [--batch 8]
  check_sharing.py --config build/<preset>/trellis_compiler.json --self-test

benchmarks/compile/gen_graph.py makes one random graph per seed (1 to 40 components, fan-in 1-3 or
10, singleton and unique lifetimes, and the request forms T, T&, const T&, T*, const T*,
unique_ptr and shared_ptr wherever plan §6.3 allows them; weak_ptr, lazy and provider are not
generated) and a program that wires the graphs with each variant (manual: written by hand;
trellis: Trellis's injector, T1.9), resolves each graph's root twice from one injector and once
from a second, and prints an identity checksum of the objects it reached, with the number of
objects it built. The checksum shows which requests share an object, and every object built must
be reached, so the line must equal the one gen_graph.py computes from its own model of the graph.
This is the equivalence oracle of plan §9.1: every variant must print the model's line for every
graph, so Trellis's checksums also equal the hand-wired ones.

Seeds: --seeds, else the TRELLIS_PROPERTY_SEEDS environment variable, else DEFAULT_SEEDS (1-32). A
list is "1-32", "7,9" or "1000000+64" (64 seeds from 1000000). CI sets TRELLIS_PROPERTY_SEEDS from
the run number, so that it tries seeds local runs never use: .github/workflows/ci.yml 64 per run,
nightly.yml 256. --shard I/N takes every Nth seed starting with the Ith; CTest runs the shards as
separate tests.

The programs, BATCH graphs each, are compiled with the build's compiler and flags (the preset's
flags, sanitizers included, -std=c++20, the include directory and the project warnings, from
trellis_compiler.json) plus -Werror, then run. Sources, binaries and compiler output go to
<build>/property/. A failure names the seed and the commands that reproduce it.

--self-test checks the oracle itself. Each of four deliberate sharing bugs (singletons shared
between injectors, a singleton made anew for each request, a unique object shared by its
requests, and every singleton built twice with the first object dropped, which nothing reaches)
is planted in the smallest graphs of seeds SELF_TEST_SEEDS in which it is observable,
GRAPHS_PER_BUG each, all in one program, and every one of those graphs must print a line other
than the expected one. The default fan-in 1-3 graph must also be the research harness's graph,
and generating graphs twice must give the same program and the same expected lines.

Exit status: 0 when every check passed, 1 when one failed, 2 for a usage or configuration error.
"""
import argparse
import dataclasses
import importlib.util
import json
import os
import sys
from pathlib import Path

sys.dont_write_bytecode = True  # keep the source tree free of __pycache__

ROOT = Path(__file__).resolve().parent.parent.parent
DEFAULT_SEEDS = "1-32"
DEFAULT_VARIANTS = "manual,trellis"
SELF_TEST_SEEDS = "1-24"
DEFAULT_BATCH = 8
COMPILE_TIMEOUT_S = 300
RUN_TIMEOUT_S = 60
MEMORY_MB = 6000  # address-space cap for compiles, and for runs unless sanitizers are on


def _load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


gen_graph = _load("gen_graph", ROOT / "benchmarks" / "compile" / "gen_graph.py")
bench_run = _load("bench_run", ROOT / "benchmarks" / "compile" / "run.py")


def load_config(path):
    path = Path(path)
    try:
        config = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        raise ValueError(f"cannot read {path} ({error}); configure the build first "
                         f"(cmake --preset <name>)") from error
    missing = [key for key in ("compiler", "frontend", "flags", "binary_dir", "golden_name")
               if key not in config]
    if missing:
        raise ValueError(f"{path} lacks {', '.join(missing)}; reconfigure the build")
    return config


class Build:
    """Compiles and runs generated programs with one build's compiler and flags."""

    def __init__(self, config, config_path, work):
        self.config = config
        self.config_path = Path(config_path)
        self.work = Path(work)
        self.work.mkdir(parents=True, exist_ok=True)
        self.label = config["golden_name"]
        # Sanitizers reserve far more address space than any cap allows.
        sanitized = any("-fsanitize" in flag for flag in config["flags"])
        self.run_memory_mb = None if sanitized else MEMORY_MB
        self.env = bench_run.child_environment()

    def output_of(self, graphs, variant, stem):
        """(lines the program printed, None), or (None, why it failed)."""
        source = self.work / f"{stem}.cpp"
        source.write_text(gen_graph.program(graphs, variant), encoding="utf-8")
        exe = self.work / (stem + (".exe" if os.name == "nt" else ""))
        includes = [f"-I{ROOT / d}" for d in gen_graph.variant_of(variant).include_dirs]
        if self.config["frontend"] == "MSVC":  # untested: MSVC runs only in CI
            command = [self.config["compiler"], "/nologo", *self.config["flags"], *includes, "/WX",
                       f"/Fe{exe}", f"/Fo{self.work / (stem + '.obj')}", str(source)]
        else:
            command = [self.config["compiler"], *self.config["flags"], *includes, "-Werror",
                       str(source), "-o", str(exe)]
        built = bench_run.run_command(command, COMPILE_TIMEOUT_S, MEMORY_MB, cwd=self.work,
                                      env=self.env)
        log = self.work / f"{stem}.log"
        log.write_text("$ " + " ".join(built.command) + "\n" + built.stdout + built.stderr,
                       encoding="utf-8")
        if not built.ok:
            return None, (f"compile: {built.problem(COMPILE_TIMEOUT_S, MEMORY_MB)} "
                          f"(log {display(log)})")
        ran = bench_run.run_command([exe], RUN_TIMEOUT_S, self.run_memory_mb, cwd=self.work,
                                    env=self.env)
        if not ran.ok:
            detail = " | ".join(ran.stderr.strip().splitlines()[:3])
            return None, f"run: {ran.problem(RUN_TIMEOUT_S)} {detail}".strip()
        return ran.stdout.splitlines(), None


def display(path):
    try:
        return Path(path).resolve().relative_to(ROOT).as_posix()
    except ValueError:
        return str(path)


def reproduce(build, seed, variant):
    return (f"python3 tests/property/check_sharing.py --config {display(build.config_path)} "
            f"--seeds {seed} --variants {variant}; or see the program and its expected output "
            f"with benchmarks/compile/gen_graph.py --random {seed} --variant {variant} [--expected]")


def check_sharing(build, seeds, variants, batch):
    """The property test proper. Returns the number of failed graphs."""
    graphs = [gen_graph.random_graph(seed) for seed in seeds]
    seed_of = {graph.name: seed for graph, seed in zip(graphs, seeds)}
    failed = 0
    for start in range(0, len(graphs), batch):
        chunk = graphs[start:start + batch]
        expected = [gen_graph.identity(graph) for graph in chunk]
        for variant in variants:
            stem = f"sharing-{variant}-seeds{seed_of[chunk[0].name]}-{seed_of[chunk[-1].name]}"
            lines, problem = build.output_of(chunk, variant, stem)
            for index, graph in enumerate(chunk):
                got = lines[index] if lines is not None and index < len(lines) else None
                if got == expected[index]:
                    continue
                failed += 1
                why = problem or f"expected '{expected[index]}', got '{got}'"
                print(f"FAIL {graph.name} ({variant}, {build.label}): {why}")
                print(f"  in {display(build.work / (stem + '.cpp'))}")
                print(f"  reproduce: {reproduce(build, seed_of[graph.name], variant)}")
            if lines is not None and len(lines) > len(chunk):
                failed += 1
                print(f"FAIL {stem} ({variant}, {build.label}): {len(lines)} lines of output "
                      f"for {len(chunk)} graphs")
    total = len(graphs) * len(variants)
    status = "PASS" if failed == 0 else "FAIL"
    print(f"{status} property.sharing ({', '.join(variants)}, {build.label}): "
          f"{total - failed} of {total} graphs share objects as their lifetimes say "
          f"(seeds {format_seeds(seeds)})")
    return failed


def format_seeds(seeds):
    if len(seeds) > 6:
        return f"{', '.join(map(str, seeds[:3]))}, ..., {seeds[-1]}; {len(seeds)} seeds"
    return ", ".join(map(str, seeds))


# The oracle's self-test: hand-written wirings with one deliberate sharing bug each.

def instantiations(graph):
    """Objects of each component that one injector creates for two resolutions of Root: 1 for a
    singleton, one per request for a unique component. Consumers come after their dependencies,
    so requests are counted from Root down."""
    requests = [0] * len(graph.components)
    for edge in graph.root:
        requests[edge.target] += 2
    count = [0] * len(graph.components)
    for comp in reversed(graph.components):
        count[comp.index] = 1 if comp.lifetime == gen_graph.SINGLETON else requests[comp.index]
        for edge in comp.deps:
            requests[edge.target] += count[comp.index]
    return count


def shared_ptr_requests(graph, target):
    """How many shared_ptr requests of TARGET one injector makes for two resolutions of Root."""
    count = instantiations(graph)
    total = sum(2 for e in graph.root if e.target == target and e.form == "shared_ptr")
    for comp in graph.components:
        total += sum(count[comp.index] for e in comp.deps
                     if e.target == target and e.form == "shared_ptr")
    return total


def _target(graph, lifetime):
    """The highest-numbered component of LIFETIME with at least two shared_ptr requests per
    injector, where sharing one object (or not) is observable; None if there is none."""
    for comp in reversed(graph.components):
        if comp.lifetime == lifetime and shared_ptr_requests(graph, comp.index) >= 2:
            return comp.index
    return None


class SingletonsSharedAcrossInjectors(gen_graph.ManualVariant):
    """Both "injectors" are one wiring object, as with process-wide singletons."""
    name = "bug-singletons-shared-across-injectors"

    def applies(self, graph):
        return any(c.lifetime == gen_graph.SINGLETON for c in graph.components)

    def walk_roots(self, graph):
        return [line.replace("const wiring second{};", "const wiring& second = first;")
                for line in super().walk_roots(graph)]


class SingletonMadePerRequest(gen_graph.ManualVariant):
    """One singleton gets a new object for each shared_ptr request."""
    name = "bug-singleton-made-per-request"

    def applies(self, graph):
        return _target(graph, gen_graph.SINGLETON) is not None

    def argument(self, graph, edge):
        if edge.target == _target(graph, gen_graph.SINGLETON) and edge.form == "shared_ptr":
            comp = graph.components[edge.target]
            return f"std::make_shared<C{comp.index}>({self.arguments(graph, comp.deps)})"
        return super().argument(graph, edge)


class UniqueSharedByRequests(gen_graph.ManualVariant):
    """One unique component's shared_ptr requests all get the same object."""
    name = "bug-unique-shared-by-requests"

    def applies(self, graph):
        return _target(graph, gen_graph.UNIQUE) is not None

    def factory(self, graph, comp, form):
        if comp.index != _target(graph, gen_graph.UNIQUE) or form != "shared_ptr":
            return super().factory(graph, comp, form)
        k, args = comp.index, self.arguments(graph, comp.deps)
        return [f"  mutable std::shared_ptr<{comp.type}> cache{k};",
                f"  std::shared_ptr<{comp.type}> make{k}_shared() const {{ "
                f"if (!cache{k}) cache{k} = std::make_shared<C{k}>({args}); return cache{k}; }}"]


class SingletonBuiltTwice(gen_graph.ManualVariant):
    """Every singleton is built twice and the first object dropped. Nothing reaches the extra
    objects, so only the construction count shows them."""
    name = "bug-singleton-built-twice"

    def applies(self, graph):
        return any(c.lifetime == gen_graph.SINGLETON for c in graph.components)

    def singleton(self, graph, comp):
        return super().singleton(graph, comp) * 2


BUGS = (SingletonsSharedAcrossInjectors(), SingletonMadePerRequest(), UniqueSharedByRequests(),
        SingletonBuiltTwice())
GRAPHS_PER_BUG = 4  # the smallest graphs where the bug is observable, to keep the compile short


class PlantedBugs(gen_graph.ManualVariant):
    """Wires each graph with the bug planted in it, so that one program covers every bug."""
    name = "planted-bugs"

    def __init__(self, bug_of):
        self.bug_of = bug_of  # graph name -> bug variant

    def wiring(self, graph):
        return self.bug_of[graph.name].wiring(graph)


def self_test(build):
    """Returns the number of failed checks."""
    failed = 0

    def result(ok, message):
        nonlocal failed
        failed += not ok
        print(f"{'PASS' if ok else 'FAIL'} {message}")

    research_file = ROOT / "research" / "experiments" / "scripts" / "gen_scaling.py"
    if research_file.is_file():
        research = _load("gen_scaling", research_file)
        sizes = list(range(1, 130)) + [250, 500, 1000]
        same = all(gen_graph.research_dependencies(n) == research.dependencies(n) for n in sizes)
        result(same, "oracle_selftest: the default fan-in 1-3 graph is the research harness's "
                     "graph, for N = 1 to 129, 250, 500 and 1000")
    else:
        print(f"SKIP oracle_selftest: research comparison ({display(research_file)} is missing)")

    seeds = gen_graph.parse_seeds(SELF_TEST_SEEDS)
    graphs = [gen_graph.random_graph(seed) for seed in seeds]
    graphs.append(gen_graph.make_graph(10))
    again = [gen_graph.random_graph(seed) for seed in seeds] + [gen_graph.make_graph(10)]
    same = (gen_graph.program(graphs) == gen_graph.program(again)
            and [gen_graph.identity(g) for g in graphs] == [gen_graph.identity(g) for g in again])
    result(same, "oracle_selftest: generating the same graphs twice gives the same program and "
                 "the same expected lines")

    planted, bug_of = [], {}
    for bug in BUGS:
        observable = sorted((g for g in graphs if bug.applies(g)), key=lambda g: len(g.components))
        if not observable:
            result(False, f"oracle_selftest: no graph to plant {bug.name} in")
        for graph in observable[:GRAPHS_PER_BUG]:
            graph = dataclasses.replace(graph, name=f"{graph.name}+{bug.name}")
            planted.append(graph)
            bug_of[graph.name] = bug
    lines, problem = build.output_of(planted, PlantedBugs(bug_of), "selftest-planted-bugs")
    if problem:
        result(False, f"oracle_selftest: the program with planted bugs: {problem}")
        return failed
    if len(lines) != len(planted):
        result(False, f"oracle_selftest: {len(lines)} lines of output for {len(planted)} graphs")
        return failed
    for bug in BUGS:
        mine = [(graph, line) for graph, line in zip(planted, lines) if bug_of[graph.name] is bug]
        missed = [graph.name for graph, line in mine if line == gen_graph.identity(graph)]
        result(bool(mine) and not missed,
               f"oracle_selftest: {bug.name} is caught in {len(mine) - len(missed)} of "
               f"{len(mine)} graphs ({build.label})"
               + (f"; missed in {', '.join(missed)}" if missed else ""))
    return failed


def parse_shard(text):
    try:
        index, count = (int(x) for x in text.split("/"))
    except ValueError:
        raise ValueError(f"bad --shard {text!r}: use I/N, such as 1/4") from None
    if not 1 <= index <= count:
        raise ValueError(f"bad --shard {text!r}: I must be between 1 and N")
    return index, count


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--config", required=True, help="path to the build's trellis_compiler.json")
    parser.add_argument("--seeds", help=f"default: $TRELLIS_PROPERTY_SEEDS, else {DEFAULT_SEEDS}")
    parser.add_argument("--shard", default="1/1", help="I/N: every Nth seed from the Ith")
    parser.add_argument("--variants", default=DEFAULT_VARIANTS,
                        help=f"comma-separated, from: {', '.join(gen_graph.VARIANTS)} "
                             f"(default {DEFAULT_VARIANTS})")
    parser.add_argument("--batch", type=int, default=DEFAULT_BATCH, help="graphs per program")
    parser.add_argument("--self-test", action="store_true", help="check the oracle itself")
    args = parser.parse_args(argv)
    bench_run.exit_on_termination_signals()
    try:
        config = load_config(args.config)
        index, count = parse_shard(args.shard)
        spec = args.seeds or os.environ.get("TRELLIS_PROPERTY_SEEDS") or DEFAULT_SEEDS
        seeds = gen_graph.parse_seeds(spec)[index - 1::count]
        variants = [v.strip() for v in args.variants.split(",") if v.strip()]
        for variant in variants:
            gen_graph.variant_of(variant)
        if args.batch < 1:
            raise ValueError("--batch must be at least 1")
    except ValueError as error:
        print(f"error: {error}", file=sys.stderr)
        return 2
    build = Build(config, args.config, Path(config["binary_dir"]) / "property"
                  / ("self-test" if args.self_test else f"shard{index}of{count}"))
    if args.self_test:
        return 1 if self_test(build) else 0
    if not seeds:
        print(f"PASS property.sharing: shard {index}/{count} has no seeds from {spec!r}")
        return 0
    return 1 if check_sharing(build, seeds, variants, args.batch) else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:  # Ctrl-C: the compiler is already stopped
        print("interrupted", file=sys.stderr)
        sys.exit(130)
