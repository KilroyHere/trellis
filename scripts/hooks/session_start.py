#!/usr/bin/env python3
"""SessionStart hook: remind Claude of the session protocol and name the next task.

Runs at startup, on resume and after compaction. What it prints is added to
Claude's context, so it stays to two lines.
"""
import json
import os
import sys
from pathlib import Path


def main():
    project = Path(os.environ.get("CLAUDE_PROJECT_DIR") or ".").resolve()
    print("Trellis: follow the session protocol in CLAUDE.md: one task, test first, "
          "`scripts/tasks.py verify` for evidence, reviewer subagent, then pass, PROGRESS entry and commit.")
    try:
        tasks = json.loads((project / "docs" / "tasks.json").read_text())["tasks"]
    except (OSError, ValueError, KeyError):
        return 0
    done = {t["id"] for t in tasks if t.get("passes")}
    order = sorted(range(len(tasks)), key=lambda i: (tasks[i]["phase"], i))
    upcoming = next((tasks[i] for i in order
                     if not tasks[i].get("passes") and all(d in done for d in tasks[i]["depends_on"])), None)
    summary = f"Tasks passing: {len(done)}/{len(tasks)}."
    if upcoming:
        summary += f" Next ready task: {upcoming['id']} {upcoming['title']}."
    print(summary)
    return 0


if __name__ == "__main__":
    sys.exit(main())
