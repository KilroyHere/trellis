#!/usr/bin/env python3
"""PostToolUse hook: clang-format a C++ file right after Claude edits it.

Formatting is fixed outside the model, so no tokens go to style. The version is
pinned (clang-format-18) because different versions format differently; the CI
format check uses the same version.
"""
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

CPP_SUFFIXES = {".h", ".hpp", ".hh", ".ipp", ".tpp", ".cc", ".cpp", ".cxx", ".cppm", ".ixx"}
SKIPPED = ("research/", "build/", "out/")


def main():
    try:
        event = json.load(sys.stdin)
    except json.JSONDecodeError:
        return 0
    file_path = (event.get("tool_input") or {}).get("file_path")
    if not file_path:
        return 0
    project = Path(os.environ.get("CLAUDE_PROJECT_DIR") or event.get("cwd") or ".").resolve()
    path = Path(file_path).resolve()
    try:
        rel = path.relative_to(project).as_posix()
    except ValueError:
        return 0
    if path.suffix not in CPP_SUFFIXES or rel.startswith(SKIPPED) or not path.is_file():
        return 0
    if not (project / ".clang-format").is_file():
        return 0
    clang_format = shutil.which("clang-format-18") or shutil.which("clang-format")
    if clang_format:
        subprocess.run([clang_format, "-i", "--style=file", str(path)], cwd=project,
                       timeout=60, check=False)
    return 0


if __name__ == "__main__":
    sys.exit(main())
