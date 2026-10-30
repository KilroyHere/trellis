#!/usr/bin/env python3
"""Stop hook: before Claude ends its turn, run the fast check if code or build files changed.

Off unless TRELLIS_VERIFY_ON_STOP=1 (set it in .claude/settings.local.json under
"env"), so that sessions coordinating other agents aren't blocked mid-task.
Exit code 2 sends the failure back to Claude, which keeps working. When
stop_hook_active is set, Claude is already continuing because of this hook, so we
let it stop instead of looping.
"""
import json
import os
import subprocess
import sys
from pathlib import Path

WATCHED = ("include/", "tests/", "benchmarks/", "modules/", "cmake/", "tools/",
           "CMakeLists.txt", "CMakePresets.json")


def main():
    if os.environ.get("TRELLIS_VERIFY_ON_STOP") != "1":
        return 0
    try:
        event = json.load(sys.stdin)
    except json.JSONDecodeError:
        event = {}
    if event.get("stop_hook_active"):
        return 0
    project = Path(os.environ.get("CLAUDE_PROJECT_DIR") or event.get("cwd") or ".").resolve()
    check = project / "scripts" / "check-fast.sh"
    if not check.is_file():
        return 0
    status = subprocess.run(["git", "status", "--porcelain"], cwd=project,
                            capture_output=True, text=True).stdout
    changed = [line[3:] for line in status.splitlines()]
    if not any(name.startswith(WATCHED) for name in changed):
        return 0
    try:
        proc = subprocess.run([str(check)], cwd=project, capture_output=True, text=True, timeout=1500)
    except subprocess.TimeoutExpired:
        print("scripts/check-fast.sh did not finish within 25 minutes.", file=sys.stderr)
        return 2
    if proc.returncode == 0:
        return 0
    tail = "\n".join((proc.stdout + proc.stderr).splitlines()[-40:])
    print(f"scripts/check-fast.sh failed (exit {proc.returncode}). Fix it, or explain why it "
          f"can't be fixed in this task, before stopping:\n{tail}", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main())
