#!/usr/bin/env python3
# The single-header generator: the headers of include/trellis/ as one file.
# Copyright (c) 2026 Aryan Patel
# SPDX-License-Identifier: BSL-1.0
"""Generate the single header, single_include/trellis.hpp, from the headers in include/trellis/
(plan §10, ADR-0008).

  tools/amalgamate.py [--output FILE] [--root DIR]
  tools/amalgamate.py --check [--cxx COMPILER] [--root DIR]

The single header holds every header under include/trellis/, the umbrella header and the internal
headers alike, each once, in the order the preprocessor reads them when the umbrella header is
included and then every other header, by path: each #include of a Trellis header is replaced by
that header where it stands, unless the header has been started already, as its include guard
would make it. A header that includes another at its end therefore comes right before it, as
static/injector.hpp does with static/resolve.hpp, so that either can be included first. Each header
loses its include guard, its #include lines of Trellis headers and its copyright and license lines;
one notice at the top covers the whole file, and one include guard,
TRELLIS_SINGLE_INCLUDE_TRELLIS_HPP, surrounds it. A standard header that a header includes
unconditionally is included once, at the top; an #include inside #if stays where it is.

Generation is strict rather than clever, so that it fails instead of producing a header that
means something else. It fails when a header:
  - doesn't start with an include guard (#ifndef G, #define G) that its last directive closes,
    with nothing but comments before it;
  - names another header's include guard macro, which is gone from the single header;
  - includes a Trellis header inside #if, or includes one that doesn't exist;
  - uses #include "..." or #pragma once;
  - lacks the copyright or SPDX line, or carries one that differs from the other headers'.

Without --check, it writes the single header to FILE (default single_include/trellis.hpp, which
.gitignore excludes: a build artifact, which releases will attach), only if the text changed.
CMake runs it with --output into the build tree for the unit tests that use the single header
(tests/single_header/, label single_header). The self-test is tests/tools/amalgamate_selftest.py.

--check writes nothing. It fails when the single header can't be generated, when the result
doesn't compile on its own (C++20, syntax only, -Wall -Wextra -Wundef as errors, included twice,
with no include path to include/), or when single_include/trellis.hpp exists and is stale. The
compiler is --cxx, else $CXX, else c++, with GCC-style flags.

Exit status: 0 on success, 1 when the check or the generation fails, 2 for a usage error.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_OUTPUT = Path("single_include") / "trellis.hpp"
OUTER_GUARD = "TRELLIS_SINGLE_INCLUDE_TRELLIS_HPP"
COMPILE_TIMEOUT = 300

DIRECTIVE = re.compile(r"\s*#\s*(\w+)\s*(.*)", re.S)
INCLUDE = re.compile(r"""([<"])([^>"]+)[>"]\s*(?://.*)?$""", re.S)
COPYRIGHT = re.compile(r"//\s*Copyright\b.*")
SPDX = re.compile(r"//\s*SPDX-License-Identifier:.*")
VERSION = re.compile(r"#define\s+TRELLIS_VERSION_(MAJOR|MINOR|PATCH)\s+(\d+)")
# An identifier, or a pp-number ([lex.ppnumber]), whose ' digit separators (1'000) don't start a
# character literal. Taking identifiers whole keeps u8'a' a character literal with its prefix.
# A digit separator inside an #if broke Clang's module scanning for Boost.DI (plan R15).
TOKEN = re.compile(r"[A-Za-z_]\w*|\.?\d(?:[eEpP][+-]|'[0-9A-Za-z_]|[\w.])*")


class AmalgamationError(Exception):
    """A header that the single header can't be generated from, or another fatal problem."""


def logical_lines(text):
    """TEXT as (first line number, text, starts in code) triples: a line continued with a trailing
    backslash comes with its continuation lines, unchanged, and "starts in code" says whether it
    starts outside a comment, which is where a directive can be. Tracks /* */ comments, string and
    character literals and raw strings, so that a "#" or "/*" inside one changes nothing."""
    physical = text.split("\n")
    in_block = False
    raw_end = None
    index = 0
    while index < len(physical):
        start = index
        line = physical[index]
        while line.endswith("\\") and index + 1 < len(physical):
            index += 1
            line += "\n" + physical[index]
        index += 1
        code_line = not in_block and raw_end is None
        in_block, raw_end = scan(line, in_block, raw_end)
        yield start + 1, line, code_line


