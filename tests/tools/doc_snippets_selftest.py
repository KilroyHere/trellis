#!/usr/bin/env python3
# Self-test of tools/doc_snippets.py, the check that a document's C++ blocks are excerpts of files
# that the build compiles.
# Copyright (c) 2026 Aryan Patel
# SPDX-License-Identifier: BSL-1.0
"""Self-test of tools/doc_snippets.py: run the check on small trees whose verdict is known.

Each case writes a document and source files to a temporary directory and runs the check on them
with --root. The check must pass when every C++ block names a source under examples/ or tests/unit/
and shows a run of its lines, with spaces and blank lines not counting, with comment lines of the
source left out, and with code left out through the two elisions; and must fail when a block
changes, adds, reorders or skips a line, shows a prefix of one or drops its trailing comment, has
a line after an elision that isn't the file's, when an elided class body names another class,
when a block names no file, a missing file or a file the build doesn't compile, and when a snippet
comment isn't followed by a C++ block. Fences are read as CommonMark reads them: an info string
whose first word is the language, tildes, fences of four or more characters, and closing fences of
the same character and at least the same length. A fence the check can't classify fails it:
without a language or with an unknown one, never closed, in a block quote, or with a backtick in
the info string. Blocks in other languages aren't checked. The repository's own
docs/getting-started.md must pass. CTest runs this as tools.doc_snippets_selftest.
"""
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
CHECK = ROOT / "tools" / "doc_snippets.py"

SOURCE = """\
// An example.
#include <memory>

// The logger interface.
struct ILogger { virtual ~ILogger() = default; virtual void log(int) = 0; };

class ConsoleLogger final : public ILogger {
 public:
  void log(int value) override {
    last_ = value;
  }

 private:
  int last_ = 0;
};

int main() {
  // Make the injector.
  auto injector = trellis::make_injector();  // a comment on a line of code counts
  return 0;
}
"""

TEST_SOURCE = """\
TEST(Suite, Name) {
  int value = 1;
  EXPECT_EQ(value, 1);
}
"""


def document(*blocks):
    """A document with BLOCKS, each a (path or None, body, language) tuple, between other text."""
    parts = ["# Guide", "", "Some text.", ""]
    for path, body, language in blocks:
        if path is not None:
            parts.append(f"<!-- snippet: {path} -->")
        parts += [f"```{language}", body, "```", "", "More text.", ""]
    return "\n".join(parts)


def block(body, path="examples/demo.cpp", language="cpp"):
    return (path, body, language)


def fenced(body, opening="```cpp", closing="```", path="examples/demo.cpp"):
    """A document with one block, between the fence lines OPENING and CLOSING (none if empty)."""
    lines = ["# Guide", "", f"<!-- snippet: {path} -->", opening, body]
    if closing:
        lines.append(closing)
    return "\n".join(lines + ["", "More text.", ""])


def run_check(text, files=None):
    """Run the check on a document with TEXT, in a tree that holds FILES (by default
    examples/demo.cpp and tests/unit/demo_test.cpp). Returns its exit status and output."""
    files = {"examples/demo.cpp": SOURCE, "tests/unit/demo_test.cpp": TEST_SOURCE} \
        if files is None else files
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        for path, content in files.items():
            (root / path).parent.mkdir(parents=True, exist_ok=True)
            (root / path).write_text(content, encoding="utf-8")
        guide = root / "docs" / "guide.md"
        guide.parent.mkdir(parents=True, exist_ok=True)
        guide.write_text(text, encoding="utf-8")
        result = subprocess.run([sys.executable, "-I", str(CHECK), "--root", str(root),
                                 str(guide)], capture_output=True, text=True, timeout=60)
        return result.returncode, result.stdout + result.stderr


