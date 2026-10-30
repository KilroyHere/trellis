#!/usr/bin/env python3
"""Compile-fail tests: each snippet must fail to compile, for the reason its directives state.

  tools/compile_fail.py --preset clang-18-debug [FILE ...]
  tools/compile_fail.py --config build/<preset>/trellis_compiler.json [FILE ...]

Without FILE, runs every tests/compile_fail/**/*.cpp. CTest runs one file per test, under the label
compile_fail (`ctest --preset clang-18-debug -L compile_fail`).

The compiler and flags are the build's, from trellis_compiler.json, which CMake writes at configure
time (cmake/TrellisTesting.cmake): the preset's compiler, CMAKE_CXX_FLAGS and build-type flags,
-std=c++20, the include directory and the project warnings. -Werror is left out, and -Wno-error is
added, because misuse must be a hard error even without it (plan NFR-07). Colors are turned off;
otherwise the output is what users see. The compiler is called directly, never through ccache, with
LC_ALL=C and a clean environment (spike S3): it keeps only what a compiler needs to start (PATH,
the temporary directory, and what Windows, MSVC and Xcode need), so that nothing inherited, such
as CPATH, CPLUS_INCLUDE_PATH, GCC_COLORS, GCC_URLS or MSVC's CL, changes what it includes or how
it reports. tools/golden.py compiles the same way, with plainer output (see compile_source).

Directives, in the comment block at the top of the file:

  // compile-fail: <summary>      required: the misuse the snippet shows, in one line
  // expect-id: TRL-E001          the first error line must contain this ID, as a whole token
  // expect-regex: <regex>        repeatable: each Python regex must match the diagnostics
  // expect-max-lines: <n>        optional, default 60: the output may have at most n lines

A test needs an expect-id or at least one expect-regex; otherwise any error, even a missing
header, would pass it. A comment anywhere in the file that looks like a directive (it starts with
"compile" or "expect", in any case, with "-" or "_", and has a colon) must be a known directive in
the top comment block: a misspelt or misplaced one is an error, never silently ignored.

A test passes only when the compiler reports an error, without crashing, running out of memory,
dying by a signal or timing out, and every expectation holds. Regexes are searched (re.MULTILINE)
in the diagnostics only: the output with colors removed and without the quoted source and caret
lines, so a word that appears only in the snippet's own code doesn't match. The line budget counts
every line, as users see them. On timeout the compiler's whole process group is killed, so that
cc1plus doesn't run on. Each test's full compiler output goes to <build>/compile_fail/<name>.log.

Exit status: 0 when every test passed, 1 when one failed, 2 for a usage or configuration error.
"""
import argparse
import json
import os
import re
import signal
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TESTS_DIR = ROOT / "tests" / "compile_fail"
CONFIG_NAME = "trellis_compiler.json"
DEFAULT_MAX_LINES = 60
DEFAULT_TIMEOUT = 120  # seconds per compile
DEFAULT_MEMORY_MB = 6000  # address-space cap per compile (POSIX only), as CLAUDE.md asks

# Anything that looks like a directive, anywhere on a line: "//", then a word starting with
# "compile" or "expect" in any case and spelling ("expect_regex", "Expect-ID"), then a colon.
DIRECTIVE_LIKE = re.compile(r"//\s*((?:compile|expect)[\w-]*)\s*:(.*)$", re.IGNORECASE)
KNOWN_DIRECTIVES = ("compile-fail", "expect-id", "expect-regex", "expect-max-lines")
ERROR_ID = re.compile(r"^TRL-[ER]\d{3}$")
# Terminal escapes: colors and other CSI sequences (ESC [ params final, as in GCC's ESC[01m ESC[K),
# and OSC-8 hyperlinks (ESC ] 8 ; params ; URL, then BEL, GCC's default, or ESC \), which GCC puts
# around warning options when URLs are on. The link text stays.
ANSI_ESCAPE = re.compile(
    r"\x1b\[[0-?]*[ -/]*[@-~]|\x1b\]8;[^;\x07\x1b]*;[^\x07\x1b]*(?:\x07|\x1b\\)")
