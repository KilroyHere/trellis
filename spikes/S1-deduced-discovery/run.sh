#!/usr/bin/env bash
# Spike S1 (deduced constructor discovery): rebuild every result quoted in
# docs/spikes/S1-deduced-discovery.md, on gcc-13, gcc-14, clang-18, clang-20 and
# clang-18 with libc++.
#
#   bash spikes/S1-deduced-discovery/run.sh        # from the repository root
#
# Outputs go to spikes/S1-deduced-discovery/out/ (git-ignored; override with
# S1_OUT=dir): logs/ (every compiler and program output), diag/ (why each failing
# form cell fails), bench/ (timings), summary.md (the tables). S1_REPS sets the
# timing repetitions (default 5). Every compile runs alone, under timeout 300 and
# a 3 GB address-space cap. The exit code is 0 only if every check passes.
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="${S1_OUT:-$HERE/out}"
REPS="${S1_REPS:-5}"
CONFIGS=(gcc13 gcc14 clang18 clang20 clang18-libcxx)
WARN=(-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror)

rm -rf "$OUT"
mkdir -p "$OUT/logs" "$OUT/diag" "$OUT/bench" "$OUT/bin" "$OUT/gen"
failures=0
fail() {
  echo "FAIL: $*"
  failures=$((failures + 1))
}

cxx_of() {
  case "$1" in
    gcc13) echo g++-13 ;;
    gcc14) echo g++-14 ;;
    clang18 | clang18-libcxx) echo clang++-18 ;;
    clang20) echo clang++-20 ;;
  esac
}
stdlib_of() {
  if [ "$1" = clang18-libcxx ]; then echo -stdlib=libc++; fi
}
# capped CMD...: run one compile under the time and memory caps.
capped() { (ulimit -v 3000000 && exec timeout 300 "$@"); }
# compile CONFIG ARGS...: the configuration's compiler, C++20, its standard library.
compile() {
  local config=$1
  shift
  # shellcheck disable=SC2046
  capped "$(cxx_of "$config")" -std=c++20 $(stdlib_of "$config") "$@"
}

{
  echo "date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "host: $(nproc) vCPU, load average $(cut -d' ' -f1-3 /proc/loadavg)"
  for c in g++-13 g++-14 clang++-18 clang++-20; do echo "$c: $($c --version | head -1)"; done
  echo "libc++: $(echo '#include <version>
_LIBCPP_VERSION' | clang++-18 -std=c++20 -stdlib=libc++ -E -P -x c++ - | tail -1)"
} > "$OUT/versions.txt"
cat "$OUT/versions.txt"

echo "== 1. forms x operator sets (forms.cpp)"
for c in "${CONFIGS[@]}"; do
  if ! compile "$c" -Wall -Wextra -O0 "$HERE/forms.cpp" -o "$OUT/bin/forms-$c" > "$OUT/logs/forms-$c.build.log" 2>&1; then
    fail "forms.cpp does not build on $c (see out/logs/forms-$c.build.log)"
    continue
  fi
  if "$OUT/bin/forms-$c" > "$OUT/forms-$c.txt" 2>&1; then
    echo "$c: $(tail -1 "$OUT/forms-$c.txt")"
  else
    fail "$c: $(tail -1 "$OUT/forms-$c.txt")"
  fi
  # Compile each failing cell on its own to record the compiler's reason.
  grep '^ROW|' "$OUT/forms-$c.txt" | while IFS='|' read -r _ set form status _ _ _ si fi; do
    [ "$status" = "not-constructible" ] || continue
    safe=$(printf '%s' "$c-$set-$form" | sed -E 's/[^A-Za-z0-9]+/_/g')
    compile "$c" -fsyntax-only -DS1_DIAG_SET="$si" -DS1_DIAG_FORM="$fi" "$HERE/forms.cpp" > "$OUT/diag/$safe.log" 2>&1
    grep -m1 -E 'error' "$OUT/diag/$safe.log" | sed -E 's/^[^ ]*: //' > "$OUT/diag/$safe.txt"
  done
done

echo "== 2. limits (limits.cpp) and the injector stub (stub_test.cpp), warnings as errors"
for c in "${CONFIGS[@]}"; do
  for t in limits stub_test; do
    if compile "$c" "${WARN[@]}" -O0 "$HERE/$t.cpp" -o "$OUT/bin/$t-$c" > "$OUT/logs/$t-$c.build.log" 2>&1; then
      if "$OUT/bin/$t-$c" > "$OUT/logs/$t-$c.txt" 2>&1; then
        echo "$c $t: $(tail -1 "$OUT/logs/$t-$c.txt")"
      else
        fail "$c $t: $(tail -1 "$OUT/logs/$t-$c.txt")"
      fi
    else
      fail "$t.cpp does not build on $c: $(grep -m1 error "$OUT/logs/$t-$c.build.log")"
    fi
  done
done
cp "$OUT/logs/limits-gcc14.txt" "$OUT/limits.txt" 2> /dev/null || true

