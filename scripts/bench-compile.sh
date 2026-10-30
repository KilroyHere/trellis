#!/usr/bin/env bash
# Compile-time benchmarks (plan §8.4): benchmarks/compile/run.py compiles the scenario program and
# generated dependency graphs one at a time, never through ccache, each under a timeout and a memory
# cap, checks every program's output, and checks benchmarks/compile/budgets.json.
#   scripts/bench-compile.sh --quick       N = 10, 50 and 100 with gcc-13 and clang-18 (minutes)
#   scripts/bench-compile.sh [OPTION...]   the full sweep of plan §8.4 (up to N = 1000, fan-in 1-3
#                                          and 10), or any run.py options: run.py --help
# Prints the results table. results.json and summary.txt go to build/bench/compile/, the full output
# to build/logs/bench-compile/run.log.
set -uo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/lib/common.sh"
cd "$TRELLIS_ROOT" || exit 1

LOG_DIR=$(log_dir bench-compile)
LOG="$LOG_DIR/run.log"
SUMMARY=build/bench/compile/summary.txt
start=$SECONDS
run_logged "$LOG" python3 -I benchmarks/compile/run.py "$@"
status=$?
# run.py writes the summary at the end, also when a program or a budget failed.
[[ -f $SUMMARY ]] && cat "$SUMMARY"
counts=$(grep -E '^[0-9]+ programs, ' "$LOG" | tail -n1 | sed -E 's/; results in .*//')
if ((status == 0)); then
  printf 'PASS bench-compile: %s, %ss; results in build/bench/compile/, log %s\n' \
    "$counts" $((SECONDS - start)) "$(relpath "$LOG")"
else
  printf 'FAIL bench-compile: %s, %ss, log %s\n' "${counts:-run.py exited with status $status}" \
    $((SECONDS - start)) "$(relpath "$LOG")"
  printf '     %s\n' "$(first_error "$LOG")"
fi
exit "$status"
