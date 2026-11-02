#!/usr/bin/env python3
"""Self-test of tools/compile_fail.py: run it on fixtures whose outcome is known.

Nine kinds of case:
  - CASES compile each fixture in compile_fail_fixtures/ with the build's compiler. Each fixture
    breaks one rule of the runner (passes.cpp and nfr07_passes.cpp break none), so a file that
    compiles, or one that misses an expectation or has a bad directive, must be reported as a
    failure for that reason. With a compiler whose output NFR-07 doesn't bound, such as MSVC's cl,
    the two fixtures with an expect-max-lines-nfr07 budget pass and say it was skipped
    (NFR07_SKIPPED). CL_CASES do the same for the fixtures of expect-regex-msvc,
    expect-max-lines-msvc and expect-fail-msvc, whose verdict depends on whether the compiler is
    cl, so that each build checks its own side: GCC and Clang ignore the directives, and on cl the
    first replaces every expect-regex line, the second expect-max-lines, and the third turns a
    test that misses an expectation into a pass (XFAIL) and one that meets them all into a failure
    (XPASS).
  - The NFR-07 checks run in process, without a compiler: NFR-07's count (owner decision 1A) on
    outputs shaped as GCC, Clang and MSVC print them, where the include chain and the quoted
    lines of the user's code go, and those of Trellis's headers and the standard library stay;
    which compilers it applies to; and the verdicts of expect-max-lines-nfr07 next to
    expect-max-lines, which still counts every line.
  - The NFR-07 skip check runs a fixture with expect-max-lines-nfr07: 1 on the stub that prints two
    lines, with the compiler ID of Clang, where it fails, and of MSVC, where the verdict says the
    budget was skipped.
  - The selection check runs the runner's evaluate() on synthetic GCC and cl output, with either
    compiler ID, so that every build checks cl's rules for expect-regex-msvc too: the cl lines
    replace the expect-regex lines on cl only, and expect-id and expect-max-lines apply on both.
    Then the same for expect-max-lines-msvc, which replaces expect-max-lines on cl only, and for
    expect-fail-msvc: on cl only, any miss passes and no miss fails, judged by cl's own
    expectations, and a compile that isn't a proper compile error fails all the same.
  - The expected-failure check runs the two expect-fail-msvc fixtures on the stub, with the
    compiler ID of cl, where the one that misses its expect-regex passes as XFAIL and lists the
    miss, and the one that meets every expectation fails as XPASS, and of GCC and Clang, which
    ignore the directive.
  - STUB_CASES compile passes.cpp with stub_compiler.py, through a temporary config. The stub prints
    the error passes.cpp expects and then crashes, dies by a signal or hangs, and the runner must
    still report a failure. For the hang, neither the stub nor its own child process (playing
    cc1plus) may survive the timeout.
  - INTERRUPTS (POSIX only) start the runner on the hanging stub and then interrupt the runner
    itself, with SIGINT as Ctrl-C does and with SIGTERM as an outer `timeout` does. The compiler
    runs in its own process group, which these signals don't reach, so the runner must take the
    stub and its child down before it exits.
  - The invocation check runs the runner, with stub_compiler.INHERITED in its environment, on the
    stub that reports its arguments, environment and working directory. The compiler must get
    LC_ALL=C and none of the inherited variables, and the output users see (colors off, but quoted
    source and carets kept, as expect-max-lines counts them). It must run in a scratch directory
    of its own, which the runner removes afterwards.
  - The concurrency check starts two runners at once, on that stub lingering after its report, so
    that the two compiles overlap; their working directories must differ.
CASES, CL_CASES and STUB_CASES are checked against the runner's verdict (its first output line)
and exit status. CTest runs this as tools.compile_fail_selftest with the build's --config.
"""
import argparse
import json
import os
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path

sys.dont_write_bytecode = True  # keep tests/ and tools/ free of __pycache__
HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(ROOT / "tools"))
import compile_fail  # noqa: E402  (where the runner logs a snippet's output)
import stub_compiler  # noqa: E402  (what the invocation check adds to the environment)

RUNNER = ROOT / "tools" / "compile_fail.py"
FIXTURES = HERE / "compile_fail_fixtures"
STUB = HERE / "stub_compiler.py"
HANG_TIMEOUT = 2  # seconds the runner gives the hanging stub

# (fixture, expected exit status, text the runner's verdict line must contain)
CASES = [
    ("compiles.cpp", 1, "compiled without errors"),
    ("regex_mismatch.cpp", 1, "expect-regex did not match: this text never appears"),
    ("regex_in_quoted_source.cpp", 1, "expect-regex did not match: gadget"),
    ("id_mismatch.cpp", 1, "the first error line does not contain TRL-E999"),
    ("id_prefix.cpp", 1, "the first error line does not contain TRL-E001"),
    ("too_many_lines.cpp", 1, "more than expect-max-lines (1)"),
    ("missing_summary.cpp", 1, "missing the '// compile-fail: <summary>' directive"),
    ("no_expectation.cpp", 1, "needs an expect-id or an expect-regex"),
    ("unknown_directive.cpp", 1, "unknown directive 'expect-regexp'"),
    ("underscore_directive.cpp", 1, "unknown directive 'expect_regex'"),
    ("late_directive.cpp", 1, "line 4: 'expect-id' must be in the comment block at the top"),
    ("passes.cpp", 0, "PASS "),
    ("nfr07_too_many_lines.cpp", 1, "more than expect-max-lines-nfr07 (1)"),
    ("nfr07_not_a_number.cpp", 1, "expect-max-lines-nfr07 needs a positive number"),
    ("nfr07_passes.cpp", 0, "by NFR-07's count): "),
    ("msvc_fail_no_reason.cpp", 1, "line 3: 'expect-fail-msvc' needs a reason"),
]
# With a compiler whose output NFR-07 doesn't bound, such as MSVC's cl, these fixtures pass with
# their budget skipped: (expected exit status, text the runner's verdict line must contain).
NFR07_SKIPPED = {
    "nfr07_too_many_lines.cpp": (0, "expect-max-lines-nfr07 skipped"),
    "nfr07_passes.cpp": (0, "expect-max-lines-nfr07 skipped"),
}

