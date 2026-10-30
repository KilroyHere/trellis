#!/usr/bin/env bash
# The full local matrix: configure, build and test every configure preset in CMakePresets.json
# (GCC 13 and 14, Clang 18 and 20, Clang 18 with libc++, release builds, ASan and UBSan, TSan and the
# timetrace build), one preset at a time, each with at most TRELLIS_JOBS (default 2) parallel jobs.
# MSVC and macOS run only in CI.
#   scripts/check-full.sh [PRESET...]   only these presets
# Full output goes to build/logs/check-full/<preset>-{configure,build,test}.log.
set -uo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/lib/common.sh"
cd "$TRELLIS_ROOT" || exit 1

LOG_DIR=$(log_dir check-full)
if (($#)); then
  presets=("$@")
else
  mapfile -t presets < <(python3 -I -c '
import json, sys
for preset in json.load(open(sys.argv[1]))["configurePresets"]:
    if not preset.get("hidden"):
        print(preset["name"])' CMakePresets.json)
fi

passed=0
failed=0
start=$SECONDS
for preset in "${presets[@]}"; do
  if run_preset "$preset" "$LOG_DIR/$preset"; then
    passed=$((passed + 1))
  else
    failed=$((failed + 1))
  fi
done

printf 'check-full: %d of %d presets passed, %d failed, %ss; logs in %s\n' \
  "$passed" "${#presets[@]}" "$failed" $((SECONDS - start)) "$(relpath "$LOG_DIR")"
((failed == 0))
