#!/usr/bin/env python3
"""Compile-time benchmarks (plan §8.4): compile the scenario program and generated graphs, one at a
time, and record what each compile costs.

  run.py [--quick] [--n LIST] [--fan-in LIST] [--forms LIST] [--compilers LIST]
         [--variants LIST] [--runs 3] [--timeout SECONDS] [--memory-mb 6000] [--no-scenario]
         [--no-graphs] [--out DIR]

--quick means --n 10,50,100 --fan-in 1-3 --compilers gcc-13,clang-18. Without it the defaults are
the sweep of plan §8.4: N = 10, 25, 50, 100, 250, 500 and 1000, fan-in 1-3 and 10, the same two
compilers. LIST is comma-separated; a compiler is gcc-13, gcc-14, clang-18 or clang-20, or
NAME=EXECUTABLE.

The programs, for each compiler:
  scenario  benchmarks/scenario/<variant>.cpp, compiled as the research's compile matrix was
            (-std=c++20 -O2 -Wall -Wextra): the baseline of NFR-03 and NFR-06. It must print
            "<variant>: wiring_errors=0 handled=6 log_lines=8 repo_size=6".
  graph     a program from gen_graph.py for each N, fan-in profile and request-form set,
            compiled as the research's scaling phase was (-std=c++20 -O2): the baseline of
            NFR-04 and NFR-05. It must print the identity line gen_graph.py predicts.
Each program is compiled --runs times (the first compile also checks that it builds; after a
compile longer than 60 s one sample is enough), then linked, stripped and run. Before the first
measured compile of each compiler, one untimed compile loads the compiler and the standard
headers into the page cache.

Measured: the median wall time of the compiles (with min and max, and the median CPU time, user
plus system, which a busy machine disturbs less), the compiler's peak RSS (from wait4, which
includes cc1plus or clang -cc1 under the driver), and the sizes of the object file, the binary,
the stripped binary (strip --strip-all) and the binary's text: code and read-only data, as `size`
reports it, which shows small changes that the stripped size, rounded to 4 KiB pages, hides.

Isolation: the compilers are called directly, never through ccache (a ccache link found in PATH,
as in /usr/lib/ccache, is skipped, and CCACHE_DISABLE=1 is set), one compile at a time, each in a
new process group under an address-space cap (--memory-mb, RLIMIT_AS) and a timeout that kills the
whole group (--timeout: 300 s with --quick; 900 s for the full sweep, whose hand-wired graph of
1,000 components with fan-in 10 takes more than 300 s with GCC 13 on a 4-vCPU machine). A compile
that hits a cap is recorded as a failed result, not retried.

Output, under --out (default build/bench/compile/): results.json (every record, the machine and
the settings; rewritten after every program, so an interrupted run keeps what it measured),
summary.txt (the table printed at the end), and work/ (generated sources, objects, binaries and
the compiler output of failed steps).

Budgets: benchmarks/compile/budgets.json holds NFR-03, NFR-04 and NFR-05, relative to the
hand-written wiring ("baseline"). Every other variant is checked against the baseline measured in
the same run, for the same compiler and program.

Exit status: 0 when every program built, ran and printed the expected output and no budget was
missed; 1 otherwise; 2 for a usage error.
"""
import argparse
import importlib.util
import json
import os
import platform
import re
import shutil
import signal
import statistics
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

sys.dont_write_bytecode = True  # keep the source tree free of __pycache__

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
SCENARIO_DIR = ROOT / "benchmarks" / "scenario"
BUDGETS_FILE = HERE / "budgets.json"
DEFAULT_OUT = ROOT / "build" / "bench" / "compile"

COMPILERS = {"gcc-13": "g++-13", "gcc-14": "g++-14", "clang-18": "clang++-18",
             "clang-20": "clang++-20"}