echo "== 3. compile-fail: each intended error is the first error"
for c in "${CONFIGS[@]}"; do
  for f in "$HERE"/compile_fail/*.cpp; do
    name=$(basename "$f" .cpp)
    want=$(sed -n 's#^// expect: ##p' "$f" | head -1)
    log="$OUT/logs/cf-$name-$c.log"
    if compile "$c" -fsyntax-only "$f" > "$log" 2>&1; then
      fail "$c $name compiled, expected $want"
      continue
    fi
    first=$(grep -m1 -E 'error' "$log")
    lines=$(wc -l < "$log")
    if printf '%s' "$first" | grep -q "$want"; then
      printf 'CF|%s|%s|%s|%s\n' "$c" "$name" "$lines" "ok" >> "$OUT/compile_fail.txt"
    else
      printf 'CF|%s|%s|%s|%s\n' "$c" "$name" "$lines" "first error lacks $want" >> "$OUT/compile_fail.txt"
      fail "$c $name: first error does not contain $want: $first"
    fi
  done
  echo "$c: $(grep -c "^CF|$c|.*|ok$" "$OUT/compile_fail.txt") of $(ls "$HERE"/compile_fail/*.cpp | wc -l) compile-fail cases give the intended error first"
done

echo "== 4. arity probing cost (bench/gen_bench.py, -fsyntax-only, $REPS runs each)"
# name | generator variant | defines | classes
VARIANTS=(
  "baseline|baseline||100"
  "explicit|explicit||100"
  "search-flat-16|search|-DS1_SEARCH=0 -DS1_MAX_ARITY=16|100"
  "search-rec-16|search|-DS1_SEARCH=1 -DS1_MAX_ARITY=16|100"
  "search-fold-16|search|-DS1_SEARCH=2 -DS1_MAX_ARITY=16|100"
  "search-flat-8|search|-DS1_SEARCH=0 -DS1_MAX_ARITY=8|100"
  "discover-flat-16|discover|-DS1_SEARCH=0 -DS1_MAX_ARITY=16|100"
  "baseline-1000|baseline||1000"
  "discover-flat-16-1000|discover|-DS1_SEARCH=0 -DS1_MAX_ARITY=16|1000"
)
for spec in "${VARIANTS[@]}"; do
  IFS='|' read -r name gen _ n <<< "$spec"
  python3 "$HERE/bench/gen_bench.py" "$gen" "$n" "$OUT/gen/$name.cpp"
done
for c in "${CONFIGS[@]}"; do
  printf 'variant\trun\tseconds\tkib\n' > "$OUT/bench/$c.tsv"
  for spec in "${VARIANTS[@]}"; do
    IFS='|' read -r name _ defines _ <<< "$spec"
    for run in $(seq 1 "$REPS"); do
      # timeit.py applies the same time and memory caps as capped().
      # shellcheck disable=SC2046,SC2086
      if ! python3 "$HERE/bench/timeit.py" "$OUT/bench/$c.tsv" "$name" "$run" "$OUT/logs/bench-$name-$c.log" -- \
        "$(cxx_of "$c")" -std=c++20 $(stdlib_of "$c") -fsyntax-only -I"$HERE" $defines "$OUT/gen/$name.cpp"; then
        fail "bench $name does not compile on $c (see out/logs/bench-$name-$c.log)"
        break
      fi
    done
  done
  echo "$c: timings in out/bench/$c.tsv"
done

echo "== 5. summary (out/summary.md)"
python3 "$HERE/summarize.py" "$OUT" "${CONFIGS[@]}" > "$OUT/summary.md"
{
  echo
  echo "### Limits (limits.cpp; identical on every configuration except where noted in the source)"
  echo
  echo "| Case | Status | Arity | Arity - 1 also viable |"
  echo "|---|---|---|---|"
  grep '^LIMIT|' "$OUT/limits.txt" | awk -F'|' '{printf "| %s | %s | %s | %s |\n", $2, $3, $4, $5}'
  echo
  echo "### Compile-fail cases (first error contains the ID; total lines of output)"
  echo
  printf '| Case |'
  printf ' %s |' "${CONFIGS[@]}"
  printf '\n|---|'
  printf -- '---|%.0s' "${CONFIGS[@]}"
  printf '\n'
  for f in "$HERE"/compile_fail/*.cpp; do
    name=$(basename "$f" .cpp)
    row="| $name ($(sed -n 's#^// expect: ##p' "$f" | head -1))"
    for c in "${CONFIGS[@]}"; do
      cell=$(grep "^CF|$c|$name|" "$OUT/compile_fail.txt" | awk -F'|' '{print ($5=="ok" ? "ok, " $4 " lines" : $5)}')
      row="$row | ${cell:-missing}"
    done
    echo "$row |"
  done
} >> "$OUT/summary.md"
cat "$OUT/summary.md"

echo
if [ "$failures" -eq 0 ]; then
  echo "S1: all checks passed on ${CONFIGS[*]}"
  exit 0
fi
echo "S1: $failures check(s) failed"
exit 1