# NFR-07's count (owner decision 1A) on outputs shaped as the compilers print them, in the source
# tree TREE, a directory named trellis outside include/. Each line is (kept, line), kept True for
# a line the count keeps. GCC 14 and Clang 18 are cut from
# explain/alternating_chain_four_headers_call_over_three_lines.cpp's output: the include chain and
# the quoted call in the user's file go, the quoted lines of Trellis's headers stay.
TREE = "/home/u/trellis"
NFR07_GCC = [
    (False, f"In file included from {TREE}/include/trellis/trellis.hpp:12,"),
    (False, f"                 from {TREE}/tests/layout/app.hpp:4,"),
    (False, f"                 from {TREE}/tests/main.cpp:19:"),
    (True, f"{TREE}/include/trellis/diagnostics/errors.hpp: In instantiation of 'constexpr void "
           "trellis::v1::detail::raise() [with Error = trellis::error::E012<app::sensor>]':"),
    (True, f"{TREE}/include/trellis/diagnostics/explain.hpp:485:83:   required from 'static void "
           "trellis::v1::detail::explain_cause<Path, Parameter, R, Injector>::run()'"),
    (True, "  485 |     detail::raise<typename report::error, report::message>();"),
    (True, "      |     ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~^~"),
    (True, f"{TREE}/include/trellis/diagnostics/explain.hpp:723:13:   [ skipping 13 instantiation "
           "contexts, use -ftemplate-backtrace-limit=0 to disable ]"),
    (True, f"{TREE}/tests/main.cpp:27:27:   required from here"),
    (False, "   26 |       injector"),
    (False, "      |       ~~~~~~~~             "),
    (False, "   27 |           .create<app::c0>();"),
    (False, "      |           ~~~~~~~~~~~~~~~~^~"),
    (True, f"{TREE}/include/trellis/diagnostics/errors.hpp:715:86: error: static assertion failed: "
           "TRL-E012: unbound concrete type requested by reference, pointer or shared_ptr"),
    (True, "  715 |     static_assert(trellis::error::E012<typename args::first>::reported,"),
    (True, "      |                                                       ^~~~~~~~"),
    (True, f"{TREE}/include/trellis/diagnostics/errors.hpp:715:86: note: "
           "'trellis::error::E012<app::sensor>::reported' evaluates to false"),
]
NFR07_CLANG = [
    (False, f"In file included from {TREE}/tests/main.cpp:19:"),
    (False, f"In file included from {TREE}/tests/layout/app.hpp:4:"),
    (False, f"In file included from {TREE}/include/trellis/trellis.hpp:12:"),
    (True, f"{TREE}/include/trellis/diagnostics/errors.hpp:715:19: error: static assertion failed "
           "due to requirement 'trellis::error::E012<app::sensor>::reported': TRL-E012: unbound "
           "concrete type requested by reference, pointer or shared_ptr"),
    (True, "  715 |     static_assert(trellis::error::E012<typename args::first>::reported,"),
    (True, "      |                   ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"),
    (True, f"{TREE}/include/trellis/diagnostics/explain.hpp:723:32: note: (skipping 14 contexts "
           "in backtrace; use -ftemplate-backtrace-limit=0 to see all)"),
    (True, "  723 |             ::new (where) impl(next<s, Js>{}...);"),
    (True, "      |                                ^"),
    (True, f"{TREE}/include/trellis/diagnostics/explain.hpp:690:32: note: in instantiation of "
           "function template specialization 'explain_arg<...>::operator app::c2 &' requested "
           "here"),
    (True, f"{TREE}/tests/main.cpp:27:12: note: in instantiation of function template "
           "specialization 'trellis::v1::injector<>::create<app::c0>' requested here"),
    (False, "   27 |           .create<app::c0>();"),
    (False, "      |            ^"),
    (True, "1 error generated."),
]
# The edges, in TREE. Quoted lines count before any location. A location may have no column.
# Quoted lines belong to the line just before them, so they count after a line that names no
# location, even one that names the user's file, and after an include chain. A fix-it line goes
# with its location. The standard library's quoted lines count, and so do those of a Trellis
# installed outside the tree.
NFR07_EDGES = [
    (True, "   12 |   code before any location"),
    (True, f"{TREE}/tests/main.cpp:9: note: a location with a line and no column"),
    (False, "    9 |   f();"),
    (True, f"{TREE}/tests/main.cpp: In function 'int main()':"),
    (True, "    9 |   quoted under a line that names no location"),
    (True, f"{TREE}/tests/main.cpp:10:3: error: expected ';' after expression"),
    (False, "   10 |   f()"),
    (False, "      |      ^"),
    (False, "      |      ;"),
    (False, f"In file included from {TREE}/tests/main.cpp:1:"),
    (True, "    1 |   quoted under an include chain"),
    (True, "/usr/include/c++/14/type_traits:1234:5: note: in instantiation of template class "
           "'std::is_same<int, int>' requested here"),
    (True, " 1234 |     : public bool_constant<__is_same(_Tp, _Up)>"),
    (True, "      |       ^"),
    (True, "/usr/local/include/trellis/static/injector.hpp:403:44: note: in instantiation of "
           "member function 'run' requested here"),
    (True, "  403 |       detail::explain_root_t<injector, R>::run();"),
    (True, "      |                                            ^"),
]
# Windows paths, with the tree at WIN_TREE: either separator, and the drive letter in either case.
WIN_TREE = "C:\\u\\trellis"
NFR07_WINDOWS = [
    (True, "C:\\u\\trellis\\include\\trellis\\static\\injector.hpp:403:44: note: in instantiation "
           "of member function 'run' requested here"),
    (True, "  403 |       detail::explain_root_t<injector, R>::run();"),
    (True, "      |                                            ^"),
    (True, "c:/u/trellis/tests/main.cpp:9:3: error: expected ';' after expression"),
    (False, "    9 |   f()"),
    (False, "      |      ^"),
]
# MSVC prints no include chain and no quoted source: every line counts.
NFR07_MSVC = [
    (True, "main.cpp"),
    (True, "C:\\u\\trellis\\include\\trellis/diagnostics/errors.hpp(715): error C2338: static "
           "assertion failed: 'TRL-E012: unbound concrete type requested by reference, pointer or "
           "shared_ptr'"),
    (True, "C:\\u\\trellis\\tests\\main.cpp(9): note: see reference to function template "
           "instantiation 'app::c0 trellis::v1::injector<>::create<app::c0>(void)' being compiled"),
]

