#!/usr/bin/env bash
# Configure/build the two frameworks that cannot be used straight from their
# source tree. Everything is configured out-of-tree with `cmake -S <src> -B
# <build>`; nothing is executed from inside the downloaded directories.
#
#   * Fruit: a compiled library. Built as a static library, without Boost,
#     once per compiler and C++ standard: <build>/fruit-<cc>-<std>/
#     (headers: <deps>/fruit/include + <build>/fruit-<cc>-<std>/include,
#      library: <build>/fruit-<cc>-<std>/src/libfruit.a)
#   * Kangaru: header-only, but needs a CMake-generated config header:
#     <build>/kangaru-config/generated/include/kangaru/detail/config.hpp
#
# Usage: scripts/prepare_deps.sh <deps-dir> <build-dir>
set -euo pipefail

DEPS="$(cd "${1:?usage: prepare_deps.sh <deps-dir> <build-dir>}" && pwd -P)"
mkdir -p "${2:?usage: prepare_deps.sh <deps-dir> <build-dir>}"
BUILD="$(cd "$2" && pwd -P)"
GEN="Unix Makefiles"
command -v ninja >/dev/null 2>&1 && GEN="Ninja"

for cc in gcc clang; do
  if [ "$cc" = gcc ]; then cxx=g++; else cxx=clang++; fi
  for std in 17 20; do
    dir="$BUILD/fruit-$cc-$std"
    if [ -f "$dir/src/libfruit.a" ]; then
      echo "[prepare] fruit $cc c++$std already built"
      continue
    fi
    echo "[prepare] building fruit with $cxx -std=c++$std"
    start=$(date +%s.%N)
    cmake -S "$DEPS/fruit" -B "$dir" -G "$GEN" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER="$cxx" \
      -DCMAKE_CXX_STANDARD="$std" \
      -DCMAKE_CXX_FLAGS_RELEASE="-O2 -DNDEBUG" \
      -DBUILD_SHARED_LIBS=OFF \
      -DFRUIT_USES_BOOST=False \
      -DFRUIT_TESTS_USE_PRECOMPILED_HEADERS=OFF \
      > "$dir.configure.log" 2>&1 || { tail -30 "$dir.configure.log"; exit 1; }
    # Only the library target: Fruit's "all" target would also build test PCHs.
    cmake --build "$dir" --target fruit -j "$(nproc)" > "$dir.build.log" 2>&1 \
      || { tail -30 "$dir.build.log"; exit 1; }
    end=$(date +%s.%N)
    secs="$(awk -v a="$start" -v b="$end" 'BEGIN { printf "%.2f", b - a }')"
    echo "[prepare] fruit $cc c++$std configured+built in ${secs}s (wall, -j$(nproc))"
    echo "$secs" > "$dir.build_seconds"
  done
done

dir="$BUILD/kangaru-config"
out="$dir/generated/include/kangaru/detail/config.hpp"
if [ ! -f "$out" ]; then
  # Kangaru's CMake configure step would generate this header from
  # cmake/config/config.hpp.in. To avoid running Kangaru's own CMake scripts,
  # do the same substitution here, treating the template as data
  # (configure_file semantics: "#cmakedefine VAR" -> "#define VAR" when the
  # option is ON, "/* #undef VAR */" otherwise). Options used:
  # KANGARU_REVERSE_DESTRUCTION=ON (recommended by Kangaru), the others OFF.
  echo "[prepare] generating kangaru detail/config.hpp from config.hpp.in"
  mkdir -p "$(dirname "$out")"
  python3 -I - "$DEPS/kangaru/cmake/config/config.hpp.in" "$out" <<'PY'
import re, sys
enabled = {"KGR_KANGARU_REVERSE_DESTRUCTION"}
src, dst = sys.argv[1], sys.argv[2]
def sub(m):
    var = m.group(1)
    return f"#define {var}" if var in enabled else f"/* #undef {var} */"
text = re.sub(r"^#cmakedefine (\w+)$", sub, open(src).read(), flags=re.M)
open(dst, "w").write(text)
PY
fi
echo "[prepare] kangaru config: $out"
