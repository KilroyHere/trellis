#!/usr/bin/env python3
# The modules' export list check: modules/*.cppm export exactly the public names of include/.
# Copyright (c) 2026 Aryan Patel
# SPDX-License-Identifier: BSL-1.0
"""Check that the C++20 modules, modules/*.cppm, export every public name of include/trellis/ and
nothing else, each module the names of the headers it includes.

  tools/module_exports.py --config build/<preset>/trellis_compiler.json [--root DIR]
  tools/module_exports.py --compiler clang++-18 [--root DIR]

The public names come from Clang, not from reading the headers as text: the check compiles a
translation unit that includes some headers, with -Xclang -ast-dump=json
-Xclang -ast-dump-filter=trellis, and walks the syntax tree of namespace trellis. A public name is
a named declaration in namespace trellis, in one of its inline namespaces (v1), or in a nested
namespace other than detail, at any depth (trellis::error, trellis::testing). Specializations,
deduction guides and implicit declarations aren't names of their own: they come with their
template. The enumerators of an unscoped enumeration are names of the namespace, so they count,
and a using-declaration counts under the name it declares.

Each file under modules/ is the interface of one module:

    module;
    #include <trellis/trellis.hpp>          the headers the module provides
    export module trellis;
    export import other.module;            optional: a module of modules/ that it re-exports
    export namespace trellis::inline v1 {
    using trellis::v1::bind;               using-declarations only, in blocks like this one
    }

A module provides the public names of the Trellis headers its global module fragment includes, and
must export exactly those, less the ones that a module it re-exports with `export import` provides.
Every using-declaration names an entity of its block's own namespace. That placement is required,
not style: GCC 14 doesn't make `using trellis::v1::bind;` visible to importers when the declaration
sits in namespace trellis, outside v1 (docs/internals/delivery.md).

It fails on:
  - a public name that its module doesn't export, giving the line to add, its block and its file;
  - an export that isn't a public name its module provides: a name in detail, a misspelling, a
    removed entity, or a name of a module it re-exports;
  - an export placed outside the namespace that declares the entity, or exported twice;
  - a public name that no module provides, because no module includes its header;
  - anything else in a module's purview, an `export import` of a module not in modules/, and two
    files declaring one module, or none declaring trellis;
  - a public declaration a using-declaration can't export, such as a namespace alias.

The flags come from --config, the JSON file that CMake writes for tools/compile_fail.py, without
its include and warning flags; --root checks another tree with the same layout, which is how the
self-test, tests/tools/module_exports_selftest.py, plants mistakes. CTest runs the check as
module.exports in Clang builds with the module enabled (`ctest -L module`).

Exit status: 0 when the export lists match, 1 when they don't or the headers don't compile, 2 for
a usage error.
"""
import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TIMEOUT = 300

# Declarations that aren't names of their own, and those whose contents belong to the enclosing
# namespace.
NOT_NAMES = {
    "ClassTemplateSpecializationDecl", "ClassTemplatePartialSpecializationDecl",
    "VarTemplateSpecializationDecl", "VarTemplatePartialSpecializationDecl", "StaticAssertDecl",
    "EmptyDecl", "UsingDirectiveDecl", "UsingShadowDecl", "CXXDeductionGuideDecl", "FriendDecl",
}
TRANSPARENT = {"LinkageSpecDecl", "ExportDecl"}
# Public declarations that a using-declaration can't export; the module would need another form.
UNSUPPORTED = {"NamespaceAliasDecl": "a namespace alias",
               "UsingEnumDecl": "a using-enum-declaration"}

IDENTIFIER = r"[A-Za-z_]\w*"
MODULE_NAME = rf"{IDENTIFIER}(?:\.{IDENTIFIER})*"
# export namespace trellis::inline v1 { ... }, with no braces inside.
NAMESPACE_BLOCK = re.compile(
    rf"export\s+namespace\s+((?:inline\s+)?{IDENTIFIER}(?:\s*::\s*(?:inline\s+)?{IDENTIFIER})*)"
    r"\s*\{([^{}]*)\}")