class DocSnippetsCheckTest(unittest.TestCase):
    def assert_passes(self, result):
        status, output = result
        self.assertEqual(status, 0, output)
        self.assertTrue(output.startswith("PASS doc-snippets: "), output)

    def assert_fails(self, result, *fragments, status=1):
        actual, output = result
        self.assertEqual(actual, status, output)
        self.assertTrue(output.startswith("FAIL doc-snippets:"), output)
        for fragment in fragments:
            self.assertIn(fragment, output)

    def test_the_repository_guide_passes(self):
        result = subprocess.run([sys.executable, "-I", str(CHECK)], capture_output=True,
                                text=True, timeout=60)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_a_run_of_lines_passes(self):
        self.assert_passes(run_check(document(block(
            "int main() {\n  // Make the injector.\n"
            "  auto injector = trellis::make_injector();  // a comment on a line of code counts\n"
            "  return 0;\n}"))))

    def test_spaces_blank_lines_and_left_out_comments_dont_count(self):
        self.assert_passes(run_check(document(block(
            "#include <memory>\n\n\nstruct ILogger {  virtual ~ILogger() = default; "
            "virtual void log(int) = 0; };"))))

    def test_blocks_from_several_files_pass(self):
        self.assert_passes(run_check(document(
            block("int main() {"),
            block("  int value = 1;\n  EXPECT_EQ(value, 1);", path="tests/unit/demo_test.cpp"),
            block("cmake_minimum_required(VERSION 3.21)", path=None, language="cmake"),
            block("not code at all", path=None, language="text"))))

    def test_the_two_elisions_pass(self):
        self.assert_passes(run_check(document(block(
            "class ConsoleLogger final : public ILogger { /* ... */ };\n\nint main() {\n"
            "  // ...\n}"))))
        self.assert_passes(run_check(document(block(
            "class ConsoleLogger final : public ILogger {\n public:\n  // ...\n};"))))

    def test_indented_blocks_are_checked(self):
        text = ("- An item:\n\n  <!-- snippet: examples/demo.cpp -->\n  ```cpp\n"
                "  int main() {\n  ```\n")
        self.assert_passes(run_check(text))
        self.assert_fails(run_check(text.replace("int main() {", "int main(int) {")),
                          "int main(int) {")

    def test_a_changed_line_fails(self):
        self.assert_fails(run_check(document(block(
            "int main() {\n  auto injector = trellis::make_injector(1);"))),
            "guide.md:", "block 1", "examples/demo.cpp",
            "auto injector = trellis::make_injector(1);")

    def test_a_trailing_comment_must_match(self):
        self.assert_fails(run_check(document(block(
            "  auto injector = trellis::make_injector();  // another comment"))), "block 1")

    def test_a_line_left_out_without_an_elision_fails(self):
        self.assert_fails(run_check(document(block("int main() {\n  return 0;"))),
                          "return 0;")

    def test_reordered_lines_fail(self):
        self.assert_fails(run_check(document(block("  return 0;\nint main() {"))), "block 1")

    def test_an_added_comment_fails(self):
        self.assert_fails(run_check(document(block(
            "int main() {\n  // A comment the file doesn't have.\n  return 0;"))),
            "A comment the file doesn't have.")

    def test_an_elided_body_must_name_the_class(self):
        self.assert_fails(run_check(document(block(
            "class FileLogger final : public ILogger { /* ... */ };"))), "FileLogger")

    def test_only_the_failing_block_is_reported(self):
        status, output = run_check(document(block("int main() {"), block("int main(int) {")))
        self.assertEqual(status, 1, output)
        self.assertIn("block 2", output)
        self.assertNotIn("block 1", output)

    def test_a_block_that_names_no_file_is_an_error(self):
        self.assert_fails(run_check(document(block("int main() {", path=None))),
                          "names no file", status=2)

    def test_a_missing_file_is_an_error(self):
        self.assert_fails(run_check(document(block("int main() {", path="examples/gone.cpp"))),
                          "examples/gone.cpp", "doesn't exist", status=2)

    def test_a_file_the_build_doesnt_compile_is_an_error(self):
        files = {"tests/compile_fail/bad.cpp": SOURCE, "include/demo.hpp": SOURCE,
                 "examples/notes.txt": SOURCE}
        for path in files:
            self.assert_fails(run_check(document(block("int main() {", path=path)), files),
                              path, "isn't a C++ source", status=2)
        self.assert_fails(run_check(document(block(
            "int main() {", path="examples/../tests/compile_fail/bad.cpp")), files),
            "isn't a C++ source", status=2)

    def test_a_snippet_comment_without_a_cpp_block_is_an_error(self):
        self.assert_fails(run_check(document(block("int main() {", language="text"))),
                          "no C++ block follows", status=2)
        self.assert_fails(run_check(
            "<!-- snippet: examples/demo.cpp -->\n\nSome text.\n\n```cpp\nint main() {\n```\n"),
            "no C++ block follows", "names no file", status=2)

    def test_an_empty_block_fails(self):
        self.assert_fails(run_check(document(block(""))), "is empty")

    # ---- what follows an elision, and whole-line comparison -----------------------------------

    def test_a_line_after_an_elision_must_be_in_the_file(self):
        self.assert_fails(run_check(document(block("int main() {\n  // ...\n  NOT IN THE FILE;"))),
                          "NOT IN THE FILE;")

    def test_a_dropped_trailing_comment_fails(self):
        self.assert_fails(run_check(document(block(
            "int main() {\n  // Make the injector.\n"
            "  auto injector = trellis::make_injector();"))),
            "auto injector = trellis::make_injector();")

    def test_a_prefix_of_a_line_fails(self):
        self.assert_fails(run_check(document(block("int main("))), "int main(")
        self.assert_fails(run_check(document(block("class ConsoleLogger final"))),
                          "class ConsoleLogger final")

    # ---- fences, read as CommonMark reads them ------------------------------------------------

    def test_the_language_is_the_first_word_of_the_info_string(self):
        self.assert_passes(run_check(fenced("int main() {", opening='```cpp title="demo"')))
        self.assert_fails(run_check(fenced("TOTALLY WRONG CODE;", opening='```cpp title="demo"')),
                          "TOTALLY WRONG CODE;")
        self.assert_fails(run_check(fenced("WRONG;", opening="```  C++ showLineNumbers")),
                          "WRONG;")

    def test_a_tilde_fence_is_checked(self):
        self.assert_passes(run_check(fenced("int main() {", opening="~~~cpp", closing="~~~")))
        self.assert_fails(run_check(fenced("TOTALLY WRONG CODE;", opening="~~~cpp",
                                           closing="~~~")), "TOTALLY WRONG CODE;")
        # A backtick fence doesn't close a tilde fence: its line belongs to the block.
        self.assert_fails(run_check(fenced("int main() {\n```", opening="~~~cpp", closing="~~~")),
                          "'```'")

    def test_a_longer_fence_is_checked_and_closed_only_by_one_as_long(self):
        self.assert_fails(run_check(fenced("TOTALLY WRONG CODE;", opening="````cpp",
                                           closing="````")), "TOTALLY WRONG CODE;")
        # Inside a block of four backticks, three backticks are lines of the block.
        self.assert_fails(run_check(fenced("int main() {\n```", opening="````cpp",
                                           closing="````")), "'```'")
        self.assert_passes(run_check(
            "````text\n<!-- snippet: examples/demo.cpp -->\n```cpp\nNOT CHECKED: TEXT;\n```\n"
            "````\n"))

    def test_a_fence_it_cant_classify_is_an_error(self):
        self.assert_fails(run_check("```\nint main() {\n```\n"), "without a language", status=2)
        self.assert_fails(run_check("```cplusplus\nint main() {\n```\n"), "'cplusplus'",
                          status=2)
        self.assert_fails(run_check("```hpp\nWRONG;\n```\n"), "names no file", status=2)
        self.assert_fails(run_check(fenced("int main() {", closing="")), "no fence closes",
                          status=2)
        self.assert_fails(run_check("> ```cpp\n> int main() {\n> ```\n"), "block quote",
                          status=2)
        self.assert_fails(run_check("```cpp`x`\nint main() {\n```\n"), "holds a backtick",
                          status=2)


if __name__ == "__main__":
    unittest.main()
