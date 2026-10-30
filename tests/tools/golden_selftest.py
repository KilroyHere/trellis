#!/usr/bin/env python3
"""Self-test of tools/golden.py: normalization, the compiler invocation, and the update and compare
cycle.

The normalization cases use canned Clang, GCC and MSVC output (MSVC isn't installed here). The
invocation test runs golden.py on a stub compiler that reports its arguments and environment. The
update cycle compiles the sample compile-fail test with the build's compiler. Both write into a
temporary directory in the build tree, never into tests/ or docs/. CTest runs this as
tools.golden_selftest with the build's --config.
"""
import argparse
import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(HERE))
import golden  # noqa: E402
import stub_compiler  # noqa: E402  (INHERITED: what the invocation test adds to the environment)

GOLDEN_SCRIPT = ROOT / "tools" / "golden.py"
STUB = HERE / "stub_compiler.py"
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
            "<src>/tests/compile_fail/a/b.cpp:L:C: error: static assertion failed due to "
            "requirement 'x<int>'\n"
            "<src>/tests/compile_fail/a/b.cpp:L:C: note: in instantiation of 'f<int>' requested "
            "here\n"
            "<std>/memory:L:C: note: candidate (lambda at <src>/tests/x.cpp:L:C)\n"
            "<build>/gen/y.hpp:L:C: note: generated\n")
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
            "<src>/tests/compile_fail/a/b.cpp: In instantiation of 'void f() [with T = int]':\n"
            "<src>/tests/compile_fail/a/b.cpp:L:C:   required from here\n")
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
            "<src>/tests/compile_fail/a/b.cpp(L,C): error C2338: static_assert failed: "
            "'trellis [TRL-E000]'\n"
            "<src>/tests/compile_fail/a/b.cpp(L,C): note: see reference to 'void f<int>(void)'\n"
            "<external>/memory(L,C): note: see 'std::unique_ptr'\n")
        self.assertEqual(golden.normalize(output, "C:\\src\\trellis"), expected)

    def test_type_names_and_error_namespaces_are_kept(self):
        line = ("x.cpp: note: 'trellis::error::E001<std::array<int, 3>, a::b>' (a:b) "
                "ratio<1, 1000>\n")
        self.assertEqual(golden.normalize(line, "/work/trellis"), line)

    def test_colours_and_hyperlinks_are_stripped(self):
        # GCC's colours (ESC[01m ESC[K ... ESC[m ESC[K) and Clang's (ESC[0;1;31m), and OSC-8
        # hyperlinks around warning options, which GCC ends with BEL by default and with ESC \
        # when GCC_URLS=st.
        output = (
            "\x1b[01m\x1b[K/work/trellis/a.cpp:2:7:\x1b[m\x1b[K \x1b[01;35m\x1b[Kwarning: "
            "\x1b[m\x1b[Kunused variable '\x1b[01m\x1b[Kx\x1b[m\x1b[K' [\x1b]8;;https://gcc.gnu.org"
            "/onlinedocs/gcc/Warning-Options.html#index-Wunused-variable\x07-Wunused-variable"
            "\x1b]8;;\x07]\n"
            "/work/trellis/a.cpp:3:1: \x1b[0;1;31merror: \x1b[0mno [\x1b]8;;https://example.org/x"
            "\x1b\\-Wx\x1b]8;;\x1b\\]\n")
        expected = (
            "<src>/a.cpp:L:C: warning: unused variable 'x' [-Wunused-variable]\n"
            "<src>/a.cpp:L:C: error: no [-Wx]\n")
        self.assertEqual(golden.normalize(output, "/work/trellis"), expected)

    def test_quoted_source_include_chains_and_banners_are_dropped(self):
        output = (
            "In file included from /work/trellis/tests/compile_fail/a/b.cpp:1:\n"
            "In file included from /work/trellis/include/trellis/trellis.hpp:6:\n"
            "/work/trellis/include/trellis/x.hpp:5:17: error: no binding\n"
            "    5 |   static_assert(x<T>);\n"
            "      |                 ^~~~\n"
            "      |                 y<T>\n"
            "In file included from /usr/include/c++/13/memory:78,\n"
            "                 from /work/trellis/tests/compile_fail/a/b.cpp:9:\n"
            "/usr/include/c++/13/bits/unique_ptr.h:522:7: note: declared here\n"
            "  522 |       unique_ptr(const unique_ptr&) = delete;\n"
            "      |       ^~~~~~~~~~\n"
            "compilation terminated.\n"
            "1 error generated.\n"
            "2 errors generated.\n"
            "1 warning and 3 errors generated.\n")
        expected = (
            "<src>/include/trellis/x.hpp:L:C: error: no binding\n"
            "<std>/bits/unique_ptr.h:L:C: note: declared here\n")
        self.assertEqual(golden.normalize(output, "/work/trellis"), expected)

    def test_standard_library_paths(self):
        # libstdc++ as GCC and Clang find it, and its target-specific directory; libc++ in an LLVM
        # installation and in Xcode's SDK; MSVC's STL, whose path has spaces.
        output = (
            "/usr/include/c++/13/bits/unique_ptr.h:522:7: note: a\n"
            "/usr/bin/../lib/gcc/x86_64-linux-gnu/14/../../../../include/c++/14/bits/"
            "stl_construct.h:94:5: note: b\n"
            "/usr/include/x86_64-linux-gnu/c++/13/bits/c++config.h:3:1: note: c\n"
            "/usr/lib/llvm-18/bin/../include/c++/v1/__memory/unique_ptr.h:5:1: note: d\n"
            "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/"
            "MacOSX.sdk/usr/include/c++/v1/memory:1:1: note: e\n"
            "C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise\\VC\\Tools\\MSVC\\"
            "14.40.33807\\include\\xmemory(1234): note: f\n")
        expected = (
            "<std>/bits/unique_ptr.h:L:C: note: a\n"
            "<std>/bits/stl_construct.h:L:C: note: b\n"
            "<std>/bits/c++config.h:L:C: note: c\n"
            "<std>/__memory/unique_ptr.h:L:C: note: d\n"
            "<std>/memory:L:C: note: e\n"
            "<std>/xmemory(L,C): note: f\n")
        self.assertEqual(golden.normalize(output, "/work/trellis"), expected)

    def test_windows_paths_get_forward_slashes(self):
        output = (
            "c:\\src\\trellis\\tests\\compile_fail\\a\\b.cpp(18): error C2338: x\n"
            "C:\\src\\trellis\\include\\trellis\\x.hpp(3,9): note: y\n"
            "C:\\src\\trellis\\tests\\x.cpp:3:9: error: no member named 'foo' in "
            "'(lambda at C:\\src\\trellis\\tests\\x.cpp:7:36)'\n"
            "C:/src/trellis/include/trellis/trellis.hpp:6:1: note: z\n"
            "D:\\deps\\include\\other.hpp:12:3: note: w\n")
        expected = (
            "<src>/tests/compile_fail/a/b.cpp(L,C): error C2338: x\n"
            "<src>/include/trellis/x.hpp(L,C): note: y\n"
            "<src>/tests/x.cpp:L:C: error: no member named 'foo' in "
            "'(lambda at <src>/tests/x.cpp:L:C)'\n"
            "<src>/include/trellis/trellis.hpp:L:C: note: z\n"
            "<external>/other.hpp:L:C: note: w\n")
        self.assertEqual(golden.normalize(output, "C:\\src\\trellis"), expected)

    def test_line_only_locations_external_paths_and_message_text(self):
        # A location without a column gets the same placeholder. A directory that merely ends
        # like the source directory is not it. Numbers in the message text stay.
        output = (
            "/work/trellis/tests/a.cpp:25: note: line only\n"
            "/opt/deps/include/dep/b.hpp:3:4: note: outside the repository\n"
            "/home/u/work/trellis/c.hpp:5:6: note: another tree\n"
            "/work/trellis/d.cpp:7:8: error: TRL-E000: a:1 size(3): b:2:3\n")
        expected = (
            "<src>/tests/a.cpp:L:C: note: line only\n"
            "<external>/b.hpp:L:C: note: outside the repository\n"
            "<external>/c.hpp:L:C: note: another tree\n"
            "<src>/d.cpp:L:C: error: TRL-E000: a:1 size(3): b:2:3\n")
        self.assertEqual(golden.normalize(output, "/work/trellis"), expected)