def scan(line, in_block, raw_end):
    """The comment and raw-string state after LINE, starting in IN_BLOCK (inside /* */) or inside a
    raw string that RAW_END closes."""
    position = 0
    while position < len(line):
        if raw_end is not None:
            end = line.find(raw_end, position)
            if end < 0:
                return in_block, raw_end
            position, raw_end = end + len(raw_end), None
        elif in_block:
            end = line.find("*/", position)
            if end < 0:
                return True, None
            position, in_block = end + 2, False
        elif line.startswith("//", position):
            return False, None
        elif line.startswith("/*", position):
            position, in_block = position + 2, True
        elif (raw := re.match(r'(?:u8|[uUL])?R"([^()\\\s]{0,16})\(', line[position:])):
            position, raw_end = position + raw.end(), f'){raw.group(1)}"'
        elif (token := TOKEN.match(line, position)):
            position = token.end()  # an identifier or a number, digit separators included
        elif line[position] in "\"'":
            quote, position = line[position], position + 1
            while position < len(line) and line[position] != quote:
                position += 2 if line[position] == "\\" else 1
            position += 1
        else:
            position += 1
    return in_block, raw_end


class Include:
    """An #include of a Trellis header in a header's body, which the single header replaces with
    that header where it stands."""

    def __init__(self, target):
        self.target = target


class Header:
    """One header of include/trellis/, read and checked: its body without the include guard, the
    notice lines and the unconditional standard includes, with an Include for each Trellis include;
    the standard headers it includes unconditionally; its guard macro; and its notice."""

    def __init__(self, root, path):
        self.path = path
        self.name = path.relative_to(root / "include").as_posix()  # trellis/meta/result.hpp
        self.standard = set()
        self.body = []
        self.guard = None
        self.copyright = self.spdx = None
        self._read(root, path.read_text(encoding="utf-8"))

    def fail(self, line, message):
        raise AmalgamationError(f"include/{self.name}:{line}: {message}")

    def _read(self, root, text):
        directives = []  # (line number, name, argument)
        depth = 0
        guard_line = None
        for number, line, code_line in logical_lines(text.rstrip("\n")):
            directive = DIRECTIVE.match(line) if code_line else None
            if directive is None:
                if not directives:  # the leading comment, before the include guard
                    if code_line and line.strip() and not line.lstrip().startswith(("//", "/*")):
                        self.fail(number, "code before the include guard")
                    if COPYRIGHT.fullmatch(line.strip()):
                        self.copyright = self._once(self.copyright, line.strip(), number)
                        continue
                    if SPDX.fullmatch(line.strip()):
                        self.spdx = self._once(self.spdx, line.strip(), number)
                        continue
                self.body.append(line)
                continue
            name, argument = directive.group(1), directive.group(2).strip()
            directives.append((number, name, argument))
            if len(directives) == 1:
                if name != "ifndef" or not re.fullmatch(r"\w+", argument):
                    self.fail(number, "a header must start with an include guard, #ifndef G and "
                                      "#define G, after its leading comment")
                self.guard, guard_line, depth = argument, number, 1
                continue
            if len(directives) == 2:
                if name != "define" or argument != self.guard:
                    self.fail(number, f"#ifndef {self.guard} must be followed by #define "
                                      f"{self.guard}")
                continue
            if depth == 0:
                self.fail(number, f"#{name} after the end of the include guard {self.guard}; "
                                  f"the guard must enclose the whole header")
            if name in ("if", "ifdef", "ifndef"):
                depth += 1
            elif name == "endif":
                depth -= 1
                if depth == 0:
                    continue  # the guard's #endif
            elif name in ("else", "elif", "elifdef", "elifndef") and depth == 1:
                self.fail(number, f"#{name} of the include guard {self.guard}; the guard must "
                                  f"enclose the whole header")
            elif name == "pragma" and argument.split()[:1] == ["once"]:
                self.fail(number, "#pragma once: Trellis headers use include guards")
            elif name == "include":
                if self._include(root, number, argument, depth):
                    continue
            self.body.append(line)
        if self.guard is None:
            self.fail(1, "no include guard")
        if depth != 0:
            self.fail(guard_line, f"the include guard {self.guard} isn't closed at the end")
        if self.copyright is None or self.spdx is None:
            self.fail(1, "the leading comment needs a copyright line and an "
                         "SPDX-License-Identifier line, which the single header's notice replaces")

    def _once(self, previous, line, number):
        if previous is not None:
            self.fail(number, f"a second notice line: {line}")
        return line

    def _include(self, root, number, argument, depth):
        """Records the include on line NUMBER; returns whether the line leaves the body."""
        include = INCLUDE.match(argument)
        if include is None:
            self.fail(number, f"can't read #include {argument}")
        bracket, target = include.groups()
        if bracket == '"':
            self.fail(number, f'#include "{target}": include headers with <...>')
        if target.startswith("trellis/"):
            if depth != 1:
                self.fail(number, f"#include <{target}> inside #if: the single header holds each "
                                  f"Trellis header once, unconditionally")
            if not (root / "include" / target).is_file():
                self.fail(number, f"#include <{target}>: no such header under include/")
            self.body.append(Include(target))
            return True
        if depth == 1:
            self.standard.add(target)
            return True
        return False  # a conditional standard include stays in place