# (fixture, (expected exit status, verdict text) on cl, the same on any other compiler): the
# fixtures of expect-regex-msvc, which replaces the expect-regex lines on cl only, and of
# expect-max-lines-msvc, which replaces expect-max-lines on cl only. Then those of
# expect-fail-msvc, which on cl turns a test that misses an expectation into a pass (XFAIL) and
# one that meets them all into a failure (XPASS), and which GCC and Clang ignore.
NEVER = "this text never appears in the compiler output"
REASON = "fixture: a known cl limitation"  # the expect-fail-msvc fixtures' reason
CL_CASES = [
    ("msvc_regex_ignored.cpp", (1, f"expect-regex-msvc did not match: {NEVER}"), (0, "PASS ")),
    ("msvc_regex_replaces.cpp", (0, "PASS "), (1, f"expect-regex did not match: {NEVER}")),
    ("msvc_max_lines_ignored.cpp", (1, "more than expect-max-lines-msvc (1)"), (0, "PASS ")),
    ("msvc_max_lines_replaces.cpp", (0, "PASS "), (1, "more than expect-max-lines (1)")),
    ("msvc_fail_expected.cpp", (0, f"XFAIL (cl): {REASON}"),
     (1, f"expect-regex did not match: {NEVER}")),
    ("msvc_fail_xpass.cpp", (1, f"XPASS (cl): {REASON}; remove expect-fail-msvc"), (0, "PASS ")),
]

# (stub scenario, expected exit status, text the runner's verdict line must contain)
STUB_CASES = [
    ("normal-error", 0, "PASS "),
    ("gcc-killed", 1, "the compiler crashed: g++: fatal error: Killed signal terminated program"),
    ("gcc-out-of-memory", 1, "the compiler crashed: cc1plus: out of memory allocating"),
    ("virtual-memory-exhausted", 1, "the compiler crashed: virtual memory exhausted"),
    ("llvm-error", 1, "the compiler crashed: LLVM ERROR: out of memory"),
    ("internal-compiler-error", 1, "internal compiler error: in tsubst_copy"),
    ("clang-crash", 1, "the compiler crashed: PLEASE submit a bug report"),
]
if os.name == "posix":
    STUB_CASES += [
        ("killed-by-signal", 1, "the compiler was killed by signal 9"),
        ("hang", 1, f"the compiler did not finish within {HANG_TIMEOUT} s"),
    ]

# (signal sent to the runner, the runner's expected exit status)
INTERRUPTS = []
if os.name == "posix":
    INTERRUPTS = [(signal.SIGINT, 130), (signal.SIGTERM, 128 + signal.SIGTERM)]