class InvocationTest(unittest.TestCase):
    """golden.py compiles with LC_ALL=C, in an environment that keeps nothing of its own beyond
    what a compiler needs to start, and with the plainest output each compiler offers (spike S3):
    -fdiagnostics-plain-output on GCC, -fno-color-diagnostics and -fno-caret-diagnostics on Clang.
    A stub compiler reports what it got; golden.py saves that report as the normalized output."""

    def setUp(self):
        config = json.loads(Path(CONFIG).read_text(encoding="utf-8"))
        binary_dir = Path(config["binary_dir"])
        binary_dir.mkdir(parents=True, exist_ok=True)
        self.scratch = tempfile.TemporaryDirectory(prefix="golden_invocation-", dir=binary_dir)
        self.base = config

    def tearDown(self):
        self.scratch.cleanup()

    def invocation(self, compiler_id):
        """Run golden.py on the stub playing COMPILER_ID; return its arguments and environment."""
        scratch = Path(self.scratch.name) / compiler_id
        scratch.mkdir()
        config = scratch / "config.json"
        config.write_text(json.dumps(dict(
            self.base, compiler=sys.executable, compiler_id=compiler_id, frontend="GNU",
            golden_name="stub-1", golden_tests=True, binary_dir=str(scratch),
            flags=["-I", str(STUB), "--scenario", "report-invocation"])), encoding="utf-8")
        manifest = scratch / "manifest.txt"
        manifest.write_text(f"{SAMPLE} stub-1\n", encoding="utf-8")
        env = dict(os.environ, LANG="de_DE.UTF-8", **stub_compiler.INHERITED)
        result = subprocess.run(
            [sys.executable, "-I", str(GOLDEN_SCRIPT), "--config", str(config),
             "--golden-root", str(scratch / "goldens"), "--manifest", str(manifest),
             "--changes-file", str(scratch / "changes.md"), SAMPLE],
            env=env, capture_output=True, text=True, check=False)
        # There is no golden, so the run fails, but only after it saved the normalized output.
        self.assertIn("no golden", result.stdout, result.stdout + result.stderr)
        saved = (scratch / "golden" / f"{SAMPLE}.txt").read_text(encoding="utf-8")
        found = {}
        for line in saved.splitlines():
            for key in ("arguments", "environment"):
                if line.startswith(f"stub {key}: "):
                    found[key] = line.split(": ", 1)[1].split()
        self.assertEqual(sorted(found), ["arguments", "environment"], saved)
        return found["arguments"], found["environment"]

    def assert_clean(self, environment):
        names = {entry.split("=", 1)[0] for entry in environment}
        self.assertEqual(sorted(set(stub_compiler.INHERITED) & names), [], environment)
        self.assertIn("LC_ALL=C", environment)
        self.assertIn("LANG=C", environment)

    def test_gcc(self):
        arguments, environment = self.invocation("GNU")
        self.assertIn("-fdiagnostics-plain-output", arguments)
        self.assert_clean(environment)

    def test_clang(self):
        for compiler_id in ("Clang", "AppleClang"):
            arguments, environment = self.invocation(compiler_id)
            self.assertIn("-fno-color-diagnostics", arguments, compiler_id)
            self.assertIn("-fno-caret-diagnostics", arguments, compiler_id)
            self.assertNotIn("-fdiagnostics-plain-output", arguments, compiler_id)
            self.assert_clean(environment)


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
        # Paths start with <src>/ and use '/', also on Windows; GCC and Clang locations end in
        # :L:C, MSVC's in (L,C).
        self.assertIn("<src>/tests/compile_fail/sample/static_assert_message.cpp", text)
        self.assertRegex(text, r"static_assert_message\.cpp(?::L:C|\(L,C\)): error")
        self.assertNotIn(str(ROOT), text)
        self.assertNotIn("\\", text)
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

    def test_check_manifest_reports_a_golden_outside_a_compiler_directory(self):
        self.golden_root.mkdir(parents=True)
        (self.golden_root / "stray.txt").write_text("output\n", encoding="utf-8")
        stray = self.run_golden("--check-manifest")
        self.assertEqual(stray.returncode, 1, stray.stdout + stray.stderr)
        self.assertIn("stray.txt: goldens belong in a <compiler>-<major>/ directory", stray.stdout)
        self.assertNotIn("Traceback", stray.stderr)


def main():
    global CONFIG
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", required=True, help="the build's trellis_compiler.json")
    args, rest = parser.parse_known_args()
    CONFIG = args.config
    unittest.main(argv=[sys.argv[0], *rest], verbosity=2)


if __name__ == "__main__":
    main()
