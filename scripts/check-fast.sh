#!/usr/bin/env bash
# The quick check before a commit: clang-format-18 on the changed C++ files, then configure, build
# and test the clang-18-debug preset. "Changed" means changed since the merge base with the
# upstream branch, committed or not, plus untracked files. Full output goes to build/logs/check-fast/.
set -uo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/lib/common.sh"
cd "$TRELLIS_ROOT" || exit 1

PRESET=clang-18-debug
LOG_DIR=$(log_dir check-fast)
passed=0
failed=0

mapfile -t files < <(changed_cpp_files)
if ((${#files[@]} == 0)); then
  echo "PASS format: no changed C++ files"
  passed=$((passed + 1))
elif ! command -v clang-format-18 >/dev/null; then
  echo "FAIL format: clang-format-18 not found"
  failed=$((failed + 1))
elif run_logged "$LOG_DIR/format.log" clang-format-18 --dry-run --Werror "${files[@]}"; then
  echo "PASS format: ${#files[@]} changed C++ files are clang-formatted"
  passed=$((passed + 1))
else
  unformatted=$(grep -E 'error: code should be clang-formatted' "$LOG_DIR/format.log" |
    cut -d: -f1 | sort -u | wc -l)
  printf 'FAIL format: %d of %d changed files need clang-format-18 -i, log %s\n' \
    "$unformatted" "${#files[@]}" "$(relpath "$LOG_DIR/format.log")"
  printf '     %s\n' "$(first_error "$LOG_DIR/format.log")"
  failed=$((failed + 1))
fi

if run_preset "$PRESET" "$LOG_DIR/$PRESET"; then
  passed=$((passed + 1))
else
  failed=$((failed + 1))
fi

printf 'check-fast: %d passed, %d failed; logs in %s\n' "$passed" "$failed" "$(relpath "$LOG_DIR")"
((failed == 0))
