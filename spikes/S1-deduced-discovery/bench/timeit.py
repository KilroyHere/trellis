#!/usr/bin/env python3
"""Time one compile: wall seconds and peak RSS (KiB) of the compiler's process tree.

usage: timeit.py TSV NAME RUN LOG -- COMMAND...

Appends "NAME<TAB>RUN<TAB>seconds<TAB>kib" to TSV and writes the command's
output to LOG. The command runs under a 3 GB address-space cap and, through
timeout(1), a 300 s limit. (subprocess's own timeout would poll wait() in 50 ms
steps and quantize the measurement.) Exits with the command's status.
"""
import resource
import subprocess
import sys
import time


def cap():
    limit = 3_000_000 * 1024
    resource.setrlimit(resource.RLIMIT_AS, (limit, limit))


def main():
    sep = sys.argv.index("--")
    tsv, name, run, log = sys.argv[1:sep]
    cmd = ["timeout", "300", *sys.argv[sep + 1:]]
    start = time.monotonic()
    with open(log, "w") as out:
        status = subprocess.run(cmd, stdout=out, stderr=subprocess.STDOUT, preexec_fn=cap).returncode
    seconds = time.monotonic() - start
    kib = resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss
    if status == 0:
        with open(tsv, "a") as f:
            f.write(f"{name}\t{run}\t{seconds:.3f}\t{kib}\n")
    return status


if __name__ == "__main__":
    sys.exit(main())
