#!/usr/bin/env bash
# Spike S3 (diagnostics rendering): rebuild every number in docs/spikes/S3-diagnostics.md.
#
# Usage, from the repository root:  bash spikes/S3-diagnostics/run.sh
#
# Needs g++-13, g++-14, clang++-18, clang++-20 (and libc++-18 for one type-name run).
# Compiles run one at a time, each in a subshell under `ulimit -v 3000000` and
# `timeout 300`. Deterministic results (logs/, results.md) are rewritten in place and
# come out byte-identical on every run; timings go to build/spikes/S3-diagnostics/.
# Exit status is non-zero if any check in results.md fails.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$root"

for tool in g++-13 g++-14 clang++-18 clang++-20 python3; do
  command -v "$tool" >/dev/null || { echo "run.sh: $tool not found" >&2; exit 2; }
done

rm -rf spikes/S3-diagnostics/logs build/spikes/S3-diagnostics
exec python3 -I spikes/S3-diagnostics/tools/measure.py
