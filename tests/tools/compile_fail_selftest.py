#!/usr/bin/env python3
"""Self-test of tools/compile_fail.py: run it on fixtures whose outcome is known.

Four kinds of case:
  - CASES compile each fixture in compile_fail_fixtures/ with the build's compiler. Each fixture
    breaks one rule of the runner (passes.cpp breaks none), so a file that compiles, or one that
    misses an expectation or has a bad directive, must be reported as a failure for that reason.
  - STUB_CASES compile passes.cpp with stub_compiler.py, through a temporary config. The stub prints
    the error passes.cpp expects and then crashes, dies by a signal or hangs, and the runner must
    still report a failure. For the hang, neither the stub nor its own child process (playing
    cc1plus) may survive the timeout.
  - INTERRUPTS (POSIX only) start the runner on the hanging stub and then interrupt the runner
    itself, with SIGINT as Ctrl-C does and with SIGTERM as an outer `timeout` does. The compiler
    runs in its own process group, which these signals don't reach, so the runner must take the
    stub and its child down before it exits.
  - The invocation check runs the runner, with stub_compiler.INHERITED in its environment, on the
    stub that reports its arguments and environment. The compiler must get LC_ALL=C and none of
    the inherited variables, and the output users see (colours off, but quoted source and carets
    kept, as expect-max-lines counts them).
CASES and STUB_CASES are checked against the runner's verdict (its first output line) and exit
status. CTest runs this as tools.compile_fail_selftest with the build's --config.
"""
import argparse
import json
import os
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path

sys.dont_write_bytecode = True  # keep tests/tools free of __pycache__
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import stub_compiler  # noqa: E402  (INHERITED: what the invocation check adds to the environment)

ROOT = HERE.parent.parent
RUNNER = ROOT / "tools" / "compile_fail.py"
FIXTURES = HERE / "compile_fail_fixtures"
STUB = HERE / "stub_compiler.py"
HANG_TIMEOUT = 2  # seconds the runner gives the hanging stub

# (fixture, expected exit status, text the runner's verdict line must contain)
CASES = [
    ("compiles.cpp", 1, "compiled without errors"),
    ("regex_mismatch.cpp", 1, "expect-regex did not match: this text never appears"),
    ("regex_in_quoted_source.cpp", 1, "expect-regex did not match: gadget"),
    ("id_mismatch.cpp", 1, "the first error line does not contain TRL-E999"),
    ("id_prefix.cpp", 1, "the first error line does not contain TRL-E001"),
    ("too_many_lines.cpp", 1, "more than expect-max-lines (1)"),
    ("missing_summary.cpp", 1, "missing the '// compile-fail: <summary>' directive"),
    ("no_expectation.cpp", 1, "needs an expect-id or an expect-regex"),
    ("unknown_directive.cpp", 1, "unknown directive 'expect-regexp'"),
    ("underscore_directive.cpp", 1, "unknown directive 'expect_regex'"),
    ("late_directive.cpp", 1, "line 4: 'expect-id' must be in the comment block at the top"),
    ("passes.cpp", 0, "PASS "),
]

# (stub scenario, expected exit status, text the runner's verdict line must contain)
STUB_CASES = [
    ("normal-error", 0, "PASS "),
    ("gcc-killed", 1, "the compiler crashed: g++: fatal error: Killed signal terminated program"),
    ("gcc-out-of-memory", 1, "the compiler crashed: cc1plus: out of memory allocating"),
    ("virtual-memory-exhausted", 1, "the compiler crashed: virtual memory exhausted"),
    ("llvm-error", 1, "the compiler crashed: LLVM ERROR: out of memory"),
    ("internal-compiler-error", 1, "internal compiler error: in tsubst_copy"),
    ("clang-crash", 1, "the compiler crashed: PLEASE submit a bug report"),
]
if os.name == "posix":
    STUB_CASES += [
        ("killed-by-signal", 1, "the compiler was killed by signal 9"),
        ("hang", 1, f"the compiler did not finish within {HANG_TIMEOUT} s"),
    ]

