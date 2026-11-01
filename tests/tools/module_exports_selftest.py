#!/usr/bin/env python3
# Self-test of tools/module_exports.py, the module's export list check.
# Copyright (c) 2026 Aryan Patel
# SPDX-License-Identifier: BSL-1.0
"""Self-test of tools/module_exports.py: run the check on small trees whose verdict is known.

Each case builds a tree in a temporary directory, with a header under include/trellis/ and a
module interface modules/trellis.cppm, sometimes with a second header and module
(trellis.testing), and runs the check on it with --root and the build's Clang (--config). The header declares one of each kind of public name: a class template with a
deduction guide and a specialization, a variable, a function, an operator, an operator named by a
keyword (co_await), a concept, a scoped and an unscoped enumeration, a using-declaration of a name
in detail, a name in a nested public namespace and one in trellis::error, beside names in detail.
The check must pass the complete export list and fail when the module misses a public name,
exports a name in detail or a misspelling, places a using-declaration outside the namespace that
declares the entity, exports a name twice, or holds anything but export blocks of
using-declarations; when a header declares a public namespace alias; and, with a line rather than
a traceback, when the compiler's output isn't a JSON syntax tree. With a second module, which
re-exports trellis, the check must pass when each module exports its own names, and fail when the
second misses one or exports one of trellis's, when no module includes a header, when a module
re-exports a module that doesn't exist, and when two files declare one module.
CTest runs this as tools.module_exports_selftest, in Clang builds with the module enabled.
"""
import argparse
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
CHECK = ROOT / "tools" / "module_exports.py"
CONFIG = None  # set by main()

HEADER = """\
#ifndef TRELLIS_SELFTEST_API_HPP
#define TRELLIS_SELFTEST_API_HPP
namespace trellis::inline v1 {
namespace detail {
struct hidden {};
struct helper {};
}  // namespace detail
using detail::helper;
struct awaitable {};
inline awaitable operator co_await(awaitable value) { return value; }
template <class T>
struct widget {
  T value;
};
template <class T>
widget(T) -> widget<T>;
template <>
struct widget<void> {};
inline constexpr int answer = 42;
int twice(int value);
inline bool operator==(widget<int> a, widget<int> b) { return a.value == b.value; }
template <class T>
concept small = sizeof(T) < 8;
enum class color { red, green };
enum plain { first, second };
namespace tools {
struct gadget {};
}  // namespace tools
}  // namespace trellis::inline v1
namespace trellis::error {
template <class T>
struct E999 {
  static constexpr bool reported = false;
};
}  // namespace trellis::error
namespace {
struct internal {};
}  // namespace
#endif
"""

V1_EXPORTS = ["answer", "awaitable", "color", "first", "helper", "operator co_await",
              "operator==", "plain", "second", "small", "twice", "widget"]


def module(v1=V1_EXPORTS, extra_v1=(), tools=("gadget",), error=("E999",), tail=""):
    """A module interface that exports the names V1 and EXTRA_V1 from trellis::v1, TOOLS from
    trellis::v1::tools and ERROR from trellis::error, followed by TAIL."""
    def block(namespace, qualifier, names):
        lines = "".join(f"using {qualifier}::{name};\n" for name in names)
        return f"export namespace {namespace} {{\n{lines}}}  // namespace {namespace}\n"

    return ("// The module.\nmodule;\n#include <trellis/api.hpp>\nexport module trellis;\n"
            + block("trellis::inline v1", "trellis::v1", [*v1, *extra_v1])
            + block("trellis::inline v1::tools", "trellis::v1::tools", tools)
            + block("trellis::error", "trellis::error", error) + tail)


EXTRA_HEADER = """\
#ifndef TRELLIS_SELFTEST_EXTRA_HPP
#define TRELLIS_SELFTEST_EXTRA_HPP
#include <trellis/api.hpp>
namespace trellis::inline v1::testing {
struct probe {};
}  // namespace trellis::inline v1::testing
#endif
"""


def testing_module(exports=("probe",), imports="export import trellis;\n", extra_v1=()):
    """A second module, trellis.testing, over EXTRA_HEADER, that re-exports trellis (IMPORTS) and
    exports EXPORTS from trellis::v1::testing and EXTRA_V1 from trellis::v1."""
    lines = "".join(f"using trellis::v1::testing::{name};\n" for name in exports)
    v1 = "".join(f"using trellis::v1::{name};\n" for name in extra_v1)
    return ("module;\n#include <trellis/testing/extra.hpp>\nexport module trellis.testing;\n"
            + imports + "export namespace trellis::inline v1::testing {\n" + lines + "}\n"
            + (f"export namespace trellis::inline v1 {{\n{v1}}}\n" if v1 else ""))


