#!/usr/bin/env python3
"""The chain-depth test (NFR-09, ADR-0014 item 3, task T1.11): the static engine compiles a
dependency chain of 200 hops at each compiler's default template depth, 150 on MSVC before 19.50.

  check_chain_depth.py --config build/<preset>/trellis_compiler.json [--shapes LIST] [--hops N]
                       [--jobs N]
  check_chain_depth.py --preset <preset> --deepest [--shapes LIST] [--record] [--jobs N]
  check_chain_depth.py --preset <preset> --self-test [--shapes LIST]
  check_chain_depth.py --program SHAPE HOPS
  check_chain_depth.py --list-shapes

A chain of N hops is N + 1 classes, C<N> -> C<N-1> -> ... -> C0, each taking the next as a
constructor parameter, so that creating C<N> resolves N requests, each inside the one before (the
count of spike S2 and T1.5). The root, C<N>, is unbound and requested by value with
injector.create<C<N>>(); its parameter and every other one is a request in the shape's form. The
shapes (SHAPES, and --list-shapes) cover every request form of plan §6.3 that Phase 1 serves, each
in a chain of its own, plus declared constructors, aggregates, an eager injector and one chain that
mixes all of them. Bindings are listed from the root down, so that an eager injector checks and
builds the whole chain from its first binding (a lazy injector doesn't depend on the order).

The test (the default) generates each shape's chain at the length NFR-09 asks of this compiler,
NFR09_HOPS or, for MSVC before 19.50, NFR09_MSVC_BEFORE_19_50_HOPS (or --hops N), and compiles it
with the build's compiler and flags from trellis_compiler.json, warnings as errors, and never
-ftemplate-depth or /templateDepth: so the compiler's default template depth decides.
  - GCC and Clang compile with -fsyntax-only. That instantiates everything a full compile does, the
    function templates that create the objects included (the chain's depth is the same, which T1.11
    checked at the limit), and skips code generation, which costs 5 to 10 times as much for a chain
    of singletons, whose every symbol names the injector's 200 bindings.
  - MSVC compiles with /c, as spike S2's MSVC addendum did (T1.13), without debug information
    (/Zi, /ZI and /Z7 are dropped: they don't change what instantiates, and they cost time).
Sources and compiler output go to <binary_dir>/chain_depth/<compiler>/. Each shape is a CTest test,
chain_depth.<shape> (tests/chain_depth/CMakeLists.txt); a passing test also prints the deepest chain
recorded for this preset and shape, and whether it was measured on this include/. A build whose
flags set -ftemplate-depth, -fconstexpr-depth or /templateDepth is refused: they move the limits.

--deepest finds, per shape, the longest chain that compiles at the default limits: starting from
the recorded result if there is one, otherwise from the length the test uses, up by doubling, then
by bisection, each compile as the test compiles. Only a compiler limit counts as the end: the
template depth limit, Clang's warning that its stack is nearly exhausted (an error with the
warnings, and what stops Clang 20's aggregate chain), cl's stack and heap limits, a crash, a timeout
or the memory cap; any other error stops the search with a failure. --record writes the results
into RECORD (tests/chain_depth/deepest.json), under the preset's name (and cl's version on MSVC),
with the commit and the tree id of include/, which a rebase keeps; run it for one preset at a time.
Re-measure after changing the static engine or discovery (include/trellis/static/,
include/trellis/discovery/).

--self-test compiles, links and runs every shape's chain with the build's flags, at a few hops (37
for the mixed shape, which then has every request form with every kind of class), and checks the
line it prints: that every object of the chain was built, so the chains the test compiles are what
they claim to be. CTest runs it as chain_depth.selftest.

--jobs N compiles N shapes at a time. In GitHub Actions a passing shape also prints a notice
annotation, and a failing one prints a line starting "FAIL", which the problem matchers turn into an
error annotation (.github/problem-matchers.json).

Exit status: 0 when every shape passed, 1 when one failed, 2 for a usage or configuration error.
"""
import argparse
import importlib.util
import json
import os
import re
import subprocess
import sys
import threading
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass, field
from datetime import date
from pathlib import Path

