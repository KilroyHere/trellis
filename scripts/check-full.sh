#!/usr/bin/env bash
# The full local matrix: configure, build and test every configure preset in CMakePresets.json
# that this host can run (on Linux: GCC 13 and 14, Clang 18 and 20, Clang 18 with libc++, release
# builds, ASan and UBSan, TSan and the timetrace build), one preset at a time, each with at most
# TRELLIS_JOBS (default 2) parallel jobs. appleclang-debug and msvc-debug have a condition on the
# host system, so CMake disables them elsewhere and they are skipped; they run in CI
# (.github/workflows/platforms.yml).
#   scripts/check-full.sh [PRESET...]   only these presets
# Full output goes to build/logs/check-full/<preset>-{configure,build,test}.log.
set -uo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/lib/common.sh"
cd "$TRELLIS_ROOT" || exit 1

LOG_DIR=$(log_dir check-full)
if (($#)); then
  presets=("$@")
else
  # CMake lists only the presets whose condition holds on this host (CMakeUserPresets.json's too);
  # keep those that CMakePresets.json defines, in its order.
  if ! listing=$(cmake --list-presets=configure 2>&1); then
    printf 'FAIL presets: cmake --list-presets failed: %s\n' "$(head -n1 <<<"$listing")"
    exit 1
  fi
  mapfile -t presets < <(sed -n -E 's/^ +"([^"]+)".*/\1/p' <<<"$listing" | python3 -I -c '
import json, sys
enabled = set(sys.stdin.read().split())
for preset in json.load(open(sys.argv[1]))["configurePresets"]:
    if not preset.get("hidden") and preset["name"] in enabled:
        print(preset["name"])' CMakePresets.json)
  if ((${#presets[@]} == 0)); then
    echo "FAIL presets: CMakePresets.json has no configure preset that this host can run"
    exit 1
  fi
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
