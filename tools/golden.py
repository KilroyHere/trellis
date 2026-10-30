#!/usr/bin/env python3
"""Golden diagnostics: a compile-fail snippet's normalized compiler output must match a snapshot.

  tools/golden.py --preset clang-18-debug [NAME ...]                            compare
  tools/golden.py --preset clang-18-debug --update --reason "<why>" [NAME ...]  rewrite
  tools/golden.py --check-manifest                                              check the manifest

NAME is a compile-fail test: its path under tests/compile_fail/ without .cpp, for example
sample/static_assert_message. Its golden is tests/diagnostics_golden/<compiler>-<major>/NAME.txt,
where <compiler>-<major> names the build's compiler (gcc-13, clang-18, ...). Compiler, flags and
environment are those of tools/compile_fail.py.

tests/golden_manifest.txt says which tests have goldens, for which compilers. Without NAME, every
test it lists for the build's compiler is used. CTest registers one golden test per listed test
and compiler, under the label golden, so a missing or deleted golden fails instead of going
untested. A NAME the manifest doesn't list for the build's compiler fails, and --update refuses
it: list it first. --check-manifest compiles nothing; it checks that every listed test exists and
has its goldens, and that every golden is listed. CTest runs it as tools.golden_manifest.

Builds configured with TRELLIS_GOLDEN_TESTS=OFF refuse to compare or update. The libc++ preset
does that, because libc++ spells standard-library names differently from the libstdc++ that the
clang-18 goldens were made with.

Normalization keeps what the compiler says and drops what depends on the machine or on unrelated
edits:
  - paths in the repository become repository-relative, paths in the build directory start with
    <build>/, and other absolute paths become <external>/<file name>;
  - line and column numbers go, including those in the margin of quoted source lines;
  - colors, compiler banners and trailing whitespace go, and typographic quotes become plain ones.

--update writes the goldens (new ones only for the NAMEs given) and appends the reason to
docs/golden-changes.md. Golden changes need the owner's approval; see the update-golden skill.
Every run saves the normalized output to <build>/golden/NAME.txt.

Exit status: 0 when every golden matched or was written, 1 when one differs or the snippet
compiled, 2 for a usage or configuration error.
"""
import argparse
import datetime
import difflib
import re
import sys
from pathlib import Path

sys.dont_write_bytecode = True  # keep tools/ free of __pycache__
sys.path.insert(0, str(Path(__file__).resolve().parent))
import compile_fail  # noqa: E402  (shares the configuration and the compiler invocation)

ROOT = compile_fail.ROOT
GOLDEN_ROOT = ROOT / "tests" / "diagnostics_golden"
MANIFEST = ROOT / "tests" / "golden_manifest.txt"
CHANGES_FILE = ROOT / "docs" / "golden-changes.md"
MAX_DIFF_LINES = 40
# A manifest line: a test name, then one or more compiler names such as clang-18.
MANIFEST_LINE = re.compile(r"^(?P<name>[\w./-]+)(?P<compilers>(?:\s+[a-z][a-z-]*-\d+)+)$")

ANSI_ESCAPE = compile_fail.ANSI_ESCAPE
BANNER = re.compile(
    r"^(?:Microsoft \(R\) C/C\+\+ Optimizing Compiler Version .*"
    r"|Copyright \(C\) Microsoft Corporation\..*"
    r"|(?:\S+ )*clang version \d.*|Target: .*|Thread model: .*|InstalledDir: .*"
    r"|gcc version \d.*|Using built-in specs\.|COLLECT_GCC=.*|COLLECT_LTO_WRAPPER=.*"
    r"|Configured with: .*)$")
# Absolute paths outside the repository and the build directory, POSIX and Windows style. The
# last group is the file name, which is kept. (A "/" right after "<build>" starts no path.)
POSIX_PATH = re.compile(r"(?<![\w.~/:>-])/(?:[^\s/:'\"(),<>\[\]]+/)+([^\s/:'\"(),<>\[\]]+)")
WINDOWS_PATH = re.compile(
    r"(?<![\w])[A-Za-z]:[\\/](?:[^\s\\/:'\"(),<>]+[\\/])*([^\s\\/:'\"(),<>]+)")
