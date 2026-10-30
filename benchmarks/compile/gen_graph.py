#!/usr/bin/env python3
"""Generate dependency graphs and C++ programs that wire them (plan §8.4 and §9.1).

  gen_graph.py --n 100 [--fan-in 1-3|10] [--forms shared_ptr|reference|mixed] [--seed S]
               [--variant manual] [-o FILE] [--expected]
  gen_graph.py --random SEEDS [--variant manual] [-o FILE] [--expected]

The first form makes one graph of N components, for the compile-time benchmarks
(benchmarks/compile/run.py). The second makes one random graph per seed, all in one program, for
the property tests (tests/property/check_sharing.py). SEEDS is a list such as "1-32", "7,9" or
"1000000+64" (64 seeds from 1000000). The program goes to FILE or stdout; --expected prints the
lines the program must print instead.

The graph. Component k has an interface I<k> with an implementation C<k>, or only a concrete
class C<k>. Its lifetime is singleton (one object per injector) or unique (a new object per
request), and its constructor takes one parameter per dependency, on earlier components, in a
request form from plan §6.3: std::shared_ptr<T>, std::unique_ptr<T>, T&, const T&, T*, const T* or
T by value. A class Root depends on every component that nothing else depends on (and, in mixed
graphs, on up to two more).

  fan-in 1-3  Component k depends on k-1 and on up to two earlier components, chosen by the
              research harness's LCG, whose state starts at the seed. With the default seed,
              12345, this is the research graph (research/experiments/scripts/gen_scaling.py).
  fan-in 10   Fruit's published setting: the first 10% of the components have no dependencies,
              and every other component k depends on k-1 and on min(10, k) - 1 others.
  shared_ptr  Every component is a singleton requested as std::shared_ptr<I<k>>: the research
              setting, and the default.
  reference   Every component is a singleton requested as I<k>&.
  mixed       Random lifetimes, interfaces and request forms, as plan §6.3 allows them, with
              owning forms for unique dependencies of singletons (the captive rule). A unique
              component that would create more than UNIQUE_CAP objects per request becomes a
              singleton, so that object counts stay small. The property tests use this.

The identity checksum. Every class derives from gen::node. A program resolves Root twice from one
injector and once from a second injector, then walks the three roots depth-first, visiting
constructor dependencies in parameter order and numbering each object the first time the walk
reaches it. It prints, per graph,

  graph <name>: instances=<objects reached> identity=<16 hex digits>

where the identity hashes the sequence of numbers the walk sees. That sequence depends only on
which requests share an object: not on addresses, construction order or the order in which
arguments are evaluated. identity() computes the same line from this script's model of the graph.
A wiring that shares a unique object, makes two objects of a singleton, or shares singletons
between injectors prints a different line. That is the equivalence oracle: every variant of a
graph must print the line identity() predicts.

Variants. A variant emits the wiring of a graph: `includes()` gives the #include lines it needs
and `wiring()` the code, inside the graph's namespace, that defines

  void walk_roots(gen::walker& w)

which builds two injectors, resolves Root twice from the first and once from the second, and calls
w.edge() on the three roots, in that order, while everything is alive. `include_dirs` lists the
include directories it needs, relative to the repository. ManualVariant is the hand-written
baseline; add a variant to VARIANTS to measure it (T1.9 adds trellis).
"""
import argparse
import sys
from dataclasses import dataclass
from pathlib import Path

RESEARCH_SEED = 12345  # the initial LCG state of research/experiments/scripts/gen_scaling.py
FAN_INS = ("1-3", "10")
FORM_SETS = ("shared_ptr", "reference", "mixed")
UNIQUE_CAP = 16  # in mixed graphs, the most objects one request of a unique component may create
RANDOM_MAX_N = 40  # random graphs (--random) have 1 to RANDOM_MAX_N components

SINGLETON = "singleton"
UNIQUE = "unique"
# Request forms, by the names used here. "value" is T by value, only for concrete unique targets.
FORMS = ("shared_ptr", "unique_ptr", "ref", "const_ref", "ptr", "const_ptr", "value")
SHARING_FORMS = ("shared_ptr", "ref", "const_ref", "ptr", "const_ptr")  # for singleton targets

MASK64 = (1 << 64) - 1
HASH_OFFSET = 14695981039346656037  # FNV-1a's 64-bit offset basis and prime, one step per number
HASH_PRIME = 1099511628211


@dataclass(frozen=True)
class Edge:
    """One constructor parameter: a request of component `target` in form `form`."""
    target: int
    form: str


