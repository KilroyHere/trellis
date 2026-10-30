#!/usr/bin/env bash
# Runtime benchmarks (plan §8.4): build benchmarks/runtime/ (Google Benchmark) with the gcc-13-release
# and clang-18-release presets and run it pinned to the last CPU. The cases are the research
# scenario's graph and resolve, measured as the research harness did: 7 repetitions of 200,000
# iterations each (TRELLIS_BENCH_REPETITIONS and TRELLIS_BENCH_ITERATIONS change that).
#   scripts/bench-runtime.sh [PRESET...]
# Each preset is configured in its own build directory, build/bench/runtime/<preset>/, with the
# benchmarks on and the tests off, so the preset's own build (build/<preset>, which check-full.sh
# uses) never needs Google Benchmark. Prints the table of medians. Results go to
# build/bench/runtime/: <preset>.json (Google Benchmark's output) and summary.json; the full output
# to build/logs/bench-runtime/.
set -uo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/lib/common.sh"
cd "$TRELLIS_ROOT" || exit 1

LOG_DIR=$(log_dir bench-runtime)
OUT=build/bench/runtime
REPETITIONS=${TRELLIS_BENCH_REPETITIONS:-7}
ITERATIONS=${TRELLIS_BENCH_ITERATIONS:-200000}
if (($#)); then
  presets=("$@")
else
  presets=(gcc-13-release clang-18-release)
fi
mkdir -p "$OUT"
pin=()
if command -v taskset >/dev/null; then
  pin=(taskset -c $(($(nproc) - 1)))
fi

failed=0
results=()
for preset in "${presets[@]}"; do
  start=$SECONDS
  prefix="$LOG_DIR/$preset"
  rm -f "$OUT/$preset.json"
  ok=1
  build_dir="$OUT/$preset"
  for step in configure build run; do
    log="$prefix-$step.log"
    case $step in
      configure)
        command=(cmake --preset "$preset" -B "$build_dir" -DTRELLIS_BUILD_TESTS=OFF
          -DTRELLIS_BUILD_BENCHMARKS=ON)
        ;;
      build) command=(cmake --build "$build_dir" --target trellis_runtime_bench -j "$TRELLIS_JOBS") ;;
      run)
        # ${pin[@]+...}: an empty array is an error under set -u before bash 4.4 (macOS has 3.2).
        command=(${pin[@]+"${pin[@]}"} "$build_dir/benchmarks/runtime/trellis_runtime_bench"
          "--benchmark_repetitions=$REPETITIONS" "--benchmark_min_time=${ITERATIONS}x"
          --benchmark_min_warmup_time=0.02 "--benchmark_out=$OUT/$preset.json"
          --benchmark_out_format=json)
        ;;
    esac
    if ! run_logged "$log" "${command[@]}"; then
      printf 'FAIL %s: %s failed, %ss, log %s\n' "$preset" "$step" $((SECONDS - start)) \
        "$(relpath "$log")"
      printf '     %s\n' "$(first_error "$log")"
      ok=0
      break
    fi
  done
  if ((ok)); then
    printf 'PASS %s: %d repetitions of %d iterations per case, %ss\n' "$preset" "$REPETITIONS" \
      "$ITERATIONS" $((SECONDS - start))
    results+=("$OUT/$preset.json")
  else
    failed=$((failed + 1))
  fi
done

if ((${#results[@]})) &&
  ! python3 -I benchmarks/runtime/report.py --out "$OUT/summary.json" "${results[@]}"; then
  failed=$((failed + 1))
fi
printf 'bench-runtime: %d of %d presets measured; results in %s, logs in %s\n' \
  "${#results[@]}" "${#presets[@]}" "$OUT" "$(relpath "$LOG_DIR")"
((failed == 0))
