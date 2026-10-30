# Shared helpers for scripts/*.sh. Source this file; don't run it.
#
# Every step writes its full output to a log under build/logs/<script>/ and prints one line, plus
# the first error when it fails, so that long compiler output stays out of the console.
# TRELLIS_JOBS (default 2) caps build and test parallelism; other agents may share the machine.

TRELLIS_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
TRELLIS_JOBS=${TRELLIS_JOBS:-2}

# relpath TEXT: TEXT with this repository's absolute path prefix removed.
relpath() { printf '%s\n' "${1//"$TRELLIS_ROOT"\//}"; }

# log_dir NAME: create build/logs/NAME and print its path.
log_dir() {
  local dir="$TRELLIS_ROOT/build/logs/$1"
  mkdir -p "$dir" && printf '%s\n' "$dir"
}

# run_logged LOG COMMAND...: run COMMAND with its output in LOG (after a "$ command" line).
run_logged() {
  local log=$1
  shift
  { printf '$'; printf ' %q' "$@"; printf '\n'; } >"$log"
  "$@" >>"$log" 2>&1
}

# first_error LOG: the most telling line of a failed step's log, shortened to 200 characters.
# The reasons tests give come first (a failing compile-fail test's log also holds the expected
# compiler errors), then ctest's own verdicts, then compiler and CMake errors, then anything that
# looks like an error.
first_error() {
  local log=$1 pattern line=""
  for pattern in \
    '^FAIL |: Failure$|\[  FAILED  \]' \
    '\*\*\*(Failed|Timeout|Exception)|Could not find executable|Unable to find executable' \
    ': (fatal )?error|^error:|undefined reference|^CMake Error' \
    'FAILED:|[Ee]rror'; do
    line=$(tail -n +2 "$log" | grep -m1 -E -- "$pattern") && break
  done
  [[ -n $line ]] || line=$(tail -n +2 "$log" | grep -v '^[[:space:]]*$' | tail -n1)
  relpath "$line" | cut -c1-200
}

# test_counts LOG: "N tests: P passed, S skipped, F failed" from a ctest log.
test_counts() {
  local log=$1 line failed total skipped
  line=$(grep -m1 -E '[0-9]+% tests passed, [0-9]+ tests? failed out of [0-9]+' "$log") || {
    echo "no test summary"
    return
  }
  failed=$(sed -E 's/.*passed, ([0-9]+) tests? failed.*/\1/' <<<"$line")
  total=$(sed -E 's/.*out of ([0-9]+).*/\1/' <<<"$line")
  skipped=$(grep -c -E '^[[:space:]]+[0-9]+ - .* \(Skipped\)$' "$log")
  printf '%d tests: %d passed' "$total" $((total - failed - skipped))
  ((skipped == 0)) || printf ', %d skipped' "$skipped"
  ((failed == 0)) || printf ', %d failed' "$failed"
  printf '\n'
}

# failed_tests LOG: up to five names from ctest's "The following tests FAILED" list.
failed_tests() {
  awk '/The following tests FAILED:/ { listing = 1; next }
       listing && /^[[:space:]]+[0-9]+ - / { sub(/^[[:space:]]+[0-9]+ - /, ""); print; next }
       listing { exit }' "$1" | head -n 5 | paste -sd ';' - | sed 's/;/; /g'
}

# run_preset PRESET LOG_PREFIX: configure, build and test one CMake preset, with logs
# LOG_PREFIX-{configure,build,test}.log. Prints one line, plus the first error when a step fails.
run_preset() {
  local preset=$1 prefix=$2 start=$SECONDS step log detail
  local -a command
  for step in configure build test; do
    log="$prefix-$step.log"
    case $step in
      configure) command=(cmake --preset "$preset") ;;
      build) command=(cmake --build --preset "$preset" -j "$TRELLIS_JOBS") ;;
      test) command=(ctest --preset "$preset" -j "$TRELLIS_JOBS") ;;
    esac
    if ! run_logged "$log" "${command[@]}"; then
      detail=""
      [[ $step != test ]] || detail=" ($(test_counts "$log"); failed: $(failed_tests "$log"))"
      printf 'FAIL %s: %s failed%s, %ss, log %s\n' "$preset" "$step" "$detail" \
        $((SECONDS - start)) "$(relpath "$log")"
      printf '     %s\n' "$(first_error "$log")"
      return 1
    fi
  done
  printf 'PASS %s: %s, %ss\n' "$preset" "$(test_counts "$prefix-test.log")" $((SECONDS - start))
}

# base_commit: where the current work starts. The merge base with the upstream branch, else with
# origin/main, else HEAD. TRELLIS_BASE overrides it.
base_commit() {
  if [[ -n ${TRELLIS_BASE:-} ]]; then
    git -C "$TRELLIS_ROOT" rev-parse "$TRELLIS_BASE"
    return
  fi
  local upstream
  for upstream in '@{upstream}' origin/main; do
    git -C "$TRELLIS_ROOT" merge-base HEAD "$upstream" 2>/dev/null && return
  done
  git -C "$TRELLIS_ROOT" rev-parse HEAD
}

# changed_cpp_files: repository-relative C++ files changed since base_commit (committed or not)
# plus untracked ones, outside research/, build/ and out/.
changed_cpp_files() {
  local base
  base=$(base_commit) || return
  {
    git -C "$TRELLIS_ROOT" diff --name-only --diff-filter=d "$base" --
    git -C "$TRELLIS_ROOT" ls-files --others --exclude-standard
  } | grep -E '\.(h|hh|hpp|hxx|ipp|tpp|cc|cpp|cxx|cppm|ixx)$' |
    grep -v -E '^(research|build|out)/' | sort -u
}