def check_nfr07_count():
    """NFR-07's count on the outputs above, the compilers it applies to, then the verdicts of
    expect-max-lines-nfr07, in process: a budget at the count passes, one line less fails unless
    the compiler is one that NFR-07 doesn't bound, and expect-max-lines still counts every line. No
    compiler is involved."""
    results = []
    for label, tree, sample in (("GCC", TREE, NFR07_GCC), ("Clang", TREE, NFR07_CLANG),
                                ("edges", TREE, NFR07_EDGES),
                                ("Windows paths", WIN_TREE, NFR07_WINDOWS),
                                ("MSVC", WIN_TREE, NFR07_MSVC)):
        lines = [line for _, line in sample]
        expected = [line for kept, line in sample if kept]
        kept = compile_fail.nfr07_lines(lines, tree)
        if kept == expected:
            print(f"ok   NFR-07 count, {label}: {len(kept)} of {len(lines)} lines")
            results.append(True)
            continue
        print(f"FAIL NFR-07 count, {label}: kept {len(kept)} of {len(lines)} lines, expected "
              f"{len(expected)}. Wrongly kept or dropped:")
        for line in lines:
            if (line in kept) != (line in expected):
                print(f"     {'kept' if line in kept else 'dropped'}: {line[:100]}")
        results.append(False)

    applies = {compiler_id: compile_fail.nfr07_applies({"compiler_id": compiler_id})
               for compiler_id in ("GNU", "Clang", "AppleClang", "MSVC")}
    if applies == {"GNU": True, "Clang": True, "AppleClang": True, "MSVC": False}:
        print("ok   NFR-07 applies to the compiler IDs GNU, Clang and AppleClang, not MSVC")
        results.append(True)
    else:
        print(f"FAIL NFR-07 applies to the compiler IDs: {applies}")
        results.append(False)

    # The GCC output with its tree moved to this source tree, which the runner's count uses.
    output = "\n".join(line.replace(TREE, str(ROOT)) for _, line in NFR07_GCC)
    total, counted = len(NFR07_GCC), sum(kept for kept, _ in NFR07_GCC)
    head = "// compile-fail: in-process check\n// expect-id: TRL-E012\n"
    over = (f"{counted} lines by NFR-07's count (without the include chain and the user's "
            f"excerpts), more than expect-max-lines-nfr07 ({counted - 1})")
    verdicts = [  # (directives, whether NFR-07 applies to the compiler, the problem expected)
        (f"// expect-max-lines-nfr07: {counted}\n", True, ""),
        (f"// expect-max-lines-nfr07: {counted - 1}\n", True, over),
        (f"// expect-max-lines-nfr07: {counted - 1}\n", False, ""),
        (f"// expect-max-lines: {total - 1}\n// expect-max-lines-nfr07: {counted}\n", True,
         f"{total} lines of output, more than expect-max-lines ({total - 1})"),
        ("// expect-max-lines-nfr07: 40\n// expect-max-lines-nfr07: 40\n", True,
         "line 4: 'expect-max-lines-nfr07' appears more than once"),
        ("// expect-max-lines-nfr07: forty\n", True,
         "expect-max-lines-nfr07 needs a positive number"),
    ]
    for directives, nfr07, expected in verdicts:
        spec = compile_fail.parse_directives(head + directives + "int main() {}\n")
        problems = spec.problems or compile_fail.evaluate(
            spec, compile_fail.Result(["stub"], returncode=1, output=output), check_nfr07=nfr07)
        label = "; ".join(directives.strip().replace("// ", "").splitlines())
        label += "" if nfr07 else ", with a compiler NFR-07 doesn't bound"
        if (expected and len(problems) == 1 and expected in problems[0]) or (
                not expected and not problems):
            print(f"ok   {label}: {problems[0] if problems else 'passes'}")
            results.append(True)
        else:
            print(f"FAIL {label}: expected {expected or 'no problem'!r}, got {problems}")
            results.append(False)
    return results


def check_nfr07_skip(config, scratch):
    """expect-max-lines-nfr07 is checked with GCC and Clang only: nfr07_too_many_lines.cpp's budget
    of one line, on the stub that prints two, fails with Clang's compiler ID and is skipped, which
    the verdict says, with MSVC's."""
    results = []
    for compiler_id, expected_status, expected_text in (
            ("Clang", 1, "2 lines by NFR-07's count"),
            ("MSVC", 0, "expect-max-lines-nfr07 skipped: NFR-07's 40 lines are GCC's and Clang's")):
        stub = stub_config(config, scratch, "two-lines", compiler_id=compiler_id)
        status, report = run_runner(stub, FIXTURES / "nfr07_too_many_lines.cpp")
        results.append(check(f"NFR-07 with the compiler ID {compiler_id}", status, report,
                             expected_status, expected_text))
    return results


def run_runner(config, source, *extra, env=None):
    """Run the runner from the repository root with a relative path, as people do."""
    relative = Path(source).relative_to(ROOT).as_posix()
    command = [sys.executable, "-I", str(RUNNER), "--config", str(config), *extra, relative]
    proc = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, text=True, check=False)
    return proc.returncode, proc.stdout + proc.stderr


def check(label, status, report, expected_status, expected_text):
    verdict = report.splitlines()[0] if report else ""
    if status == expected_status and expected_text in verdict:
        print(f"ok   {label}: exit {status}, '{expected_text}'")
        return True
    print(f"FAIL {label}: expected exit {expected_status} and '{expected_text}' in the verdict, "
          f"got exit {status}. Runner output:")
    for line in report.splitlines()[:20]:
        print(f"     {line}")
    return False


