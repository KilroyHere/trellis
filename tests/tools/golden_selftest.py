#!/usr/bin/env python3
"""Self-test of tools/golden.py: normalization, and the update and compare cycle.

The normalization cases use canned Clang, GCC and MSVC output (MSVC isn't installed here). The
update cycle compiles the sample compile-fail test with the build's compiler and writes into a
temporary directory in the build tree, never into tests/ or docs/. CTest runs this as
tools.golden_selftest with the build's --config.
"""
import argparse
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import golden  # noqa: E402

GOLDEN_SCRIPT = ROOT / "tools" / "golden.py"
SAMPLE = "sample/static_assert_message"
CONFIG = None  # set by main() from --config


class NormalizeTest(unittest.TestCase):
    def test_clang_output(self):
        output = (
            "\x1b[1m/work/trellis/tests/compile_fail/a/b.cpp:18:17: \x1b[0;1;31merror: \x1b[0m"
            "static assertion failed due to requirement 'x<int>'\n"
            "   18 |   static_assert(x<T>, \"TRL-E000: a:1\");\n"
            "      |                 ^~~~\n"
            "/work/trellis/tests/compile_fail/a/b.cpp:25:22: note: in instantiation of 'f<int>' "
            "requested here\n"
            "In file included from /work/trellis/include/trellis/trellis.hpp:6:\n"
            "/usr/lib/llvm-18/include/c++/v1/memory:12:3: note: candidate "
            "(lambda at /work/trellis/tests/x.cpp:3:9)\n"
            "/work/trellis/build/clang-18-debug/gen/y.hpp:4:1: note: generated   \n"
            "1 error generated.\n\n")
        expected = (
            "tests/compile_fail/a/b.cpp: error: static assertion failed due to requirement "
            "'x<int>'\n"
            " |   static_assert(x<T>, \"TRL-E000: a:1\");\n"
            " |                 ^~~~\n"
            "tests/compile_fail/a/b.cpp: note: in instantiation of 'f<int>' requested here\n"
            "In file included from include/trellis/trellis.hpp:\n"
            "<external>/memory: note: candidate (lambda at tests/x.cpp)\n"
            "<build>/gen/y.hpp: note: generated\n"
            "1 error generated.\n")
        self.assertEqual(golden.normalize(output, "/work/trellis",
                                          "/work/trellis/build/clang-18-debug"), expected)

    def test_gcc_output(self):
        output = (
            "In file included from /usr/include/c++/13/memory:78,\n"
            "                 from /work/trellis/tests/compile_fail/a/b.cpp:9:\n"
            "/work/trellis/tests/compile_fail/a/b.cpp: In instantiation of "
            "\u2018void f() [with T = int]\u2019:\n"
            "/work/trellis/tests/compile_fail/a/b.cpp:25:53:   required from here\n"
            "  125 |   f<int>();\n")
        expected = (
            "In file included from <external>/memory,\n"
            "                 from tests/compile_fail/a/b.cpp:\n"
            "tests/compile_fail/a/b.cpp: In instantiation of 'void f() [with T = int]':\n"
            "tests/compile_fail/a/b.cpp:   required from here\n"
            " |   f<int>();\n")
        self.assertEqual(golden.normalize(output, "/work/trellis"), expected)

    def test_msvc_output(self):
        output = (
            "Microsoft (R) C/C++ Optimizing Compiler Version 19.40.33811 for x64\r\n"
            "Copyright (C) Microsoft Corporation.  All rights reserved.\r\n"
            "\r\n"
            "c:\\src\\trellis\\tests\\compile_fail\\a\\b.cpp(18,17): error C2338: "
            "static_assert failed: 'trellis [TRL-E000]'\r\n"
            "C:\\src\\trellis\\tests\\compile_fail\\a\\b.cpp(25): note: see reference to "
            "'void f<int>(void)'\r\n"
            "C:\\Program Files (x86)\\MSVC\\include\\memory(12): note: see 'std::unique_ptr'\r\n")
        expected = (
            "tests\\compile_fail\\a\\b.cpp: error C2338: static_assert failed: "
            "'trellis [TRL-E000]'\n"
            "tests\\compile_fail\\a\\b.cpp: note: see reference to 'void f<int>(void)'\n"
            "<external>/memory: note: see 'std::unique_ptr'\n")
        self.assertEqual(golden.normalize(output, "C:\\src\\trellis"), expected)

    def test_type_names_and_error_namespaces_are_kept(self):
        line = ("x.cpp: note: 'trellis::error::E001<std::array<int, 3>, a::b>' (a:b) "
                "ratio<1, 1000>\n")
        self.assertEqual(golden.normalize(line, "/work/trellis"), line)