def has_code(lines):
    """Whether LINES hold anything but blank lines and // comments."""
    return any(line.strip() and not line.lstrip().startswith("//") for line in lines)


def expansion(headers):
    """The single header's pieces, (header, continued, lines), in the order the preprocessor reads
    the headers when the umbrella header and then every other header, by path, is included: each
    Include is replaced by its header where it stands, unless that header has been started, which
    its include guard would skip. A header's leading comment stays with its first code, after the
    headers it includes first; a header whose code an Include interrupts continues in a second
    piece."""
    started, pieces = set(), []

    def emit(name):
        if name in started:
            return
        started.add(name)
        header = headers[name]
        chunk, count = [], 0

        def flush():
            nonlocal chunk, count
            lines = squeezed(chunk)
            if lines or count == 0:
                pieces.append((header, count > 0, lines))
                count += 1
            chunk = []

        for item in header.body:
            if not isinstance(item, Include):
                chunk.append(item)
            elif item.target not in started:
                if has_code(chunk):
                    flush()
                emit(item.target)
        flush()

    umbrella = "trellis/trellis.hpp"
    for name in ([umbrella] if umbrella in headers else []) + sorted(headers):
        emit(name)
    return pieces


def squeezed(lines):
    """LINES without leading and trailing blank lines, and with runs of blank lines made one."""
    result = []
    for line in lines:
        if line.strip() or (result and result[-1].strip()):
            result.append(line.rstrip())
    while result and not result[-1].strip():
        result.pop()
    return result


def amalgamate(root):
    """The single header's text, and the number of headers in it."""
    include_dir = root / "include" / "trellis"
    paths = sorted(include_dir.rglob("*.hpp"))
    if not paths:
        raise AmalgamationError(f"no headers under {include_dir}")
    headers = {header.name: header for header in (Header(root, path) for path in paths)}
    order = list(headers.values())
    pieces = expansion(headers)

    notices = {(header.copyright, header.spdx) for header in order}
    if len(notices) != 1:
        raise AmalgamationError("the headers' copyright or SPDX lines differ: "
                                + "; ".join(" / ".join(notice) for notice in sorted(notices)))
    copyright_line, spdx_line = notices.pop()

    # Stripping the guards is safe only if nothing tests them: a header that checks whether another
    # was included would read the guard as undefined in the single header.
    guards = {header.guard: header.name for header in order}
    if len(guards) != len(order):
        raise AmalgamationError("two headers share an include guard")
    guard_use = re.compile(r"\b(" + "|".join(map(re.escape, guards)) + r")\b")
    for header in order:
        for line in header.body:
            if isinstance(line, Include):
                continue
            if used := guard_use.search(line):
                raise AmalgamationError(
                    f"include/{header.name} names {used.group(1)}, the include guard of "
                    f"include/{guards[used.group(1)]}, which the single header doesn't define: "
                    f"{line.strip()}")

    config = headers.get("trellis/config.hpp")
    version = dict(VERSION.findall(config.path.read_text(encoding="utf-8"))) if config else {}
    title = "Trellis"
    if len(version) == 3:
        title += f" {version['MAJOR']}.{version['MINOR']}.{version['PATCH']}"
    standard = sorted(set().union(*(header.standard for header in order)))
    out = [
        f"// {title}: header-only C++20 dependency injection, as a single header.",
        copyright_line,
        spdx_line,
        "//",
        f"// Generated by tools/amalgamate.py from the {len(order)} headers of include/trellis/,"
        " in the order",
        "// the preprocessor reads them; don't edit it. It provides what every one of those"
        " headers provides.",
        "// Include either this file or the headers in a translation unit, not both: their include"
        " guards",
        "// are gone here, so whichever comes second would declare everything again.",
        f"#ifndef {OUTER_GUARD}",
        f"#define {OUTER_GUARD}",
        "",
        "// The standard headers that the headers below include unconditionally, gathered here;"
        " the",
        "// comments that introduce them stay in the headers.",
        *(f"#include <{name}>" for name in standard),
    ]
    for header, continued, lines in pieces:
        # The headers' own sections use "// ----".
        rule = f"// ==== include/{header.name}{' (continued)' if continued else ''} "
        out += ["", rule + "=" * max(4, 100 - len(rule)), ""]
        out += lines
    out += ["", f"#endif  // {OUTER_GUARD}", ""]
    return "\n".join(out), len(order)


