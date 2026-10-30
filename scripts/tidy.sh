#!/usr/bin/env bash
# clang-tidy-18 with the checks in .clang-tidy (bugprone, performance, modernize, include-cleaner;
# warnings are errors).
#   scripts/tidy.sh FILE...   check these files in full
#   scripts/tidy.sh           check only the lines changed since the merge base with the upstream
#                             branch, committed or not, through clang-tidy-diff; untracked files
#                             are checked in full
# Compile flags come from build/clang-18-debug/compile_commands.json, which is configured first if
# it is missing. Only files the build compiles are checked, plus the public headers in include/,
# which clang-tidy checks with flags borrowed from a similar file. Snippets that must not compile
# (tests/compile_fail/) and files built elsewhere (spikes/, tests/package/) are skipped.
# Full output goes to build/logs/tidy/.
set -uo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/lib/common.sh"
cd "$TRELLIS_ROOT" || exit 1

PRESET=clang-18-debug
DB_DIR="build/$PRESET"
LOG_DIR=$(log_dir tidy)
LOG="$LOG_DIR/tidy.log"

if ! CLANG_TIDY=$(command -v clang-tidy-18); then
  echo "FAIL tidy: clang-tidy-18 not found"
  exit 1
fi
if [[ ! -f $DB_DIR/compile_commands.json ]] &&
  ! run_logged "$LOG_DIR/configure.log" cmake --preset "$PRESET"; then
  printf 'FAIL tidy: cmake --preset %s failed, log %s\n' "$PRESET" "$(relpath "$LOG_DIR/configure.log")"
  printf '     %s\n' "$(first_error "$LOG_DIR/configure.log")"
  exit 1
fi

# lintable FILE...: the files clang-tidy can check, as given: public headers, and files in the
# compile database.
lintable() {
  (($#)) || return 0
  python3 -I -c '
import json, os, sys
database = {os.path.realpath(os.path.join(entry["directory"], entry["file"]))
            for entry in json.load(open(sys.argv[1]))}
for name in sys.argv[2:]:
    if name.startswith("include/") or os.path.realpath(name) in database:
        print(name)' "$DB_DIR/compile_commands.json" "$@"
}

# The clang-tidy-diff script for LLVM 18 has several names, depending on the distribution.
find_tidy_diff() {
  local candidate
  for candidate in clang-tidy-diff-18.py clang-tidy-diff-18 /usr/lib/llvm-18/share/clang/clang-tidy-diff.py; do
    if command -v "$candidate" >/dev/null; then
      command -v "$candidate"
      return 0
    fi
  done
  return 1
}

if (($#)); then
  mapfile -t files < <(lintable "$@")
  skipped=$(($# - ${#files[@]}))
  if ((${#files[@]} == 0)); then
    echo "PASS tidy: nothing to check ($skipped file(s) not compiled by the build)"
    exit 0
  fi
  run_logged "$LOG" "$CLANG_TIDY" -p "$DB_DIR" --quiet "${files[@]}"
  status=$?
  what="${#files[@]} file(s)"
else
  mapfile -t changed < <(changed_cpp_files)
  mapfile -t files < <(lintable "${changed[@]}")
  skipped=$((${#changed[@]} - ${#files[@]}))
  if ((${#files[@]} == 0)); then
    echo "PASS tidy: no changed files to check ($skipped changed file(s) not compiled by the build)"
    exit 0
  fi
  tracked=()
  untracked=()
  for file in "${files[@]}"; do
    if git ls-files --error-unmatch -- "$file" >/dev/null 2>&1; then
      tracked+=("$file")
    else
      untracked+=("$file")
    fi
  done
  base=$(base_commit)
  # The first log line names the run; first_error skips it, as it skips run_logged's command line.
  printf '$ clang-tidy on lines changed since %s: %s\n' "$base" "${files[*]}" >"$LOG"
  status=0
  if ((${#tracked[@]})); then
    if ! TIDY_DIFF=$(find_tidy_diff); then
      echo "FAIL tidy: clang-tidy-diff for LLVM 18 not found"
      exit 1
    fi
    git diff -U0 --no-color "$base" -- "${tracked[@]}" |
      python3 "$TIDY_DIFF" -p1 -path "$DB_DIR" -clang-tidy-binary "$CLANG_TIDY" -quiet \
        -j "$TRELLIS_JOBS" >>"$LOG" 2>&1 || status=1
  fi
  if ((${#untracked[@]})); then
    "$CLANG_TIDY" -p "$DB_DIR" --quiet "${untracked[@]}" >>"$LOG" 2>&1 || status=1
  fi
  what="changed lines in ${#tracked[@]} file(s), ${#untracked[@]} new file(s) in full"
fi

note=""
((skipped == 0)) || note="; skipped $skipped file(s) the build doesn't compile"
if ((status == 0)); then
  printf 'PASS tidy: %s%s; log %s\n' "$what" "$note" "$(relpath "$LOG")"
  exit 0
fi
problems=$(grep -c -E ': (error|warning): ' "$LOG")
printf 'FAIL tidy: %s problems in %s%s; log %s\n' "$problems" "$what" "$note" "$(relpath "$LOG")"
printf '     %s\n' "$(first_error "$LOG")"
exit 1