# ":12:5" or ":12" after a file name: "file:12:5: error", "(lambda at file:12:5)",
# "In file included from file:12,".
LINE_COLUMN = re.compile(r"(?<=[^\s:])(?::\d+){1,2}(?=[:,)\s]|$)")
# MSVC locations at the start of a line, "file(12,5): error" and "file(12): note". The file may
# contain spaces and parentheses ("C:\Program Files (x86)\...").
MSVC_LOCATION = re.compile(r"^(?P<file>.+?)\(\d+(?:,\d+)?\)(?=\s?: )")
# The margin of a quoted source line: "   12 |   code" and "      |   ^~~~" both become " |".
EXCERPT_MARGIN = re.compile(r"^ *\d* \|")
TYPOGRAPHIC_QUOTES = str.maketrans({"‘": "'", "’": "'", "“": '"', "”": '"'})


def _spellings(directory):
    """How a directory can appear in compiler output: as given and resolved, with either slash."""
    spellings = set()
    for path in {str(directory), str(Path(directory).resolve())}:
        path = path.rstrip("/\\")
        spellings |= {path, path.replace("\\", "/"), path.replace("/", "\\")}
    return sorted(spellings, key=len, reverse=True)


def _replace_prefix(text, directory, replacement):
    for spelling in _spellings(directory):
        windows = re.match(r"^[A-Za-z]:", spelling) is not None
        pattern = re.escape(spelling) + r"[\\/]"
        text = re.sub(pattern, lambda _: replacement, text, flags=re.IGNORECASE if windows else 0)
    return text


def _msvc_location(match):
    """An MSVC location without its (line,column); an absolute file becomes <external>/name."""
    file = match.group("file")
    if re.match(r"^[A-Za-z]:[\\/]", file):
        file = "<external>/" + re.split(r"[\\/]", file)[-1]
    return file


def normalize(output, source_dir, binary_dir=None):
    """Compiler output in the machine-independent form that goldens store."""
    text = ANSI_ESCAPE.sub("", output).replace("\r\n", "\n").translate(TYPOGRAPHIC_QUOTES)
    if binary_dir:
        text = _replace_prefix(text, binary_dir, "<build>/")
    text = _replace_prefix(text, source_dir, "")
    lines = []
    for line in text.split("\n"):
        if BANNER.match(line):
            continue
        margin = EXCERPT_MARGIN.match(line)
        if margin:  # quoted source: keep the code as it is, drop the line number
            line = " |" + line[margin.end():]
        else:
            line = MSVC_LOCATION.sub(_msvc_location, line)
            line = POSIX_PATH.sub(r"<external>/\1", line)
            line = WINDOWS_PATH.sub(r"<external>/\1", line)
            line = LINE_COLUMN.sub("", line)
        lines.append(line.rstrip())
    while lines and not lines[0]:
        lines.pop(0)
    while lines and not lines[-1]:
        lines.pop()
    return "\n".join(lines) + "\n" if lines else ""