# The only environment variables a compiler inherits. PATH: the driver's helper programs. TMPDIR,
# TMP and TEMP: scratch files. SYSTEMROOT, SYSTEMDRIVE and WINDIR: Windows programs need them to
# start. INCLUDE, LIB and LIBPATH: where MSVC finds the standard library and the Windows SDK, as the
# developer prompt (vcvars64.bat) sets them. DEVELOPER_DIR: on macOS, the Xcode whose clang
# /usr/bin/clang++ runs. Everything else is dropped.
KEPT_VARIABLES = ("PATH", "TMPDIR", "TMP", "TEMP", "SYSTEMROOT", "SYSTEMDRIVE", "WINDIR",
                  "INCLUDE", "LIB", "LIBPATH", "DEVELOPER_DIR")
# Golden runs ask for the plainest output (spike S3). GCC's -fdiagnostics-plain-output (GCC 11 or
# later) turns off colors, URLs, quoted source, carets and line-number margins. Clang 18 and 20
# reject it as an unknown argument, so colors and carets go separately there.
PLAIN_OUTPUT_FLAGS = {
    "GNU": ["-fdiagnostics-plain-output"],
    "Clang": ["-fno-color-diagnostics", "-fno-caret-diagnostics"],
    "AppleClang": ["-fno-color-diagnostics", "-fno-caret-diagnostics"],
}
# A line reporting an error: "file:1:2: error: ...", "error: ..." or MSVC's
# "file(1): error C2338: ...". "trellis::error::E001" in a type name doesn't count.
ERROR_LINE = re.compile(r"(?:^|: )(?:fatal )?error(?: [A-Z]+\d+)?: ")
# Quoted source with its margin ("   12 |   code" or "      |   ^~~"): never an error line, and
# not searched by expect-regex.
SOURCE_EXCERPT = re.compile(r"^\s*\d*\s*\|")
# A compiler that crashed or ran out of memory, possibly after printing the expected error: an
# internal compiler error; Clang's crash report; a killed or out-of-memory cc1plus under the GCC
# driver; LLVM's fatal errors; MSVC's internal error and heap exhaustion.
COMPILER_CRASH = re.compile(
    r"internal compiler error|PLEASE submit a bug report|clang frontend command failed|"
    r"unable to execute command|LLVM ERROR|Killed signal terminated program|"
    r"out of memory allocating|virtual memory exhausted|terminate called after throwing|"
    r"fatal error C1001|out of heap space", re.IGNORECASE)


class ConfigError(Exception):
    """The compiler configuration is missing or unreadable."""


def load_config(path):
    path = Path(path)
    if not path.is_file():
        raise ConfigError(f"{path} not found; configure the build first (cmake --preset <name>)")
    try:
        config = json.loads(path.read_text(encoding="utf-8"))
    except ValueError as error:
        raise ConfigError(f"{path} is not valid JSON: {error}") from error
    missing = [key for key in ("compiler", "frontend", "golden_name", "flags", "binary_dir")
               if key not in config]
    if missing:
        raise ConfigError(f"{path} lacks {', '.join(missing)}; reconfigure the build")
    return config


def config_path(args):
    """The --config file, or the one of --preset's build directory."""
    if args.config:
        return Path(args.config)
    return ROOT / "build" / args.preset / CONFIG_NAME


def display(path):
    """A path for messages: repository-relative when it is inside the repository."""
    path = Path(path)
    try:
        return path.resolve().relative_to(ROOT).as_posix()
    except ValueError:
        return str(path)


def snippet_name(source):
    """A relative name for messages and log files: tests/compile_fail/sample/x.cpp -> sample/x,
    another file in the repository -> its repository path without .cpp, anything else -> x."""
    source = Path(source).resolve()
    for base in (TESTS_DIR, ROOT):
        try:
            return source.relative_to(base).with_suffix("").as_posix()
        except ValueError:
            continue
    return source.stem


class Spec:
    """The directives of one snippet, plus any problems with them."""

    def __init__(self):
        self.summary = None
        self.expect_id = None
        self.regexes = []
        self.max_lines = DEFAULT_MAX_LINES
        self.problems = []


