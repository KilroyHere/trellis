#!/usr/bin/env python3
"""Check that the code README.md shows is examples/hello_world.cpp's, both ways (T1.5).

README.md's section "What it looks like" shows plan §6.1's hello world, which
examples/hello_world.cpp builds and runs as the test example.hello_world. The example marks the
parts the README shows, each between the comment lines "// README.md: begin" and
"// README.md: end". The section's C++ blocks show those parts, in order, one block per
part, line for line: every line of a block is the next line of its part, and every line of the
part is shown, so a line added to or deleted from either file fails the check. Lines are compared
with their spaces collapsed; blank lines, and comment lines of the example, don't count. A block
may leave code out in two ways:

  class C final : public I { /* ... */ };   stands for the example's class C, whatever its body
  // ...                                    stands for a private: section, up to the line the
                                            block shows next

CTest runs this as tools.readme_example, and tests/tools/readme_example_selftest.py tests it.

  tools/readme_example.py [--readme README.md] [--example examples/hello_world.cpp]

Exit status: 0 when the blocks show the parts, 1 when they don't, 2 when the section, its blocks or
the example's parts are missing.
"""
import argparse
import re
import sys
from pathlib import Path

sys.dont_write_bytecode = True

ROOT = Path(__file__).resolve().parent.parent
SECTION = "## What it looks like"
BEGIN = "README.md: begin"
END = "README.md: end"
ELIDED_BODY = "{ /* ... */ }"
ELIDED_LINES = "// ..."
PRIVATE = "private:"
FENCE = re.compile(r"^```(\w*)\s*$")


def normalized(line):
    """LINE without leading and trailing spaces, and with each run of spaces collapsed."""
    return " ".join(line.split())


def section_lines(readme_text):
    """The lines of the README section, from its heading to the next heading of the same level."""
    lines = readme_text.splitlines()
    try:
        start = lines.index(SECTION)
    except ValueError:
        return None
    end = next((i for i in range(start + 1, len(lines)) if lines[i].startswith("## ")), len(lines))
    return lines[start + 1:end]


def cpp_blocks(lines):
    """The ```cpp blocks among LINES, each as its non-blank lines, normalized."""
    blocks, block, language = [], None, None
    for line in lines:
        fence = FENCE.match(line)
        if fence:
            if language is None:
                language = fence.group(1)
                block = []
            else:
                if language == "cpp":
                    blocks.append(block)
                language, block = None, None
            continue
        if language == "cpp" and line.strip():
            block.append(normalized(line))
    return blocks


def example_parts(example_text):
    """The example's marked parts, each as its lines that are neither blank nor comments,
    normalized; or an error message."""
    parts, part = [], None
    for number, line in enumerate(example_text.splitlines(), start=1):
        text = normalized(line)
        if text == f"// {BEGIN}":
            if part is not None:
                return None, f"line {number}: '{BEGIN}' inside a part"
            part = []
        elif text == f"// {END}":
            if part is None:
                return None, f"line {number}: '{END}' outside a part"
            parts.append(part)
            part = None
        elif part is not None and text and not text.startswith("//"):
            part.append(text)
    if part is not None:
        return None, f"a part has no '{END}' line"
    return parts, None


def class_end(part, start):
    """The index after the line that closes the brace opened on PART[START], which has one."""
    depth = 0
    for index in range(start, len(part)):
        depth += part[index].count("{") - part[index].count("}")
        if depth <= 0:
            return index + 1
    return len(part)


def compare(block, part):
    """None when BLOCK shows PART line for line, with its elisions; otherwise what differs."""
    position = 0
    for index, shown in enumerate(block):
        if shown == ELIDED_LINES:
            following = block[index + 1] if index + 1 < len(block) else None
            if following is None:
                return "'// ...' ends a block, so it would stand for the rest of the part"
            if position >= len(part) or part[position] != PRIVATE:
                shown_next = part[position] if position < len(part) else "the end of the part"
                return f"'// ...' stands for a '{PRIVATE}' section, where the example has " \
                       f"'{shown_next}'"
            while position < len(part) and part[position] != following:
                position += 1
            continue
        if position >= len(part):
            return f"README.md shows '{shown}', after the end of the example's part"
        if ELIDED_BODY in shown:
            head = shown.split(ELIDED_BODY)[0].rstrip()
            if not part[position].startswith(head + " {"):
                return (f"README.md shows '{shown}', where the example has "
                        f"'{part[position]}'")
            position = class_end(part, position)
            continue
        if part[position] != shown:
            return f"README.md shows '{shown}', where the example has '{part[position]}'"
        position += 1
    if position < len(part):
        return f"the example's part goes on with '{part[position]}', which README.md doesn't show"
    return None


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--readme", default=str(ROOT / "README.md"))
    parser.add_argument("--example", default=str(ROOT / "examples" / "hello_world.cpp"))
    args = parser.parse_args(argv)
    example_name = Path(args.example).name

    section = section_lines(Path(args.readme).read_text(encoding="utf-8"))
    if section is None:
        print(f"FAIL readme-example: {args.readme} has no section '{SECTION}'")
        return 2
    blocks = cpp_blocks(section)
    if not blocks:
        print(f"FAIL readme-example: the section '{SECTION}' has no C++ block")
        return 2
    parts, error = example_parts(Path(args.example).read_text(encoding="utf-8"))
    if error:
        print(f"FAIL readme-example: {example_name}: {error}")
        return 2
    if not parts:
        print(f"FAIL readme-example: {example_name} marks no part with '{BEGIN}'")
        return 2
    if len(blocks) != len(parts):
        print(f"FAIL readme-example: README.md has {len(blocks)} C++ blocks in '{SECTION}', and "
              f"{example_name} marks {len(parts)} parts")
        return 1
    failures = []
    for number, (block, part) in enumerate(zip(blocks, parts), start=1):
        difference = compare(block, part)
        if difference:
            failures.append(f"  block {number}: {difference}")
    if failures:
        print(f"FAIL readme-example: README.md's blocks don't show {example_name}'s parts:")
        print("\n".join(failures))
        return 1
    print(f"PASS readme-example: README.md's {len(blocks)} C++ blocks show {example_name}'s "
          f"{len(parts)} parts, line for line")
    return 0


if __name__ == "__main__":
    sys.exit(main())