SCENARIO_FLAGS = ["-std=c++20", "-O2", "-Wall", "-Wextra"]  # the research's compile matrix
GRAPH_FLAGS = ["-std=c++20", "-O2"]  # the research's scaling phase
LINK_FLAGS = ["-O2"]
# The full sweep's timeout is longer because the largest hand-wired graph (N = 1000, fan-in 10) takes
# more than 300 s with GCC 13 on the 4-vCPU development machine: a timeout is meant to stop runaway
# compiles, not the baseline.
QUICK = {"n": "10,50,100", "fan_in": "1-3", "compilers": "gcc-13,clang-18", "timeout": 300}
FULL = {"n": "10,25,50,100,250,500,1000", "fan_in": "1-3,10", "compilers": "gcc-13,clang-18",
        "timeout": 900}
LONG_COMPILE_S = 60
RUN_TIMEOUT_S = 60
SCENARIO_OUTPUT = "{variant}: wiring_errors=0 handled=6 log_lines=8 repo_size=6"
# A compiler that ran out of memory under the cap, or was killed (as research/experiments did).
MEMORY_ERROR = re.compile(r"out of memory|memory exhausted|Cannot allocate memory|bad_alloc|"
                          r"Killed signal|LLVM ERROR", re.IGNORECASE)


def load_generator():
    spec = importlib.util.spec_from_file_location("gen_graph", HERE / "gen_graph.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


gen_graph = load_generator()


# Running one command: the same discipline as tools/compile_fail.py (a new process group, killed
# as a whole on timeout or when this script stops), plus wait4 for the peak RSS and CPU time.

class Outcome:
    """What one command did. Times are seconds; peak_rss_kb is in KiB (Linux's ru_maxrss)."""

    def __init__(self, command, returncode=None, seconds=0.0, cpu_seconds=0.0, peak_rss_kb=0,
                 stdout="", stderr="", timed_out=False, launch_error=None):
        self.command = [str(part) for part in command]
        self.returncode = returncode
        self.seconds = seconds
        self.cpu_seconds = cpu_seconds
        self.peak_rss_kb = peak_rss_kb
        self.stdout = stdout
        self.stderr = stderr
        self.timed_out = timed_out
        self.launch_error = launch_error

    @property
    def ok(self):
        return self.launch_error is None and not self.timed_out and self.returncode == 0

    def problem(self, timeout=None, memory_mb=None):
        """Why the command failed, in a few words, or None if it succeeded."""
        if self.launch_error:
            return f"could not run {self.command[0]}: {self.launch_error}"
        if self.timed_out:
            return f"timed out after {timeout} s"
        if self.returncode == 0:
            return None
        if self.returncode < 0:
            reason = f"killed by signal {-self.returncode}"
        else:
            reason = f"exit status {self.returncode}"
        if memory_mb and MEMORY_ERROR.search(self.stderr):
            reason += f", out of memory under the {memory_mb} MB address-space cap"
        first = next((line for line in self.stderr.splitlines() if "error" in line.lower()), "")
        return f"{reason}: {first.strip()[:200]}" if first else reason


def _memory_limiter(memory_mb):
    """A preexec_fn that caps the child's address space, or None where that isn't possible. An
    existing, lower hard limit (an outer `ulimit -v`) stays in force."""
    if os.name != "posix" or not memory_mb:
        return None
    import resource  # POSIX only

    def limit():
        cap = memory_mb * 1024 * 1024
        try:
            _, hard = resource.getrlimit(resource.RLIMIT_AS)
            if hard != resource.RLIM_INFINITY:
                cap = min(cap, hard)
            resource.setrlimit(resource.RLIMIT_AS, (cap, cap))
        except (ValueError, OSError):
            pass  # some systems (macOS) don't support the limit; run without it

    return limit


def _kill_group(proc):
    if os.name == "posix":
        try:
            os.killpg(proc.pid, signal.SIGKILL)
        except (ProcessLookupError, PermissionError):
            pass  # already gone
    else:
        proc.kill()


def _exit_code(status):
    if hasattr(os, "waitstatus_to_exitcode"):  # Python 3.9 and later
        return os.waitstatus_to_exitcode(status)
    return -os.WTERMSIG(status) if os.WIFSIGNALED(status) else os.WEXITSTATUS(status)


def _read(handle):
    handle.seek(0)
    return handle.read().decode("utf-8", errors="replace").replace("\r\n", "\n")


def run_command(command, timeout=None, memory_mb=None, cwd=None, env=None):
    """Run COMMAND to completion and measure it. Output goes to temporary files, not pipes, so a
    compiler that prints a lot can't block. On POSIX the command leads a new process group, which
    a timeout, Ctrl-C, SIGTERM or SIGHUP kills as a whole, so that no cc1plus runs on."""
    command = [str(part) for part in command]
    with tempfile.TemporaryFile() as out, tempfile.TemporaryFile() as err:
        started = time.perf_counter()
        try:
            proc = subprocess.Popen(command, cwd=cwd, env=env, stdin=subprocess.DEVNULL,
                                    stdout=out, stderr=err, preexec_fn=_memory_limiter(memory_mb),
                                    start_new_session=os.name == "posix")
        except OSError as error:
            return Outcome(command, launch_error=str(error))
        if os.name != "posix":
            timed_out = False
            try:
                proc.wait(timeout=timeout)
            except subprocess.TimeoutExpired:
                _kill_group(proc)
                proc.wait()
                timed_out = True
            except BaseException:
                _kill_group(proc)
                proc.wait()
                raise
            return Outcome(command, proc.returncode, time.perf_counter() - started,
                           stdout=_read(out), stderr=_read(err), timed_out=timed_out)
        expired = threading.Event()

        def expire():
            expired.set()
            _kill_group(proc)

        timer = threading.Timer(timeout, expire) if timeout else None
        if timer:
            timer.daemon = True
            timer.start()
        try:
            # Wait for the exit without reaping, so that the timer can't kill a reused process
            # group: the group id stays taken until wait4 reaps the leader.
            if hasattr(os, "waitid"):
                os.waitid(os.P_PID, proc.pid, os.WEXITED | os.WNOWAIT)
            seconds = time.perf_counter() - started
            if timer:
                timer.cancel()
                timer.join()
            _, status, usage = os.wait4(proc.pid, 0)
        except BaseException:
            # This script is stopping (Ctrl-C, or SIGTERM or SIGHUP turned into SystemExit by
            # exit_on_termination_signals). The signal didn't reach the command's own process
            # group, so take it down, and reap it, before going.
            if timer:
                timer.cancel()
            _kill_group(proc)
            try:
                os.waitpid(proc.pid, 0)
            except ChildProcessError:
                pass
            proc.returncode = -signal.SIGKILL
            raise
        proc.returncode = _exit_code(status)  # reaped here, so Popen must not wait again
        return Outcome(command, proc.returncode, seconds, usage.ru_utime + usage.ru_stime,
                       usage.ru_maxrss, _read(out), _read(err), expired.is_set())


def exit_on_termination_signals():
    """Make SIGTERM and SIGHUP raise SystemExit, as SIGINT raises KeyboardInterrupt, so that
    run_command() can stop the command before this script exits (status 128 + signal)."""

    def stop(signum, _frame):
        raise SystemExit(128 + signum)

    for name in ("SIGTERM", "SIGHUP"):
        if hasattr(signal, name):
            signal.signal(getattr(signal, name), stop)


def child_environment():
    env = dict(os.environ, LC_ALL="C", LANG="C", CCACHE_DISABLE="1")
    env.pop("LANGUAGE", None)
    return env


def resolve_compiler(spec):
    """(name, absolute path) for "gcc-13" and the like, or for "NAME=EXECUTABLE". PATH entries
    that are ccache links (/usr/lib/ccache/g++-13 -> ccache) are skipped."""
    name, _, executable = spec.partition("=")
    executable = executable or COMPILERS.get(name)
    if not executable:
        raise ValueError(f"unknown compiler {name!r}: use {', '.join(COMPILERS)} or NAME=EXECUTABLE")
    if os.sep in executable:
        candidates = [Path(executable)]
    else:
        candidates = [Path(d) / executable for d in os.environ.get("PATH", "").split(os.pathsep) if d]
    for candidate in candidates:
        if candidate.is_file() and os.access(candidate, os.X_OK):
            if Path(os.path.realpath(candidate)).name.startswith("ccache"):
                continue
            # Keep the name, not the symlink's target: clang++-18 links to clang, which links as
            # C (without the C++ standard library) when called by that name.
            return name, os.path.abspath(candidate)
    raise ValueError(f"{executable} not found (ccache links don't count)")


def first_line(command):
    try:
        output = subprocess.run(command, capture_output=True, text=True, timeout=30).stdout
        return output.splitlines()[0] if output else ""
    except (OSError, subprocess.SubprocessError):
        return ""


def environment(compilers):
    cpu = ""
    try:
        cpu = next((line.split(":", 1)[1].strip() for line in
                    Path("/proc/cpuinfo").read_text().splitlines() if line.startswith("model name")),
                   "")
    except OSError:
        pass
    return {
        "date_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "uname": " ".join(platform.uname()),
        "cpu": cpu,
        "nproc": os.cpu_count(),
        "loadavg_at_start": os.getloadavg() if hasattr(os, "getloadavg") else None,
        "python": platform.python_version(),
        "compilers": {name: {"path": path, "version": first_line([path, "--version"])}
                      for name, path in compilers},
        "flags": {"scenario": SCENARIO_FLAGS, "graph": GRAPH_FLAGS, "link": LINK_FLAGS},
    }


# Measuring one program.

def relative(path):
    try:
        return Path(path).resolve().relative_to(ROOT).as_posix()
    except ValueError:
        return str(path)


def text_size(binary):
    """The text size from `size` in Berkeley format (code and read-only data), or None."""
    if not shutil.which("size"):
        return None
    out = run_command(["size", "--format=berkeley", binary], timeout=RUN_TIMEOUT_S)
    lines = out.stdout.splitlines()
    if not out.ok or len(lines) < 2:
        return None
    try:
        return int(lines[1].split()[0])
    except (ValueError, IndexError):
        return None


def measure(record, compiler, source, flags, include_dirs, expected, work, args):
    """Compile, link, strip and run one program, filling in RECORD. Returns RECORD."""
    stem = source.stem
    obj, exe, stripped = work / f"{stem}.o", work / stem, work / f"{stem}.stripped"
    command = [compiler, *flags, *(f"-I{ROOT / d}" for d in include_dirs), "-c", source, "-o", obj]
    record["source"] = relative(source)
    record["command"] = " ".join(str(c) for c in command).replace(str(ROOT) + "/", "")
    env = child_environment()
    times, cpu_times, peaks = [], [], []
    for _ in range(args.runs):
        out = run_command(command, args.timeout, args.memory_mb, cwd=work, env=env)
        if not out.ok:
            log = work / f"{stem}.compile.log"
            log.write_text(out.stderr, encoding="utf-8")
            record["error"] = f"compile: {out.problem(args.timeout, args.memory_mb)} (log {relative(log)})"
            record["failed_after_s"] = round(out.seconds, 2)
            if not out.timed_out:  # after a timeout kill, the driver never reaped cc1plus
                record["peak_rss_mb_at_failure"] = round(out.peak_rss_kb / 1024, 1)
            return record
        times.append(out.seconds)
        cpu_times.append(out.cpu_seconds)
        peaks.append(out.peak_rss_kb)
        if out.seconds > LONG_COMPILE_S:
            break
    record["compile_s"] = {"median": round(statistics.median(times), 3),
                           "min": round(min(times), 3), "max": round(max(times), 3),
                           "runs": len(times)}
    record["cpu_s"] = round(statistics.median(cpu_times), 3)
    record["peak_rss_mb"] = round(max(peaks) / 1024, 1)
    record["object_bytes"] = obj.stat().st_size
    link = run_command([compiler, *LINK_FLAGS, obj, "-o", exe], args.timeout, args.memory_mb,
                       cwd=work, env=env)
    if not link.ok:
        record["error"] = f"link: {link.problem(args.timeout, args.memory_mb)}"
        return record
    record["binary_bytes"] = exe.stat().st_size
    shutil.copyfile(exe, stripped)
    strip = run_command(["strip", "--strip-all", stripped], timeout=RUN_TIMEOUT_S)
    record["stripped_bytes"] = stripped.stat().st_size if strip.ok else None
    record["text_bytes"] = text_size(exe)
    result = run_command([exe], RUN_TIMEOUT_S, args.memory_mb, cwd=work, env=env)
    record["output_ok"] = result.ok and result.stdout.strip() == expected.strip()
    if not record["output_ok"]:
        got = result.problem(RUN_TIMEOUT_S) or result.stdout.strip()[:200]
        record["error"] = f"output: expected {expected.strip()!r}, got {got!r}"
    return record


def measured(record):
    return "compile_s" in record and record.get("output_ok") and "error" not in record


# Budgets.

def load_budgets(path=BUDGETS_FILE):
    """budgets.json, checked for the fields the checker reads. Raises ValueError if malformed."""
    try:
        data = json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        raise ValueError(f"cannot read {relative(path)}: {error}") from error
    if not isinstance(data.get("baseline"), str) or not isinstance(data.get("budgets"), list):
        raise ValueError(f"{relative(path)}: needs a 'baseline' variant and a 'budgets' list")
    for budget in data["budgets"]:
        where = f"{relative(path)}: budget {budget.get('id', '?')}"
        if budget.get("program") not in ("scenario", "graph"):
            raise ValueError(f"{where}: 'program' must be 'scenario' or 'graph'")
        if budget.get("measure") not in ("compile_s", "peak_rss_mb"):
            raise ValueError(f"{where}: 'measure' must be 'compile_s' or 'peak_rss_mb'")
        if "max_extra" in budget:
            if not isinstance(budget["max_extra"], (int, float)) or budget["max_extra"] < 0:
                raise ValueError(f"{where}: 'max_extra' must be a non-negative number")
        elif isinstance(budget.get("checkpoints"), list) and budget["checkpoints"]:
            for point in budget["checkpoints"]:
                if not isinstance(point.get("n"), int) or not isinstance(point.get("max_ratio"), (int, float)):
                    raise ValueError(f"{where}: each checkpoint needs an integer 'n' and a 'max_ratio'")
        else:
            raise ValueError(f"{where}: needs 'max_extra' or a non-empty 'checkpoints' list")
    return data


def _value(record, measure_name):
    return record["compile_s"]["median"] if measure_name == "compile_s" else record["peak_rss_mb"]


def check_budgets(records, budgets):
    """One entry per (budget, variant record) that a budget covers, with the measured value."""
    baseline = budgets["baseline"]

    def key(r):
        return (r["program"], r["compiler"], r.get("n"), r.get("fan_in"), r.get("forms"),
                r.get("seed"))

    bases = {key(r): r for r in records if r["variant"] == baseline and measured(r)}
    checks = []
    for budget in budgets["budgets"]:
        for r in records:
            if r["variant"] == baseline or r["program"] != budget["program"]:
                continue
            if "max_extra" in budget:
                limit, kind = budget["max_extra"], "extra"
            else:
                point = next((p for p in budget["checkpoints"] if p["n"] == r.get("n")), None)
                if point is None:
                    continue  # no target at this size
                limit, kind = point["max_ratio"], "ratio"
            check = {"budget": budget["id"], "variant": r["variant"], "baseline": baseline,
                     "compiler": r["compiler"], "program": r["program"], "n": r.get("n"),
                     "fan_in": r.get("fan_in"), "forms": r.get("forms"),
                     "measure": budget["measure"], "limit": limit, "kind": kind}
            base = bases.get(key(r))
            if base is None or not measured(r):
                check.update(ok=False, note="not measured (the program or its baseline failed)")
            else:
                value, reference = _value(r, budget["measure"]), _value(base, budget["measure"])
                result = value - reference if kind == "extra" else value / reference
                check.update(value=round(result, 3), ok=result <= limit)
            checks.append(check)
    return checks


def describe_check(c):
    where = f"{c['compiler']} {c['program']}"
    if c["program"] == "graph":
        where += f" n={c['n']} fan-in {c['fan_in']} {c['forms']}"
    if "value" not in c:
        return f"{'ok  ' if c['ok'] else 'MISS'} {c['budget']} {c['variant']} {where}: {c['note']}"
    unit = " s" if c["measure"] == "compile_s" else " MB"
    shown = (f"{c['value']:+.2f}{unit} (max +{c['limit']}{unit})" if c["kind"] == "extra"
             else f"{c['value']:.2f}x (max {c['limit']}x)")
    return (f"{'ok  ' if c['ok'] else 'MISS'} {c['budget']} {c['variant']} {where}: "
            f"{c['measure']} {shown} against {c['baseline']}")


# The table.

def table(records):
    header = ["compiler", "program", "variant", "N", "fan-in", "forms", "compile s", "min-max",
              "cpu s", "peak MB", "object KB", "stripped KB", "text KB", "result"]
    rows = []
    for r in records:
        c = r.get("compile_s", {})

        def kb(field):
            return f"{r[field] / 1024:.1f}" if r.get(field) is not None else "-"

        rows.append([r["compiler"], r["program"], r["variant"], str(r.get("n", "-")),
                     r.get("fan_in", "-"), r.get("forms", "-"),
                     f"{c['median']:.2f}" if c else "-",
                     f"{c['min']:.2f}-{c['max']:.2f}" if c else "-",
                     f"{r['cpu_s']:.2f}" if "cpu_s" in r else "-",
                     f"{r['peak_rss_mb']:.1f}" if "peak_rss_mb" in r else "-",
                     kb("object_bytes"), kb("stripped_bytes"), kb("text_bytes"),
                     "ok" if measured(r) else "FAIL"])
    widths = [max(len(row[i]) for row in [header] + rows) for i in range(len(header))]
    return "\n".join("  ".join(cell.ljust(w) for cell, w in zip(row, widths)).rstrip()
                     for row in [header] + rows) + "\n"


def parse_list(text):
    return [item.strip() for item in text.split(",") if item.strip()]


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--quick", action="store_true",
                        help="N = 10, 50 and 100, fan-in 1-3, gcc-13 and clang-18")
    parser.add_argument("--n", help=f"graph sizes (default {FULL['n']})")
    parser.add_argument("--fan-in", help=f"fan-in profiles: {', '.join(gen_graph.FAN_INS)} "
                                         f"(default {FULL['fan_in']})")
    parser.add_argument("--forms", default="shared_ptr",
                        help=f"request-form sets: {', '.join(gen_graph.FORM_SETS)} (default shared_ptr)")
    parser.add_argument("--seed", type=int, default=gen_graph.RESEARCH_SEED)
    parser.add_argument("--compilers", help=f"default {FULL['compilers']}")
    parser.add_argument("--variants", default="manual",
                        help=f"wirings to measure: {', '.join(gen_graph.VARIANTS)} (default manual)")
    parser.add_argument("--runs", type=int, default=3, help="compiles per program (default 3)")
    parser.add_argument("--timeout", type=int,
                        help=f"seconds per compile (default {QUICK['timeout']} with --quick, "
                             f"else {FULL['timeout']})")
    parser.add_argument("--memory-mb", type=int, default=6000,
                        help="address-space cap per compile in MiB, 0 for none (default 6000)")
    parser.add_argument("--no-scenario", action="store_true", help="skip the scenario program")
    parser.add_argument("--no-graphs", action="store_true", help="skip the generated graphs")
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT)
    args = parser.parse_args(argv)
    exit_on_termination_signals()

    defaults = QUICK if args.quick else FULL
    if args.timeout is None:
        args.timeout = defaults["timeout"]
    try:
        sizes = [int(x) for x in parse_list(args.n or defaults["n"])]
        fan_ins = parse_list(args.fan_in or defaults["fan_in"])
        form_sets = parse_list(args.forms)
        variants = parse_list(args.variants)
        compilers = [resolve_compiler(c) for c in parse_list(args.compilers or defaults["compilers"])]
        for fan_in in fan_ins:
            if fan_in not in gen_graph.FAN_INS:
                raise ValueError(f"unknown fan-in {fan_in!r}")
        for forms in form_sets:
            if forms not in gen_graph.FORM_SETS:
                raise ValueError(f"unknown request forms {forms!r}")
        for variant in variants:
            gen_graph.variant_of(variant)
            if not args.no_scenario and not (SCENARIO_DIR / f"{variant}.cpp").is_file():
                raise ValueError(f"no scenario program {relative(SCENARIO_DIR / f'{variant}.cpp')}")
        if min(sizes, default=1) < 1 or args.runs < 1:
            raise ValueError("sizes and --runs must be at least 1")
        budgets = load_budgets()
    except ValueError as error:
        print(f"error: {error}", file=sys.stderr)
        return 2

    out = args.out.resolve()
    work = out / "work"
    (work / "src").mkdir(parents=True, exist_ok=True)
    for stale in ("results.json", "summary.txt"):
        (out / stale).unlink(missing_ok=True)
    data = {"env": environment(compilers),
            "settings": {"quick": args.quick, "runs": args.runs, "timeout_s": args.timeout,
                         "memory_mb": args.memory_mb, "seed": args.seed},
            "records": [], "budget_checks": []}

    def save():
        (out / "results.json").write_text(json.dumps(data, indent=1) + "\n", encoding="utf-8")

    # The programs, generated once and compiled with every compiler.
    programs = []
    if not args.no_scenario:
        for variant in variants:
            programs.append(({"program": "scenario", "variant": variant},
                             SCENARIO_DIR / f"{variant}.cpp", SCENARIO_FLAGS,
                             SCENARIO_OUTPUT.format(variant=variant)))
    if not args.no_graphs:
        for fan_in in fan_ins:
            for forms in form_sets:
                for n in sizes:
                    graph = gen_graph.make_graph(n, fan_in, forms, args.seed)
                    for variant in variants:
                        source = work / "src" / f"graph-{variant}-{graph.name}.cpp"
                        source.write_text(gen_graph.program([graph], variant), encoding="utf-8")
                        programs.append(({"program": "graph", "variant": variant, "n": n,
                                          "fan_in": fan_in, "forms": forms, "seed": args.seed},
                                         source, GRAPH_FLAGS, gen_graph.identity(graph)))
    if not programs:
        print("error: nothing to measure", file=sys.stderr)
        return 2

    save()
    for name, path in compilers:
        compiler_work = work / name
        compiler_work.mkdir(parents=True, exist_ok=True)
        _, warm_source, warm_flags, _ = programs[0]
        run_command([path, *warm_flags, "-c", warm_source, "-o", compiler_work / "warm-up.o"],
                    args.timeout, args.memory_mb, cwd=compiler_work, env=child_environment())
        for fields, source, flags, expected in programs:
            record = {**fields, "compiler": name}
            include_dirs = gen_graph.variant_of(fields["variant"]).include_dirs
            measure(record, path, source, flags, include_dirs, expected, compiler_work, args)
            data["records"].append(record)
            save()
            label = fields["program"] if fields["program"] == "scenario" else (
                f"graph n={fields['n']} fan-in {fields['fan_in']} {fields['forms']}")
            if measured(record):
                print(f"{name} {label} {fields['variant']}: {record['compile_s']['median']:.2f} s "
                      f"(cpu {record['cpu_s']:.2f} s), {record['peak_rss_mb']:.1f} MB, "
                      f"stripped {record.get('stripped_bytes') or '?'} bytes, output ok", flush=True)
            else:
                print(f"{name} {label} {fields['variant']}: FAILED: {record['error']}", flush=True)

    checks = check_budgets(data["records"], budgets)
    data["budget_checks"] = checks
    save()
    summary = table(data["records"])
    if checks:
        summary += "\nbudgets (benchmarks/compile/budgets.json):\n"
        summary += "".join(describe_check(c) + "\n" for c in checks)
    else:
        summary += (f"\nbudgets: only the baseline ({budgets['baseline']}) was measured, "
                    f"so there is nothing to check against benchmarks/compile/budgets.json\n")
    (out / "summary.txt").write_text(summary, encoding="utf-8")
    print()
    print(summary, end="")
    failed = sum(not measured(r) for r in data["records"])
    missed = sum(not c["ok"] for c in checks)
    print(f"\n{len(data['records'])} programs, {failed} failed, {missed} budgets missed; "
          f"results in {relative(out / 'results.json')}")
    return 1 if failed or missed else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:  # Ctrl-C: the compiler is already stopped
        print("interrupted", file=sys.stderr)
        sys.exit(130)