def parse_directives(text):
    """Read the directives. They belong in the comment block at the top of the file, which ends at
    the first line of code; anything directive-like elsewhere, or misspelt, is a problem."""
    spec = Spec()
    seen = set()
    in_header = True
    for number, raw in enumerate(text.splitlines(), start=1):
        line = raw.strip()
        if line and not line.startswith("//"):
            in_header = False
        match = DIRECTIVE_LIKE.search(raw)
        if not match:
            continue
        name, value = match.group(1), match.group(2).strip()
        if name not in KNOWN_DIRECTIVES:
            spec.problems.append(f"line {number}: unknown directive '{name}' "
                                 f"(known: {', '.join(KNOWN_DIRECTIVES)})")
            continue
        if not in_header or not line.startswith("//"):
            spec.problems.append(f"line {number}: '{name}' must be in the comment block at the "
                                 f"top of the file, before the first line of code")
            continue
        if name in seen and name != "expect-regex":
            spec.problems.append(f"line {number}: '{name}' appears more than once")
            continue
        seen.add(name)
        if not value:
            spec.problems.append(f"line {number}: '{name}' needs a value")
        elif name == "compile-fail":
            spec.summary = value
        elif name == "expect-id":
            if ERROR_ID.match(value):
                spec.expect_id = value
            else:
                spec.problems.append(f"line {number}: '{value}' is not an error ID like TRL-E001")
        elif name == "expect-regex":
            try:
                spec.regexes.append(re.compile(value, re.MULTILINE))
            except re.error as error:
                spec.problems.append(f"line {number}: invalid regex '{value}': {error}")
        elif name == "expect-max-lines":
            if value.isdigit() and int(value) > 0:
                spec.max_lines = int(value)
            else:
                spec.problems.append(f"line {number}: expect-max-lines needs a positive number")
    if spec.summary is None:
        spec.problems.append("missing the '// compile-fail: <summary>' directive")
    if spec.expect_id is None and not spec.regexes:
        spec.problems.append("a compile-fail test needs an expect-id or an expect-regex; "
                             "otherwise any error, even a missing header, would pass it")
    return spec


class Result:
    """What one compile produced. output is decoded, without terminal escapes (colors, hyperlinks)
    and with \\n line ends."""

    def __init__(self, command, returncode=None, output="", timed_out=False, launch_error=None):
        self.command = command
        self.returncode = returncode
        self.output = output
        self.timed_out = timed_out
        self.launch_error = launch_error

    @property
    def lines(self):
        return self.output.splitlines()


def _decode(data):
    text = (data or b"").decode("utf-8", errors="replace")
    return ANSI_ESCAPE.sub("", text).replace("\r\n", "\n")


def _memory_limiter(megabytes):
    """A preexec_fn that caps the child's address space, or None where that isn't possible."""
    if os.name != "posix" or not megabytes:
        return None
    import resource  # POSIX only

    def limit():
        cap = megabytes * 1024 * 1024
        try:
            resource.setrlimit(resource.RLIMIT_AS, (cap, cap))
        except (ValueError, OSError):
            pass  # some systems (macOS) don't support the limit; compile without it

    return limit


def compiler_environment(config):
    """The clean environment a compile runs in: KEPT_VARIABLES, and English messages with plain
    quotes. GCC prints typographic quotes in UTF-8 locales and translates its messages in others;
    LC_ALL=C stops both. VSLANG=1033 is MSVC's counterpart."""
    env = {name: os.environ[name] for name in KEPT_VARIABLES if name in os.environ}
    env.update(LC_ALL="C", LANG="C")
    if config["frontend"] == "MSVC":
        env["VSLANG"] = "1033"
    return env


def compile_command(config, source, plain=False):
    """The syntax-only compile of SOURCE with the build's compiler and flags. Colors are off, but
    the output is otherwise what users see, as the line budget counts it. plain, for golden runs,
    asks for PLAIN_OUTPUT_FLAGS instead: no quoted source or carets."""
    if config["frontend"] == "MSVC":
        return [config["compiler"], "/nologo", *config["flags"], "/WX-", "/Zs", str(source)]
    diagnostics = ["-fdiagnostics-color=never"]
    if plain:
        diagnostics = PLAIN_OUTPUT_FLAGS.get(config.get("compiler_id"), diagnostics)
    return [config["compiler"], *config["flags"], "-Wno-error", *diagnostics, "-fsyntax-only",
            str(source)]


