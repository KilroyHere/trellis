#!/usr/bin/env bash
# CI status from GitHub Actions, through gh: the latest run of each workflow on a branch, and the
# first errors of every failed job (from gh run view --log-failed, trimmed).
#   scripts/ci-status.sh [--wait] [BRANCH]
# BRANCH defaults to the branch the current one pushes to (its upstream), else the current one.
# The verdict comes from the ci workflow (ci.yml) run on push: for the current branch, the latest
# such run for HEAD; for a BRANCH given by name, the latest one. --wait first waits, for up to
# TRELLIS_CI_WAIT seconds (default 1800), until that run exists and has finished.
# Exit status: 0 when the verdict run passed, 1 when it failed, 2 when there is no verdict yet (no
# run, still running, HEAD not pushed) or gh failed. GH_REPO=owner/name selects another repository.
set -uo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/lib/common.sh"
cd "$TRELLIS_ROOT" || exit 2

ERROR_LINES=10  # error lines shown per failed job
# Lines worth showing from a failed step: compiler, CMake, ninja, CTest, GoogleTest, sanitizer and
# tools/*.py errors ("FAIL name: reason"). Compile commands also match (-Werror) and are dropped.
ERROR_PATTERN='error[: ]|Error[: ]|ERROR|FAILED|(^|[^[:alnum:]_])FAIL([^[:alnum:]_]|$)|\((Failed|Timeout|Exception|SEGFAULT|Subprocess aborted)\)|\*\*\*(Failed|Timeout|Exception)|Sanitizer|runtime error|Errors while running'
COMPILE_COMMAND=' -c .* -o | /Fo'
# gh's JSON as tab-separated fields. Empty fields become "-": bash's read merges adjacent tabs.
JQ_FIELDS='def field: if . == null or . == "" then "-" else tostring end;'

wait_for_run=0
branch=""
for arg in "$@"; do
  case $arg in
    --wait) wait_for_run=1 ;;
    -h | --help)
      awk 'NR > 1 && /^#/ { sub(/^# ?/, ""); print; next } NR > 1 { exit }' "${BASH_SOURCE[0]}"
      exit 0
      ;;
    -*)
      echo "ci-status: unknown option $arg (see --help)" >&2
      exit 2
      ;;
    *) branch=$arg ;;
  esac
done

if ! command -v gh >/dev/null; then
  echo "ci-status: gh (the GitHub CLI) is not installed" >&2
  exit 2
fi

# For the current branch, the verdict is about HEAD; for a named branch, about its newest run.
head_sha=""
if [[ -z $branch ]]; then
  local_branch=$(git branch --show-current)
  if [[ -z $local_branch ]]; then
    echo "ci-status: HEAD is detached; name the branch: scripts/ci-status.sh BRANCH" >&2
    exit 2
  fi
  merge=$(git config "branch.$local_branch.merge" || true)
  branch=${merge#refs/heads/}
  branch=${branch:-$local_branch}
  head_sha=$(git rev-parse HEAD) || exit 2
fi

# verdict_run: "id status conclusion sha url" (tab-separated) of the ci.yml push run that decides,
# or nothing when there is none yet. Runs are matched by the workflow's name, "ci", rather than with
# gh run list --workflow, which looks the file up on the default branch only.
verdict_run() {
  local -a filter=(--branch "$branch" --event push --limit 20)
  [[ -z $head_sha ]] || filter+=(--commit "$head_sha")
  gh run list "${filter[@]}" --json databaseId,workflowName,status,conclusion,headSha,url \
    --jq "$JQ_FIELDS"' [.[] | select(.workflowName == "ci")][0] // empty |
          [.databaseId, .status, .conclusion, .headSha, .url] | map(field) | @tsv'
}

if ! verdict=$(verdict_run); then
  echo "ci-status: gh run list failed" >&2
  exit 2
fi

if ((wait_for_run)) && [[ $(cut -f2 <<<"$verdict") != completed ]]; then
  if [[ -n $head_sha ]] && ! git merge-base --is-ancestor HEAD '@{upstream}' 2>/dev/null; then
    printf 'ci: HEAD %s is not pushed to %s; nothing to wait for\n' "${head_sha:0:7}" "$branch"
    exit 2
  fi
  deadline=$((SECONDS + ${TRELLIS_CI_WAIT:-1800}))
  announced=""
  while [[ $(cut -f2 <<<"$verdict") != completed ]] && ((SECONDS < deadline)); do
    if [[ -n $verdict && -z $announced ]]; then
      printf 'waiting for %s\n' "$(cut -f5 <<<"$verdict")"
      announced=1
    fi
    sleep 20
    verdict=$(verdict_run) || verdict=""
  done
fi

# The newest run of each workflow on the branch, newest first, for the summary.
if ! latest=$(gh run list --branch "$branch" --limit 50 \
  --json databaseId,workflowName,status,conclusion,headSha,event,createdAt,url \
  --jq "$JQ_FIELDS"' unique_by(.workflowName) | sort_by(.createdAt) | reverse | .[] |
        [.databaseId, .workflowName, .status, .conclusion, .headSha[0:7], .event, .createdAt,
         .url] | map(field) | @tsv'); then
  echo "ci-status: gh run list failed" >&2
  exit 2
fi

printf 'CI on branch %s' "$branch"
[[ -z $head_sha ]] || printf ' (HEAD %s)' "${head_sha:0:7}"
printf '\n'
[[ -n $latest ]] || echo "  no workflow runs on this branch"
failed_runs=()
while IFS=$'\t' read -r id workflow status conclusion sha event created url; do
  [[ -n $id ]] || continue
  result=$conclusion
  [[ $result != - ]] || result=$status
  printf '  %-10s %-12s %s  %-18s %s  %s\n' "$workflow" "$result" "$sha" "$event" "$created" "$url"
  case $conclusion in
    failure | timed_out | startup_failure) failed_runs+=("$id $workflow") ;;
  esac
done <<<"$latest"
# HEAD's ci run can be older than the branch's newest one; show its errors too.
IFS=$'\t' read -r verdict_id _ verdict_conclusion _ <<<"$verdict"
case $verdict_conclusion in
  failure | timed_out | startup_failure)
    [[ " ${failed_runs[*]} " == *" $verdict_id ci "* ]] || failed_runs+=("$verdict_id ci")
    ;;