# (signal sent to the runner, the runner's expected exit status)
INTERRUPTS = []
if os.name == "posix":
    INTERRUPTS = [(signal.SIGINT, 130), (signal.SIGTERM, 128 + signal.SIGTERM)]


def run_runner(config, source, *extra, env=None):
    """Run the runner from the repository root with a relative path, as people do."""
    relative = Path(source).relative_to(ROOT).as_posix()
    command = [sys.executable, "-I", str(RUNNER), "--config", str(config), *extra, relative]
    proc = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, text=True, check=False)
    return proc.returncode, proc.stdout + proc.stderr


def check(label, status, report, expected_status, expected_text):
    verdict = report.splitlines()[0] if report else ""
    if status == expected_status and expected_text in verdict:
        print(f"ok   {label}: exit {status}, '{expected_text}'")
        return True
    print(f"FAIL {label}: expected exit {expected_status} and '{expected_text}' in the verdict, "
          f"got exit {status}. Runner output:")
    for line in report.splitlines()[:20]:
        print(f"     {line}")
    return False


def stub_config(real_config, scratch, scenario, pid_file=None):
    """A copy of the build's config whose compiler is stub_compiler.py playing SCENARIO."""
    config = json.loads(Path(real_config).read_text(encoding="utf-8"))
    flags = ["-I", str(STUB), "--scenario", scenario]
    if pid_file:
        flags += ["--pid-file", str(pid_file)]
    config.update(compiler=sys.executable, frontend="GNU", golden_name="stub-1", flags=flags,
                  binary_dir=str(scratch))
    path = scratch / f"{scenario}.json"
    path.write_text(json.dumps(config, indent=2), encoding="utf-8")
    return path


def process_alive(pid):
    """True while PID runs. A zombie (killed, not yet reaped) counts as dead."""
    if Path("/proc").is_dir():
        try:
            stat = Path(f"/proc/{pid}/stat").read_text(encoding="utf-8")
        except OSError:
            return False
        return stat.rsplit(")", 1)[1].split()[0] not in ("Z", "X")
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    return True


def read_pids(pid_file):
    """The hanging stub's process ID and its child's, or None if it didn't record them."""
    try:
        return [int(pid) for pid in pid_file.read_text(encoding="utf-8").split()]
    except (OSError, ValueError):
        return None


def check_processes_died(label, pid_file):
    """The hanging stub and its child must both be gone, not run on as orphans."""
    pids = read_pids(pid_file)
    if not pids:
        print(f"FAIL {label}: the stub didn't record its process IDs")
        return False
    deadline = time.monotonic() + 3
    while any(process_alive(pid) for pid in pids) and time.monotonic() < deadline:
        time.sleep(0.1)
    survivors = [pid for pid in pids if process_alive(pid)]
    if not survivors:
        print(f"ok   {label}: the stub and its child process died")
        return True
    for pid in survivors:
        os.kill(pid, signal.SIGKILL)
    print(f"FAIL {label}: processes {survivors} of the stub outlived the runner (killed them now)")
    return False


def reported_invocation(text):
    """The arguments and the environment entries that the report-invocation stub printed."""
    found = {}
    for line in text.splitlines():
        for key in ("arguments", "environment"):
            if line.startswith(f"stub {key}: "):
                found[key] = line.split(": ", 1)[1].split()
    return found.get("arguments"), found.get("environment")


