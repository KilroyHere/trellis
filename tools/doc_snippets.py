#!/usr/bin/env python3
"""Check that every C++ block of a document is an excerpt of a file that the build compiles (T1.10).

docs/getting-started.md shows code from the examples and the unit tests, which the build compiles
and runs. Each ```cpp block names the file it comes from in an HTML comment, which renders as
nothing, on the last non-blank line before the block's opening fence:

    <!-- snippet: examples/getting_started.cpp -->
    ```cpp
    ...
    ```

The path is relative to the repository root, and the file must be a C++ source under examples/ or
tests/unit/, whose programs the build compiles with the project's warnings as errors and runs as
tests. The block's lines must be lines of that file, in order and without gaps: an excerpt. Lines
are compared with their spaces collapsed, and blank lines don't count. A comment line of the file
may be left out of the block, so that a block can show code without the comments around it, but
every line it shows must be the file's. A block may leave code out in two ways:

  class C final : public I { /* ... */ };   stands for the file's class C, whatever its body
  // ...                                    stands for any lines the block leaves out there

So the code the document shows compiles, and a change to the file that the document doesn't
follow, or the other way round, fails the check. A C++ block without a snippet comment, a snippet
comment that names no such file or isn't followed by a C++ block, and a block that isn't an
excerpt each fail it.

Fenced code blocks are read as CommonMark reads them: a fence is three or more backticks or
tildes, the block's language is the first word of the info string after it, and the block ends at
a fence of the same character that is at least as long. So no C++ block escapes the check through
a fence that this tool would misread, every fence must be one it can classify: a C++ language
(cpp, c++, cxx, cc, hpp, hxx, h++, hh), which it checks, or another language it knows, such as
text, bash or cmake, which it doesn't. A fence without a language or with an unknown one, a fence
that nothing closes, a line that starts like a backtick fence but has a backtick in its info string
(which CommonMark doesn't take as a fence), and a fence inside a block quote each fail the check.
Container blocks aren't parsed otherwise: a fence may be indented, as inside a list item.
Indented code blocks aren't checked, so a document shows code in fences only.

CTest runs this as tools.doc_snippets, and tests/tools/doc_snippets_selftest.py tests it.

  tools/doc_snippets.py [--root DIR] [DOCUMENT...]   (default: docs/getting-started.md)

Exit status: 0 when every block is an excerpt of its file, 1 when one isn't, 2 when a document
can't be read, has a fence the tool can't classify, or a block that names no file or a file that
isn't a source the build compiles.
"""
import argparse
import re
import sys
from pathlib import Path

sys.dont_write_bytecode = True

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_DOCUMENTS = ["docs/getting-started.md"]
# The languages of the blocks that are checked, and of those that are known not to be C++. A fence
# with any other language, or with none, fails the check (module documentation).
CPP_LANGUAGES = {"cpp", "c++", "cxx", "cc", "hpp", "hxx", "h++", "hh"}
OTHER_LANGUAGES = {"bash", "sh", "shell", "zsh", "console", "powershell", "pwsh", "bat", "cmd",
                   "cmake", "make", "text", "txt", "plaintext", "json", "yaml", "yml", "toml",
                   "ini", "python", "py", "diff", "markdown", "md", "html", "xml", "mermaid"}
# Where a snippet may come from: the directories of the programs the build compiles and runs
# (examples/CMakeLists.txt, tests/unit/CMakeLists.txt). Never tests/compile_fail/, whose snippets
# don't compile.
SOURCE_DIRECTORIES = ("examples/", "tests/unit/")
SOURCE_SUFFIXES = {".cpp", ".hpp"}
# CommonMark's code fences: an opening fence and its info string, and a closing fence, which only
# spaces or tabs may follow. The indentation isn't limited, as container blocks aren't parsed.
OPENING_FENCE = re.compile(r"^[ \t]*(?P<fence>`{3,}|~{3,})(?P<info>.*)$")
CLOSING_FENCE = re.compile(r"^[ \t]*(?P<fence>`{3,}|~{3,})[ \t]*$")
QUOTED_FENCE = re.compile(r"^[ \t]*(?:>[ \t]*)+(?:`{3}|~{3})")
SNIPPET = re.compile(r"^\s*<!--\s*snippet:\s*(\S+)\s*-->\s*$")
ELIDED_BODY = "{ /* ... */ }"
ELIDED_LINES = "// ..."


def normalized(line):
    """LINE without leading and trailing spaces, and with each run of spaces collapsed."""
    return " ".join(line.split())


def closing_fence(lines, start, fence):
    """The index of the line that closes the block that FENCE opens on LINES[START], or None: a
    fence of the same character, at least as long."""
    for index in range(start + 1, len(lines)):
        closing = CLOSING_FENCE.match(lines[index])
        if (closing and closing.group("fence")[0] == fence[0]
                and len(closing.group("fence")) >= len(fence)):
            return index
    return None


