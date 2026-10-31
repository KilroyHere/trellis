#!/usr/bin/env python3
# The error-catalog check: every error ID used in include/ has a page in docs/errors/.
# Copyright (c) 2026 Aryan Patel
# SPDX-License-Identifier: BSL-1.0
"""Check the error catalog, docs/errors/, against the error IDs that include/ uses.

  tools/error_catalog.py [--root DIR]

An error ID is TRL-, then E (found at compile time) or R (found at run time), then three digits:
TRL-E001, TRL-R004 (plan §7.8). Every ID that appears in a file under include/, whether in a
static_assert message, an exception message or a comment, needs a page docs/errors/<ID>.md, and
the catalog's index, docs/errors/README.md, must link every page. The check reads text, so an ID
counts only where it is written out in full, as a whole token, the way tools/compile_fail.py
matches expect-id: "trellis [TRL-E001]:" and "`TRL-E001`" count; TRL-E0xx, a pattern in prose,
doesn't, and neither does an ID assembled from pieces.

It fails on:
  - an ID used in include/ that has no page;
  - a malformed ID in include/, such as TRL-E0012 or TRL-X001;
  - TRL-E000 in include/: the placeholder ID of the tooling's own tests, which the library never
    emits (docs/errors/TRL-E000.md);
  - a page, docs/errors/TRL-*.md, that the index doesn't link, or a missing index.
IDs that nothing emits yet need no page: the index lists them, with links, ahead of time.

CTest runs this on the repository as tools.error_catalog, in every build, so scripts/check-fast.sh
runs it too. --root checks another tree with the same layout: the self-test,
tests/tools/error_catalog_selftest.py, plants IDs in temporary trees and checks them that way.

Exit status: 0 when the catalog is complete, 1 when it isn't, 2 for a usage error.
"""
import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
# Anything shaped like an error ID: TRL-, a capital letter, digits, and nothing word-like or "-"
# on either side, so that TRL-E001 is found in "trellis [TRL-E001]:" but TRL-E0xx isn't an ID.
ID_LIKE = re.compile(r"(?<![\w-])TRL-[A-Z]\d+(?![\w-])")
ERROR_ID = re.compile(r"TRL-[ER]\d{3}")  # the shape tools/compile_fail.py accepts for expect-id
PLACEHOLDER = "TRL-E000"
# The index's link to a page: ](TRL-E001.md), also as ](./TRL-E001.md) or with an #anchor.
PAGE_LINK = re.compile(r"\]\(\s*(?:\./)?(TRL-[ER]\d{3})\.md(?:#[^)\s]*)?\s*\)")


def find_ids(include_dir):
    """Every ID-like token in the files under INCLUDE_DIR: {token: [(path, line), ...]}, with the
    places in path order."""
    uses = {}
    for path in sorted(p for p in include_dir.rglob("*") if p.is_file()):
        text = path.read_text(encoding="utf-8", errors="replace")
        for number, line in enumerate(text.splitlines(), start=1):
            for token in ID_LIKE.findall(line):
                uses.setdefault(token, []).append((path, number))
    return uses


def check(root):
    """The catalog's problems under ROOT, one message each, and the summary of a clean check."""
    def show(path):
        return path.relative_to(root).as_posix()

    errors_dir = root / "docs" / "errors"
    index = errors_dir / "README.md"
    problems = []
    used = []
    for token, places in sorted(find_ids(root / "include").items()):
        path, line = places[0]
        where = f"{show(path)}:{line}"
        if len(places) > 1:
            where += f" (and {len(places) - 1} more)"
        if not ERROR_ID.fullmatch(token):
            problems.append(f"{where}: {token} is not an error ID; IDs look like TRL-E001 "
                            f"(compile time) or TRL-R001 (run time)")
        elif token == PLACEHOLDER:
            problems.append(f"{where}: {token} is the placeholder ID of the tooling's own tests, "
                            f"and the library must not use it ({show(errors_dir)}/{token}.md)")
        elif not (errors_dir / f"{token}.md").is_file():
            problems.append(f"{where}: {token} has no page {show(errors_dir)}/{token}.md; start "
                            f"one from {show(errors_dir)}/_template.md and link it from the index")
        else:
            used.append(token)

    pages = sorted(path.stem for path in errors_dir.glob("TRL-*.md"))
    if not index.is_file():
        problems.append(f"{show(index)} is missing: it is the catalog's index, which links "
                        f"every page")
    else:
        linked = set(PAGE_LINK.findall(index.read_text(encoding="utf-8", errors="replace")))
        problems += [f"{show(errors_dir)}/{page}.md is not linked from the index, {show(index)}"
                     for page in pages if page not in linked]

    def count(number, noun):
        return f"{number} {noun}{'' if number == 1 else 's'}"

    ids = (f"include/ uses {count(len(used), 'error ID')} ({', '.join(used)}), each with a page"
           if used else "include/ uses no error ID yet")
    summary = (f"{ids}; {count(len(pages), 'page')} in {show(errors_dir)}/, all linked from "
               f"the index")
    return problems, summary


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--root", type=Path, default=ROOT,
                        help="the tree to check (default: this repository)")
    args = parser.parse_args(argv)
    root = args.root.resolve()
    if not (root / "include").is_dir():
        print(f"error: {root} has no include/ directory", file=sys.stderr)
        return 2
    problems, summary = check(root)
    for problem in problems:
        print(f"FAIL error catalog: {problem}")
    if problems:
        return 1
    print(f"PASS error catalog: {summary}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