def read_manifest(path):
    """{test name: [compiler, ...]} from the manifest, and a list of problems with its lines."""
    entries, problems = {}, []
    try:
        text = Path(path).read_text(encoding="utf-8")
    except OSError as error:
        return entries, [f"cannot read {compile_fail.display(path)}: {error}"]
    for number, raw in enumerate(text.splitlines(), start=1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        match = MANIFEST_LINE.match(line)
        where = f"{compile_fail.display(path)}:{number}"
        if not match:
            problems.append(f"{where}: expected '<test> <compiler>-<major> ...', got '{line}'")
        elif match.group("name") in entries:
            problems.append(f"{where}: {match.group('name')} is listed twice")
        else:
            entries[match.group("name")] = match.group("compilers").split()
    return entries, problems


def check_manifest(entries, golden_root):
    """Problems that leave a golden untested: a listed test or golden that doesn't exist, and a
    golden that no manifest line lists."""
    show = compile_fail.display
    problems = []
    for name, compilers in entries.items():
        if not (compile_fail.TESTS_DIR / f"{name}.cpp").is_file():
            problems.append(f"{name}: listed, but there is no tests/compile_fail/{name}.cpp")
        for compiler in compilers:
            if not (golden_root / compiler / f"{name}.txt").is_file():
                problems.append(f"{name}: listed for {compiler}, but "
                                f"{show(golden_root / compiler / name)}.txt is missing")
    for golden in sorted(golden_root.rglob("*.txt")):
        relative = golden.relative_to(golden_root)
        compiler, name = relative.parts[0], Path(*relative.parts[1:]).with_suffix("").as_posix()
        if compiler not in entries.get(name, []):
            problems.append(f"{show(golden)}: no manifest line lists {name} for {compiler}")
    return problems


def snippet_names(args, entries, compiler):
    """The NAMEs to work on: the ones given (as names or .cpp paths), else every test the
    manifest lists for this compiler."""
    if not args.names:
        return [name for name, compilers in entries.items() if compiler in compilers]
    names = []
    for name in args.names:
        if name.endswith(".cpp"):
            name = compile_fail.snippet_name(name)
        names.append(name)
    return names


def append_change(changes_file, config, reason, changed):
    if not changes_file.exists():
        changes_file.parent.mkdir(parents=True, exist_ok=True)
        changes_file.write_text(
            "# Golden diagnostics changes\n\n"
            "`tools/golden.py --update --reason \"...\"` adds an entry here whenever it writes "
            "golden files under `tests/diagnostics_golden/`. Golden changes need the owner's "
            "approval.\n", encoding="utf-8")
    date = datetime.date.today().isoformat()
    entry = (f"\n## {date}: {config['golden_name']} ({config.get('compiler_version', '?')})\n\n"
             f"{reason}\n\n" + "".join(f"- {state}: {name}\n" for name, state in changed))
    with changes_file.open("a", encoding="utf-8") as stream:
        stream.write(entry)


def run_one(config, name, golden_dir, args, changed, listed):
    """Compare (or with --update, write) one golden. Returns True unless it failed."""
    show = compile_fail.display
    compiler = config["golden_name"]
    result = compile_fail.compile_source(config, compile_fail.TESTS_DIR / f"{name}.cpp")
    problem = compile_fail.compile_problem(result)
    if problem:
        print(f"FAIL golden {name} ({compiler}): {problem}")
        return False
    actual = normalize(result.output, config.get("source_dir", ROOT), config["binary_dir"])
    saved = Path(config["binary_dir"]) / "golden" / f"{name}.txt"
    saved.parent.mkdir(parents=True, exist_ok=True)
    saved.write_text(actual, encoding="utf-8")
    if not listed:
        print(f"FAIL golden {name} ({compiler}): the manifest doesn't list it for {compiler}; "
              f"its normalized output is in {show(saved)}")
        return False
    golden = golden_dir / f"{name}.txt"
    expected = golden.read_text(encoding="utf-8") if golden.is_file() else None
    if args.update:
        if expected != actual:
            golden.parent.mkdir(parents=True, exist_ok=True)
            golden.write_text(actual, encoding="utf-8")
            changed.append((name, "added" if expected is None else "updated"))
        print(f"{'unchanged' if expected == actual else 'wrote'} {show(golden)}")
        return True
    if expected is None:
        print(f"FAIL golden {name} ({compiler}): no golden {show(golden)}; "
              f"create it with --update --reason, at the owner's request")
        return False
    if expected != actual:
        print(f"FAIL golden {name} ({compiler}): the output differs from {show(golden)}")
        diff = list(difflib.unified_diff(expected.splitlines(), actual.splitlines(),
                                         "golden", "actual", lineterm="", n=1))
        for line in diff[:MAX_DIFF_LINES]:
            print(f"  {line}")
        if len(diff) > MAX_DIFF_LINES:
            print(f"  ... {len(diff) - MAX_DIFF_LINES} more diff lines")
        print(f"  normalized output: {show(saved)}")
        return False
    print(f"PASS golden {name} ({compiler})")
    return True


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    where = parser.add_mutually_exclusive_group()
    where.add_argument("--preset", help="use build/<preset>/" + compile_fail.CONFIG_NAME)
    where.add_argument("--config", help="path to a " + compile_fail.CONFIG_NAME)
    where.add_argument("--check-manifest", action="store_true",
                       help="check the manifest against the tests and goldens; compiles nothing")
    parser.add_argument("--update", action="store_true", help="rewrite the goldens")
    parser.add_argument("--reason", help="why the goldens change (required with --update)")
    # For the self-test, which must not write into tests/ or docs/.
    parser.add_argument("--golden-root", type=Path, default=GOLDEN_ROOT, help=argparse.SUPPRESS)
    parser.add_argument("--manifest", type=Path, default=MANIFEST, help=argparse.SUPPRESS)
    parser.add_argument("--changes-file", type=Path, default=CHANGES_FILE, help=argparse.SUPPRESS)
    parser.add_argument("names", nargs="*",
                        help="compile-fail tests, such as sample/static_assert_message")
    args = parser.parse_args(argv)
    show = compile_fail.display
    if not (args.preset or args.config or args.check_manifest):
        parser.error("one of --preset, --config or --check-manifest is required")
    if args.update and not (args.reason or "").strip():
        print("error: --update needs --reason \"<why the diagnostics changed>\"", file=sys.stderr)
        return 2
    entries, problems = read_manifest(args.manifest)
    if problems:
        for problem in problems:
            print(f"error: {problem}", file=sys.stderr)
        return 2
    if args.check_manifest:
        problems = check_manifest(entries, args.golden_root)
        for problem in problems:
            print(f"FAIL golden manifest: {problem}")
        if problems:
            return 1
        print(f"PASS golden manifest: {len(entries)} tests, "
              f"{sum(len(c) for c in entries.values())} goldens, all present and listed")
        return 0

    try:
        config = compile_fail.load_config(compile_fail.config_path(args))
    except compile_fail.ConfigError as error:
        print(f"error: {error}", file=sys.stderr)
        return 2
    compiler = config["golden_name"]
    if not config.get("golden_tests", True):
        print(f"error: golden tests are off in this build (TRELLIS_GOLDEN_TESTS=OFF), so its "
              f"output must not be compared with or written to the {compiler} goldens",
              file=sys.stderr)
        return 2
    golden_dir = args.golden_root / compiler
    names = snippet_names(args, entries, compiler)
    if not names:
        print(f"no goldens for {compiler}: {show(args.manifest)} lists no test for it")
        return 0
    missing = [n for n in names if not (compile_fail.TESTS_DIR / f"{n}.cpp").is_file()]
    if missing:
        print(f"error: no compile-fail test for {', '.join(missing)} in "
              f"{show(compile_fail.TESTS_DIR)}", file=sys.stderr)
        return 2
    unlisted = [n for n in names if compiler not in entries.get(n, [])]
    if args.update and unlisted:
        print(f"error: {show(args.manifest)} doesn't list {', '.join(unlisted)} for {compiler}; "
              f"add a line for it first", file=sys.stderr)
        return 2

    changed = []
    passed = sum(run_one(config, name, golden_dir, args, changed, name not in unlisted)
                 for name in names)
    if changed:
        append_change(args.changes_file, config, args.reason.strip(), changed)
        print(f"recorded the reason in {compile_fail.display(args.changes_file)}")
    return 0 if passed == len(names) else 1


if __name__ == "__main__":
    sys.exit(main())
