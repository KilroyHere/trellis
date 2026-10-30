#!/usr/bin/env python3
"""A stand-in compiler for the self-tests of tools/compile_fail.py and tools/golden.py.

The self-tests write temporary configs whose compiler is the Python interpreter and whose first
flags are `-I`, this script and `--scenario NAME`, so that the runner's own command line reaches
it. Every scenario first prints the error that passes.cpp expects. Most then fail in their own
way: a crash message, death by a signal, or a hang with a child process (like cc1plus under the
g++ driver) that must die with it. report-invocation instead reports the arguments and the
environment that the runner gave the compiler.
"""
import argparse
import os
import signal
import subprocess
import sys
import time

EXPECTED_ERROR = "{source}:6:15: error: static assertion failed: fixture: reports TRL-E000"

# scenario: (what the compiler prints after the expected error, exit status)
SCENARIOS = {
    "normal-error": ("", 1),
    "gcc-killed": ("g++: fatal error: Killed signal terminated program cc1plus\n"
                   "compilation terminated.", 1),
    "gcc-out-of-memory": ("cc1plus: out of memory allocating 1048576 bytes after a total of "
                          "2147483648 bytes", 1),
    "virtual-memory-exhausted": ("virtual memory exhausted: Cannot allocate memory", 1),
    "llvm-error": ("LLVM ERROR: out of memory\nAllocation failed", 1),
    "internal-compiler-error": ("{source}:6:15: internal compiler error: in tsubst_copy, at "
                                "cp/pt.cc:17000", 1),
    "clang-crash": ("PLEASE submit a bug report to https://github.com/llvm/llvm-project/issues/ "
                    "and include the crash backtrace.\n"
                    "clang++: error: clang frontend command failed with exit code 139 "
                    "(use -v to see invocation)", 1),
}

# What the self-tests add to a runner's environment before a report-invocation run: include paths,
# color, URL and terminal-width settings, a translation, options that GCC, Clang and MSVC take
# from the environment, and a variable that no compiler needs. None of them may reach the compiler.
INHERITED = {
    "CPATH": "inherited", "C_INCLUDE_PATH": "inherited", "CPLUS_INCLUDE_PATH": "inherited",
    "GCC_COLORS": "error=01;31", "GCC_URLS": "st", "TERM_URLS": "st", "COLUMNS": "40",
    "GCC_EXTRA_DIAGNOSTIC_OUTPUT": "fixits-v2", "CCC_OVERRIDE_OPTIONS": "#+-fcolor-diagnostics",
    "CL": "/Wall", "_CL_": "/Wall", "LANGUAGE": "de", "LC_MESSAGES": "de_DE.UTF-8",
    "TRELLIS_SELFTEST_INHERITED": "1",
}
# report-invocation shows the values of these variables, and only the names of the others.
SHOWN_VALUES = ("LANG", "LC_ALL", "VSLANG")


def report_invocation(compiler_args):
    """Two lines, which a self-test reads back: the compiler arguments after the stub's own, without
    the source file, and the environment. Each fits on one line, so that passes.cpp's line budget
    holds."""
    print("stub arguments: " + " ".join(compiler_args[:-1]))
    print("stub environment: " + " ".join(
        f"{name}={value}" if name in SHOWN_VALUES else name
        for name, value in sorted(os.environ.items())), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scenario", required=True,
                        choices=[*SCENARIOS, "killed-by-signal", "hang", "report-invocation"])
    parser.add_argument("--pid-file",
                        help="hang: where to write this process's ID and its child's")
    args, compiler_args = parser.parse_known_args()
    source = compiler_args[-1] if compiler_args else "input.cpp"
    print(EXPECTED_ERROR.format(source=source), flush=True)

    if args.scenario == "report-invocation":
        report_invocation(compiler_args)
        return 1
    if args.scenario == "killed-by-signal":
        os.kill(os.getpid(), signal.SIGKILL)
    if args.scenario == "hang":
        child = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(300)"])
        # Written to a temporary name and renamed, so that a reader never sees half of it.
        partial = args.pid_file + ".partial"
        with open(partial, "w", encoding="utf-8") as stream:
            stream.write(f"{os.getpid()} {child.pid}")
        os.replace(partial, args.pid_file)
        time.sleep(300)
        return 0
    text, status = SCENARIOS[args.scenario]
    if text:
        print(text.format(source=source), flush=True)
    return status


if __name__ == "__main__":
    sys.exit(main())
