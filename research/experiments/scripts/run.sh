#!/usr/bin/env bash
# Fetch the pinned frameworks, build what needs building, and run every
# measurement. Usage:
#   scripts/run.sh <deps-dir> <build-dir> [results-dir] [extra measure.py args...]
# Example (quick smoke run):
#   scripts/run.sh /tmp/di-deps /tmp/di-build /tmp/di-results --runs 1 \
#       --scaling-n 10,25 --bench-iters 20000 --bench-repeats 3
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
DEPS="${1:?usage: run.sh <deps-dir> <build-dir> [results-dir] [measure.py args...]}"
BUILD="${2:?usage: run.sh <deps-dir> <build-dir> [results-dir] [measure.py args...]}"
RESULTS="${3:-$BUILD/results}"
shift $(( $# < 3 ? $# : 3 ))

for tool in g++ clang++ cmake git python3 strip; do
  command -v "$tool" >/dev/null || { echo "missing tool: $tool" >&2; exit 1; }
done
if [ ! -f /usr/include/boost/version.hpp ]; then
  echo "note: Boost headers not found in /usr/include; Hypodermic builds will fail" >&2
  echo "      (Debian/Ubuntu: apt-get install libboost-dev)" >&2
fi

"$HERE/fetch.sh" "$DEPS"
"$HERE/prepare_deps.sh" "$DEPS" "$BUILD"
# -I: isolated mode, so Python never imports modules from the working directory.
python3 -I "$HERE/measure.py" --deps "$DEPS" --build "$BUILD" --results "$RESULTS" "$@"