sys.dont_write_bytecode = True  # keep the source tree free of __pycache__

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
RECORD = HERE / "deepest.json"
CONFIG_NAME = "trellis_compiler.json"
NFR09_HOPS = 200
# NFR-09 on MSVC before 19.50, an assumed boundary: cl 19.44 has a fixed recursion limit of about
# 500 levels and stopped spike S2's lean stub at 161 hops (ADR-0014 item 3, provisional, T1.13).
NFR09_MSVC_BEFORE_19_50_HOPS = 150
MSVC_TEMPLATE_DEPTH_SWITCH = (19, 50)
SELF_TEST_HOPS = 4
MAX_HOPS = 4096  # --deepest looks no further
COMPILE_TIMEOUT_S = 300  # below CTest's 600 s per test (CMakePresets.json), so that this one fires
RUN_TIMEOUT_S = 60
MEMORY_MB = 6000  # address-space cap per compile, and per run unless sanitizers are on


def _load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


# run_command(): a new process group, a timeout that kills it as a whole, an address-space cap.
bench_run = _load("bench_run", ROOT / "benchmarks" / "compile" / "run.py")


# ---- the chains -------------------------------------------------------------------------------

@dataclass
class Chain:
    """What a shape generates: the classes C0 .. C<N> (inside namespace chain), what follows them
    (ctor_traits specializations), the arguments of make_injector, its options, and how many objects
    the classes' constructors count when the root is created (aggregates have no constructor)."""
    classes: list
    bindings: list = field(default_factory=list)
    after: list = field(default_factory=list)
    options: str = ""
    expected: int = 0


def _leaf():
    return "struct C0 { C0() { ++constructed; } };"


def _by_value(hops):
    lines = [_leaf()]
    lines += [f"struct C{k} {{ explicit C{k}(C{k - 1}) {{ ++constructed; }} }};"
              for k in range(1, hops + 1)]
    return Chain(lines, expected=hops + 1)


def _concrete_singletons(hops, parameter):
    """C0 .. C<N-1> bound as singletons, each class taking the next as `parameter` (a format with
    {t}, the type)."""
    lines = [_leaf()]
    lines += [f"struct C{k} {{ explicit C{k}({parameter.format(t=f'C{k - 1}')}) "
              f"{{ ++constructed; }} }};" for k in range(1, hops + 1)]
    bindings = [f"trellis::bind<chain::C{k}>.in(trellis::singleton)"
                for k in range(hops - 1, -1, -1)]
    return Chain(lines, bindings, expected=hops + 1)


def _interfaces(hops, parameter, lifetime):
    """I0 .. I<N-1>, each bound to its class C<k> with `lifetime` (a .in(...) or ""), each class
    taking the next interface as `parameter`; the root C<N> has no interface."""
    lines = ["struct I0 { virtual ~I0() = default; };",
             "struct C0 final : I0 { C0() { ++constructed; } };"]
    for k in range(1, hops + 1):
        if k < hops:
            lines.append(f"struct I{k} {{ virtual ~I{k}() = default; }};")
        base = f" final : I{k}" if k < hops else ""
        lines.append(f"struct C{k}{base} {{ explicit C{k}({parameter.format(t=f'I{k - 1}')}) "
                     f"{{ ++constructed; }} }};")
    bindings = [f"trellis::bind<chain::I{k}>.to<chain::C{k}>(){lifetime}"
                for k in range(hops - 1, -1, -1)]
    return Chain(lines, bindings, expected=hops + 1)


def value_shape(hops):
    return _by_value(hops)


def unique_ptr_shape(hops):
    return _interfaces(hops, "std::unique_ptr<{t}>", "")


def reference_shape(hops):
    return _concrete_singletons(hops, "{t}&")


def pointer_shape(hops):
    return _concrete_singletons(hops, "{t}*")


def shared_ptr_shape(hops):
    return _interfaces(hops, "std::shared_ptr<{t}>", ".in(trellis::singleton)")


def weak_ptr_shape(hops):
    return _interfaces(hops, "std::weak_ptr<{t}>", ".in(trellis::singleton)")


def declared_shape(hops):
    lines = [_leaf()]
    lines += [f"struct C{k} {{ using trellis_ctor = trellis::ctor<C{k - 1}>; "
              f"explicit C{k}(C{k - 1}) {{ ++constructed; }} }};" for k in range(1, hops + 1)]
    return Chain(lines, expected=hops + 1)


def aggregate_shape(hops):
    lines = [_leaf()] + [f"struct C{k} {{ C{k - 1} dep; }};" for k in range(1, hops + 1)]
    return Chain(lines, expected=1)


def eager_shape(hops):
    chain = _concrete_singletons(hops, "{t}&")
    chain.options = "<trellis::options{.eager = true}>"
    return chain