@dataclass(frozen=True)
class Component:
    index: int
    lifetime: str
    interface: bool  # bound as I<k> -> C<k>; otherwise only the concrete class C<k> exists
    deps: tuple

    @property
    def type(self):
        """The type that requests name: the interface if there is one."""
        return f"I{self.index}" if self.interface else f"C{self.index}"


@dataclass(frozen=True)
class Graph:
    name: str
    components: tuple
    root: tuple  # Root's constructor parameters, as Edges


class Rng:
    """SplitMix64: small, fast, and the same in every Python version (unlike `random`)."""

    def __init__(self, seed):
        self.state = seed & MASK64

    def below(self, bound):
        self.state = (self.state + 0x9E3779B97F4A7C15) & MASK64
        z = self.state
        z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & MASK64
        z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & MASK64
        return (z ^ (z >> 31)) % bound


def research_dependencies(n, seed=RESEARCH_SEED):
    """The research harness's graph: k depends on k-1 and on up to two earlier components drawn
    with its LCG (a duplicate draw is dropped). gen_scaling.py's dependencies(n), with the LCG's
    initial state as a parameter."""
    state = seed
    deps = []
    for k in range(n):
        if k == 0:
            deps.append([])
            continue
        chosen = [k - 1]
        for _ in range(2):
            state = (state * 1103515245 + 12345) % (2 ** 31)
            if k - 1 > 0:
                d = state % (k - 1)
                if d not in chosen:
                    chosen.append(d)
        deps.append(sorted(chosen))
    return deps