def check_regex_selection():
    """expect-regex-msvc on synthetic output, for cl (compiler ID "MSVC") and for GCC and Clang:
    on cl the expect-regex-msvc lines replace the expect-regex lines, elsewhere they are ignored,
    and expect-id and expect-max-lines apply on both. No compiler runs, so every build checks
    cl's rules. Then the parser: the directive repeats, its regex must compile, and it doesn't
    count as the expectation that every test needs, since GCC and Clang would then check none."""
    header = "// compile-fail: selection\n// expect-id: TRL-E000\n// expect-max-lines: 3\n"
    both = compile_fail.parse_directives(
        header + "// expect-regex: E000<a, b>\n// expect-regex: one error\n"
        "// expect-regex-msvc: E000<a,b>\n// expect-regex-msvc: one error\n")
    gnu_only = compile_fail.parse_directives(header + "// expect-regex: E000<a, b>\n")
    gnu = ("x.cpp:1:1: error: static assertion failed: TRL-E000: one error\n"
           "x.cpp:1:1: note: in E000<a, b>")
    cl = ("x.cpp(1): error C2338: static assertion failed: 'TRL-E000: one error'\n"
          "x.cpp(1): note: see reference to E000<a,b>")
    # (label, spec, output, compiler ID, the problems evaluate() must give, as prefixes)
    cases = [
        ("GCC's output on GCC", both, gnu, "GNU", []),
        ("Clang's output on Clang", both, gnu, "Clang", []),
        ("cl's output on cl", both, cl, "MSVC", []),
        ("GCC's output on cl", both, gnu, "MSVC", ["expect-regex-msvc did not match: E000<a,b>"]),
        ("cl's output on Clang", both, cl, "Clang", ["expect-regex did not match: E000<a, b>"]),
        ("cl's output on cl, without expect-regex-msvc", gnu_only, cl, "MSVC",
         ["expect-regex did not match: E000<a, b>"]),
        ("the wrong ID on cl", both, cl.replace("TRL-E000", "TRL-E001"), "MSVC",
         ["the first error line does not contain TRL-E000"]),
        ("four lines on cl", both, cl + "\nx.cpp(1): note: a\nx.cpp(1): note: b", "MSVC",
         ["4 lines of output, more than expect-max-lines (3)"]),
    ]
    results = []
    for label, spec, output, compiler_id, expected in cases:
        result = compile_fail.Result(["cc"], returncode=1, output=output)
        problems = compile_fail.evaluate(spec, result, compiler_id=compiler_id)
        if len(problems) == len(expected) and all(
                problem.startswith(prefix) for problem, prefix in zip(problems, expected)):
            print(f"ok   selection, {label}: {problems or 'passes'}")
            results.append(True)
        else:
            print(f"FAIL selection, {label}: expected {expected or 'no problem'}, got {problems}")
            results.append(False)
    # (label, directives after the summary, the first problem parse_directives must give, or
    # None, and how many expect-regex-msvc lines it must keep)
    parses = [
        ("two lines", "// expect-regex: a\n// expect-regex-msvc: b\n// expect-regex-msvc: c\n",
         None, 2),
        ("a bad regex", "// expect-regex: a\n// expect-regex-msvc: (\n",
         "line 3: invalid regex '('", 0),
        ("no other expectation", "// expect-regex-msvc: b\n",
         "a compile-fail test needs an expect-id or an expect-regex", 1),
    ]
    for label, directives, problem, kept in parses:
        spec = compile_fail.parse_directives("// compile-fail: parse\n" + directives)
        first = spec.problems[0] if spec.problems else None
        if (first is None if problem is None else first is not None and first.startswith(problem)) \
                and len(spec.msvc_regexes) == kept:
            print(f"ok   parse, {label}: {first or 'no problem'}, {kept} kept")
            results.append(True)
        else:
            print(f"FAIL parse, {label}: expected {problem or 'no problem'} and {kept} kept, got "
                  f"{spec.problems} and {len(spec.msvc_regexes)} kept")
            results.append(False)
    return (results + check_budget_selection(header, cl)
            + check_expected_failure_selection(header, cl))


def check_budget_selection(header, cl):
    """expect-max-lines-msvc on synthetic output, as check_regex_selection does for the regexes:
    on cl it replaces expect-max-lines, elsewhere it is ignored, and a budget at the line count
    passes where one line less fails. Then the parser: a positive number, once, and not the
    expectation that every test needs."""
    spec = compile_fail.parse_directives(
        header + "// expect-regex: one error\n// expect-max-lines-msvc: 5\n")
    four = cl + "\nx.cpp(1): note: a\nx.cpp(1): note: b"
    five = four + "\nx.cpp(1): note: c"
    six = five + "\nx.cpp(1): note: d"
    # (label, output, compiler ID, the problems evaluate() must give, as prefixes)
    cases = [
        ("four lines on cl, budget 5", four, "MSVC", []),
        ("five lines on cl, budget 5", five, "MSVC", []),
        ("six lines on cl, budget 5", six, "MSVC",
         ["6 lines of output, more than expect-max-lines-msvc (5)"]),
        ("four lines on GCC, which ignores it", four, "GNU",
         ["4 lines of output, more than expect-max-lines (3)"]),
        ("four lines on Clang, which ignores it", four, "Clang",
         ["4 lines of output, more than expect-max-lines (3)"]),
    ]
    results = []
    for label, output, compiler_id, expected in cases:
        result = compile_fail.Result(["cc"], returncode=1, output=output)
        problems = compile_fail.evaluate(spec, result, compiler_id=compiler_id)
        if len(problems) == len(expected) and all(
                problem.startswith(prefix) for problem, prefix in zip(problems, expected)):
            print(f"ok   budget selection, {label}: {problems or 'passes'}")
            results.append(True)
        else:
            print(f"FAIL budget selection, {label}: expected {expected or 'no problem'}, got "
                  f"{problems}")
            results.append(False)
    # (label, directives after the summary, the first problem parse_directives must give, or
    # None, and the cl budget it must keep)
    parses = [
        ("a budget", "// expect-id: TRL-E000\n// expect-max-lines-msvc: 120\n", None, 120),
        ("not a number", "// expect-id: TRL-E000\n// expect-max-lines-msvc: many\n",
         "line 3: expect-max-lines-msvc needs a positive number", None),
        ("zero", "// expect-id: TRL-E000\n// expect-max-lines-msvc: 0\n",
         "line 3: expect-max-lines-msvc needs a positive number", None),
        ("twice", "// expect-id: TRL-E000\n// expect-max-lines-msvc: 9\n"
         "// expect-max-lines-msvc: 9\n", "line 4: 'expect-max-lines-msvc' appears more than once",
         9),
        ("no other expectation", "// expect-max-lines-msvc: 9\n",
         "a compile-fail test needs an expect-id or an expect-regex", 9),
    ]
    for label, directives, problem, kept in parses:
        spec = compile_fail.parse_directives("// compile-fail: parse\n" + directives)
        first = spec.problems[0] if spec.problems else None
        if (first is None if problem is None else first is not None and first.startswith(problem)) \
                and spec.max_lines_msvc == kept:
            print(f"ok   parse, {label}: {first or 'no problem'}, budget {kept}")
            results.append(True)
        else:
            print(f"FAIL parse, {label}: expected {problem or 'no problem'} and budget {kept}, got "
                  f"{spec.problems} and budget {spec.max_lines_msvc}")
            results.append(False)
    return results