def find_compiler(cxx):
    """The compiler for --check: --cxx, else $CXX, else c++."""
    name = cxx or os.environ.get("CXX") or "c++"
    path = shutil.which(name)
    if path is None:
        raise AmalgamationError(f"no compiler {name!r} for the compile check; pass --cxx or set "
                                f"CXX")
    return path


def compile_check(text, compiler):
    """Compile TEXT on its own, as C++20, included twice, in a directory that holds nothing else."""
    with tempfile.TemporaryDirectory(prefix="trellis-amalgamate-") as directory:
        work = Path(directory)
        (work / "trellis.hpp").write_text(text, encoding="utf-8")
        (work / "check.cpp").write_text('#include "trellis.hpp"\n#include "trellis.hpp"  '
                                        "// the outer include guard\n", encoding="utf-8")
        command = [compiler, "-std=c++20", "-fsyntax-only", "-Wall", "-Wextra", "-Wundef",
                   "-Werror", "check.cpp"]
        try:
            result = subprocess.run(command, cwd=work, capture_output=True, text=True,
                                    timeout=COMPILE_TIMEOUT, check=False)
        except (OSError, subprocess.TimeoutExpired) as error:
            raise AmalgamationError(f"{compiler} failed to run: {error}") from error
        if result.returncode != 0:
            first = next((line for line in result.stderr.splitlines() if "error" in line),
                         result.stderr.strip()[:300])
            raise AmalgamationError(f"the single header doesn't compile on its own with "
                                    f"{compiler}: {first}")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--check", action="store_true",
                        help="generate in memory, compile, and compare with the artifact")
    parser.add_argument("--output", type=Path, help=f"where to write (default {DEFAULT_OUTPUT})")
    parser.add_argument("--cxx", help="the compiler for --check (default $CXX, else c++)")
    parser.add_argument("--root", type=Path, default=ROOT,
                        help="the tree to read (default: this repository)")
    args = parser.parse_args(argv)
    if args.check and args.output:
        parser.error("--check writes nothing; it compares with single_include/trellis.hpp")
    root = args.root.resolve()
    artifact = root / DEFAULT_OUTPUT
    try:
        text, count = amalgamate(root)
        lines = text.count("\n")
        if args.check:
            compiler = find_compiler(args.cxx)
            compile_check(text, compiler)
            if not artifact.is_file():
                state = f"{DEFAULT_OUTPUT.as_posix()} isn't generated, so nothing is stale"
            elif artifact.read_text(encoding="utf-8") != text:
                raise AmalgamationError(f"{DEFAULT_OUTPUT.as_posix()} is stale; regenerate it "
                                        f"with python3 tools/amalgamate.py")
            else:
                state = f"{DEFAULT_OUTPUT.as_posix()} is up to date"
            print(f"PASS amalgamate: the single header ({count} headers, {lines} lines) compiles "
                  f"on its own with {Path(compiler).name} (C++20); {state}")
            return 0
        output = (args.output or artifact).resolve()
        if output.is_file() and output.read_text(encoding="utf-8") == text:
            print(f"amalgamate: {output} is up to date ({count} headers, {lines} lines)")
            return 0
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(text, encoding="utf-8")
        print(f"amalgamate: wrote {output} ({count} headers, {lines} lines)")
        return 0
    except AmalgamationError as error:
        print(f"FAIL amalgamate: {error}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