def check_invocation(config, scratch):
    """A compile-fail run starts the compiler with LC_ALL=C, nothing of the runner's own environment
    beyond what a compiler needs to start, and the usual diagnostics flags (no plain output)."""
    label = "invocation"
    env = dict(os.environ, LANG="de_DE.UTF-8", **stub_compiler.INHERITED)
    status, report = run_runner(stub_config(config, scratch, "report-invocation"),
                                FIXTURES / "passes.cpp", env=env)
    if not check(label, status, report, 0, "PASS "):
        return False
    log = scratch / "compile_fail" / "tests" / "tools" / "compile_fail_fixtures" / "passes.log"
    arguments, environment = reported_invocation(log.read_text(encoding="utf-8"))
    if arguments is None or environment is None:
        print(f"FAIL {label}: the stub's report is missing from {log}")
        return False
    names = {entry.split("=", 1)[0] for entry in environment}
    problems = [f"{name} was passed on" for name in sorted(set(stub_compiler.INHERITED) & names)]
    problems += [f"{entry} is missing" for entry in ("LC_ALL=C", "LANG=C")
                 if entry not in environment]
    if "-fdiagnostics-color=never" not in arguments:
        problems.append("-fdiagnostics-color=never is missing")
    problems += [f"{flag} must stay off: expect-max-lines counts the lines users see"
                 for flag in ("-fdiagnostics-plain-output", "-fno-caret-diagnostics")
                 if flag in arguments]
    if problems:
        print(f"FAIL {label}: {'; '.join(problems)}")
        print(f"     arguments: {' '.join(arguments)}")
        print(f"     environment: {' '.join(environment)}")
        return False
    print(f"ok   {label}: LC_ALL=C, no inherited variable, arguments {' '.join(arguments)}")
    return True


def check_interrupt(config, scratch, signum, expected_status):
    """Interrupt the runner itself while the stub hangs; it must stop the stub before exiting."""
    label = f"runner interrupted by {signal.Signals(signum).name}"
    pid_file = scratch / f"interrupt-{signum}.pid"
    relative = (FIXTURES / "passes.cpp").relative_to(ROOT).as_posix()
    command = [sys.executable, "-I", str(RUNNER), "--config",
               str(stub_config(config, scratch, "hang", pid_file)), "--timeout", "120", relative]
    runner = subprocess.Popen(command, cwd=ROOT, stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT, text=True)
    deadline = time.monotonic() + 20
    while not pid_file.exists() and runner.poll() is None and time.monotonic() < deadline:
        time.sleep(0.05)
    if not pid_file.exists():
        runner.kill()
        print(f"FAIL {label}: the stub never started; runner output: {runner.communicate()[0]}")
        return [False]
    runner.send_signal(signum)
    try:
        output, _ = runner.communicate(timeout=20)
    except subprocess.TimeoutExpired:
        runner.kill()
        output, _ = runner.communicate()
    results = [check_processes_died(label, pid_file)]
    if runner.returncode == expected_status:
        print(f"ok   {label}: exit {runner.returncode}")
        results.append(True)
    else:
        print(f"FAIL {label}: expected exit {expected_status}, got {runner.returncode}. Output:")
        for line in output.splitlines()[:10]:
            print(f"     {line}")
        results.append(False)
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--config", required=True, help="the build's trellis_compiler.json")
    args = parser.parse_args()
    config = Path(args.config).resolve()

    results = []
    fixtures = sorted(path.name for path in FIXTURES.glob("*.cpp"))
    covered = sorted(case[0] for case in CASES)
    if fixtures != covered:
        print(f"FAIL fixtures and CASES differ: fixtures {fixtures}, cases {covered}")
        results.append(False)
    for fixture, expected_status, expected_text in CASES:
        status, report = run_runner(config, FIXTURES / fixture)
        results.append(check(fixture, status, report, expected_status, expected_text))

    binary_dir = Path(json.loads(config.read_text(encoding="utf-8"))["binary_dir"])
    binary_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="compile_fail_selftest-", dir=binary_dir) as name:
        scratch = Path(name)
        for scenario, expected_status, expected_text in STUB_CASES:
            pid_file = scratch / "hang.pid" if scenario == "hang" else None
            extra = ["--timeout", str(HANG_TIMEOUT)] if scenario == "hang" else []
            status, report = run_runner(stub_config(config, scratch, scenario, pid_file),
                                        FIXTURES / "passes.cpp", *extra)
            results.append(check(f"stub {scenario}", status, report, expected_status,
                                 expected_text))
            if pid_file:
                results.append(check_processes_died("hang", pid_file))
        results.append(check_invocation(config, scratch))
        for signum, expected_status in INTERRUPTS:
            results.extend(check_interrupt(config, scratch, signum, expected_status))

    failures = results.count(False)
    if failures:
        print(f"FAIL {failures} of {len(results)} checks")
        return 1
    print(f"all {len(results)} checks behaved as expected")
    return 0


if __name__ == "__main__":
    sys.exit(main())
