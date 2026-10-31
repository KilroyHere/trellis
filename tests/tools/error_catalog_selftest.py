#!/usr/bin/env python3
# Self-test of tools/error_catalog.py, the error-catalog check.
# Copyright (c) 2026 Aryan Patel
# SPDX-License-Identifier: BSL-1.0
"""Self-test of tools/error_catalog.py: run the check on small trees whose verdict is known.

Each case builds a tree in a temporary directory, with files under include/ and a catalog in
docs/errors/ (an index, a template and pages), and runs the check on it with --root. The check
must fail when include/ uses an error ID that has no page, in a static_assert message or a doc
comment, in any file under include/; when include/ uses the placeholder TRL-E000 or a malformed
ID; when a page isn't linked from the index; and when there is no index. It must pass a complete
catalog, ignore text that only looks like an ID, and read nothing outside include/.
CTest runs this as tools.error_catalog_selftest.
"""
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
CHECK = ROOT / "tools" / "error_catalog.py"
INDEX_HEAD = "# Error catalog\n\n| ID | Meaning | Status |\n|---|---|---|\n"


def assertion(error_id):
    """A line of a header that raises ERROR_ID the way plan §7.8 renders errors."""
    return f'  static_assert(always_false<T>, "{error_id}: no binding for an abstract type");\n'


def run_check(files, pages=(), linked=None, index=True):
    """Build a tree with FILES ({path relative to the root: text}), a page docs/errors/<ID>.md
    for each ID in PAGES and an index that links the IDs in LINKED (default: PAGES), or no index;
    then run the check on it. Returns its exit status and its output."""
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        for name, text in files.items():
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text, encoding="utf-8")
        errors = root / "docs" / "errors"
        errors.mkdir(parents=True, exist_ok=True)
        (errors / "_template.md").write_text("# TRL-E0xx: <meaning>\n", encoding="utf-8")
        for page in pages:
            (errors / f"{page}.md").write_text(f"# {page}: meaning\n", encoding="utf-8")
        if index:
            rows = "".join(f"| [{i}]({i}.md) | meaning | Not yet emitted |\n"
                           for i in (pages if linked is None else linked))
            (errors / "README.md").write_text(INDEX_HEAD + rows, encoding="utf-8")
        result = subprocess.run([sys.executable, "-I", str(CHECK), "--root", str(root)],
                                capture_output=True, text=True, timeout=60)
        return result.returncode, result.stdout + result.stderr


class ErrorCatalogCheckTest(unittest.TestCase):
    def assert_passes(self, result):
        status, output = result
        self.assertEqual(status, 0, output)
        self.assertTrue(output.startswith("PASS error catalog: "), output)

    def assert_fails(self, result, *fragments):
        status, output = result
        self.assertEqual(status, 1, output)
        self.assertTrue(output.startswith("FAIL error catalog: "), output)
        for fragment in fragments:
            self.assertIn(fragment, output)

    def test_a_complete_catalog_passes(self):
        self.assert_passes(run_check({
            "include/trellis/create.hpp": "template <class T>\nvoid create() {\n"
                                          + assertion("TRL-E001") + "}\n",
            "include/trellis/lazy.hpp": "/// @par Errors\n/// - `TRL-R004`: re-entrant.\n",
        }, pages=["TRL-E001", "TRL-R004"]))

    def test_a_planted_id_without_a_page_fails(self):
        self.assert_fails(run_check({
            "include/trellis/create.hpp": "template <class T>\nvoid create() {\n"
                                          + assertion("TRL-E013") + "}\n",
        }, pages=["TRL-E001"]), "include/trellis/create.hpp:3: TRL-E013 has no page",
            "docs/errors/TRL-E013.md")

    def test_an_id_right_after_an_escape_needs_a_page_too(self):
        # A C escape such as \n or \t ends in a letter, so a plain word-boundary check misses the ID.
        self.assert_fails(run_check({
            "include/trellis/create.hpp": 'static_assert(false, "context\\nTRL-E013: x");\n'
                                          'inline constexpr const char* note = "\\tTRL-R006";\n',
        }, pages=["TRL-E001"]), "include/trellis/create.hpp:1: TRL-E013 has no page",
            "include/trellis/create.hpp:2: TRL-R006 has no page")

    def test_an_id_in_a_doc_comment_needs_a_page_too(self):
        self.assert_fails(run_check({
            "include/trellis/scope.hpp": "/// @par Errors\n/// - `TRL-E011`: outside its scope.\n",
        }), "include/trellis/scope.hpp:2: TRL-E011 has no page")

    def test_every_file_under_include_is_read(self):
        self.assert_fails(run_check({
            "include/trellis/detail/explain.ipp": "// reports TRL-E006\n",
        }), "include/trellis/detail/explain.ipp:1: TRL-E006 has no page")

    def test_a_missing_page_is_reported_once_at_its_first_use(self):
        status, output = run_check({
            "include/trellis/a.hpp": "\n// TRL-E013\n" + assertion("TRL-E013"),
            "include/trellis/b.hpp": assertion("TRL-E013"),
        })
        self.assert_fails((status, output), "include/trellis/a.hpp:2 (and 2 more): TRL-E013")
        self.assertEqual(len(output.splitlines()), 1, output)

    def test_a_page_missing_from_the_index_fails(self):
        self.assert_fails(run_check({"include/trellis/create.hpp": assertion("TRL-E001")},
                                    pages=["TRL-E001", "TRL-E002"], linked=["TRL-E001"]),
                          "docs/errors/TRL-E002.md is not linked from the index")

    def test_a_missing_index_fails(self):
        self.assert_fails(run_check({"include/trellis/create.hpp": assertion("TRL-E001")},
                                    pages=["TRL-E001"], index=False),
                          "docs/errors/README.md")

    def test_a_malformed_id_fails(self):
        self.assert_fails(run_check({
            "include/trellis/create.hpp": assertion("TRL-E0012") + assertion("TRL-X001"),
        }, pages=["TRL-E001"]), "TRL-E0012 is not an error ID", "TRL-X001 is not an error ID")

    def test_the_placeholder_id_is_for_tooling_tests_only(self):
        self.assert_fails(run_check({"include/trellis/create.hpp": assertion("TRL-E000")},
                                    pages=["TRL-E000"]),
                          "include/trellis/create.hpp:1: TRL-E000 is the placeholder")

    def test_text_that_only_looks_like_an_id_is_ignored(self):
        self.assert_passes(run_check({
            "include/trellis/config.hpp": "// IDs look like TRL-E0xx or TRL-R0xx (plan §7.8).\n"
                                          "// Not IDs: XTRL-E013, TRL-E013-draft, TRL-E01a.\n",
        }))

    def test_only_include_is_read(self):
        self.assert_passes(run_check({
            "include/trellis/config.hpp": "#pragma once\n",
            "tests/tools/fixture.cpp": "// expect-id: TRL-E999\n",
            "docs/plan.md": "TRL-E013 is planned.\n",
        }))


if __name__ == "__main__":
    unittest.main(verbosity=2)