def run_check(module_text, header=HEADER, compiler_output=None, extra_files=None):
    """Build a tree with HEADER as include/trellis/api.hpp, MODULE_TEXT as modules/trellis.cppm
    and EXTRA_FILES ({path relative to the root: text}), run the check on it, and return its exit
    status and output. With COMPILER_OUTPUT, the compiler is a script that prints it instead of
    Clang."""
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        (root / "include" / "trellis").mkdir(parents=True)
        (root / "include" / "trellis" / "api.hpp").write_text(header, encoding="utf-8")
        (root / "modules").mkdir()
        (root / "modules" / "trellis.cppm").write_text(module_text, encoding="utf-8")
        for name, text in (extra_files or {}).items():
            (root / name).parent.mkdir(parents=True, exist_ok=True)
            (root / name).write_text(text, encoding="utf-8")
        source = ["--config", str(CONFIG)]
        if compiler_output is not None:
            fake = root / "fake-clang"
            fake.write_text(f"#!/bin/sh\necho '{compiler_output}'\n", encoding="utf-8")
            fake.chmod(0o755)
            source = ["--compiler", str(fake)]
        result = subprocess.run([sys.executable, "-I", str(CHECK), *source, "--root", str(root)],
                                capture_output=True, text=True, timeout=120, check=False)
        return result.returncode, result.stdout + result.stderr