def fruit_dependencies(n, seed):
    """Fruit's benchmark setting: the first 10% of the components (at least one) have no
    dependencies; component k depends on k-1 and on min(10, k) - 1 others below k-1."""
    rng = Rng(seed)
    leaves = max(1, n // 10)
    deps = []
    for k in range(n):
        if k < leaves:
            deps.append([])
            continue
        pool = list(range(k - 1))
        chosen = [k - 1]
        for i in range(min(10, k) - 1):  # a partial Fisher-Yates shuffle of the pool
            j = i + rng.below(len(pool) - i)
            pool[i], pool[j] = pool[j], pool[i]
            chosen.append(pool[i])
        deps.append(sorted(chosen))
    return deps


DEPENDENCIES = {"1-3": research_dependencies, "10": fruit_dependencies}


def _mixed_lifetimes(structure, rng):
    """Random lifetimes, about 40% unique. A unique component costs one object plus the cost of
    its unique dependencies on every request; one that would cost more than UNIQUE_CAP becomes a
    singleton, which bounds the number of objects in every walk."""
    lifetimes, cost = [], []
    for deps in structure:
        lifetime = UNIQUE if rng.below(100) < 40 else SINGLETON
        total = 1 + sum(cost[d] for d in deps if lifetimes[d] == UNIQUE)
        if lifetime == UNIQUE and total > UNIQUE_CAP:
            lifetime = SINGLETON
        lifetimes.append(lifetime)
        cost.append(total if lifetime == UNIQUE else 0)
    return lifetimes


def _mixed_form(consumer_lifetime, target_lifetime, target_interface, rng):
    """A request form that plan §6.3 allows for this edge. Singletons are requested through a
    sharing form. Unique targets are requested through an owning form (unique_ptr, or by value if
    concrete); a unique consumer may also take one as shared_ptr, its sole owner. A singleton
    consumer never holds a unique dependency through a sharing form (the captive rule, E006)."""
    if target_lifetime == SINGLETON:
        choices = SHARING_FORMS
    else:
        choices = ("unique_ptr",)
        if consumer_lifetime == UNIQUE:
            choices += ("shared_ptr",)
        if not target_interface:
            choices += ("value",)
    return choices[rng.below(len(choices))]


def make_graph(n, fan_in="1-3", forms="shared_ptr", seed=RESEARCH_SEED, name=None):
    """A deterministic graph of n components; see the module documentation."""
    if n < 1:
        raise ValueError(f"a graph needs at least one component, not {n}")
    if fan_in not in DEPENDENCIES:
        raise ValueError(f"unknown fan-in profile {fan_in!r} (known: {', '.join(FAN_INS)})")
    if forms not in FORM_SETS:
        raise ValueError(f"unknown request forms {forms!r} (known: {', '.join(FORM_SETS)})")
    structure = DEPENDENCIES[fan_in](n, seed)
    # The structure and the attributes use separate random streams, so that a fan-in 1-3 graph
    # keeps the research structure whatever its forms.
    rng = Rng(seed ^ 0x5EED5EED5EED5EED)
    if forms == "mixed":
        lifetimes = _mixed_lifetimes(structure, rng)
        interfaces = [rng.below(100) < 70 for _ in range(n)]
    else:
        lifetimes = [SINGLETON] * n
        interfaces = [True] * n

    def form_of(consumer_lifetime, target):
        if forms == "shared_ptr":
            return "shared_ptr"
        if forms == "reference":
            return "ref"
        return _mixed_form(consumer_lifetime, lifetimes[target], interfaces[target], rng)

    components = tuple(
        Component(k, lifetimes[k], interfaces[k],
                  tuple(Edge(d, form_of(lifetimes[k], d)) for d in structure[k]))
        for k in range(n))
    # Root depends on every component that nothing depends on, so that the whole graph is
    # reachable from it (with fan-in 1-3 that is only N-1, as in the research). Mixed graphs add
    # up to two more requests, so that roots also share objects with the components.
    used = {d for deps in structure for d in deps}
    targets = [k for k in range(n) if k not in used]
    if forms == "mixed":
        for _ in range(rng.below(3)):
            extra = rng.below(n)
            if extra not in targets:
                targets.append(extra)
    root = tuple(Edge(t, form_of(UNIQUE, t)) for t in sorted(targets))
    if name is None:
        name = f"n{n}-fanin{fan_in}-{forms}-seed{seed}"
    return Graph(name, components, root)


def random_graph(seed):
    """The property tests' graph for one seed: 1 to RANDOM_MAX_N components, fan-in 1-3 (three
    times in four) or 10, mixed lifetimes and forms."""
    rng = Rng(seed)
    n = 1 + rng.below(RANDOM_MAX_N)
    fan_in = "10" if rng.below(4) == 0 else "1-3"
    return make_graph(n, fan_in, "mixed", seed, name=f"random-seed{seed}")


def parse_seeds(spec):
    """Seeds from a list such as "1-32", "7,9" or "1000000+64" (64 seeds from 1000000)."""
    seeds = []
    for item in filter(None, (part.strip() for part in spec.split(","))):
        try:
            if "+" in item:
                start, count = (int(x) for x in item.split("+", 1))
                seeds.extend(range(start, start + count))
            elif "-" in item:
                first, last = (int(x) for x in item.split("-", 1))
                if last < first:
                    raise ValueError
                seeds.extend(range(first, last + 1))
            else:
                seeds.append(int(item))
        except ValueError:
            raise ValueError(f"bad seed list item {item!r}: use N, A-B (A <= B) or A+COUNT") from None
    if not seeds or min(seeds) < 0:
        raise ValueError(f"the seed list {spec!r} names no seeds, or a negative one")
    return seeds


# The model: which objects a correct wiring creates, and the identity walk over them.

class _Object:
    __slots__ = ("children", "label")

    def __init__(self, children):
        self.children = children
        self.label = 0


def _injector(graph):
    """A model injector: a function that returns the object for one request of a component."""
    singletons = {}

    def request(target):
        if graph.components[target].lifetime == SINGLETON:
            return singletons[target]
        # Recursion runs only through unique components, at most UNIQUE_CAP deep.
        return _Object([request(e.target) for e in graph.components[target].deps])

    for comp in graph.components:  # dependencies come first, so build in index order
        if comp.lifetime == SINGLETON:
            singletons[comp.index] = _Object([request(e.target) for e in comp.deps])
    return request


def identity(graph):
    """The line the program prints for this graph, from the model."""
    first, second = _injector(graph), _injector(graph)
    roots = [_Object([injector(e.target) for e in graph.root]) for injector in (first, first, second)]
    objects, value = 0, HASH_OFFSET
    for root in roots:  # gen::walker::edge on each root, without recursion
        stack = [iter([root])]
        while stack:
            obj = next(stack[-1], None)
            if obj is None:
                stack.pop()
                continue
            fresh = obj.label == 0
            if fresh:
                objects += 1
                obj.label = objects
            value = ((value ^ obj.label) * HASH_PRIME) & MASK64
            if fresh:
                stack.append(iter(obj.children))
    return f"graph {graph.name}: instances={objects} identity={value:016x}"


# The C++ program.

_PARAMETER = {  # form -> (parameter, member, member initializer, object the walk visits)
    "shared_ptr": ("std::shared_ptr<{t}> {p}", "std::shared_ptr<{t}> {m}", "{m}(std::move({p}))", "*{m}"),
    "unique_ptr": ("std::unique_ptr<{t}> {p}", "std::unique_ptr<{t}> {m}", "{m}(std::move({p}))", "*{m}"),
    "ref": ("{t}& {p}", "{t}* {m}", "{m}(&{p})", "*{m}"),
    "const_ref": ("const {t}& {p}", "const {t}* {m}", "{m}(&{p})", "*{m}"),
    "ptr": ("{t}* {p}", "{t}* {m}", "{m}({p})", "*{m}"),
    "const_ptr": ("const {t}* {p}", "const {t}* {m}", "{m}({p})", "*{m}"),
    "value": ("{t} {p}", "{t} {m}", "{m}(std::move({p}))", "{m}"),
}

_SUPPORT = r"""namespace gen {
struct walker;

// Every class of a generated graph derives from node, so that a walker can visit the objects.
struct node {
  virtual ~node() = default;
  virtual void visit(walker& w) const = 0;
  mutable std::uint64_t label = 0;  // 0 until the walk reaches this object
};

// Walks object graphs depth-first, in constructor-parameter order, numbering each object the
// first time it is reached. The hash covers the number of every object reached, in order, so it
// shows which requests share an object, whatever the addresses and the construction order.
struct walker {
  std::uint64_t objects = 0;
  std::uint64_t hash = 14695981039346656037ULL;
  void edge(const node& n) {
    const bool fresh = n.label == 0;
    if (fresh) n.label = ++objects;
    hash = (hash ^ n.label) * 1099511628211ULL;
    if (fresh) n.visit(*this);
  }
};

inline void report(const char* name, void (*walk_roots)(walker&)) {
  walker w;
  walk_roots(w);
  std::printf("graph %s: instances=%llu identity=%016llx\n", name,
              static_cast<unsigned long long>(w.objects), static_cast<unsigned long long>(w.hash));
}
}  // namespace gen
"""


def _class(name, base, edges, components):
    """One generated class: a constructor taking `edges`, members holding them, and visit()."""
    if not edges:
        return [f"struct {name} final : {base} {{ void visit(gen::walker&) const override {{}} }};"]
    params, members, inits, visits = [], [], [], []
    for e in edges:
        parameter, member, init, visited = _PARAMETER[e.form]
        names = {"t": components[e.target].type, "p": f"d{e.target}", "m": f"m{e.target}_"}
        params.append(parameter.format(**names))
        members.append(member.format(**names) + ";")
        inits.append(init.format(**names))
        visits.append(f"w.edge({visited.format(**names)});")
    return [f"struct {name} final : {base} {{",
            f"  explicit {name}({', '.join(params)}) : {', '.join(inits)} {{}}",
            f"  void visit(gen::walker& w) const override {{ {' '.join(visits)} }}",
            f"  {' '.join(members)}",
            "};"]


def domain(graph):
    """The classes of a graph: the same code for every variant."""
    lines = []
    for comp in graph.components:
        base = "gen::node"
        if comp.interface:
            lines.append(f"struct I{comp.index} : gen::node {{}};")
            base = f"I{comp.index}"
        lines += _class(f"C{comp.index}", base, comp.deps, graph.components)
    lines += _class("Root", "gen::node", graph.root, graph.components)
    return lines


class ManualVariant:
    """Hand-written wiring: the baseline every other variant is measured against.

    A `wiring` object plays the injector. Its constructor creates every singleton, in dependency
    order, and it has a factory per unique component and request form, which builds a new object
    on every call. The singletons are assigned in the constructor body rather than initialized in
    member initializers, because the factories they call take `this`, and GCC 13 warns
    (-Wmaybe-uninitialized) about calls on an object whose members are still being initialized.
    The methods are separate so that the oracle's self-test can derive wirings with deliberate
    sharing bugs from this one."""

    name = "manual"
    include_dirs = ()

    def includes(self, graph):
        return []

    def wiring(self, graph):
        requested = {(e.target, e.form) for comp in graph.components for e in comp.deps}
        requested |= {(e.target, e.form) for e in graph.root}
        singletons = [c for c in graph.components if c.lifetime == SINGLETON]
        lines = ["struct wiring {", "  wiring() {"]
        for comp in singletons:
            lines.append(f"    s{comp.index} = "
                         f"std::make_shared<C{comp.index}>({self.arguments(graph, comp.deps)});")
        lines.append("  }")
        for comp in graph.components:
            if comp.lifetime == UNIQUE:
                for form in sorted(f for t, f in requested if t == comp.index):
                    lines += self.factory(graph, comp, form)
        lines.append(f"  Root make_root() const {{ return Root({self.arguments(graph, graph.root)}); }}")
        lines += [f"  std::shared_ptr<{comp.type}> s{comp.index};" for comp in singletons]
        lines.append("};")
        return lines + self.walk_roots(graph)

    def arguments(self, graph, edges):
        return ", ".join(self.argument(graph, e) for e in edges)

    def argument(self, graph, edge):
        """The expression that satisfies one request."""
        t = edge.target
        if graph.components[t].lifetime == SINGLETON:
            return {"shared_ptr": f"s{t}", "ptr": f"s{t}.get()",
                    "const_ptr": f"s{t}.get()"}.get(edge.form, f"*s{t}")
        return {"value": f"make{t}()", "unique_ptr": f"make{t}_unique()",
                "shared_ptr": f"make{t}_shared()"}[edge.form]

    def factory(self, graph, comp, form):
        """A member function that builds a new object of a unique component, for one form."""
        k, args = comp.index, self.arguments(graph, comp.deps)
        if form == "value":
            return [f"  C{k} make{k}() const {{ return C{k}({args}); }}"]
        holder = "std::unique_ptr" if form == "unique_ptr" else "std::shared_ptr"
        maker = "std::make_unique" if form == "unique_ptr" else "std::make_shared"
        suffix = "unique" if form == "unique_ptr" else "shared"
        return [f"  {holder}<{comp.type}> make{k}_{suffix}() const {{ return {maker}<C{k}>({args}); }}"]

    def walk_roots(self, graph):
        return ["inline void walk_roots(gen::walker& w) {",
                "  const wiring first{};",
                "  const wiring second{};",
                "  const Root a = first.make_root();",
                "  const Root b = first.make_root();",
                "  const Root c = second.make_root();",
                "  w.edge(a);",
                "  w.edge(b);",
                "  w.edge(c);",
                "}"]


VARIANTS = {"manual": ManualVariant()}


def variant_of(variant):
    """A variant object, given one or its name in VARIANTS."""
    if isinstance(variant, str):
        if variant not in VARIANTS:
            raise ValueError(f"unknown variant {variant!r} (known: {', '.join(VARIANTS)})")
        return VARIANTS[variant]
    return variant


def program(graphs, variant="manual"):
    """A C++ program that wires `graphs` with `variant` and prints one identity line per graph,
    in order. Each graph lives in its own namespace, g0, g1, ..."""
    variant = variant_of(variant)
    names = ", ".join(g.name for g in graphs)
    lines = ["// Generated by benchmarks/compile/gen_graph.py; do not edit.",
             f"// variant {variant.name}, graphs: {names}",
             "#include <cstdint>", "#include <cstdio>", "#include <memory>", "#include <utility>"]
    seen = set()
    for graph in graphs:
        for include in variant.includes(graph):
            if include not in seen:
                seen.add(include)
                lines.append(include)
    lines += ["", _SUPPORT.rstrip("\n"), ""]
    for index, graph in enumerate(graphs):
        lines.append(f"namespace g{index} {{")
        lines += domain(graph)
        lines += variant.wiring(graph)
        lines += [f"}}  // namespace g{index}", ""]
    lines.append("int main() {")
    for index, graph in enumerate(graphs):
        lines.append(f'  gen::report("{graph.name}", g{index}::walk_roots);')
    lines += ["  return 0;", "}"]
    return "\n".join(lines) + "\n"


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    what = parser.add_mutually_exclusive_group(required=True)
    what.add_argument("--n", type=int, help="one graph of N components")
    what.add_argument("--random", metavar="SEEDS", help="one random graph per seed")
    parser.add_argument("--fan-in", default="1-3", choices=FAN_INS)
    parser.add_argument("--forms", default="shared_ptr", choices=FORM_SETS)
    parser.add_argument("--seed", type=int, default=RESEARCH_SEED)
    parser.add_argument("--variant", default="manual", choices=sorted(VARIANTS))
    parser.add_argument("-o", "--output", help="write the program here (default: stdout)")
    parser.add_argument("--expected", action="store_true",
                        help="print the lines the program must print, instead of the program")
    args = parser.parse_args(argv)
    try:
        if args.random:
            graphs = [random_graph(seed) for seed in parse_seeds(args.random)]
        else:
            graphs = [make_graph(args.n, args.fan_in, args.forms, args.seed)]
    except ValueError as error:
        parser.error(str(error))
    text = ("\n".join(identity(g) for g in graphs) + "\n" if args.expected
            else program(graphs, args.variant))
    if args.output:
        Path(args.output).write_text(text, encoding="utf-8")
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
