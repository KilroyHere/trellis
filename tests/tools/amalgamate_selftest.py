#!/usr/bin/env python3
# Self-test of tools/amalgamate.py, the single-header generator.
# Copyright (c) 2026 Aryan Patel
# SPDX-License-Identifier: BSL-1.0
"""Self-test of tools/amalgamate.py: generate single headers from small trees of known result.

Each case builds include/trellis/ in a temporary directory and runs the generator on it with
--root. On a well-formed tree the result must hold each header once, after the headers it
includes; without include guards, Trellis includes and per-file notices; with one notice, one
outer guard and each unconditional standard include once at the top; with a conditional standard
include, a continued #define and a "#include" inside a comment or a string left where they are.
Generation must fail on a header without a proper include guard, with code before it, an #else of
it or a directive after it, with #pragma once, with a quoted include, with a Trellis include
inside #if or of a missing header, with a missing or different notice, or that names another
header's guard. A header that another includes at its end comes right after that header's code,
whichever is reached first, as the preprocessor's include guards make it (static/injector.hpp and
static/resolve.hpp). --check must pass a fresh artifact or none, and fail a
stale one and a header that doesn't compile. CTest runs this as tools.amalgamate_selftest; the
--check cases need --cxx, a GCC or Clang driver, and are skipped without it.
"""
import argparse
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
TOOL = ROOT / "tools" / "amalgamate.py"
CXX = None  # set by main()
CXX_GIVEN = any(arg == "--cxx" or arg.startswith("--cxx=") for arg in sys.argv)
NOTICE = "// Copyright (c) 2026 Aryan Patel\n// SPDX-License-Identifier: BSL-1.0\n"


def header(guard, body, intro="// A header.\n", notice=NOTICE):
    """A header with the project's layout: a leading comment with the notice, an include guard."""
    return f"{intro}{notice}#ifndef {guard}\n#define {guard}\n\n{body}\n#endif  // {guard}\n"


CONFIG = header("TRELLIS_CONFIG_HPP", """\
#include <version>  // IWYU pragma: keep

#define TRELLIS_VERSION_MAJOR 1
#define TRELLIS_VERSION_MINOR 2
#define TRELLIS_VERSION_PATCH 3
#define TRELLIS_VERSION \\
  (TRELLIS_VERSION_MAJOR * 10000 + TRELLIS_VERSION_MINOR * 100 + TRELLIS_VERSION_PATCH)
#if defined(__cpp_lib_expected)
#define TRELLIS_HAS_EXPECTED 1
#else
#define TRELLIS_HAS_EXPECTED 0
#endif
""", intro="// Trellis configuration.\n")

LIST = header("TRELLIS_META_LIST_HPP", """\
#include <trellis/config.hpp>

#include <cstddef>
#include <type_traits>

#if TRELLIS_HAS_EXPECTED
#include <expected>
#endif

/* A block comment that mentions
#include <trellis/missing.hpp>
on a line of its own. */
namespace trellis::inline v1::detail {
inline constexpr const char* spelling = "#include <trellis/missing.hpp>";
inline constexpr char8_t letter = u8'a';
inline constexpr long thousand = 1'000; /* a digit separator, then a comment
#include <trellis/missing.hpp>
that a separator read as a quote would expose */
template <class... Ts>
struct type_list {
  static constexpr std::size_t size = sizeof...(Ts);
};
}  // namespace trellis::inline v1::detail
""", intro="// Type lists.\n")

API = header("TRELLIS_API_HPP", """\
#include <trellis/config.hpp>
#include <trellis/meta/list.hpp>

#include <cstddef>
#include <utility>

namespace trellis::inline v1 {
inline constexpr std::size_t answer = detail::type_list<int, long>::size + TRELLIS_VERSION;
}  // namespace trellis::inline v1
""", intro="// The API.\n")

UMBRELLA = header("TRELLIS_TRELLIS_HPP", """\
#include <trellis/api.hpp>  // IWYU pragma: export
#include <trellis/config.hpp>  // IWYU pragma: export
""", intro="// The umbrella header.\n")

TREE = {
    "trellis/trellis.hpp": UMBRELLA,
    "trellis/api.hpp": API,
    "trellis/config.hpp": CONFIG,
    "trellis/meta/list.hpp": LIST,
}