def check_expected_failure_selection(header, cl):
    """expect-fail-msvc on synthetic output, as check_regex_selection does for the regexes: on cl,
    a test that misses an expectation, whichever it is, passes (XFAIL) and missed_expectations()
    still gives the miss, and one that meets them all fails (XPASS), judged by cl's own
    expectations (expect-regex-msvc); a compile that isn't a proper compile error still fails; GCC
    and Clang ignore the directive. Then the parser: the reason is required, the directive appears
    once, and it isn't the expectation that every test needs."""
    spec = compile_fail.parse_directives(
        header + "// expect-regex: one error\n// expect-fail-msvc: a reason\n")
    cl_regexes = compile_fail.parse_directives(
        header + "// expect-regex: E000<a, b>\n// expect-regex-msvc: E000<a,b>\n"
        "// expect-fail-msvc: a reason\n")
    other_id = cl.replace("TRL-E000", "TRL-E001")
    other_message = cl.replace("one error", "an error")
    four = cl + "\nx.cpp(1): note: a\nx.cpp(1): note: b"
    crash = cl + "\nx.cpp(1): fatal error C1001: Internal compiler error."
    xpass = ["XPASS (cl): a reason; remove expect-fail-msvc"]
    wrong_id = ["the first error line does not contain TRL-E000"]
    no_match = ["expect-regex did not match: one error"]
    too_long = ["4 lines of output, more than expect-max-lines (3)"]
    gnu_regex = ["expect-regex did not match: E000<a, b>"]
    # (label, spec, output, exit status, compiler ID, the problems evaluate() must give, and the
    # misses missed_expectations() must give, as prefixes, or None where there are none to give)
    cases = [
        ("everything met on cl", spec, cl, 1, "MSVC", xpass, []),
        ("everything met on GCC", spec, cl, 1, "GNU", [], []),
        ("everything met on Clang", spec, cl, 1, "Clang", [], []),
        ("the wrong ID on cl", spec, other_id, 1, "MSVC", [], wrong_id),
        ("the wrong ID on GCC", spec, other_id, 1, "GNU", wrong_id, wrong_id),
        ("a regex missed on cl", spec, other_message, 1, "MSVC", [], no_match),
        ("a regex missed on Clang", spec, other_message, 1, "Clang", no_match, no_match),
        ("four lines on cl", spec, four, 1, "MSVC", [], too_long),
        ("four lines on GCC", spec, four, 1, "GNU", too_long, too_long),
        ("cl's own regexes met on cl", cl_regexes, cl, 1, "MSVC", xpass, []),
        ("cl's output on GCC, which checks expect-regex", cl_regexes, cl, 1, "GNU", gnu_regex,
         gnu_regex),
        ("no error on cl", spec, "", 0, "MSVC", ["compiled without errors"], None),
        ("a crash on cl", spec, crash, 1, "MSVC", ["the compiler crashed"], None),
    ]

    def given(problems, prefixes):
        return len(problems) == len(prefixes) and all(
            problem.startswith(prefix) for problem, prefix in zip(problems, prefixes))

    results = []
    for label, case_spec, output, status, compiler_id, expected, missed in cases:
        result = compile_fail.Result(["cc"], returncode=status, output=output)
        problems = compile_fail.evaluate(case_spec, result, compiler_id=compiler_id)
        misses = None if missed is None else compile_fail.missed_expectations(
            case_spec, result, compiler_id=compiler_id)
        if given(problems, expected) and (missed is None or given(misses, missed)):
            print(f"ok   expected failure, {label}: {problems or 'passes'}"
                  + (f", missing {misses}" if misses else ""))
            results.append(True)
        else:
            print(f"FAIL expected failure, {label}: expected {expected or 'no problem'} and the "
                  f"misses {missed}, got {problems} and {misses}")
            results.append(False)
    # (label, directives after the summary, the first problem parse_directives must give, or
    # None, and the reason it must keep)
    parses = [
        ("a reason", "// expect-id: TRL-E000\n// expect-fail-msvc: cl's limitation\n", None,
         "cl's limitation"),
        ("no reason", "// expect-id: TRL-E000\n// expect-fail-msvc:\n",
         "line 3: 'expect-fail-msvc' needs a reason", None),
        ("a blank reason", "// expect-id: TRL-E000\n// expect-fail-msvc:   \n",
         "line 3: 'expect-fail-msvc' needs a reason", None),
        ("twice", "// expect-id: TRL-E000\n// expect-fail-msvc: one\n// expect-fail-msvc: two\n",
         "line 4: 'expect-fail-msvc' appears more than once", "one"),
        ("no other expectation", "// expect-fail-msvc: a reason\n",
         "a compile-fail test needs an expect-id or an expect-regex", "a reason"),
    ]
    for label, directives, problem, kept in parses:
        spec = compile_fail.parse_directives("// compile-fail: parse\n" + directives)
        first = spec.problems[0] if spec.problems else None
        if (first is None if problem is None else first is not None and first.startswith(problem)) \
                and spec.fail_msvc == kept:
            print(f"ok   parse, {label}: {first or 'no problem'}, reason {kept!r}")
            results.append(True)
        else:
            print(f"FAIL parse, {label}: expected {problem or 'no problem'} and reason {kept!r}, "
                  f"got {spec.problems} and reason {spec.fail_msvc!r}")
            results.append(False)
    return results


