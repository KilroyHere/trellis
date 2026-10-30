#!/usr/bin/env bash
# Spike S4 (storage and threading): rebuilds every number in docs/spikes/S4-storage-threading.md.
#
#   bash spikes/S4-storage-threading/run.sh        (from the repository root)
#
# Output goes to build/spikes/S4-storage-threading/ (override with S4_OUT): logs/, the benchmark
# JSON files, sizes-*.md, include-cost.md and summary.md. Nothing under the source tree changes.
# S4_REPETITIONS sets the benchmark repetitions (default 5).
#
# Exit status is non-zero if any build, stress test or ThreadSanitizer run fails. Benchmarks and
# the std::call_once hazard demonstrations report numbers and behaviour; they don't fail the run.
#
# The machine is shared: builds run one at a time, each single-threaded, and non-sanitizer
# compiles run under `ulimit -v 3000000`. Sanitizer builds get no ulimit (TSan reserves a large
# virtual address range).
set -uo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/../.." || exit 1
SPIKE=spikes/S4-storage-threading
OUT=${S4_OUT:-build/spikes/S4-storage-threading}
REPS=${S4_REPETITIONS:-5}
rm -rf "$OUT"
mkdir -p "$OUT/logs"

WARN=(-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror)
failed=()

# name|compiler|extra flags
CONFIGS=(
  "gcc-13|g++-13|"
  "gcc-14|g++-14|"
  "clang-18|clang++-18|"
  "clang-20|clang++-20|"
  "clang-18-libcxx|clang++-18|-stdlib=libc++"
)
TSAN_CONFIGS=(
  "clang-18-tsan|clang++-18|"
  "clang-18-libcxx-tsan|clang++-18|-stdlib=libc++"
  "gcc-14-tsan|g++-14|"
)

capped() { (ulimit -v 3000000 && timeout 600 "$@"); }

# build LOG COMMAND...: run a compile, keeping its output in LOG.
build() {
  local log=$1
  shift
  if "$@" >"$log" 2>&1; then return 0; fi
  echo "   BUILD FAILED ($log):"
  grep -m 5 -E 'error|cannot find' "$log" | sed 's/^/     /'
  return 1
}

{
  echo "date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "cpus: $(nproc); load average: $(cut -d' ' -f1-3 /proc/loadavg)"
  echo "kernel: $(uname -r)"
  for c in g++-13 g++-14 clang++-18 clang++-20; do echo "$c: $($c --version | head -1)"; done
  echo "google benchmark: $(dpkg-query -W -f='${Version}' libbenchmark-dev 2>/dev/null)"
} >"$OUT/environment.txt"
cat "$OUT/environment.txt"

# ---- plain builds: stress tests, sizes, hazards, benchmarks --------------------------------------
for entry in "${CONFIGS[@]}"; do
  IFS='|' read -r name cxx extra <<<"$entry"
  echo "== $name"
  # shellcheck disable=SC2086  # $extra is intentionally split
  if build "$OUT/logs/build-stress-$name.log" capped "$cxx" -std=c++20 -O2 -g "${WARN[@]}" $extra \
    "$SPIKE/stress.cpp" -o "$OUT/stress-$name" -pthread; then
    if timeout 300 "$OUT/stress-$name" --rounds 30 >"$OUT/logs/stress-$name.log" 2>&1; then
      echo "   stress: $(tail -1 "$OUT/logs/stress-$name.log")"
    else
      echo "   stress FAILED:"
      grep -E '^\[FAIL\]|check failed' "$OUT/logs/stress-$name.log" | head -10 | sed 's/^/     /'
      failed+=("stress-$name")
    fi
    "$OUT/stress-$name" --sizes >"$OUT/sizes-$name.md"
    timeout 30 "$OUT/stress-$name" --hazards >"$OUT/logs/hazards-$name.log" 2>&1
    sed 's/^/   /' "$OUT/logs/hazards-$name.log"
  else
    failed+=("build-stress-$name")
  fi

  if [[ $extra == *libc++* ]]; then
    echo "   bench: skipped (the installed libbenchmark is built against libstdc++)"
    continue
  fi
  # shellcheck disable=SC2086
  if build "$OUT/logs/build-bench-$name.log" capped "$cxx" -std=c++20 -O2 -DNDEBUG "${WARN[@]}" $extra \
    "$SPIKE/bench.cpp" -o "$OUT/bench-$name" -lbenchmark -pthread; then
    if timeout 900 "$OUT/bench-$name" --benchmark_repetitions="$REPS" --benchmark_min_time=0.05s \
      --benchmark_enable_random_interleaving=true --benchmark_out="$OUT/bench-$name.json" \
      --benchmark_out_format=json >"$OUT/logs/bench-$name.log" 2>&1; then
      echo "   bench: $(grep -c '"run_type": "iteration"' "$OUT/bench-$name.json") runs written"
    else
      echo "   bench FAILED ($OUT/logs/bench-$name.log)"
      failed+=("bench-$name")
    fi
  else
    failed+=("build-bench-$name")
  fi
done