# The mixed shape's request forms, in turn: class C<k> is requested in form MIXED_FORMS[k % 9]:
# (parameter type, binding of C<k> or None, interface?, unique?). A std::shared_ptr of a unique
# object goes only to a unique consumer, as plan §6.3's captive rule asks: the form after it,
# std::unique_ptr, makes its consumer unique (mixed_shape checks it).
MIXED_FORMS = (
    ("{c}", None, False, True),
    ("std::shared_ptr<{c}>", "trellis::bind<{c}>", False, True),
    ("std::unique_ptr<{i}>", "trellis::bind<{i}>.to<{c}>()", True, True),
    ("std::shared_ptr<{i}>", "trellis::bind<{i}>.to<{c}>().in(trellis::singleton)", True, False),
    ("{c}&", "trellis::bind<{c}>.in(trellis::singleton)", False, False),
    ("const {c}&", "trellis::bind<{c}>.in(trellis::singleton_inline)", False, False),
    ("{c}*", "trellis::bind<{c}>.in(trellis::singleton)", False, False),
    ("const {c}*", "trellis::bind<{c}>.in(trellis::singleton_inline)", False, False),
    ("std::weak_ptr<{i}>", "trellis::bind<{i}>.to<{c}>().in(trellis::singleton)", True, False),
)
# How class C<k> of the mixed shape declares its constructor, in turn (k % 4): deduced, a
# ctor_traits specialization, a trellis_ctor member alias, or none, as an aggregate (where C<k> has
# no interface; otherwise deduced).
MIXED_KINDS = ("deduced", "ctor_traits", "member_alias", "aggregate")


def mixed_shape(hops):
    def form(k):
        return MIXED_FORMS[k % len(MIXED_FORMS)]

    def names(k, qualified=False):
        prefix = "chain::" if qualified else ""
        return {"c": f"{prefix}C{k}", "i": f"{prefix}I{k}"}

    for k in range(len(MIXED_FORMS)):  # the captive rule: a shared unique object, unique consumer
        assert not (form(k)[1] and form(k)[3] and "shared_ptr" in form(k)[0]) or form(k + 1)[3]
    lines, bindings, after, counted = [_leaf()], [], [], 1
    if form(0)[2]:
        lines.insert(0, "struct I0 { virtual ~I0() = default; };")
        lines[1] = "struct C0 final : I0 { C0() { ++constructed; } };"
    for k in range(1, hops + 1):
        parameter = form(k - 1)[0].format(**names(k - 1))
        interface = k < hops and form(k)[2]
        kind = MIXED_KINDS[k % len(MIXED_KINDS)]
        if interface:
            lines.append(f"struct I{k} {{ virtual ~I{k}() = default; }};")
            if kind == "aggregate":
                kind = "deduced"
        base = f" final : I{k}" if interface else ""
        if kind == "aggregate":
            lines.append(f"struct C{k} {{ {parameter} dep; }};")
            continue
        alias = (f"using trellis_ctor = trellis::ctor<{parameter}>; " if kind == "member_alias"
                 else "")
        lines.append(f"struct C{k}{base} {{ {alias}explicit C{k}({parameter}) "
                     f"{{ ++constructed; }} }};")
        counted += 1
        if kind == "ctor_traits":
            qualified = form(k - 1)[0].format(**names(k - 1, qualified=True))
            after.append(f"template <> struct trellis::ctor_traits<chain::C{k}> "
                         f": trellis::ctor<{qualified}> {{}};")
    for k in range(hops - 1, -1, -1):
        binding = form(k)[1]
        if binding:
            bindings.append(binding.format(**names(k, qualified=True)))
    return Chain(lines, bindings, after, expected=counted)


@dataclass(frozen=True)
class Shape:
    name: str
    make: object
    summary: str
    self_test_hops: int = SELF_TEST_HOPS  # --self-test's length


SHAPES = {s.name: s for s in (
    Shape("value", value_shape,
          "deduced classes, each taking the next by value, nothing bound (unbound classes are "
          "unique)"),
    Shape("unique_ptr", unique_ptr_shape,
          "interfaces bound to deduced classes, unique, each class taking the next interface as "
          "std::unique_ptr"),
    Shape("reference", reference_shape, "deduced singletons, each taking the next by reference"),
    Shape("pointer", pointer_shape, "deduced singletons, each taking the next by pointer"),
    Shape("shared_ptr", shared_ptr_shape,
          "interfaces bound to deduced singletons, each class taking the next interface as "
          "std::shared_ptr (the benchmarks' form)"),
    Shape("weak_ptr", weak_ptr_shape,
          "interfaces bound to deduced singletons, each class taking the next interface as "
          "std::weak_ptr"),
    Shape("declared", declared_shape,
          "classes that declare their constructor with a trellis_ctor alias, each taking the next "
          "by value"),
    Shape("aggregate", aggregate_shape, "aggregates, each holding the next by value"),
    Shape("eager", eager_shape,
          "the reference shape in an eager injector, whose make_injector checks and builds the "
          "singletons"),
    Shape("mixed", mixed_shape,
          "the request forms in turn: by value, std::shared_ptr of a unique class, "
          "std::unique_ptr, std::shared_ptr, a reference, a const reference to a "
          "singleton_inline, a pointer, a const pointer to a singleton_inline and std::weak_ptr, "
          "with deduced and declared constructors and aggregates",
          # every request form with every kind of class: 9 x 4 classes, then the root
          len(MIXED_FORMS) * len(MIXED_KINDS) + 1),
)}