def check_expected_failure(config, scratch):
    """expect-fail-msvc through the runner, as CL_CASES check it with the build's compiler, but on
    the stub that prints the error both fixtures expect, with the compiler IDs of cl, GCC and
    Clang in turn, so that every build checks cl's side too. On cl, msvc_fail_expected.cpp misses
    its expect-regex and passes as XFAIL, and the output lists the miss under the verdict;
    msvc_fail_xpass.cpp meets every expectation and fails as XPASS. GCC and Clang ignore the
    directive: the first fails on its regex, and the second passes."""
    listed = f"  - expect-regex did not match: {NEVER}"
    results = []
    for compiler_id in ("MSVC", "GNU", "Clang"):
        on_cl = compiler_id == "MSVC"
        stub = stub_config(config, scratch, "normal-error", compiler_id=compiler_id)
        label = f"expect-fail-msvc with the compiler ID {compiler_id}"
        status, report = run_runner(stub, FIXTURES / "msvc_fail_expected.cpp")
        expected = ((0, f"XFAIL (cl): {REASON}") if on_cl
                    else (1, f"expect-regex did not match: {NEVER}"))
        passed = check(f"{label}, a miss", status, report, *expected)
        if passed and on_cl and listed not in report.splitlines()[1:]:
            print(f"FAIL {label}, a miss: the output doesn't list it under the verdict "
                  f"('{listed.strip()}'). Runner output:")
            for line in report.splitlines()[:20]:
                print(f"     {line}")
            passed = False
        results.append(passed)
        status, report = run_runner(stub, FIXTURES / "msvc_fail_xpass.cpp")
        expected = ((1, f"XPASS (cl): {REASON}; remove expect-fail-msvc") if on_cl
                    else (0, "PASS "))
        results.append(check(f"{label}, every expectation met", status, report, *expected))
    return results


def stub_config(real_config, scratch, scenario, pid_file=None, linger=None, compiler_id=None):
    """A copy of the build's config whose compiler is stub_compiler.py playing SCENARIO, with
    COMPILER_ID in place of the build's compiler ID if given."""
    config = json.loads(Path(real_config).read_text(encoding="utf-8"))
    flags = ["-I", str(STUB), "--scenario", scenario]
    if pid_file:
        flags += ["--pid-file", str(pid_file)]
    if linger:
        flags += ["--linger", str(linger)]
    config.update(compiler=sys.executable, frontend="GNU", golden_name="stub-1", flags=flags,
                  binary_dir=str(scratch))
    if compiler_id:
        config["compiler_id"] = compiler_id
    suffix = ("-linger" if linger else "") + (f"-{compiler_id}" if compiler_id else "")
    path = scratch / f"{scenario}{suffix}.json"
    path.write_text(json.dumps(config, indent=2), encoding="utf-8")
    return path


def process_alive(pid):
    """True while PID runs. A zombie (killed, not yet reaped) counts as dead."""
    if Path("/proc").is_dir():
        try:
            stat = Path(f"/proc/{pid}/stat").read_text(encoding="utf-8")
        except OSError:
            return False
        return stat.rsplit(")", 1)[1].split()[0] not in ("Z", "X")
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    return True


def read_pids(pid_file):
    """The hanging stub's process ID and its child's, or None if it didn't record them."""
    try:
        return [int(pid) for pid in pid_file.read_text(encoding="utf-8").split()]
    except (OSError, ValueError):
        return None


def check_processes_died(label, pid_file):
    """The hanging stub and its child must both be gone, not run on as orphans."""
    pids = read_pids(pid_file)
    if not pids:
        print(f"FAIL {label}: the stub didn't record its process IDs")
        return False
    deadline = time.monotonic() + 3
    while any(process_alive(pid) for pid in pids) and time.monotonic() < deadline:
        time.sleep(0.1)
    survivors = [pid for pid in pids if process_alive(pid)]
    if not survivors:
        print(f"ok   {label}: the stub and its child process died")
        return True
    for pid in survivors:
        os.kill(pid, signal.SIGKILL)
    print(f"FAIL {label}: processes {survivors} of the stub outlived the runner (killed them now)")
    return False


def check_invocation(config, scratch):
    """A compile-fail run starts the compiler with LC_ALL=C, nothing of the runner's own environment
    beyond what a compiler needs to start, and the usual diagnostics flags (no plain output)."""
    label = "invocation"
    env = dict(os.environ, LANG="de_DE.UTF-8", **stub_compiler.INHERITED)
    source = FIXTURES / "passes.cpp"
    status, report = run_runner(stub_config(config, scratch, "report-invocation"), source, env=env)
    if not check(label, status, report, 0, "PASS "):
        return False
    log = compile_fail.log_path({"binary_dir": scratch}, compile_fail.snippet_name(source))
    text = log.read_text(encoding="utf-8") if log.is_file() else ""
    arguments, environment = stub_compiler.read_report(text)
    directory = stub_compiler.read_directory(text)
    if arguments is None or environment is None or directory is None:
        print(f"FAIL {label}: the stub's report is missing from {log}")
        return False
    names = {entry.split("=", 1)[0] for entry in environment}
    problems = [f"{name} was passed on" for name in sorted(set(stub_compiler.INHERITED) & names)]
    problems += [f"{entry} is missing" for entry in ("LC_ALL=C", "LANG=C", "PATH")
                 if entry not in environment]
    if "-fdiagnostics-color=never" not in arguments:
        problems.append("-fdiagnostics-color=never is missing")
    problems += [f"{flag} must stay off: expect-max-lines counts the lines users see"
                 for flag in ("-fdiagnostics-plain-output", "-fno-caret-diagnostics")
                 if flag in arguments]
    # Each compile runs in a scratch directory of its own, removed afterwards, so that compiles in
    # parallel never share one (MSVC's vc140.pdb: compile_fail.compile_source).
    work_root = (Path(scratch) / "compile_fail" / "work").resolve()
    if Path(directory).parent != work_root:
        problems.append(f"the compiler ran in {directory}, not in a directory of its own under "
                        f"{work_root}")
    elif Path(directory).exists():
        problems.append(f"the compile's scratch directory {directory} was left behind")
    if problems:
        print(f"FAIL {label}: {'; '.join(problems)}")
        print(f"     arguments: {' '.join(arguments)}")
        print(f"     environment: {' '.join(environment)}")
        return False
    print(f"ok   {label}: LC_ALL=C, no inherited variable, arguments {' '.join(arguments)}")
    return True