# ---- spin-count sweep for C+spin (gcc-14, same-slot contention only; 128 is the main run) -----
echo "== C+spin sweep (gcc-14)"
for spin in 8 32; do
  if build "$OUT/logs/build-bench-spin$spin.log" capped g++-14 -std=c++20 -O2 -DNDEBUG \
    -DS4_SPIN=$spin "${WARN[@]}" "$SPIKE/bench.cpp" -o "$OUT/bench-spin$spin" -lbenchmark -pthread; then
    timeout 600 "$OUT/bench-spin$spin" --benchmark_filter='^cold8_same.*/(cv|wait|wait_spin)/' \
      --benchmark_repetitions="$REPS" --benchmark_enable_random_interleaving=true \
      --benchmark_out="$OUT/bench-spin$spin.json" --benchmark_out_format=json \
      >"$OUT/logs/bench-spin$spin.log" 2>&1 || failed+=("bench-spin$spin")
  else
    failed+=("build-bench-spin$spin")
  fi
done
cp "$OUT/bench-gcc-14.json" "$OUT/bench-spin128.json" 2>/dev/null

# ---- ThreadSanitizer --------------------------------------------------------------------------
probe="$OUT/tsan-probe.cpp"
echo 'int main() {}' >"$probe"
if ! clang++-18 -fsanitize=thread "$probe" -o "$OUT/tsan-probe" >"$OUT/logs/tsan-probe.log" 2>&1; then
  echo "== clang-18 cannot link ThreadSanitizer programs: its runtime is missing."
  echo "   Install it with: apt-get install -y libclang-rt-18-dev"
  failed+=("tsan-runtime")
fi
for entry in "${TSAN_CONFIGS[@]}"; do
  IFS='|' read -r name cxx extra <<<"$entry"
  echo "== $name"
  # shellcheck disable=SC2086
  if build "$OUT/logs/build-stress-$name.log" timeout 600 "$cxx" -std=c++20 -O1 -g -fsanitize=thread \
    "${WARN[@]}" $extra "$SPIKE/stress.cpp" -o "$OUT/stress-$name" -pthread; then
    log="$OUT/logs/stress-$name.log"
    if TSAN_OPTIONS="halt_on_error=1 second_deadlock_stack=1" timeout 600 \
      "$OUT/stress-$name" --rounds 200 >"$log" 2>&1 && ! grep -q 'WARNING: ThreadSanitizer' "$log"; then
      echo "   TSan stress: clean, $(tail -1 "$log")"
      grep '^\[SKIP\]' "$log" | sed 's/^/   /'
    else
      echo "   TSan stress FAILED ($log):"
      grep -m 10 -E 'WARNING: ThreadSanitizer|^\[FAIL\]|check failed' "$log" | sed 's/^/     /'
      failed+=("tsan-$name")
    fi
    TSAN_OPTIONS="halt_on_error=0" timeout 30 "$OUT/stress-$name" --hazards \
      >"$OUT/logs/hazards-$name.log" 2>&1
    grep '^hazard' "$OUT/logs/hazards-$name.log" | sed 's/^/   /'
  else
    failed+=("build-stress-$name")
  fi
done

# ---- system calls on the uncontended cold path (strace, gcc-14 build) -------------------------
echo "== futex system calls for 64 first accesses on one thread (strace; 'none' is the baseline)"
if command -v strace >/dev/null && [[ -x "$OUT/stress-gcc-14" ]]; then
  for design in none once cv wait; do
    strace -f -e trace=futex -o "$OUT/logs/strace-$design.log" "$OUT/stress-gcc-14" \
      --first-access "$design" 64 >/dev/null 2>&1 || failed+=("strace-$design")
    echo "| $design | $(grep -c 'futex(' "$OUT/logs/strace-$design.log") |"
  done >"$OUT/syscalls.md"
  sed 's/^/   /' "$OUT/syscalls.md"
else
  echo "   skipped: strace or the gcc-14 stress binary is missing"
fi

# ---- include cost and summary -----------------------------------------------------------------
echo "== include cost"
capped python3 -I "$SPIKE/include_cost.py" "$OUT/include" g++-14 clang++-18 >"$OUT/include-cost.md" \
  2>"$OUT/logs/include-cost.log" || failed+=("include-cost")
cat "$OUT/include-cost.md"

echo "== sizes (libstdc++, from gcc-14; libc++, from clang-18-libcxx)"
cat "$OUT/sizes-gcc-14.md" "$OUT/sizes-clang-18-libcxx.md" 2>/dev/null

echo "== benchmark summary"
python3 -I "$SPIKE/summarize.py" "$OUT" gcc-13 gcc-14 clang-18 clang-20 >"$OUT/summary.md" ||
  failed+=("summary")
cat "$OUT/summary.md"

echo "== C+spin sweep summary (gcc-14)"
python3 -I "$SPIKE/summarize.py" "$OUT" spin8 spin32 spin128 | sed -n '/^### All/,$p' |
  grep -E '^(###|\| benchmark|\|---|\| cold8_same)' >"$OUT/spin-sweep.md" || failed+=("spin-sweep")
cat "$OUT/spin-sweep.md"

echo
if ((${#failed[@]})); then
  echo "S4: FAILED: ${failed[*]}"
  exit 1
fi
echo "S4: all builds, stress tests and ThreadSanitizer runs passed (results in $OUT)"