def with_api(old, new):
    """TREE with the first OLD in api.hpp replaced by NEW."""
    return {**TREE, "trellis/api.hpp": API.replace(old, new, 1)}


def write_tree(root, files):
    for name, text in files.items():
        path = root / "include" / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")


def run_tool(root, *args):
    result = subprocess.run([sys.executable, "-I", str(TOOL), "--root", str(root), *args],
                            capture_output=True, text=True, timeout=300, check=False)
    return result.returncode, result.stdout + result.stderr


class AmalgamateTest(unittest.TestCase):
    def generate(self, files):
        """Generate the single header from FILES; returns (exit status, output, text or None)."""
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            write_tree(root, files)
            status, output = run_tool(root)
            artifact = root / "single_include" / "trellis.hpp"
            text = artifact.read_text(encoding="utf-8") if artifact.is_file() else None
            return status, output, text

    def assert_fails(self, files, *fragments):
        status, output, text = self.generate(files)
        self.assertEqual(status, 1, output)
        self.assertIsNone(text, "nothing is written when generation fails")
        for fragment in fragments:
            self.assertIn(fragment, output)

    def test_well_formed_tree(self):
        status, output, text = self.generate(TREE)
        self.assertEqual(status, 0, output)
        self.assertIn("(4 headers, ", output)
        lines = text.splitlines()
        self.assertEqual(lines[0], "// Trellis 1.2.3: header-only C++20 dependency injection, "
                                   "as a single header.")
        # One notice, at the top; one outer guard around everything.
        self.assertEqual(text.count("// Copyright (c) 2026 Aryan Patel"), 1)
        self.assertEqual(text.count("SPDX-License-Identifier: BSL-1.0"), 1)
        self.assertEqual(lines[1:3], NOTICE.splitlines())
        self.assertIn("#ifndef TRELLIS_SINGLE_INCLUDE_TRELLIS_HPP\n"
                      "#define TRELLIS_SINGLE_INCLUDE_TRELLIS_HPP\n", text)
        self.assertTrue(text.endswith("#endif  // TRELLIS_SINGLE_INCLUDE_TRELLIS_HPP\n"))
        # No inner guard, no Trellis include, and each header once, after its includes.
        for guard in ("TRELLIS_CONFIG_HPP", "TRELLIS_META_LIST_HPP", "TRELLIS_API_HPP",
                      "TRELLIS_TRELLIS_HPP"):
            self.assertNotIn(guard, text)
        # The only "#include <trellis/" left are the comments' and the string's.
        self.assertEqual(text.count("#include <trellis/"), 3)
        sections = [line for line in lines if line.startswith("// ==== include/")]
        self.assertEqual([section.split()[2] for section in sections],
                         ["include/trellis/config.hpp", "include/trellis/meta/list.hpp",
                          "include/trellis/api.hpp", "include/trellis/trellis.hpp"])
        # Unconditional standard includes once, sorted, before the first header; the conditional
        # one stays in place.
        first_section = text.index("// ==== include/")
        top = text[:first_section]
        for name in ("cstddef", "type_traits", "utility", "version"):
            self.assertEqual(text.count(f"#include <{name}>"), 1, name)
            self.assertIn(f"#include <{name}>", top)
        self.assertLess(top.index("<cstddef>"), top.index("<version>"))
        self.assertIn("#if TRELLIS_HAS_EXPECTED\n#include <expected>\n#endif", text)
        # A continued #define keeps its backslash; comments and strings keep their "#include".
        self.assertIn("#define TRELLIS_VERSION \\\n  (TRELLIS_VERSION_MAJOR", text)
        self.assertIn("/* A block comment that mentions\n#include <trellis/missing.hpp>\n", text)
        self.assertIn('spelling = "#include <trellis/missing.hpp>";', text)
        # A digit separator is part of its number, not a quote that hides the comment after it
        # (plan R15), and u8'a' is one character literal.
        self.assertIn("letter = u8'a';\ninline constexpr long thousand = 1'000; /* a digit "
                      "separator, then a comment\n#include <trellis/missing.hpp>\n", text)
        # The leading comments stay, without the notice lines.
        self.assertIn("// Type lists.\n", text)

    def test_generation_is_deterministic_and_writes_only_changes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            write_tree(root, TREE)
            self.assertEqual(run_tool(root)[0], 0)
            artifact = root / "single_include" / "trellis.hpp"
            first = artifact.read_text(encoding="utf-8")
            status, output = run_tool(root)
            self.assertEqual(status, 0, output)
            self.assertIn("is up to date", output)
            self.assertEqual(artifact.read_text(encoding="utf-8"), first)
            other = root / "elsewhere" / "single.hpp"
            self.assertEqual(run_tool(root, "--output", str(other))[0], 0)
            self.assertEqual(other.read_text(encoding="utf-8"), first)

    def test_missing_include_guard_fails(self):
        self.assert_fails(with_api("#ifndef TRELLIS_API_HPP\n", ""),
                          "include/trellis/api.hpp:", "include guard")

    def test_guard_define_mismatch_fails(self):
        self.assert_fails(with_api("#define TRELLIS_API_HPP", "#define OTHER"),
                          "#ifndef TRELLIS_API_HPP must be followed by #define TRELLIS_API_HPP")

    def test_code_before_the_guard_fails(self):
        self.assert_fails({**TREE, "trellis/api.hpp": "int early;\n" + API},
                          "code before the include guard")

    def test_else_of_the_guard_fails(self):
        guarded_else = API.replace("#endif  // TRELLIS_API_HPP", "#else\nint other;\n#endif")
        self.assert_fails({**TREE, "trellis/api.hpp": guarded_else},
                          "#else of the include guard TRELLIS_API_HPP")

    def test_directive_after_the_guard_fails(self):
        self.assert_fails({**TREE, "trellis/api.hpp": API + "#define LATE 1\n"},
                          "#define after the end of the include guard TRELLIS_API_HPP")

    def test_pragma_once_fails(self):
        self.assert_fails(with_api("namespace trellis", "#pragma once\nnamespace trellis"),
                          "#pragma once")

    def test_quoted_include_fails(self):
        self.assert_fails(with_api("#include <utility>", '#include "utility"'),
                          '#include "utility": include headers with <...>')

    def test_conditional_trellis_include_fails(self):
        self.assert_fails(with_api("#include <trellis/config.hpp>\n",
                                   "#if 1\n#include <trellis/config.hpp>\n#endif\n"),
                          "#include <trellis/config.hpp> inside #if")

    def test_missing_trellis_header_fails(self):
        self.assert_fails(with_api("#include <utility>", "#include <trellis/gone.hpp>"),
                          "#include <trellis/gone.hpp>: no such header under include/")

    def test_missing_notice_fails(self):
        api = header("TRELLIS_API_HPP", "int x;", notice="")
        self.assert_fails({**TREE, "trellis/api.hpp": api},
                          "needs a copyright line and an SPDX-License-Identifier line")

    def test_different_notice_fails(self):
        api = header("TRELLIS_API_HPP", "int x;", notice=NOTICE.replace("BSL-1.0", "MIT"))
        self.assert_fails({**TREE, "trellis/api.hpp": api},
                          "the headers' copyright or SPDX lines differ")

    def test_use_of_another_guard_fails(self):
        self.assert_fails(with_api("namespace trellis",
                                   "#ifdef TRELLIS_CONFIG_HPP\n#endif\nnamespace trellis"),
                          "include/trellis/api.hpp names TRELLIS_CONFIG_HPP, the include guard of "
                          "include/trellis/config.hpp")

    def expanded(self, first):
        """The single header of a tree where injector.hpp includes resolve.hpp after its own code
        and resolve.hpp includes injector.hpp first, as static/injector.hpp and static/resolve.hpp
        do, and the umbrella header includes FIRST before the others. Returns the sections, as
        lists of words, and the text; checks that it compiles when --cxx is given."""
        injector = header("TRELLIS_INJECTOR_HPP", """\
namespace trellis::inline v1 {
struct injector_type {
  int value = 1;
};
}  // namespace trellis::inline v1

// The resolution needs the complete injector.
#include <trellis/resolve.hpp>

namespace trellis::inline v1 {
inline constexpr int after = 2;
}  // namespace trellis::inline v1
""", intro="// The injector.\n")
        resolve = header("TRELLIS_RESOLVE_HPP", """\
#include <trellis/injector.hpp>

namespace trellis::inline v1 {
inline int resolve(const injector_type& target) { return target.value; }
}  // namespace trellis::inline v1
""", intro="// The resolution.\n")
        umbrella = UMBRELLA.replace("#include <trellis/api.hpp>",
                                    f"#include <trellis/{first}>\n#include <trellis/api.hpp>")
        files = {**TREE, "trellis/trellis.hpp": umbrella, "trellis/injector.hpp": injector,
                 "trellis/resolve.hpp": resolve}
        status, output, text = self.generate(files)
        self.assertEqual(status, 0, output)
        self.assertEqual(text.count("struct injector_type"), 1)
        if CXX_GIVEN:
            with tempfile.TemporaryDirectory() as directory:
                write_tree(Path(directory), files)
                status, output = run_tool(Path(directory), "--check", "--cxx", CXX)
                self.assertEqual(status, 0, output)
        sections = [[word for word in line.split()[2:] if set(word) != {"="}]
                    for line in text.splitlines() if line.startswith("// ==== include/")]
        return sections, text

    def test_include_at_the_end_continues_the_header_after_it(self):
        # injector.hpp reached first: its code, resolve's, then the rest of injector's.
        sections, text = self.expanded("injector.hpp")
        self.assertEqual(sections[:3], [["include/trellis/injector.hpp"],
                                        ["include/trellis/resolve.hpp"],
                                        ["include/trellis/injector.hpp", "(continued)"]])
        self.assertLess(text.index("inline int resolve"), text.index("int after = 2"))

    def test_header_reached_through_its_own_include_comes_first(self):
        # resolve.hpp reached first: it includes injector.hpp, whose include of resolve.hpp the
        # guard skips, so injector comes whole, then resolve, as the preprocessor reads them.
        sections, text = self.expanded("resolve.hpp")
        self.assertEqual(sections[:2], [["include/trellis/injector.hpp"],
                                        ["include/trellis/resolve.hpp"]])
        self.assertLess(text.index("int after = 2"), text.index("inline int resolve"))

    @unittest.skipUnless(CXX_GIVEN, "needs --cxx")
    def test_check_passes_without_an_artifact_and_with_a_fresh_one(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            write_tree(root, TREE)
            status, output = run_tool(root, "--check", "--cxx", CXX)
            self.assertEqual(status, 0, output)
            self.assertIn("isn't generated, so nothing is stale", output)
            self.assertFalse((root / "single_include").exists(), "--check writes nothing")
            self.assertEqual(run_tool(root)[0], 0)
            status, output = run_tool(root, "--check", "--cxx", CXX)
            self.assertEqual(status, 0, output)
            self.assertIn("single_include/trellis.hpp is up to date", output)

    @unittest.skipUnless(CXX_GIVEN, "needs --cxx")
    def test_check_fails_on_a_stale_artifact(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            write_tree(root, TREE)
            self.assertEqual(run_tool(root)[0], 0)
            write_tree(root, {"trellis/api.hpp": API.replace("answer", "reply")})
            status, output = run_tool(root, "--check", "--cxx", CXX)
            self.assertEqual(status, 1, output)
            self.assertIn("single_include/trellis.hpp is stale", output)

    @unittest.skipUnless(CXX_GIVEN, "needs --cxx")
    def test_check_fails_when_the_result_does_not_compile(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            # Well-formed for the generator, but `missing_type` is declared nowhere.
            write_tree(root, with_api("inline constexpr std::size_t answer", "missing_type answer"))
            status, output = run_tool(root, "--check", "--cxx", CXX)
            self.assertEqual(status, 1, output)
            self.assertIn("the single header doesn't compile on its own", output)


def main():
    global CXX
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--cxx", help="a GCC or Clang driver for the --check cases, which are "
                                       "skipped without it (MSVC: --check uses GCC-style flags)")
    args, rest = parser.parse_known_args()
    CXX = args.cxx
    unittest.main(argv=[sys.argv[0], *rest], verbosity=2)


if __name__ == "__main__":
    main()