class UpdateAndCompareTest(unittest.TestCase):
    """--update writes a golden and records why; a later run compares against it. Everything
    happens in a scratch directory: a copy of the build's config (with golden tests on, so that
    builds which turn them off still test the mechanism), a manifest, goldens and a changes file."""

    def setUp(self):
        config = json.loads(Path(CONFIG).read_text(encoding="utf-8"))
        binary_dir = Path(config["binary_dir"])
        binary_dir.mkdir(parents=True, exist_ok=True)
        self.scratch = tempfile.TemporaryDirectory(prefix="golden_selftest-", dir=binary_dir)
        scratch = Path(self.scratch.name)
        self.compiler = config["golden_name"]
        self.config = scratch / "config.json"
        self.config.write_text(json.dumps(dict(config, golden_tests=True)), encoding="utf-8")
        self.golden_root = scratch / "goldens"
        self.manifest = scratch / "manifest.txt"
        self.changes = scratch / "golden-changes.md"
        self.write_manifest(f"{SAMPLE} {self.compiler}")

    def tearDown(self):
        self.scratch.cleanup()

    def write_manifest(self, *lines):
        self.manifest.write_text("# self-test manifest\n" + "".join(f"{line}\n" for line in lines),
                                 encoding="utf-8")

    def run_golden(self, *args, config=None):
        command = [sys.executable, "-I", str(GOLDEN_SCRIPT), "--golden-root", str(self.golden_root),
                   "--manifest", str(self.manifest), "--changes-file", str(self.changes), *args]
        if "--check-manifest" not in args:
            command[3:3] = ["--config", str(config or self.config)]
        return subprocess.run(command, capture_output=True, text=True, check=False)

    def test_update_then_compare(self):
        # Without NAMEs the manifest decides, so a listed test without a golden fails.
        missing = self.run_golden()
        self.assertEqual(missing.returncode, 1, missing.stdout + missing.stderr)
        self.assertIn("no golden", missing.stdout)

        written = self.run_golden("--update", "--reason", "self-test reason")
        self.assertEqual(written.returncode, 0, written.stdout + written.stderr)
        goldens = list(self.golden_root.rglob("*.txt"))
        self.assertEqual(len(goldens), 1)
        text = goldens[0].read_text(encoding="utf-8")
        self.assertIn("tests/compile_fail/sample/static_assert_message.cpp", text)
        self.assertNotIn(str(ROOT), text)
        self.assertIn("self-test reason", self.changes.read_text(encoding="utf-8"))
        self.assertIn(f"added: {SAMPLE}", self.changes.read_text(encoding="utf-8"))

        same = self.run_golden(SAMPLE)
        self.assertEqual(same.returncode, 0, same.stdout + same.stderr)
        self.assertIn("PASS golden", same.stdout)

        goldens[0].write_text(text + "an extra line\n", encoding="utf-8")
        differs = self.run_golden(SAMPLE)
        self.assertEqual(differs.returncode, 1, differs.stdout + differs.stderr)
        self.assertIn("-an extra line", differs.stdout)

        unchanged = self.changes.read_text(encoding="utf-8")
        again = self.run_golden("--update", "--reason", "restore", SAMPLE)
        self.assertEqual(again.returncode, 0, again.stdout + again.stderr)
        self.assertIn("updated: " + SAMPLE, self.changes.read_text(encoding="utf-8"))
        self.assertTrue(self.changes.read_text(encoding="utf-8").startswith(unchanged))

        goldens[0].unlink()  # a deleted golden fails; it doesn't just go untested
        deleted = self.run_golden(SAMPLE)
        self.assertEqual(deleted.returncode, 1, deleted.stdout + deleted.stderr)
        self.assertIn("no golden", deleted.stdout)

    def test_update_needs_a_reason(self):
        result = self.run_golden("--update", SAMPLE)
        self.assertEqual(result.returncode, 2)
        self.assertIn("--reason", result.stderr)
        self.assertFalse(self.golden_root.exists())

    def test_unlisted_test_is_refused(self):
        self.write_manifest()
        update = self.run_golden("--update", "--reason", "not listed", SAMPLE)
        self.assertEqual(update.returncode, 2, update.stdout + update.stderr)
        self.assertIn("add a line for it first", update.stderr)
        self.assertFalse(self.golden_root.exists())
        compare = self.run_golden(SAMPLE)
        self.assertEqual(compare.returncode, 1, compare.stdout + compare.stderr)
        self.assertIn("the manifest doesn't list it", compare.stdout)

    def test_build_with_golden_tests_off_is_refused(self):
        config = json.loads(self.config.read_text(encoding="utf-8"))
        off = self.config.with_name("off.json")
        off.write_text(json.dumps(dict(config, golden_tests=False)), encoding="utf-8")
        for args in ((SAMPLE,), ("--update", "--reason", "off", SAMPLE)):
            result = self.run_golden(*args, config=off)
            self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
            self.assertIn("golden tests are off", result.stderr)
        self.assertFalse(self.golden_root.exists())

    def test_malformed_manifest_line(self):
        self.write_manifest(SAMPLE)  # no compiler
        result = self.run_golden("--check-manifest")
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertIn("expected '<test> <compiler>-<major> ...'", result.stderr)

    def test_check_manifest(self):
        golden_file = self.golden_root / self.compiler / f"{SAMPLE}.txt"
        missing = self.run_golden("--check-manifest")
        self.assertEqual(missing.returncode, 1, missing.stdout)
        self.assertIn(f"listed for {self.compiler}, but", missing.stdout)

        golden_file.parent.mkdir(parents=True)
        golden_file.write_text("output\n", encoding="utf-8")
        consistent = self.run_golden("--check-manifest")
        self.assertEqual(consistent.returncode, 0, consistent.stdout)

        self.write_manifest()
        orphan = self.run_golden("--check-manifest")
        self.assertEqual(orphan.returncode, 1, orphan.stdout)
        self.assertIn(f"no manifest line lists {SAMPLE} for {self.compiler}", orphan.stdout)

        self.write_manifest(f"no/such_test {self.compiler}")
        unknown = self.run_golden("--check-manifest")
        self.assertEqual(unknown.returncode, 1, unknown.stdout)
        self.assertIn("there is no tests/compile_fail/no/such_test.cpp", unknown.stdout)


def main():
    global CONFIG
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", required=True, help="the build's trellis_compiler.json")
    args, rest = parser.parse_known_args()
    CONFIG = args.config
    unittest.main(argv=[sys.argv[0], *rest], verbosity=2)


if __name__ == "__main__":
    main()