# using trellis::v1::bind or using trellis::v1::operator==, without the semicolon.
USING = re.compile(rf"using\s+(?:::\s*)?((?:{IDENTIFIER}\s*::\s*)+)"
                   rf"(operator\s*[^\s;]+(?:\s*[^\s;]+)?|{IDENTIFIER})")
MODULE_DECLARATION = re.compile(rf"export\s+module\s+({MODULE_NAME})\s*;")
EXPORT_IMPORT = re.compile(rf"export\s+import\s+({MODULE_NAME})\s*;")
TRELLIS_INCLUDE = re.compile(r"^[ \t]*#[ \t]*include[ \t]*<(trellis/[^>]+)>", re.M)


class CheckError(Exception):
    """A problem that stops the check before it can compare the lists."""


def spelled(path, inline=()):
    """A namespace path as C++ spells it: ("trellis", "v1") is trellis::inline v1 where v1 is in
    INLINE, otherwise trellis::v1."""
    return "::".join(f"inline {part}" if part in inline else part for part in path)


def public_name(path, name, inline):
    """The name users write: the namespace path without its inline namespaces, then NAME."""
    return "::".join([part for part in path if part not in inline] + [name])


def compiler_and_flags(args):
    """The Clang driver and the flags to parse the headers with."""
    if args.compiler:
        return args.compiler, ["-std=c++20"]
    try:
        config = json.loads(Path(args.config).read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        raise CheckError(f"can't read {args.config}: {error}; configure the build first") from error
    if config.get("frontend") != "GNU" or "Clang" not in config.get("compiler_id", ""):
        raise CheckError(f"{args.config} names {config.get('compiler_id')} "
                         f"{config.get('compiler')}; the check needs Clang's syntax tree")
    # The language mode, standard library and target matter; the include path is replaced, and
    # warnings don't matter to a syntax tree.
    flags = [flag for flag in config["flags"] if not flag.startswith(("-I", "-W"))]
    return config["compiler"], flags


def syntax_tree(root, compiler, flags, headers):
    """The JSON syntax trees of namespace trellis, from a translation unit that includes HEADERS
    (trellis/... paths under ROOT/include): one tree per definition of the namespace."""
    include_dir = root / "include"
    source = "".join(f"#include <{header}>\n" for header in headers)
    command = [compiler, *flags, f"-I{include_dir}", "-fsyntax-only", "-Xclang", "-ast-dump=json",
               "-Xclang", "-ast-dump-filter=trellis", "-x", "c++", "-"]
    try:
        result = subprocess.run(command, input=source, capture_output=True, text=True,
                                timeout=TIMEOUT, check=False)
    except (OSError, subprocess.TimeoutExpired) as error:
        raise CheckError(f"{compiler} failed to run: {error}") from error
    if result.returncode != 0:
        first = next((line for line in result.stderr.splitlines() if "error" in line),
                     result.stderr.strip()[:300])
        raise CheckError(f"the headers don't compile with {compiler}: {first}")
    decoder = json.JSONDecoder()
    text, position, trees = result.stdout, 0, []
    while True:
        while position < len(text) and text[position].isspace():
            position += 1
        if position == len(text):
            return trees
        try:
            tree, position = decoder.raw_decode(text, position)
        except ValueError as error:
            excerpt = " ".join(text[position:position + 60].split())
            raise CheckError(f"{compiler}'s output isn't Clang's JSON syntax tree ({error}), it "
                             f"starts with: {excerpt}") from error
        trees.append(tree)


def collect(node, path, inline, names, problems):
    """Add the public names declared in the namespace NODE, whose path is PATH, to NAMES: {(path,
    name): kind}. Records the inline namespaces in INLINE, and unexportable declarations in
    PROBLEMS."""
    for child in node.get("inner", []):
        kind, name = child.get("kind"), child.get("name")
        if kind == "NamespaceDecl":
            if name and name != "detail":  # an unnamed namespace has internal linkage
                if child.get("isInline"):
                    inline.add(name)
                collect(child, path + (name,), inline, names, problems)
        elif kind in TRANSPARENT:
            collect(child, path, inline, names, problems)
        elif kind in NOT_NAMES or child.get("isImplicit") or not name:
            continue
        elif kind in UNSUPPORTED:
            problems.add(f"{public_name(path, name, inline)} is {UNSUPPORTED[kind]}, which a "
                         f"using-declaration can't export; extend the modules in modules/ and "
                         f"tools/module_exports.py")
        elif kind == "FunctionTemplateDecl" and any(
                grandchild.get("kind") == "CXXDeductionGuideDecl"
                for grandchild in child.get("inner", [])):
            continue
        else:
            if kind == "UsingDecl":
                # Clang names a using-declaration as written, "detail::helper"; the name it
                # declares in this namespace is the last part.
                name = name.rsplit("::", 1)[-1]
            names.setdefault((path, name), kind)
            if kind == "EnumDecl" and not child.get("scopedEnumTag"):
                for enumerator in child.get("inner", []):
                    if enumerator.get("kind") == "EnumConstantDecl":
                        names.setdefault((path, enumerator["name"]), "EnumConstantDecl")


class Headers:
    """The public names that sets of headers declare, each set parsed once, with the inline
    namespaces seen and the declarations that can't be exported."""

    def __init__(self, root, compiler, flags):
        self.root, self.compiler, self.flags = root, compiler, flags
        self.inline, self.problems, self.cache = set(), set(), {}

    def names(self, headers):
        """{(namespace path, name): kind} for the public names that HEADERS declare."""
        key = tuple(sorted(headers))
        if key not in self.cache:
            names = {}
            if key:
                trees = syntax_tree(self.root, self.compiler, self.flags, key)
                # The filter also dumps any other declaration whose qualified name contains
                # "trellis"; only namespace trellis itself holds the library.
                namespaces = [tree for tree in trees if tree.get("kind") == "NamespaceDecl"
                              and tree.get("name") == "trellis"]
                if not namespaces:
                    raise CheckError(f"{self.compiler}'s syntax tree has no namespace trellis")
                for tree in namespaces:
                    collect(tree, ("trellis",), self.inline, names, self.problems)
            self.cache[key] = names
        return self.cache[key]


def declared_name(text):
    """The name a using-declaration ends with, spelled as Clang spells it: "bind", "operator==",
    "operator co_await", "operator new[]", "operator\"\"_ms"."""
    if not text.startswith("operator"):
        return text
    rest = text[len("operator"):].strip()
    word = re.match(r"[A-Za-z_]\w*", rest)
    if word:  # a keyword operator keeps one space after "operator"
        tail = re.sub(r"\s+", "", rest[word.end():])
        return f"operator {word.group()}{tail}"
    return "operator" + re.sub(r"\s+", "", rest)


def strip_comments(text):
    """TEXT without // and /* */ comments; the module interface holds no string literals."""
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


class Module:
    """One module interface under modules/: its name, the Trellis headers its global module
    fragment includes, the modules it re-exports, its exports {(namespace path, name):
    using-declaration}, and the problems with its form."""

    def __init__(self, path):
        self.path = path
        self.file = f"modules/{path.name}"
        text = strip_comments(path.read_text(encoding="utf-8"))
        declaration = MODULE_DECLARATION.search(text)
        if not declaration:
            raise CheckError(f"{self.file} has no module declaration, `export module NAME;`")
        self.name = declaration.group(1)
        self.headers = TRELLIS_INCLUDE.findall(text[:declaration.start()])
        self.problems = []
        purview = text[declaration.end():]
        self.imports = EXPORT_IMPORT.findall(purview)
        self.exports = self._parse_exports(EXPORT_IMPORT.sub(" ", purview))

    def _parse_exports(self, purview):
        exports = {}

        def stray(start, end):
            """Report anything but white space in purview[start:end], between the export blocks."""
            if leftover := " ".join(purview[start:end].split()):
                self.problems.append(
                    f"unexpected text in the purview of {self.file}: {leftover[:80]}; it may hold "
                    f"only `export import ...;` and `export namespace ... {{ using ...; }}` blocks")

        position = 0
        for block in NAMESPACE_BLOCK.finditer(purview):
            stray(position, block.start())
            position = block.end()
            path = tuple(re.sub(r"^inline\s+", "", part.strip())
                         for part in block.group(1).split("::"))
            for statement in block.group(2).split(";"):
                statement = " ".join(statement.split())
                if not statement:
                    continue
                using = USING.fullmatch(statement)
                if not using:
                    self.problems.append(f"`{statement};` in `export namespace {block.group(1)}`: "
                                         f"an export block may hold only using-declarations")
                    continue
                qualifier = tuple(part.strip() for part in using.group(1).split("::")
                                  if part.strip())
                name = declared_name(using.group(2))
                if qualifier != path:
                    self.problems.append(
                        f"`{statement};` in `export namespace {block.group(1)}`: export it in the "
                        f"namespace that declares it, `export namespace {'::'.join(qualifier)} "
                        f"{{ ... }}`, as GCC 14 needs")
                    continue
                if (path, name) in exports:
                    self.problems.append(f"`{statement};` is exported twice")
                exports[(path, name)] = statement
        stray(position, len(purview))
        return exports


def check(root, compiler, flags):
    """The problems with ROOT's export lists, one message each, and the summary of a clean
    check."""
    include_dir = root / "include"
    all_headers = sorted(path.relative_to(include_dir).as_posix()
                         for path in (include_dir / "trellis").rglob("*.hpp"))
    if not all_headers:
        raise CheckError(f"no headers under {include_dir / 'trellis'}")
    files = sorted((root / "modules").glob("*.cppm"))
    modules = {}
    problems = []
    for path in files:
        module = Module(path)
        if module.name in modules:
            problems.append(f"{modules[module.name].file} and {module.file} both declare module "
                            f"{module.name}")
        modules[module.name] = module
    if "trellis" not in modules:
        raise CheckError(f"no file under {root / 'modules'} declares `export module trellis;`")

    headers = Headers(root, compiler, flags)
    all_names = headers.names(all_headers)
    provided = set()
    counts = []
    for module in modules.values():
        problems += module.problems
        names = headers.names(module.headers)
        provided |= set(names)
        expected = set(names)
        for imported in module.imports:
            if imported not in modules:
                problems.append(f"{module.file}: `export import {imported};` names no module of "
                                f"modules/")
            else:
                expected -= set(headers.names(modules[imported].headers))
        inline = headers.inline
        for path, name in sorted(expected - set(module.exports)):
            problems.append(f"{public_name(path, name, inline)} is public but not exported: add "
                            f"`using {'::'.join(path)}::{name};` to `export namespace "
                            f"{spelled(path, inline)} {{ ... }}` in {module.file}")
        for key in sorted(set(module.exports) - expected):
            problems.append(f"`{module.exports[key]};` exports something that isn't a public name "
                            f"that {module.file} provides (a name in detail, a misspelling, a "
                            f"removed entity, or a name of a module it re-exports)")
        counts.append(f"{module.name} {len(expected)}")
    for path, name in sorted(set(all_names) - provided):
        problems.append(f"{public_name(path, name, headers.inline)} is public, but no module "
                        f"includes its header: include the header in the global module fragment "
                        f"of a module in modules/")
    problems += sorted(headers.problems)
    summary = (f"the modules export the {len(all_names)} public names of the {len(all_headers)} "
               f"headers under include/trellis/, and nothing else ({', '.join(counts)})")
    return problems, summary


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--config", type=Path,
                        help="build/<preset>/trellis_compiler.json, written by CMake")
    source.add_argument("--compiler", help="a Clang driver, such as clang++-18")
    parser.add_argument("--root", type=Path, default=ROOT,
                        help="the tree to check (default: this repository)")
    args = parser.parse_args(argv)
    root = args.root.resolve()
    try:
        compiler, flags = compiler_and_flags(args)
        problems, summary = check(root, compiler, flags)
    except CheckError as error:
        print(f"FAIL module exports: {error}")
        return 1
    for problem in problems:
        print(f"FAIL module exports: {problem}")
    if problems:
        return 1
    print(f"PASS module exports: {summary}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