def compile_source(config, source, timeout=DEFAULT_TIMEOUT, memory_mb=DEFAULT_MEMORY_MB,
                   plain=False):
    """Compile one file syntax-only with the build's compiler and flags, in the clean environment.
    plain: the plainest output the compiler offers, for golden runs (see compile_command)."""
    source = Path(source).resolve()  # the compiler runs in a scratch directory
    command = compile_command(config, source, plain)
    # Run in a scratch directory, so that stray outputs (such as -ftime-trace files) stay there.
    work_dir = Path(config["binary_dir"]) / "compile_fail" / "work"
    work_dir.mkdir(parents=True, exist_ok=True)
    env = compiler_environment(config)
    # A new session makes the compiler the leader of its own process group, so that a timeout can
    # kill the driver together with the cc1plus or clang -cc1 it started (POSIX only).
    session = {"start_new_session": True} if os.name == "posix" else {}
    try:
        proc = subprocess.Popen(command, cwd=work_dir, env=env, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, preexec_fn=_memory_limiter(memory_mb),
                                **session)
    except OSError as error:
        return Result(command, launch_error=str(error))
    try:
        output, _ = proc.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        _kill_process_group(proc)
        output, _ = proc.communicate()  # returns once every process holding the pipe is gone
        return Result(command, output=_decode(output), timed_out=True)
    except BaseException:
        # The runner itself is stopping: Ctrl-C (KeyboardInterrupt), or SIGTERM or SIGHUP, which
        # exit_on_termination_signals() turns into SystemExit. Those signals don't reach the
        # compiler's own process group, so take it down too, or g++ and cc1plus run on.
        _kill_process_group(proc)
        try:
            proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            pass
        raise
    return Result(command, returncode=proc.returncode, output=_decode(output))


def exit_on_termination_signals():
    """Make SIGTERM and SIGHUP raise SystemExit, as SIGINT raises KeyboardInterrupt, so that
    compile_source() can stop the compiler before the runner exits. Exit status: 128 + signal."""

    def stop(signum, _frame):
        raise SystemExit(128 + signum)

    for name in ("SIGTERM", "SIGHUP"):
        if hasattr(signal, name):
            signal.signal(getattr(signal, name), stop)


def _kill_process_group(proc):
    """Kill a timed-out compiler and every process it started."""
    if os.name == "posix":
        try:
            os.killpg(proc.pid, signal.SIGKILL)
        except (ProcessLookupError, PermissionError):
            pass  # already gone
    else:
        proc.kill()


def diagnostic_lines(lines):
    """The compiler's messages, without the quoted source and caret lines."""
    return [line for line in lines if not SOURCE_EXCERPT.match(line)]


def error_lines(lines):
    return [line for line in diagnostic_lines(lines) if ERROR_LINE.search(line)]


def contains_id(line, error_id):
    """True if LINE has ERROR_ID as a whole token: TRL-E001 is not in TRL-E0012."""
    return re.search(rf"(?<![\w-]){re.escape(error_id)}(?![\w-])", line) is not None


def compile_problem(result, timeout=DEFAULT_TIMEOUT):
    """Why this compile is not a proper compile error, or None if it is one."""
    if result.launch_error:
        return f"could not run the compiler: {result.launch_error}"
    if result.timed_out:
        return f"the compiler did not finish within {timeout} s"
    if result.returncode < 0:
        return f"the compiler was killed by signal {-result.returncode}"
    if os.name == "posix" and 128 < result.returncode <= 192:
        return (f"the compiler exited with status {result.returncode}, as a process killed by "
                f"signal {result.returncode - 128} does")
    crash = next((line for line in diagnostic_lines(result.lines) if COMPILER_CRASH.search(line)),
                 None)
    if crash:
        return f"the compiler crashed: {crash.strip()[:200]}"
    if result.returncode == 0:
        return "compiled without errors; a compile-fail test must not compile"
    if not error_lines(result.lines):
        return f"the compiler exited with status {result.returncode} but reported no error"
    return None