def program(shape, hops):
    """The C++ program of one shape's chain of `hops` hops."""
    if hops < 1:
        raise ValueError(f"a chain needs at least one hop, not {hops}")
    chain = SHAPES[shape].make(hops)
    arguments = ",".join(f"\n      {b}" for b in chain.bindings)
    lines = [f"// Generated by tests/chain_depth/check_chain_depth.py: shape {shape}, "
             f"a chain of {hops} hops.",
             f"// {SHAPES[shape].summary}.",
             "#include <trellis/trellis.hpp>", "", "#include <cstdio>", "#include <memory>", "",
             "namespace chain {",
             "inline int constructed = 0;  // the objects the classes' constructors built",
             *chain.classes,
             "}  // namespace chain", ""]
    if chain.after:
        lines += [*chain.after, ""]
    lines += ["int main() {",
              f"  auto injector = trellis::make_injector{chain.options}({arguments});",
              f"  auto root = injector.create<chain::C{hops}>();",
              "  static_cast<void>(root);",
              f'  std::printf("{expected_line(shape, hops, "%d")}\\n", chain::constructed);',
              f"  return chain::constructed == {chain.expected} ? 0 : 1;",
              "}"]
    return "\n".join(lines) + "\n"


def expected_line(shape, hops, count=None):
    """The line a chain's program prints; COUNT defaults to the objects it must count."""
    if count is None:
        count = SHAPES[shape].make(hops).expected
    return f"chain {shape}, {hops} hops: constructed={count}"


# ---- compiling --------------------------------------------------------------------------------

class ConfigError(Exception):
    """The compiler configuration is missing or unreadable, or moves the limits under test."""


# A flag that moves the limits the test measures at their defaults: GCC's and Clang's
# -ftemplate-depth and -fconstexpr-depth (also as /clang:... for clang-cl), and cl's
# /templateDepth. Include paths and macros (-I, /I, -D, /D) don't count, whatever they name.
LIMIT_FLAG = re.compile(r"^[-/](?![IiDd]).*(?:ftemplate-depth|fconstexpr-depth|templateDepth)")


def load_config(path):
    path = Path(path)
    if not path.is_file():
        raise ConfigError(f"{path} not found; configure the build first (cmake --preset <name>)")
    try:
        config = json.loads(path.read_text(encoding="utf-8"))
    except ValueError as error:
        raise ConfigError(f"{path} is not valid JSON: {error}") from error
    missing = [key for key in ("compiler", "compiler_id", "compiler_version", "frontend",
                               "golden_name", "flags", "binary_dir") if key not in config]
    if missing:
        raise ConfigError(f"{path} lacks {', '.join(missing)}; reconfigure the build")
    moved = [flag for flag in config["flags"] if LIMIT_FLAG.search(flag)]
    if moved:
        raise ConfigError(f"the build's flags set {' '.join(moved)} ({path}), which moves the "
                          f"compiler's template limits; NFR-09 is measured at the default limits, "
                          f"so take it out of the preset or CXXFLAGS")
    return config


def version_tuple(text):
    return tuple(int(part) for part in re.findall(r"\d+", text)[:2])


def compiler_label(config):
    """The compiler's name in messages: gcc-13, clang-18, ..., or msvc-19.51, since MSVC's limits
    differ between minor versions (ADR-0014)."""
    if config["compiler_id"] == "MSVC":
        major, minor = (version_tuple(config["compiler_version"]) + (0, 0))[:2]
        return f"msvc-{major}.{minor}"
    return config["golden_name"]


