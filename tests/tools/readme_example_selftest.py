#!/usr/bin/env python3
# Self-test of tools/readme_example.py, the check that README.md shows examples/hello_world.cpp.
# Copyright (c) 2026 Aryan Patel
# SPDX-License-Identifier: BSL-1.0
"""Self-test of tools/readme_example.py: run the check on small files whose verdict is known.

Each case writes a README and an example to a temporary directory and runs the check on them with
--readme and --example. The check must pass when the README's C++ blocks show the example's marked
parts line for line, with spaces, blank lines and the example's comments not counting, and with a
class body or a run of lines left out through the two elisions; and must fail when a line is
deleted from, added to or changed in either file, also at the end of a block, when an elision
hides nothing it may stand for, and when the blocks and the parts don't pair up. The repository's
own README.md and example must pass. CTest runs this as tools.readme_example_selftest.
"""
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
CHECK = ROOT / "tools" / "readme_example.py"

EXAMPLE = """\
// An example.
#include <memory>

// README.md: begin
// clang-format off
struct ILogger { virtual ~ILogger() = default; virtual void log(int) = 0; };
// clang-format on

class ConsoleLogger final : public ILogger {
 public:
  void log(int value) override {
    last_ = value;
  }

 private:
  int last_ = 0;
};

class Service {
 public:
  explicit Service(ILogger& log);

 private:
  ILogger& log_;
};
// README.md: end

Service::Service(ILogger& log) : log_(log) {}

// README.md: begin
#include <trellis/trellis.hpp>

int main() {
  auto injector = trellis::make_injector();  // a comment on a line of code counts
}
// README.md: end
"""

DOMAIN = """\
struct ILogger   { virtual ~ILogger() = default;  virtual void log(int) = 0; };

class ConsoleLogger final : public ILogger { /* ... */ };

class Service {
 public:
  explicit Service(ILogger& log);
  // ...
};"""

ROOT_BLOCK = """\
#include <trellis/trellis.hpp>

int main() {
  auto injector = trellis::make_injector();  // a comment on a line of code counts
}"""


def readme(*blocks, section="## What it looks like"):
    """A README whose section SECTION holds BLOCKS as ```cpp blocks, between other text."""
    body = "\n\nSome text.\n\n".join(f"```cpp\n{block}\n```" for block in blocks)
    return f"# Project\n\n{section}\n\nIntro.\n\n{body}\n\n```text\nnot code\n```\n\n## Next\n"


def run_check(readme_text, example_text=EXAMPLE):
    """Run the check on README_TEXT and EXAMPLE_TEXT. Returns its exit status and its output."""
    with tempfile.TemporaryDirectory() as directory:
        readme_path = Path(directory) / "README.md"
        example_path = Path(directory) / "hello_world.cpp"
        readme_path.write_text(readme_text, encoding="utf-8")
        example_path.write_text(example_text, encoding="utf-8")
        result = subprocess.run([sys.executable, "-I", str(CHECK), "--readme", str(readme_path),
                                 "--example", str(example_path)],
                                capture_output=True, text=True, timeout=60)
        return result.returncode, result.stdout + result.stderr


class ReadmeExampleCheckTest(unittest.TestCase):
    def assert_passes(self, result):
        status, output = result
        self.assertEqual(status, 0, output)
        self.assertTrue(output.startswith("PASS readme-example: "), output)

    def assert_fails(self, result, *fragments, status=1):
        actual, output = result
        self.assertEqual(actual, status, output)
        self.assertTrue(output.startswith("FAIL readme-example: "), output)
        for fragment in fragments:
            self.assertIn(fragment, output)

    def test_blocks_that_show_the_parts_pass(self):
        self.assert_passes(run_check(readme(DOMAIN, ROOT_BLOCK)))

    def test_the_repository_readme_shows_its_example(self):
        result = subprocess.run([sys.executable, "-I", str(CHECK)], capture_output=True,
                                text=True, timeout=60)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_a_line_deleted_from_the_readme_fails(self):
        self.assert_fails(run_check(readme(DOMAIN.replace("  explicit Service(ILogger& log);\n",
                                                          ""), ROOT_BLOCK)),
                          "block 1", "explicit Service(ILogger& log);")

    def test_the_last_line_of_a_block_deleted_fails(self):
        self.assert_fails(run_check(readme(DOMAIN, ROOT_BLOCK.rsplit("\n", 1)[0])),
                          "block 2", "goes on with '}'")

    def test_a_line_deleted_from_the_example_fails(self):
        example = EXAMPLE.replace("  explicit Service(ILogger& log);\n", "")
        self.assert_fails(run_check(readme(DOMAIN, ROOT_BLOCK), example), "block 1")

    def test_a_line_added_to_the_readme_fails(self):
        self.assert_fails(run_check(readme(DOMAIN, ROOT_BLOCK.replace(
            "int main() {\n", "int main() {\n  int unused = 0;\n"))), "block 2", "int unused = 0;")

    def test_a_line_added_to_the_example_fails(self):
        example = EXAMPLE.replace("int main() {\n", "int main() {\n  int unused = 0;\n")
        self.assert_fails(run_check(readme(DOMAIN, ROOT_BLOCK), example), "block 2",
                          "int unused = 0;")

    def test_a_changed_line_fails(self):
        self.assert_fails(run_check(readme(DOMAIN, ROOT_BLOCK.replace("make_injector()",
                                                                      "make_injector(1)"))),
                          "block 2", "make_injector(1)")

    def test_an_elided_body_must_name_the_class(self):
        self.assert_fails(run_check(readme(DOMAIN.replace("class ConsoleLogger final",
                                                          "class FileLogger final"), ROOT_BLOCK)),
                          "block 1", "FileLogger")

    def test_an_elided_body_left_out_entirely_fails(self):
        domain = DOMAIN.replace("class ConsoleLogger final : public ILogger { /* ... */ };\n", "")
        self.assert_fails(run_check(readme(domain, ROOT_BLOCK)), "block 1", "ConsoleLogger")

    def test_elided_lines_may_not_end_a_block(self):
        domain = DOMAIN.rsplit("\n", 1)[0]  # the block now ends with // ...
        self.assert_fails(run_check(readme(domain, ROOT_BLOCK)), "block 1", "ends a block")

    def test_blocks_and_parts_must_pair_up(self):
        self.assert_fails(run_check(readme(DOMAIN)), "1 C++ blocks", "2 parts")

    def test_a_missing_section_or_part_marker_is_an_error(self):
        self.assert_fails(run_check(readme(DOMAIN, ROOT_BLOCK, section="## Something else")),
                          "has no section", status=2)
        self.assert_fails(run_check(readme(DOMAIN, ROOT_BLOCK),
                                    EXAMPLE.replace("// README.md: begin\n", "")),
                          "'README.md: end' outside a part", status=2)
        self.assert_fails(run_check(readme(DOMAIN, ROOT_BLOCK),
                                    EXAMPLE.replace("// README.md: end\n", "")),
                          "'README.md: begin' inside a part", status=2)


if __name__ == "__main__":
    unittest.main()