def check_concurrent_compiles(config, scratch):
    """Two runners started together, as CTest starts them, on the stub that reports its working
    directory and then lingers, so that the two compiles overlap: each must run in a directory of
    its own (with one shared directory, MSVC's second cl fails with C1041 on vc140.pdb)."""
    label = "concurrent compiles"
    stub = stub_config(config, scratch, "report-invocation", linger=1)
    sources = [FIXTURES / "passes.cpp", FIXTURES / "regex_mismatch.cpp"]
    runners = [subprocess.Popen([sys.executable, "-I", str(RUNNER), "--config", str(stub),
                                 source.relative_to(ROOT).as_posix()],
                                cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                text=True)
               for source in sources]
    for runner in runners:
        runner.communicate(timeout=60)
    directories = []
    for source in sources:
        log = compile_fail.log_path({"binary_dir": scratch}, compile_fail.snippet_name(source))
        directory = stub_compiler.read_directory(
            log.read_text(encoding="utf-8") if log.is_file() else "")
        if directory is None:
            print(f"FAIL {label}: the stub's report is missing from {log}")
            return False
        directories.append(directory)
    if directories[0] == directories[1]:
        print(f"FAIL {label}: both compiles ran in {directories[0]}")
        return False
    print(f"ok   {label}: two directories, {Path(directories[0]).name} and "
          f"{Path(directories[1]).name}")
    return True


def check_interrupt(config, scratch, signum, expected_status):
    """Interrupt the runner itself while the stub hangs; it must stop the stub before exiting."""
    label = f"runner interrupted by {signal.Signals(signum).name}"
    pid_file = scratch / f"interrupt-{signum}.pid"
    relative = (FIXTURES / "passes.cpp").relative_to(ROOT).as_posix()
    command = [sys.executable, "-I", str(RUNNER), "--config",
               str(stub_config(config, scratch, "hang", pid_file)), "--timeout", "120", relative]
    runner = subprocess.Popen(command, cwd=ROOT, stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT, text=True)
    deadline = time.monotonic() + 20
    while not pid_file.exists() and runner.poll() is None and time.monotonic() < deadline:
        time.sleep(0.05)
    if not pid_file.exists():
        runner.kill()
        print(f"FAIL {label}: the stub never started; runner output: {runner.communicate()[0]}")
        return [False]
    runner.send_signal(signum)
    try:
        output, _ = runner.communicate(timeout=20)
    except subprocess.TimeoutExpired:
        runner.kill()
        output, _ = runner.communicate()
    results = [check_processes_died(label, pid_file)]
    if runner.returncode == expected_status:
        print(f"ok   {label}: exit {runner.returncode}")
        results.append(True)
    else:
        print(f"FAIL {label}: expected exit {expected_status}, got {runner.returncode}. Output:")
        for line in output.splitlines()[:10]:
            print(f"     {line}")
        results.append(False)
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--config", required=True, help="the build's trellis_compiler.json")
    args = parser.parse_args()
    config = Path(args.config).resolve()

    results = []
    fixtures = sorted(path.name for path in FIXTURES.glob("*.cpp"))
    covered = sorted(case[0] for case in CASES + CL_CASES)
    if fixtures != covered:
        print(f"FAIL fixtures and cases differ: fixtures {fixtures}, cases {covered}")
        results.append(False)
    build = json.loads(config.read_text(encoding="utf-8"))
    nfr07 = compile_fail.nfr07_applies(build)
    for fixture, expected_status, expected_text in CASES:
        if not nfr07 and fixture in NFR07_SKIPPED:
            expected_status, expected_text = NFR07_SKIPPED[fixture]
        status, report = run_runner(config, FIXTURES / fixture)
        results.append(check(fixture, status, report, expected_status, expected_text))
    on_cl = build.get("compiler_id") == "MSVC"
    for fixture, cl_expected, other_expected in CL_CASES:
        status, report = run_runner(config, FIXTURES / fixture)
        results.append(check(fixture, status, report, *(cl_expected if on_cl else other_expected)))
    results.extend(check_nfr07_count())
    results.extend(check_regex_selection())

    binary_dir = Path(build["binary_dir"])
    binary_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="compile_fail_selftest-", dir=binary_dir) as name:
        scratch = Path(name)
        for scenario, expected_status, expected_text in STUB_CASES:
            pid_file = scratch / "hang.pid" if scenario == "hang" else None
            extra = ["--timeout", str(HANG_TIMEOUT)] if scenario == "hang" else []
            status, report = run_runner(stub_config(config, scratch, scenario, pid_file),
                                        FIXTURES / "passes.cpp", *extra)
            results.append(check(f"stub {scenario}", status, report, expected_status,
                                 expected_text))
            if pid_file:
                results.append(check_processes_died("hang", pid_file))
        results.extend(check_nfr07_skip(config, scratch))
        results.extend(check_expected_failure(config, scratch))
        results.append(check_invocation(config, scratch))
        results.append(check_concurrent_compiles(config, scratch))
        for signum, expected_status in INTERRUPTS:
            results.extend(check_interrupt(config, scratch, signum, expected_status))

    failures = results.count(False)
    if failures:
        print(f"FAIL {failures} of {len(results)} checks")
        return 1
    print(f"all {len(results)} checks behaved as expected")
    return 0


if __name__ == "__main__":
    sys.exit(main())