def record_key(config):
    """The name a build's results are recorded under: its preset, the name of its build directory,
    such as gcc-13-debug, and on MSVC cl's version too, as toolsets share a preset and their limits
    differ (ADR-0014). Presets of one compiler with other flags or another standard library, such
    as clang-18-libcxx-debug or asan-ubsan, keep records of their own."""
    preset = Path(config["binary_dir"]).name
    if config["compiler_id"] == "MSVC":
        return f"{preset}, cl {compiler_label(config)[len('msvc-'):]}"
    return preset


def required_hops(config):
    """The chain NFR-09 asks of this compiler at its default limits: 200 hops, or 150 on MSVC
    before 19.50 (provisional, ADR-0014 item 3)."""
    if (config["compiler_id"] == "MSVC"
            and version_tuple(config["compiler_version"]) < MSVC_TEMPLATE_DEPTH_SWITCH):
        return NFR09_MSVC_BEFORE_19_50_HOPS
    return NFR09_HOPS


def display(path):
    try:
        return Path(path).resolve().relative_to(ROOT).as_posix()
    except ValueError:
        return str(path)


def shorten(text):
    """TEXT without this repository's path prefix, in either separator style."""
    for root in {str(ROOT), ROOT.as_posix()}:
        text = re.sub(re.escape(root) + r"[\\/]", "", text, flags=re.IGNORECASE)
    return text


# A compiler's error line: GCC's and Clang's "file:line:col: error:", "fatal error:", cl's
# "file(line): error C2338:", "fatal error C1202:", or a driver's "error:".
ERROR_LINE = re.compile(r"(?:^|[\s:])(?:fatal )?error(?: [A-Z]+\d+)?\s*:")
# The compilers' messages for their template depth limits: GCC's -ftemplate-depth (900 by default),
# Clang's (1024), cl 19.5x's /templateDepth (1000; C2999) and cl 19.44's fixed recursion limit
# (C1202). Spike S2 and T1.13 met all four.
DEPTH_LIMIT = re.compile(r"template instantiation depth exceeds maximum"
                         r"|recursive template instantiation exceeded maximum depth"
                         r"|\bC1202\b|\bC2999\b")
# The compilers' other limits, which deep instantiation can reach: Clang's warning that its stack
# is nearly exhausted (-Wstack-exhausted, an error here with the warnings), and cl's stack, heap and
# internal errors.
COMPILER_LIMIT = re.compile(r"stack nearly exhausted|\bC10(?:01|04|60|63)\b")
MEMORY_ERROR = re.compile(r"out of memory|memory exhausted|Cannot allocate memory|bad_alloc|"
                          r"Killed signal|LLVM ERROR", re.IGNORECASE)
# The flags that only add debug information: dropped from cl's compiles (module documentation).
MSVC_DEBUG_FLAGS = re.compile(r"^[/-]Z[iI7]$")


@dataclass
class Result:
    ok: bool
    seconds: float
    kind: str = ""  # why it failed: "depth limit", "compiler limit", "timeout", ... or "error"
    first_error: str = ""
    log: Path = None

    @property
    def limit(self):
        """It failed on a limit of the compiler, not on an error in the program."""
        return not self.ok and self.kind != "error"


class Build:
    """Compiles chains with one build's compiler and flags."""

    def __init__(self, config, config_path):
        self.config = config
        self.config_path = Path(config_path)
        self.label = compiler_label(config)
        self.record_key = record_key(config)
        self.msvc = config["frontend"] == "MSVC"
        self.work = Path(config["binary_dir"]) / "chain_depth" / self.label
        self.env = bench_run.child_environment()
        # Sanitizers reserve far more address space than any cap allows.
        sanitized = any("-fsanitize" in flag for flag in config["flags"])
        self.run_memory_mb = None if sanitized else MEMORY_MB

    def compile(self, shape, hops, link=False):
        """Compile shape's chain of `hops` hops as the test does, or with link=True build, link and
        run it as the self-test does."""
        self.work.mkdir(parents=True, exist_ok=True)
        stem = f"{shape}-{hops}" + ("-run" if link else "")
        source = self.work / f"{stem}.cpp"
        source.write_text(program(shape, hops), encoding="utf-8")
        exe = self.work / (stem + (".exe" if os.name == "nt" else ""))
        if self.msvc:  # untested here: MSVC runs only in CI
            flags = [f for f in self.config["flags"] if not MSVC_DEBUG_FLAGS.match(f)]
            command = [self.config["compiler"], "/nologo", *flags, "/WX", "/bigobj",
                       f"/Fo{self.work / (stem + '.obj')}"]
            command += [f"/Fe{exe}", str(source)] if link else ["/c", str(source)]
        else:
            command = [self.config["compiler"], *self.config["flags"], "-Werror", str(source)]
            command += ["-o", str(exe)] if link else ["-fsyntax-only"]
        built = bench_run.run_command(command, COMPILE_TIMEOUT_S, MEMORY_MB, cwd=self.work,
                                      env=self.env)
        log = self.work / f"{stem}.log"
        output = built.stdout + built.stderr
        log.write_text("$ " + " ".join(built.command) + "\n" + output, encoding="utf-8")
        result = Result(built.ok, built.seconds, log=log)
        if not built.ok:
            result.kind, result.first_error = classify(built, output)
        elif link:
            ran = bench_run.run_command([exe], RUN_TIMEOUT_S, self.run_memory_mb, cwd=self.work,
                                        env=self.env)
            printed = ran.stdout.strip()
            if not ran.ok or printed != expected_line(shape, hops):
                result.ok, result.kind = False, "error"
                result.first_error = (f"the program printed '{printed}' and "
                                      f"{ran.problem(RUN_TIMEOUT_S) or 'exited with 0'}; expected "
                                      f"'{expected_line(shape, hops)}' and 0")
        return result

    def reproduce(self, shape, hops):
        return (f"python3 tests/chain_depth/check_chain_depth.py --config "
                f"{display(self.config_path)} --shapes {shape} --hops {hops}")