def cpp_blocks(text):
    """The C++ blocks of a Markdown TEXT, as (line number of the fence, the snippet path or None,
    the block's non-blank lines, normalized), and its problems, as (line number, message): snippet
    comments that no C++ block follows, and fences the check can't classify."""
    blocks, problems = [], []
    lines = text.splitlines()
    pending = None  # (line number, path) of a snippet comment not yet followed by a block

    def drop_pending():
        nonlocal pending
        if pending:
            problems.append((pending[0], "a snippet comment that no C++ block follows"))
        pending = None

    index = 0
    while index < len(lines):
        line, number = lines[index], index + 1
        if QUOTED_FENCE.match(line):
            problems.append((number, "a fence inside a block quote, which the check doesn't read"))
            drop_pending()
            index += 1
            continue
        opening = OPENING_FENCE.match(line)
        if opening:
            fence, info = opening.group("fence"), opening.group("info").strip()
            if fence[0] == "`" and "`" in info:
                problems.append((number, "a line that starts like a fence, but whose info string "
                                         "holds a backtick, so CommonMark doesn't take it as one"))
                drop_pending()
                index += 1
                continue
            language = info.split()[0].lower() if info else ""
            end = closing_fence(lines, index, fence)
            if end is None:
                problems.append((number, f"a fence ({fence}) that no fence closes"))
                end = len(lines)
            if language in CPP_LANGUAGES:
                body = [normalized(text) for text in lines[index + 1:end] if text.strip()]
                blocks.append((number, pending[1] if pending else None, body))
                pending = None
            else:
                if language not in OTHER_LANGUAGES:
                    problems.append((number, f"a fence whose language the check can't classify "
                                             f"('{language}'): name one, such as cpp or text"
                                     if language else
                                     "a fence without a language: name one, such as cpp or text"))
                drop_pending()
            index = end + 1
            continue
        snippet = SNIPPET.match(line)
        if snippet:
            drop_pending()
            pending = (number, snippet.group(1))
        elif line.strip():
            drop_pending()
        index += 1
    drop_pending()
    return blocks, problems


def class_end(lines, start):
    """The index after the line that closes the brace opened on LINES[START], or None."""
    depth = 0
    for index in range(start, len(lines)):
        depth += lines[index].count("{") - lines[index].count("}")
        if depth <= 0:
            return index + 1
    return None


class Excerpt:
    """Whether a block is an excerpt of a file's lines, and how far the best attempt got."""

    def __init__(self, block, source):
        self.block = block
        self.source = source
        self.furthest = 0  # the most lines of the block that any attempt matched

    def matches_from(self, shown, at):
        """Whether block[SHOWN:] is the file from line AT on, comment lines aside."""
        block, source = self.block, self.source
        while shown < len(block):
            self.furthest = max(self.furthest, shown)
            line = block[shown]
            if line == ELIDED_LINES:
                return any(self.matches_from(shown + 1, start)
                           for start in range(at, len(source) + 1))
            # A comment line of the file that the block doesn't show is left out.
            while at < len(source) and source[at] != line and source[at].startswith("//"):
                at += 1
            if at >= len(source):
                return False
            if ELIDED_BODY in line:
                head = line.split(ELIDED_BODY)[0].rstrip()
                end = class_end(source, at) if source[at].startswith(head + " {") else None
                if end is None:
                    return False
                at = end
            elif source[at] != line:
                return False
            else:
                at += 1
            shown += 1
        self.furthest = len(block)
        return True

    def holds(self):
        return any(self.matches_from(0, start) for start in range(len(self.source)))


def check_document(document, root):
    """Check one document. Returns (status, messages, the number of blocks, the files named)."""
    try:
        text = document.read_text(encoding="utf-8")
    except OSError as error:
        return 2, [f"{document}: can't be read: {error.strerror}"], 0, set()
    blocks, problems = cpp_blocks(text)
    name = document.name
    messages, status, files = [], 0, set()
    for line_number, problem in problems:
        messages.append(f"{name}:{line_number}: {problem}")
        status = max(status, 2)
    sources = {}
    for number, (line_number, path, body) in enumerate(blocks, start=1):
        where = f"{name}:{line_number}: block {number}"
        if path is None:
            messages.append(f"{where} names no file: put <!-- snippet: <path> --> on the line "
                            f"before its fence")
            status = max(status, 2)
            continue
        source_path = (root / path).resolve()
        if (not path.startswith(SOURCE_DIRECTORIES) or source_path.suffix not in SOURCE_SUFFIXES
                or ".." in Path(path).parts):
            messages.append(f"{where} names {path}, which isn't a C++ source under "
                            f"{' or '.join(SOURCE_DIRECTORIES)}")
            status = max(status, 2)
            continue
        if path not in sources:
            try:
                sources[path] = [normalized(line) for line in
                                 source_path.read_text(encoding="utf-8").splitlines()
                                 if line.strip()]
            except OSError:
                sources[path] = None
        if sources[path] is None:
            messages.append(f"{where} names {path}, which doesn't exist")
            status = max(status, 2)
            continue
        files.add(path)
        if not body:
            messages.append(f"{where} is empty")
            status = max(status, 1)
            continue
        excerpt = Excerpt(body, sources[path])
        if not excerpt.holds():
            if excerpt.furthest >= len(body):
                detail = "every line matches, but not as one excerpt"
            else:
                detail = (f"no excerpt of {path} goes on with "
                          f"'{body[excerpt.furthest]}' (line {excerpt.furthest + 1} of the block)")
            messages.append(f"{where} isn't an excerpt of {path}: {detail}")
            status = max(status, 1)
    return status, messages, len(blocks), files


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--root", default=str(ROOT),
                        help="the directory that snippet paths are relative to (default: the "
                             "repository root)")
    parser.add_argument("documents", nargs="*", help="the documents to check (default: "
                        f"{', '.join(DEFAULT_DOCUMENTS)}, under --root)")
    args = parser.parse_args(argv)
    root = Path(args.root).resolve()
    documents = [Path(d) for d in args.documents] or [root / d for d in DEFAULT_DOCUMENTS]
    worst, all_messages, summaries = 0, [], []
    for document in documents:
        status, messages, count, files = check_document(document, root)
        worst = max(worst, status)
        all_messages += messages
        if status == 0:
            summaries.append(f"{document.name}'s {count} C++ blocks are excerpts of "
                             f"{', '.join(sorted(files)) or 'no file'}")
    if worst:
        print("FAIL doc-snippets:")
        print("\n".join(f"  {message}" for message in all_messages))
        return worst
    print(f"PASS doc-snippets: {'; '.join(summaries)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
