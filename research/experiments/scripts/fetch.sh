#!/usr/bin/env bash
# Clone the pinned versions of every DI framework into <deps-dir>/<name>.
# Usage: scripts/fetch.sh <deps-dir>
#
# Each framework goes into its own new, empty directory. The script never runs
# anything that ships inside the downloaded trees; it only uses git to fetch
# them and then verifies that the checked-out commit is the pinned one.
set -euo pipefail

DEPS="${1:?usage: fetch.sh <deps-dir>}"
mkdir -p "$DEPS"
DEPS="$(cd "$DEPS" && pwd -P)"

# name | clone URL | tag | commit the tag must resolve to
PINS=(
  "boost_di|https://github.com/boost-ext/di.git|v1.3.2|20b6454c889f23c1e7c9eb5136cd7e3d6f6d718d"
  "fruit|https://github.com/google/fruit.git|v3.7.1|8149131119475ccaae3c1ae386fe9f2c679317f0"
  "kangaru|https://github.com/gracicot/kangaru.git|v4.3.2|ab498eb9db0de74ffebc46f395b9a31093017412"
  "dingo|https://github.com/romanpauk/dingo.git|v0.2.0|ccafcb6cbde54fb2819a3da8243106642f3b1711"
  "hypodermic|https://github.com/ybainier/Hypodermic.git|v2.5.3|0e0d85d70aa2f2391dfd84f8af4a3863d4fb1611"
  "qlibs_di|https://github.com/qlibs/di.git|v1.0.6|bdafce9baa19dfc503d6acd5f5abf91f503fa5ef"
)

for pin in "${PINS[@]}"; do
  IFS='|' read -r name url tag commit <<<"$pin"
  dest="$DEPS/$name"
  if [ -d "$dest/.git" ]; then
    have="$(git --git-dir="$dest/.git" rev-parse HEAD)"
    if [ "$have" = "$commit" ]; then
      echo "[fetch] $name already at $tag ($commit)"
      continue
    fi
    echo "[fetch] ERROR: $dest exists but is at $have, expected $commit" >&2
    exit 1
  fi
  if [ -e "$dest" ] && [ -n "$(ls -A "$dest" 2>/dev/null)" ]; then
    echo "[fetch] ERROR: $dest exists and is not empty" >&2
    exit 1
  fi
  mkdir -p "$dest"
  echo "[fetch] cloning $name $tag from $url"
  # No --recursive: submodules are never fetched.
  git -c advice.detachedHead=false clone --quiet --depth 1 --branch "$tag" "$url" "$dest"
  have="$(git --git-dir="$dest/.git" rev-parse HEAD)"
  if [ "$have" != "$commit" ]; then
    echo "[fetch] ERROR: $name tag $tag resolved to $have, expected $commit (tag moved?)" >&2
    exit 1
  fi
  echo "[fetch] $name OK at $commit"
done

# Record what was fetched next to the sources.
{
  echo "# name tag commit commit-date"
  for pin in "${PINS[@]}"; do
    IFS='|' read -r name url tag commit <<<"$pin"
    date="$(git --git-dir="$DEPS/$name/.git" log -1 --format=%cs HEAD)"
    echo "$name $tag $commit $date"
  done
} > "$DEPS/VERSIONS.txt"
cat "$DEPS/VERSIONS.txt"
