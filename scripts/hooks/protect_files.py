#!/usr/bin/env python3
"""PreToolUse hook: block direct edits to files that may only change through a reviewed path.

Claude Code passes the pending tool call as JSON on stdin. Edit and Write calls on
the paths below are denied with a reason that names the supported route. New files
may still be created under the test directories. This is a guardrail, not a security
boundary: it exists so that "make the test pass" never quietly becomes "change the test".
"""
import json
import os
import sys
from pathlib import Path

# (path or directory prefix, also blocks creating new files, reason)
RULES = [
    ("tests/compile_fail/", False,
     "existing compile-fail tests are protected. If one is wrong, stop and explain why in "
     "docs/PROGRESS.md; the owner decides."),
    ("tests/diagnostics_golden/", False,
     "golden diagnostics change only through the update-golden skill "
     "(tools/golden.py --update --reason ...), run at the owner's request."),
    ("benchmarks/compile/budgets.json", False,
     "compile-time budgets need owner sign-off. Propose the change in docs/PROGRESS.md instead."),
    ("docs/tasks.json", True,
     "change the task list with scripts/tasks.py (verify, pass, add); tasks are never removed or reworded."),
    (".claude/settings.json", False, "hook and permission settings are maintained by the repository owner."),
    ("scripts/hooks/", False, "hook scripts are maintained by the repository owner."),
]


def deny(reason):
    json.dump({"hookSpecificOutput": {"hookEventName": "PreToolUse",
                                      "permissionDecision": "deny",
                                      "permissionDecisionReason": reason}}, sys.stdout)


def main():
    try:
        event = json.load(sys.stdin)
    except json.JSONDecodeError:
        return 0
    tool_input = event.get("tool_input") or {}
    file_path = tool_input.get("file_path") or tool_input.get("notebook_path")
    if not file_path:
        return 0
    project = Path(os.environ.get("CLAUDE_PROJECT_DIR") or event.get("cwd") or ".").resolve()
    path = Path(file_path)
    if not path.is_absolute():
        path = Path(event.get("cwd") or project) / path
    try:
        rel = path.resolve().relative_to(project).as_posix()
    except ValueError:
        return 0  # outside the repository
    for pattern, blocks_new_files, reason in RULES:
        matched = rel == pattern or (pattern.endswith("/") and rel.startswith(pattern))
        if matched and (blocks_new_files or path.exists()):
            deny(f"{rel}: {reason}")
            break
    return 0


if __name__ == "__main__":
    sys.exit(main())
