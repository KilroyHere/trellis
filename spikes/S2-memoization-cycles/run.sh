#!/usr/bin/env bash
# Spike S2 (memoization and cycles): rebuild every result quoted in
# docs/spikes/S2-memoization-cycles.md.
#
#   bash spikes/S2-memoization-cycles/run.sh --large    # every number in the write-up, about an hour
#   bash spikes/S2-memoization-cycles/run.sh            # the same without N = 500 and 1000
#   bash spikes/S2-memoization-cycles/run.sh --quick    # about 6 minutes: the verify command
#
# Outputs go to spikes/S2-memoization-cycles/out/ (git-ignored; override with
# S2_OUT=dir): summary.md (every table), *.json (every record), logs/cycles/
# (each compiler's full output for each cycle case) and work/ (generated
# sources). results/ holds a committed snapshot of the final full run.
#
# The quick run covers the graph statistics, the cycle diagnostics on gcc-13
# and clang-18 (the full run adds gcc-14 and clang-20), the per-hop depth of
# S1's stub and the memo check on chains of 50 and 100 hops, and the scaling at
# N = 25 and 50 (path: N = 10 to 20), one compile each. Every compile runs alone,
# never through ccache, under timeout 600 and a 4,000,000 KiB address-space cap
# (also set here with ulimit -v, so nothing escapes it). The exit code is 0 only
# if every expectation in s2.py's check() holds.
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="${S2_OUT:-$HERE/out}"
args=()
for a in "$@"; do
  case "$a" in
    --quick | --large) args+=("$a") ;;
    *)
      echo "usage: run.sh [--quick | --large]" >&2
      exit 2
      ;;
  esac
done

rm -rf "$OUT"
mkdir -p "$OUT"
(ulimit -v 4000000 && exec python3 -B "$HERE/s2.py" --out "$OUT" "${args[@]}") 2>&1 | tee "$OUT/run.log"
status=${PIPESTATUS[0]}
exit "$status"