def classify(outcome, output):
    """(kind, first error line) of a failed compile."""
    first = next((shorten(line.strip()) for line in output.splitlines() if ERROR_LINE.search(line)),
                 "")
    if outcome.launch_error:
        return "error", f"could not run the compiler: {outcome.launch_error}"
    if outcome.timed_out:
        return "timeout", f"timed out after {COMPILE_TIMEOUT_S} s"
    depth = next((shorten(line.strip()) for line in output.splitlines()
                  if DEPTH_LIMIT.search(line)), "")
    if depth:
        return "depth limit", depth
    if COMPILER_LIMIT.search(output):
        return "compiler limit", first
    if MEMORY_ERROR.search(output):
        return "memory", first or f"ran out of memory under the {MEMORY_MB} MB cap"
    if outcome.returncode is not None and outcome.returncode < 0:
        return "crash", first or f"the compiler was killed by signal {-outcome.returncode}"
    return "error", first or f"the compiler exited with status {outcome.returncode}"


# ---- the record -------------------------------------------------------------------------------

RECORD_ABOUT = (
    "The deepest dependency chain the static engine compiles at each compiler's default template "
    "depth, per shape: written by tests/chain_depth/check_chain_depth.py --deepest --record (task "
    "T1.11), which documents the shapes and how a chain is compiled, under each preset's name, "
    "with cl's version on MSVC. NFR-09 asks for 200 hops, 150 "
    "on MSVC before 19.50 (ADR-0014). 'deepest' compiles and 'deepest' + 1 doesn't, stopped as "
    "'stopped_by' says. 'include' is the tree id of include/ at the measurement, which a rebase "
    "or a squash keeps, unlike 'commit': the tests say when include/ is no longer that tree. "
    "Re-measure after changing include/trellis/static/ or include/trellis/discovery/.")


