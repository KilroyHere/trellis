#!/usr/bin/env python3
"""A stand-in compiler for the self-test of tools/compile_fail.py.

compile_fail_selftest.py writes temporary configs whose compiler is the Python interpreter and whose
first flags are `-I`, this script and `--scenario NAME`, so that the runner's own command line
reaches it. Every scenario first prints the error that passes.cpp expects, then fails in its own
way: a crash message, death by a signal, or a hang with a child process (like cc1plus under the
g++ driver) that must die with it.
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scenario", required=True,
                        choices=[*SCENARIOS, "killed-by-signal", "hang"])
    parser.add_argument("--pid-file", help="hang: where to write the child's process ID")
    args, compiler_args = parser.parse_known_args()
    source = compiler_args[-1] if compiler_args else "input.cpp"
    print(EXPECTED_ERROR.format(source=source), flush=True)

    if args.scenario == "killed-by-signal":
        os.kill(os.getpid(), signal.SIGKILL)
    if args.scenario == "hang":
        child = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(300)"])
        with open(args.pid_file, "w", encoding="utf-8") as stream:
            stream.write(str(child.pid))
        time.sleep(300)
        return 0
    text, status = SCENARIOS[args.scenario]
    if text:
        print(text.format(source=source), flush=True)
    return status


if __name__ == "__main__":
    sys.exit(main())
