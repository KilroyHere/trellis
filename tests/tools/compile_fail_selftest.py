#!/usr/bin/env python3
"""Self-test of tools/compile_fail.py: run it on fixtures whose outcome is known.

Two kinds of case, each checked against the runner's verdict (its first output line) and exit
status:
  - CASES compile each fixture in compile_fail_fixtures/ with the build's compiler. Each fixture
    breaks one rule of the runner (passes.cpp breaks none), so a file that compiles, or one that
    misses an expectation or has a bad directive, must be reported as a failure for that reason.
  - STUB_CASES compile passes.cpp with stub_compiler.py, through a temporary config. The stub prints
    the error passes.cpp expects and then crashes, dies by a signal or hangs, and the runner must
    still report a failure. For the hang, the stub's own child process must not survive the
    timeout.
CTest runs this as tools.compile_fail_selftest with the build's --config.
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

HERE = Path(__file__).resolve().parent
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


def run_runner(config, source, *extra):
    """Run the runner from the repository root with a relative path, as people do."""
    relative = Path(source).relative_to(ROOT).as_posix()
    command = [sys.executable, "-I", str(RUNNER), "--config", str(config), *extra, relative]
    proc = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, check=False)
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


def check_child_died(pid_file):
    """The hanging stub's child must die with it, not run on as an orphan."""
    try:
        pid = int(pid_file.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        print("FAIL hang: the stub didn't record its child's process ID")
        return False
    deadline = time.monotonic() + 3
    while process_alive(pid) and time.monotonic() < deadline:
        time.sleep(0.1)
    if not process_alive(pid):
        print("ok   hang: the stub's child process died with it")
        return True
    os.kill(pid, signal.SIGKILL)
    print(f"FAIL hang: the stub's child process {pid} outlived the timeout (killed it now)")
    return False


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
                results.append(check_child_died(pid_file))

    failures = results.count(False)
    if failures:
        print(f"FAIL {failures} of {len(results)} checks")
        return 1
    print(f"all {len(results)} checks behaved as expected")
    return 0


if __name__ == "__main__":
    sys.exit(main())