esac

# print_failures RUN_ID WORKFLOW: each failed job, its failed steps and its first error lines.
print_failures() {
  local run=$1 workflow=$2 jobs log name conclusion steps lines errors
  if ! jobs=$(gh run view "$run" --json jobs --jq "$JQ_FIELDS"' .jobs[] |
      select(.conclusion == "failure" or .conclusion == "timed_out" or .conclusion == "cancelled")
      | [.name, .conclusion, ([.steps[] | select(.conclusion == "failure") | .name] | join(", "))]
      | map(field) | @tsv'); then
    printf '\n%s run %s: could not list its jobs\n' "$workflow" "$run"
    return
  fi
  if ! log=$(gh run view "$run" --log-failed); then
    printf '\n%s run %s: could not fetch the logs of its failed steps\n' "$workflow" "$run"
    log=""
  fi
  while IFS=$'\t' read -r name conclusion steps; do
    [[ -n $name ]] || continue
    printf '\n%s / %s: %s' "$workflow" "$name" "$conclusion"
    [[ $steps == - ]] || printf ' in step %s' "$steps"
    printf '\n'
    # --log-failed prints "job<TAB>step<TAB>timestamp line"; keep the line without the timestamp.
    lines=$(awk -F '\t' -v job="$name" '$1 == job { sub(/^[^\t]*\t[^\t]*\t/, ""); print }' <<<"$log" |
      sed -E 's/^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9:.]+Z ?//; s/^##\[error\]//' |
      grep -v -E '^##\[(group|endgroup)\]')
    errors=$(grep -E -- "$ERROR_PATTERN" <<<"$lines" | grep -v -E -- "$COMPILE_COMMAND")
    # Nothing recognizable: the end of the log is the next best thing.
    [[ -n $errors ]] || errors=$(grep -v '^[[:space:]]*$' <<<"$lines" | tail -n 5)
    head -n "$ERROR_LINES" <<<"$errors" | cut -c1-200 | sed 's/^/    /'
  done <<<"$jobs"
}

for entry in "${failed_runs[@]}"; do
  print_failures "${entry%% *}" "${entry#* }"
done

printf '\n'
if [[ -z $verdict ]]; then
  if [[ -n $head_sha ]]; then
    printf 'ci: no run for HEAD %s on %s yet; push it, or wait (--wait)\n' "${head_sha:0:7}" "$branch"
  else
    printf 'ci: no push run on %s yet\n' "$branch"
  fi
  exit 2
fi
IFS=$'\t' read -r _ status conclusion sha url <<<"$verdict"
if [[ $status != completed ]]; then
  printf 'ci: %s for %s, %s\n' "$status" "${sha:0:7}" "$url"
  exit 2
fi
if [[ $conclusion == success ]]; then
  printf 'ci: passed for %s, %s\n' "${sha:0:7}" "$url"
  exit 0
fi
printf 'ci: %s for %s, %s\n' "$conclusion" "${sha:0:7}" "$url"
exit 1