def read_record():
    try:
        return json.loads(RECORD.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return {}


def recorded(key, shape):
    """(the deepest chain, the commit, the include/ tree) recorded under a record_key for a shape,
    or None."""
    entry = read_record().get("presets", {}).get(key)
    if not entry or shape not in entry.get("shapes", {}):
        return None
    return entry["shapes"][shape]["deepest"], entry.get("commit", "?"), entry.get("include", "?")


def git(*args):
    """The output of a git command in this repository, or "" without git or a repository."""
    try:
        return subprocess.run(["git", "-C", str(ROOT), *args], capture_output=True, text=True,
                              timeout=30).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        return ""


def dirty():
    """"+" when include/ has uncommitted changes, else ""."""
    return "+" if git("status", "--porcelain", "--", "include") else ""


def commit():
    """HEAD's short hash, with a + when include/ has uncommitted changes."""
    return (git("rev-parse", "--short=7", "HEAD") or "unknown") + dirty()


def include_tree():
    """The tree id of include/ at HEAD, with a + when include/ has uncommitted changes: what a
    record was measured on. A rebase or a squash keeps it, as long as include/ is the same."""
    tree = git("rev-parse", "--short=12", "HEAD:include")
    return tree + dirty() if tree else "unknown"


def write_record(build, results):
    data = read_record()
    presets = data.get("presets", {})
    entry = presets.get(build.record_key, {})
    shapes = entry.get("shapes", {})
    for shape, (deepest, stopped_by) in results.items():
        shapes[shape] = {"deepest": deepest, "stopped_by": stopped_by}
    presets[build.record_key] = {
        "compiler": f"{build.config['compiler_id']} {build.config['compiler_version']}",
        "flags": shorten(" ".join(build.config["flags"])),
        "commit": commit(), "include": include_tree(), "date": date.today().isoformat(),
        "shapes": {name: shapes[name] for name in SHAPES if name in shapes}}
    record = {"about": RECORD_ABOUT, "presets": {key: presets[key] for key in sorted(presets)}}
    RECORD.write_text(json.dumps(record, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


# ---- the modes --------------------------------------------------------------------------------

print_lock = threading.Lock()


def say(*lines):
    with print_lock:
        for line in lines:
            print(line, flush=True)


def annotate(title, message):
    """A notice annotation in GitHub Actions, whose job logs scripts/ci-status.sh can't always
    read. The workflow-command escapes: % and line breaks everywhere, : and , in a property."""
    if os.environ.get("GITHUB_ACTIONS") == "true":
        def escape(text, prop=False):
            text = text.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")
            return text.replace(":", "%3A").replace(",", "%2C") if prop else text
        say(f"::notice title={escape(title, prop=True)}::{escape(message)}")


def for_each(function, shapes, jobs):
    """function(shape) for each shape, JOBS at a time; the number of shapes it returned False for.
    With one job, in this thread, so that Ctrl-C or SIGTERM reaches run_command(), which then stops
    the compiler; with more, a compile under way at that moment runs to its end."""
    if jobs == 1:
        return sum(not function(shape) for shape in shapes)
    with ThreadPoolExecutor(max_workers=jobs) as pool:
        return sum(not ok for ok in pool.map(function, shapes))


def check_shape(build, shape, hops):
    """The test proper for one shape. Returns True when the chain compiles."""
    result = build.compile(shape, hops)
    name = f"chain_depth.{shape} ({build.label})"
    if result.ok:
        record = recorded(build.record_key, shape)
        if not record:
            deepest = f"; no deepest chain recorded for {build.record_key}"
        elif record[2] == include_tree() and not record[2].endswith("+"):
            deepest = f"; the deepest recorded: {record[0]} hops, for this include/"
        else:
            deepest = (f"; the deepest recorded: {record[0]} hops, for the include/ of "
                       f"{record[1]}, not this one: re-measure with --deepest --record")
        say(f"PASS {name}: a chain of {hops} hops compiles at the default template depth "
            f"({result.seconds:.1f} s){deepest}")
        annotate(f"chain_depth.{shape} ({build.label})",
                 f"{hops} hops compile at the default template depth ({result.seconds:.0f} s)")
        return True
    say(f"FAIL {name}: a chain of {hops} hops doesn't compile at the default template depth "
        f"({result.kind}): {result.first_error}",
        f"  {SHAPES[shape].summary}; the source and the compiler output: "
        f"{display(result.log.with_suffix('.cpp'))}, {display(result.log)}",
        f"  reproduce: {build.reproduce(shape, hops)}")
    return False


def deepest_chain(build, shape, start):
    """(the longest chain that compiles, why one hop more doesn't), or raises RuntimeError if a
    compile fails for another reason than a compiler limit."""
    cache = {}

    def compiles(n):
        if n not in cache:
            result = build.compile(shape, n)
            if not result.ok and not result.limit:
                raise RuntimeError(f"{n} hops failed with an error, not a compiler limit: "
                                   f"{result.first_error} (log {display(result.log)})")
            cache[n] = result
        return cache[n].ok

    record = recorded(build.record_key, shape)
    if record and 1 <= record[0] < MAX_HOPS and compiles(record[0]) and not compiles(record[0] + 1):
        lo, hi = record[0], record[0] + 1
    elif compiles(start):
        lo, hi = start, None
        while hi is None:
            if lo >= MAX_HOPS:
                return lo, f"compiles at {MAX_HOPS} hops, where the search stops"
            n = min(2 * lo, MAX_HOPS)
            if compiles(n):
                lo = n
            else:
                hi = n
    else:
        lo, hi = 0, start
    while hi - lo > 1:
        mid = (lo + hi) // 2
        if compiles(mid):
            lo = mid
        else:
            hi = mid
    stop = cache[hi]
    return lo, f"{stop.kind}: {stop.first_error}" if stop.first_error else stop.kind


def run_deepest(build, shapes, start, jobs, record):
    results = {}

    def one(shape):
        try:
            deepest, stopped_by = deepest_chain(build, shape, start)
        except RuntimeError as error:
            say(f"FAIL chain_depth.{shape} ({build.label}) --deepest: {error}")
            return False
        results[shape] = (deepest, stopped_by)
        say(f"deepest chain_depth.{shape} ({build.label}): {deepest} hops; one more stops with "
            f"{stopped_by}")
        annotate(f"chain_depth.{shape} ({build.label})",
                 f"the deepest chain at the default limits: {deepest} hops")
        return True

    failed = for_each(one, shapes, jobs)
    if record and results:
        write_record(build, results)
        say(f"recorded {len(results)} shapes for {build.record_key} in {display(RECORD)}")
    return failed


def run_self_test(build, shapes, jobs):
    def one(shape):
        hops = SHAPES[shape].self_test_hops
        result = build.compile(shape, hops, link=True)
        if result.ok:
            say(f"PASS chain_depth.selftest {shape} ({build.label}): "
                f"'{expected_line(shape, hops)}'")
            return True
        say(f"FAIL chain_depth.selftest {shape} ({build.label}): {result.kind}: "
            f"{result.first_error} (log {display(result.log)})")
        return False

    return for_each(one, shapes, jobs)


def parse_shapes(text):
    names = list(SHAPES) if text == "all" else [s.strip() for s in text.split(",") if s.strip()]
    unknown = [n for n in names if n not in SHAPES]
    if unknown or not names:
        raise ValueError(f"unknown shapes {', '.join(unknown) or text!r}; known: "
                         f"{', '.join(SHAPES)}")
    return names


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    where = parser.add_mutually_exclusive_group()
    where.add_argument("--preset", help="use build/<preset>/" + CONFIG_NAME)
    where.add_argument("--config", help="path to a " + CONFIG_NAME)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--deepest", action="store_true",
                      help="find the longest chain that compiles, per shape")
    mode.add_argument("--self-test", action="store_true",
                      help="build, link and run each shape's chain at a few hops")
    mode.add_argument("--program", nargs=2, metavar=("SHAPE", "HOPS"),
                      help="print one chain's program")
    mode.add_argument("--list-shapes", action="store_true", help="print the shapes")
    parser.add_argument("--shapes", default="all", help="comma-separated shapes (default: all)")
    parser.add_argument("--hops", type=int,
                        help="the chain's length (default: what NFR-09 asks of the compiler)")
    parser.add_argument("--record", action="store_true",
                        help=f"with --deepest: write the results into {display(RECORD)}")
    parser.add_argument("--jobs", type=int, default=1, help="shapes compiled at a time")
    args = parser.parse_args(argv)
    bench_run.exit_on_termination_signals()

    if args.list_shapes:
        for shape in SHAPES.values():
            print(f"{shape.name}: {shape.summary}")
        return 0
    if args.program:
        shape, hops = args.program
        if shape not in SHAPES or not hops.isdigit() or int(hops) < 1:
            parser.error(f"--program takes a shape ({', '.join(SHAPES)}) and a number of hops")
        sys.stdout.write(program(shape, int(hops)))
        return 0
    if not (args.preset or args.config):
        parser.error("give --preset or --config")
    if args.record and not args.deepest:
        parser.error("--record goes with --deepest")
    if args.hops is not None and args.hops < 1:
        parser.error("--hops must be at least 1")
    if args.jobs < 1:
        parser.error("--jobs must be at least 1")
    try:
        shapes = parse_shapes(args.shapes)
    except ValueError as error:
        parser.error(str(error))
    path = Path(args.config) if args.config else ROOT / "build" / args.preset / CONFIG_NAME
    try:
        build = Build(load_config(path), path)
    except ConfigError as error:
        print(f"check_chain_depth.py: error: {error}", file=sys.stderr)
        return 2

    if args.self_test:
        failed = run_self_test(build, shapes, args.jobs)
    elif args.deepest:
        failed = run_deepest(build, shapes, args.hops or required_hops(build.config), args.jobs,
                             args.record)
    else:
        hops = args.hops or required_hops(build.config)
        failed = for_each(lambda shape: check_shape(build, shape, hops), shapes, args.jobs)
    if len(shapes) > 1:
        what = "--deepest" if args.deepest else "--self-test" if args.self_test else "test"
        say(f"chain_depth {what} ({build.label}): {len(shapes) - failed} of {len(shapes)} shapes "
            f"passed")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:  # Ctrl-C: the compiler is already stopped
        print("interrupted", file=sys.stderr)
        sys.exit(130)
