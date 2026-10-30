#!/usr/bin/env bash
# Smoke test for a fresh checkout or session: check the tools, then configure, build and test the
# clang-18-debug preset. Full output goes to build/logs/init/.
set -uo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/lib/common.sh"
cd "$TRELLIS_ROOT" || exit 1

PRESET=clang-18-debug
LOG_DIR=$(log_dir init)
failed=0

# version_of TOOL: the first version-looking number in `TOOL --version`.
version_of() { "$1" --version 2>&1 | grep -m1 -o -E '[0-9]+\.[0-9]+(\.[0-9]+)?' | head -n1; }

# at_least VERSION MINIMUM: true if VERSION >= MINIMUM (dotted numbers).
at_least() { [[ $(printf '%s\n%s\n' "$2" "$1" | sort -V | head -n1) == "$2" ]]; }

check_tools() {
  local tool missing=() found=() notes=() version
  for tool in cmake ninja clang++-18 python3 git; do
    if command -v "$tool" >/dev/null; then
      found+=("$tool $(version_of "$tool")")
    else
      missing+=("$tool")
    fi
  done
  version=$(version_of cmake 2>/dev/null)
  if [[ -n $version ]] && ! at_least "$version" 3.21; then
    missing+=("cmake >= 3.21 (found $version)")
  fi
  version=$(version_of python3 2>/dev/null)
  if [[ -n $version ]] && ! at_least "$version" 3.8; then
    missing+=("python3 >= 3.8 (found $version)")
  fi
  for tool in ccache clang-format-18 clang-tidy-18 g++-13 g++-14 clang++-20; do
    command -v "$tool" >/dev/null || notes+=("$tool")
  done
  if ((${#missing[@]})); then
    printf 'FAIL tools: missing %s\n' "$(IFS=,; echo "${missing[*]}" | sed 's/,/, /g')"
    return 1
  fi
  printf 'PASS tools: %s\n' "$(IFS=,; echo "${found[*]}" | sed 's/,/, /g')"
  if ((${#notes[@]})); then
    printf '     not found, needed by other scripts or presets: %s\n' "${notes[*]}"
  fi
}

check_tools || failed=$((failed + 1))
if ((failed == 0)); then
  run_preset "$PRESET" "$LOG_DIR/$PRESET" || failed=$((failed + 1))
fi

if ((failed)); then
  printf 'init: FAILED; logs in %s\n' "$(relpath "$LOG_DIR")"
  exit 1
fi
printf 'init: ok (tools and %s); logs in %s\n' "$PRESET" "$(relpath "$LOG_DIR")"