def evaluate(spec, result, timeout=DEFAULT_TIMEOUT):
    """Every way in which the compile misses the spec; empty when the test passes."""
    problem = compile_problem(result, timeout)
    if problem:
        return [problem]
    problems = []
    first_error = error_lines(result.lines)[0]
    if spec.expect_id and not contains_id(first_error, spec.expect_id):
        problems.append(f"the first error line does not contain {spec.expect_id}: "
                        f"{first_error.strip()[:200]}")
    diagnostics = "\n".join(diagnostic_lines(result.lines))
    for regex in spec.regexes:
        if not regex.search(diagnostics):
            problems.append(f"expect-regex did not match: {regex.pattern}")
    if len(result.lines) > spec.max_lines:
        problems.append(f"{len(result.lines)} lines of output, "
                        f"more than expect-max-lines ({spec.max_lines})")
    return problems


def write_log(config, name, result):
    log = Path(config["binary_dir"]) / "compile_fail" / f"{name}.log"
    log.parent.mkdir(parents=True, exist_ok=True)
    log.write_text("$ " + " ".join(result.command) + "\n" + result.output, encoding="utf-8")
    return log


def run_test(config, source, timeout, memory_mb):
    """Run one compile-fail test and print its result. Returns True when it passed."""
    name = snippet_name(source)
    compiler = config["golden_name"]
    try:
        text = Path(source).read_text(encoding="utf-8")
    except OSError as error:
        print(f"FAIL {name} ({compiler}): cannot read the file: {error}")
        return False
    spec = parse_directives(text)
    if spec.problems:
        # The first problem goes on the FAIL line, which the scripts' summaries quote.
        print(f"FAIL {name} ({compiler}): bad directive: {spec.problems[0]}")
        for problem in spec.problems[1:]:
            print(f"  - {problem}")
        return False
    result = compile_source(config, source, timeout, memory_mb)
    log = write_log(config, name, result)
    problems = evaluate(spec, result, timeout)
    if not problems:
        print(f"PASS {name} ({compiler}, {len(result.lines)} lines): {spec.summary}")
        return True
    print(f"FAIL {name} ({compiler}): {problems[0]}")
    for problem in problems[1:]:
        print(f"  - {problem}")
    print(f"  the test: {spec.summary}")
    shown = result.lines[:15]
    print(f"  compiler output ({len(shown)} of {len(result.lines)} lines; "
          f"full output in {display(log)}):")
    for line in shown:
        print(f"    {line}")
    return False


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    where = parser.add_mutually_exclusive_group(required=True)
    where.add_argument("--preset", help="use build/<preset>/" + CONFIG_NAME)
    where.add_argument("--config", help="path to a " + CONFIG_NAME)
    parser.add_argument("--timeout", type=int, default=DEFAULT_TIMEOUT,
                        help=f"seconds per compile (default {DEFAULT_TIMEOUT})")
    parser.add_argument("--memory-mb", type=int, default=DEFAULT_MEMORY_MB,
                        help=f"address-space cap per compile, 0 for none "
                             f"(default {DEFAULT_MEMORY_MB})")
    parser.add_argument("files", nargs="*", help="snippets to run (default: all of them)")
    args = parser.parse_args(argv)
    exit_on_termination_signals()

    try:
        config = load_config(config_path(args))
    except ConfigError as error:
        print(f"error: {error}", file=sys.stderr)
        return 2
    files = [Path(f) for f in args.files] or sorted(TESTS_DIR.rglob("*.cpp"))
    if not files:
        print(f"error: no compile-fail tests in {TESTS_DIR}", file=sys.stderr)
        return 2
    missing = [str(f) for f in files if not f.is_file()]
    if missing:
        print(f"error: no such file: {', '.join(missing)}", file=sys.stderr)
        return 2
    passed = sum(run_test(config, source, args.timeout, args.memory_mb) for source in files)
    if len(files) > 1:
        print(f"compile-fail: {passed} of {len(files)} passed")
    return 0 if passed == len(files) else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:  # Ctrl-C: the compiler is already stopped
        print("interrupted", file=sys.stderr)
        sys.exit(130)