class ModuleExportsCheckTest(unittest.TestCase):
    def assert_passes(self, result):
        status, output = result
        self.assertEqual(status, 0, output)
        self.assertIn("PASS module exports: the modules export the 14 public names", output)

    def assert_fails(self, result, *fragments):
        status, output = result
        self.assertEqual(status, 1, output)
        self.assertIn("FAIL module exports: ", output)
        for fragment in fragments:
            self.assertIn(fragment, output)

    def test_complete_list_passes(self):
        # Neither detail::hidden, the unnamed namespace's internal, the deduction guide nor the
        # specialization widget<void> is a name to export.
        self.assert_passes(run_check(module()))

    def test_missing_name_fails_with_the_line_to_add(self):
        self.assert_fails(
            run_check(module(v1=[name for name in V1_EXPORTS if name != "answer"])),
            "trellis::answer is public but not exported: add `using trellis::v1::answer;` to "
            "`export namespace trellis::inline v1 { ... }`")

    def test_using_declaration_is_exported_by_the_name_it_declares(self):
        # Clang names it "detail::helper"; the module exports it as trellis::v1::helper.
        self.assert_fails(run_check(module(v1=[name for name in V1_EXPORTS if name != "helper"])),
                          "trellis::helper is public but not exported: add "
                          "`using trellis::v1::helper;` to `export namespace trellis::inline v1")

    def test_keyword_operator_keeps_its_space(self):
        others = [name for name in V1_EXPORTS if name != "operator co_await"]
        self.assert_fails(run_check(module(v1=others)),
                          "trellis::operator co_await is public but not exported: add "
                          "`using trellis::v1::operator co_await;`")
        self.assert_passes(run_check(module(v1=[*others, "operator  co_await"])))

    def test_output_that_is_not_json_fails_without_a_traceback(self):
        status, output = run_check(module(), compiler_output="not json")
        self.assertEqual(status, 1, output)
        self.assertIn("output isn't Clang's JSON syntax tree", output)
        self.assertIn("it starts with: not json", output)
        self.assertNotIn("Traceback", output)

    def test_output_without_namespace_trellis_fails(self):
        status, output = run_check(module(), compiler_output='{"kind": "NamespaceDecl"}')
        self.assertEqual(status, 1, output)
        self.assertIn("syntax tree has no namespace trellis", output)

    def test_missing_enumerator_of_unscoped_enum_fails(self):
        self.assert_fails(run_check(module(v1=[name for name in V1_EXPORTS if name != "second"])),
                          "trellis::second is public but not exported")

    def test_missing_nested_namespace_name_fails(self):
        self.assert_fails(run_check(module(tools=())),
                          "trellis::tools::gadget is public but not exported: add "
                          "`using trellis::v1::tools::gadget;` to "
                          "`export namespace trellis::inline v1::tools { ... }`")

    def test_missing_error_vocabulary_fails(self):
        self.assert_fails(run_check(module(error=())),
                          "trellis::error::E999 is public but not exported: add "
                          "`using trellis::error::E999;` to `export namespace trellis::error")

    def test_exported_detail_name_fails(self):
        tail = ("export namespace trellis::inline v1::detail {\n"
                "using trellis::v1::detail::hidden;\n}\n")
        self.assert_fails(run_check(module(tail=tail)),
                          "`using trellis::v1::detail::hidden;` exports something that isn't a "
                          "public name")

    def test_misspelt_export_fails(self):
        self.assert_fails(run_check(module(extra_v1=["widgte"])),
                          "`using trellis::v1::widgte;` exports something that isn't a public name")

    def test_export_outside_the_declaring_namespace_fails(self):
        tail = "export namespace trellis {\nusing trellis::v1::answer;\n}\n"
        self.assert_fails(run_check(module(tail=tail)),
                          "`using trellis::v1::answer;` in `export namespace trellis`: export it "
                          "in the namespace that declares it")

    def test_name_exported_twice_fails(self):
        self.assert_fails(run_check(module(extra_v1=["answer"])),
                          "`using trellis::v1::answer;` is exported twice")

    def test_other_declaration_in_an_export_block_fails(self):
        tail = "export namespace trellis::inline v1 {\nint later();\n}\n"
        self.assert_fails(run_check(module(tail=tail)),
                          "`int later();` in `export namespace trellis::inline v1`: an export "
                          "block may hold only using-declarations")

    def test_other_text_in_the_purview_fails(self):
        self.assert_fails(run_check(module(tail="export int later();\n")),
                          "unexpected text in the purview of modules/trellis.cppm: "
                          "export int later();")

    def test_missing_module_declaration_fails(self):
        self.assert_fails(run_check("module;\n#include <trellis/api.hpp>\n"),
                          "modules/trellis.cppm has no module declaration")

    def test_second_module_exports_its_own_names(self):
        status, output = run_check(module(), extra_files={
            "include/trellis/testing/extra.hpp": EXTRA_HEADER,
            "modules/trellis.testing.cppm": testing_module()})
        self.assertEqual(status, 0, output)
        self.assertIn("the modules export the 15 public names of the 2 headers", output)
        self.assertIn("(trellis 14, trellis.testing 1)", output)

    def test_second_module_missing_its_name_fails(self):
        self.assert_fails(
            run_check(module(), extra_files={"include/trellis/testing/extra.hpp": EXTRA_HEADER,
                                             "modules/trellis.testing.cppm": testing_module(())}),
            "trellis::testing::probe is public but not exported: add "
            "`using trellis::v1::testing::probe;` to `export namespace trellis::inline v1::testing "
            "{ ... }` in modules/trellis.testing.cppm")

    def test_second_module_exporting_a_name_of_trellis_fails(self):
        self.assert_fails(
            run_check(module(), extra_files={
                "include/trellis/testing/extra.hpp": EXTRA_HEADER,
                "modules/trellis.testing.cppm": testing_module(extra_v1=["answer"])}),
            "`using trellis::v1::answer;` exports something that isn't a public name that "
            "modules/trellis.testing.cppm provides")

    def test_second_module_without_reexport_must_export_everything(self):
        # Without `export import trellis;`, importing trellis.testing alone must still give the
        # names its header provides, as including the header does.
        self.assert_fails(
            run_check(module(), extra_files={
                "include/trellis/testing/extra.hpp": EXTRA_HEADER,
                "modules/trellis.testing.cppm": testing_module(imports="")}),
            "trellis::answer is public but not exported: add `using trellis::v1::answer;` to "
            "`export namespace trellis::inline v1 { ... }` in modules/trellis.testing.cppm")

    def test_header_that_no_module_includes_fails(self):
        self.assert_fails(
            run_check(module(), extra_files={"include/trellis/testing/extra.hpp": EXTRA_HEADER}),
            "trellis::testing::probe is public, but no module includes its header")

    def test_reexport_of_a_missing_module_fails(self):
        self.assert_fails(
            run_check(module(), extra_files={
                "include/trellis/testing/extra.hpp": EXTRA_HEADER,
                "modules/trellis.testing.cppm": testing_module(
                    imports="export import trellis;\nexport import trellis.missing;\n")}),
            "modules/trellis.testing.cppm: `export import trellis.missing;` names no module of "
            "modules/")

    def test_two_files_declaring_one_module_fail(self):
        self.assert_fails(run_check(module(), extra_files={"modules/copy.cppm": module()}),
                          "modules/copy.cppm and modules/trellis.cppm both declare module trellis")

    def test_namespace_alias_is_reported(self):
        header = HEADER.replace("namespace tools {",
                                "namespace gadgets = trellis::v1;\nnamespace tools {")
        self.assert_fails(run_check(module(), header),
                          "trellis::gadgets is a namespace alias, which a using-declaration can't "
                          "export")

    def test_header_that_does_not_compile_fails(self):
        self.assert_fails(run_check(module(), HEADER.replace("int twice", "int twice(")),
                          "the headers don't compile")


def main():
    global CONFIG
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--config", type=Path, required=True,
                        help="build/<preset>/trellis_compiler.json of a Clang build")
    args, rest = parser.parse_known_args()
    CONFIG = args.config.resolve()
    unittest.main(argv=[sys.argv[0], *rest], verbosity=2)


if __name__ == "__main__":
    main()
