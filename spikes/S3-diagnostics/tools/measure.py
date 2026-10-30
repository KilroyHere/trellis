#!/usr/bin/env python3
"""Spike S3 driver: compile the diagnostics matrix, normalize, check budgets, write results.

Run through run.sh from the repository root. Every compile runs on its own, in a
subshell with `ulimit -v 3000000` and `timeout 300`, with a minimal environment
(LC_ALL=C) and relative paths, so the logs are byte-for-byte reproducible.

Deterministic outputs (compiler logs, results.md) go under spikes/S3-diagnostics/.
Timings are not deterministic; they go to build/spikes/S3-diagnostics/ and stdout.
"""
import os
import re
import statistics
import subprocess
import sys
import time
from pathlib import Path

SPIKE = Path("spikes/S3-diagnostics")
SRC = SPIKE / "src"
LOGS = SPIKE / "logs"
RESULTS = SPIKE / "results.md"
BUILD = Path("build/spikes/S3-diagnostics")

COMPILERS = {"gcc-13": "g++-13", "gcc-14": "g++-14", "clang-18": "clang++-18", "clang-20": "clang++-20"}
# C++26 spelling per compiler; gcc-13 has no c++2c/c++26 mode, so it gets its newest (c++2b).
STD26 = {"gcc-13": "-std=c++2b", "gcc-14": "-std=c++26", "clang-18": "-std=c++2c", "clang-20": "-std=c++2c"}
VARIANTS20 = [("naive", 1), ("leaf", 2), ("guard", 9), ("explain", 3), ("noret", 4),
              ("incomplete", 5), ("ext", 6), ("delete", 8)]
VARIANTS26 = [("msg26", 7), ("delete26", 8)]
CASES = [(e, d) for e in ("E001", "E005", "E012") for d in ("root", "deep")]
FAILING_TYPE = {"E001": "IRepository", "E005": "IRepository", "E012": "Clock"}
WARNINGS = ["-Wall", "-Wextra", "-Wpedantic", "-Wshadow", "-Wconversion"]  # NFR-10's set
LINE_BUDGET = 40  # NFR-07

ENV = {"PATH": os.environ.get("PATH", "/usr/bin:/bin"), "LC_ALL": "C", "HOME": "/nonexistent"}
CAPPED = 'ulimit -v 3000000 && exec timeout 300 "$0" "$@"'


def run(argv, env=None):
    """Run one command capped; return (exit code, combined output, seconds, max RSS in MB)."""
    started = time.monotonic()
    proc = subprocess.Popen(["bash", "-c", CAPPED, *argv], stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, env=env or ENV)
    out = proc.stdout.read()
    _, status, usage = os.wait4(proc.pid, 0)
    proc.returncode = os.waitstatus_to_exitcode(status)
    seconds = time.monotonic() - started
    return proc.returncode, out.decode("utf-8", "replace"), seconds, usage.ru_maxrss / 1024


# ---- normalization for golden comparisons ----------------------------------------
ANSI = re.compile(r"\x1b\[[0-9;]*[A-Za-z]|\x1b\]8;;[^\x1b]*\x1b\\")
EXCERPT = re.compile(r"^\s*\d*\s*\|")  # "  27 |   code" and "     |   ^~~~" lines
BANNER = re.compile(r"^\d+ (warning|error)s?( and \d+ (warning|error)s?)? generated\.$")
INCLUDE_CHAIN = re.compile(r"^(In file included from |\s+from ).*[:,]$")
SRC_PATH = re.compile(r"[^\s'\"(]*spikes/S3-diagnostics/src/")
STD_PATH = re.compile(r"/usr/(lib/gcc/x86_64-linux-gnu/\d+/\.\./\.\./\.\./\.\./)?include/c\+\+/(\d+|v1)/")
LINE_COL = re.compile(r"(<(?:src|std)>/[\w./+-]+):\d+(?::\d+)?")


def normalize(text):
    """The golden-test normalization rules (documented in docs/spikes/S3-diagnostics.md)."""
    text = ANSI.sub("", text)                                   # 1. colours and URLs
    text = text.replace("‘", "'").replace("’", "'")   # 2. GCC's UTF-8 quotes
    lines = []
    for line in text.splitlines():
        if EXCERPT.match(line) or BANNER.match(line) or INCLUDE_CHAIN.match(line):
            continue                                            # 3. excerpts, banners, include chains
        if line.startswith("compilation terminated."):
            continue
        line = SRC_PATH.sub("<src>/", line)                     # 4. paths
        line = STD_PATH.sub("<std>/", line)
        line = LINE_COL.sub(r"\1:L:C", line)                    # 5. line and column numbers
        lines.append(line.rstrip())
    return "\n".join(lines) + "\n"


def metrics(text, case=None):
    lines = text.splitlines()
    errors = [l for l in lines if re.search(r"(^|: )(fatal )?error: ", l)]
    warnings = [l for l in lines if ": warning: " in l]
    first = errors[0] if errors else ""
    m = {"lines": len(lines), "bytes": len(text.encode()), "errors": len(errors),
         "warnings": len(warnings), "first": first}
    if case:
        ident, ty = case[0], FAILING_TYPE[case[0]]
        m["id_first"] = ident in first
        m["type_first"] = ty in first
        m["id_type_line"] = next((i + 1 for i, l in enumerate(lines) if ident in l and ty in l), 0)
    return m


def short(text, width=170):
    text = normalize(text).strip()
    return text if len(text) <= width else text[: width - 3] + "..."


def esc(text):
    return text.replace("|", "\\|").replace("`", "'")


def versions():
    rows = []
    for name, cxx in COMPILERS.items():
        _, out, _, _ = run([cxx, "--version"])
        rows.append(f"| {name} | `{cxx}` | {out.splitlines()[0]} |")
    return rows


def compile_case(cxx, std, variant, case, extra=()):
    defines = [f"-DS3_VARIANT={variant}", f"-DCASE_{case[0]}"]
    if case[1] == "root":
        defines.append("-DDEPTH_ROOT")
    return run([cxx, std, *WARNINGS, "-fsyntax-only", *extra, *defines, str(SRC / "cases.cpp")])


def matrix(checks):
    rows, summary = [], {}
    configs = [(c, "-std=c++20", v) for v in VARIANTS20 for c in COMPILERS]
    configs += [(c, STD26[c], v) for v in VARIANTS26 for c in COMPILERS]
    for comp, std, (vname, vnum) in configs:
        for case in CASES:
            rc, out, _, _ = compile_case(COMPILERS[comp], std, vnum, case)
            log = LOGS / f"{comp}{std.replace('-std=', '.')}" / f"{vname}-{case[0]}-{case[1]}.txt"
            log.parent.mkdir(parents=True, exist_ok=True)
            log.write_text(out)
            m = metrics(out, case)
            cascade = m["errors"] > 1 or m["warnings"] > 0
            rows.append(f"| {comp} | {std[5:]} | {vname} | {case[0]} {case[1]} | {rc} | {m['errors']} "
                        f"| {m['warnings']} | {m['lines']} | {m['bytes']} | {'y' if m['id_first'] else 'n'} "
                        f"| {'y' if m['type_first'] else 'n'} | {m['id_type_line'] or '-'} "
                        f"| {'yes' if cascade else 'no'} | `{esc(short(m['first']))}` |")
            s = summary.setdefault((vname, comp, std[5:]), {"max_lines": 0, "max_bytes": 0, "max_errors": 0,
                                                             "warnings": 0, "id": True, "type": True,
                                                             "fails": True, "cascade": False})
            s["max_lines"] = max(s["max_lines"], m["lines"])
            s["max_bytes"] = max(s["max_bytes"], m["bytes"])
            s["max_errors"] = max(s["max_errors"], m["errors"])
            s["warnings"] += m["warnings"]
            s["id"] &= m["id_first"]
            s["type"] &= m["type_first"]
            s["fails"] &= rc != 0
            s["cascade"] |= cascade
            # Acceptance checks for the two recommended renderings.
            label = f"{vname} {comp} {std[5:]} {case[0]} {case[1]}"
            if vname == "explain":
                ok = (rc != 0 and m["errors"] == 1 and m["warnings"] == 0 and m["lines"] <= LINE_BUDGET
                      and m["id_first"] and 0 < m["id_type_line"] <= 10
                      and (m["type_first"] or comp.startswith("gcc")))
                checks.append((ok, f"{label}: one error, ID in the first error line, type by line "
                                   f"{m['id_type_line']}, {m['lines']} lines"))
            if vname == "msg26" and comp != "gcc-13":
                ok = (rc != 0 and m["errors"] == 1 and m["warnings"] == 0 and m["lines"] <= LINE_BUDGET
                      and m["id_first"] and m["type_first"])
                checks.append((ok, f"{label}: one error, ID and type in the first error line, "
                                   f"{m['lines']} lines"))
            if vname == "msg26" and comp == "gcc-13":
                checks.append((rc != 0 and "string-literal" in out,
                               f"{label}: gcc-13 rejects a non-literal static_assert message"))
    return rows, summary


def sanity(checks):
    rows = []
    out_dir = BUILD / "bin"
    out_dir.mkdir(parents=True, exist_ok=True)
    for comp, cxx in COMPILERS.items():
        stds = [("-std=c++20", 3)] + ([(STD26[comp], 7)] if comp != "gcc-13" else [])
        if comp in ("gcc-14", "clang-20"):
            stds.append(("-std=c++20", 6))  # the P2741 extension in C++20 mode must stay warning-free
        for std, variant in stds:
            for depth in ("deep", "root"):
                exe = out_dir / f"ok-{comp}-{std[5:]}-{variant}-{depth}"
                defines = ["-DCASE_OK", f"-DS3_VARIANT={variant}"] + (["-DDEPTH_ROOT"] if depth == "root" else [])
                rc, out, _, _ = run([cxx, std, *WARNINGS, "-pedantic-errors", "-Werror", *defines, "-o", str(exe),
                                     str(SRC / "cases.cpp")])
                if rc == 0:
                    rc, out, _, _ = run([str(exe)])
                good = rc == 0 and (depth == "root" or "ok: repository=42" in out)
                vname = {3: "explain", 6: "ext", 7: "msg26"}[variant]
                checks.append((good, f"CASE_OK {comp} {std[5:]} {vname} {depth}: compiles with -pedantic-errors "
                                     f"-Werror and runs"))
                rows.append(f"| {comp} | {std[5:]} | {vname} | {depth} | {'pass' if good else 'FAIL'} |")
    return rows


def explain_demo():
    rows = []
    out_dir = BUILD / "bin"
    for comp, cxx in COMPILERS.items():
        for case in CASES:
            exe = out_dir / f"explain-{comp}-{case[0]}-{case[1]}"
            defines = ["-DPRINT_EXPLAIN", f"-DCASE_{case[0]}"] + (["-DDEPTH_ROOT"] if case[1] == "root" else [])
            rc, out, _, _ = run([cxx, "-std=c++20", *WARNINGS, *defines, "-o", str(exe), str(SRC / "cases.cpp")])
            if rc == 0:
                rc, out, _, _ = run([str(exe)])
            rows.append(f"#### {comp}, {case[0]} {case[1]}\n\n```\n{out.rstrip()}\n```\n")
    return rows


def type_names(checks):
    rows = []
    out_dir = BUILD / "bin"
    configs = [(c, cxx, []) for c, cxx in COMPILERS.items()] + [("clang-18+libc++", "clang++-18", ["-stdlib=libc++"])]
    for comp, cxx, extra in configs:
        exe = out_dir / f"type-names-{comp}"
        rc, out, _, _ = run([cxx, "-std=c++20", *WARNINGS, "-Wpedantic", "-Werror", *extra, "-o", str(exe),
                             str(SRC / "type_names.cpp")])
        if rc == 0:
            rc, out, _, _ = run([str(exe)])
        checks.append((rc == 0, f"type names {comp}: constexpr checks compile and the demo runs"))
        rows.append(f"#### {comp}\n\n```\n{out.rstrip()}\n```\n")
    return rows


def normalization_checks(checks):
    """Each pair must normalize to the same text; otherwise the rule set is incomplete."""
    rows = []
    case = ("E001", "deep")
    for comp, cxx in COMPILERS.items():
        base_raw = compile_case(cxx, "-std=c++20", 3, case)[1]
        base = normalize(base_raw)
        probes = [("-fdiagnostics-color=always", ["-fdiagnostics-color=always"], ENV)]
        if comp.startswith("gcc"):
            probes.append(("-fdiagnostics-plain-output", ["-fdiagnostics-plain-output"], ENV))
            probes.append(("-fdiagnostics-urls=always", ["-fdiagnostics-urls=always", "-fdiagnostics-color=always"], ENV))
            probes.append(("LC_ALL=C.UTF-8", [], {**ENV, "LC_ALL": "C.UTF-8"}))
        else:
            probes.append(("-fno-caret-diagnostics", ["-fno-caret-diagnostics"], ENV))
            probes.append(("-fdiagnostics-plain-output", ["-fdiagnostics-plain-output"], ENV))
        for label, flags, env in probes:
            defines = ["-DS3_VARIANT=3", "-DCASE_E001"]
            rc, out, _, _ = run([cxx, "-std=c++20", *WARNINGS, "-fsyntax-only", *flags, *defines,
                                 str(SRC / "cases.cpp")], env=env)
            same = normalize(out) == base
            accepted = "unknown argument" not in out and "unrecognized command-line option" not in out
            raw_differs = out != base_raw
            verdict = ("yes" if same else "NO") if accepted else "n/a"
            rows.append(f"| {comp} | `{label}` | {'yes' if accepted else 'no (unknown flag)'} "
                        f"| {'yes' if raw_differs else 'no'} | {verdict} |")
            if accepted:
                checks.append((same, f"normalization {comp} {label}: same text as the default run"))
        # Absolute source path instead of a relative one.
        rc, out, _, _ = run([cxx, "-std=c++20", *WARNINGS, "-fsyntax-only", "-DS3_VARIANT=3", "-DCASE_E001",
                             str((SRC / "cases.cpp").resolve())])
        same = normalize(out) == base
        rows.append(f"| {comp} | absolute source path | yes | yes | {'yes' if same else 'NO'} |")
        checks.append((same, f"normalization {comp} absolute path: same text as the default run"))
    return rows


# ---- explain-pass cost on generated graphs -----------------------------------------
def generate_graph(depth, width=10, fan=3):
    path = BUILD / "bench" / f"graph-{depth * width}.cpp"
    path.parent.mkdir(parents=True, exist_ok=True)
    name = lambda i, j: f"N_{i}_{j}"
    out = ["// generated by tools/measure.py", "#include <cstdio>", "#include <memory>", '#include "mock.hpp"',
           "struct IRepository { virtual ~IRepository() = default; virtual int get() const = 0; };",
           "struct SqlRepository final : IRepository { int get() const override { return 1; } };"]
    for i in reversed(range(depth)):
        for j in range(width):
            if i == depth - 1:
                params = ["std::shared_ptr<IRepository>"]
            else:
                params = [f"{name(i + 1, (j + k) % width)}&" for k in range(fan)]
            out.append(f"struct {name(i, j)} {{ using inject = trellis::ctor<{', '.join(params)}>; "
                       f"explicit {name(i, j)}({', '.join(params)}) {{}} }};")
    params = [f"{name(0, k)}&" for k in range(fan)]
    out.append(f"struct App {{ using inject = trellis::ctor<{', '.join(params)}>; "
               f"explicit App({', '.join(params)}) {{}} }};")
    binds = [f"trellis::binding<{name(i, j)}, {name(i, j)}, trellis::singleton>"
             for i in range(depth) for j in range(width)]
    out.append("struct graph_module { using bindings = trellis::bindings<")
    out.append(",\n".join(binds))
    out.append("#ifdef BOUND\n, trellis::binding<IRepository, SqlRepository, trellis::singleton>\n#endif\n>; };")
    out.append("int main() {\n#ifdef PRINT_EXPLAIN\n  using X = trellis::explain<graph_module::bindings, App>;\n"
               '  std::printf("%zu hops, %zu memoized reads\\n", X::hops, X::reads);\n#else\n'
               "  trellis::injector<graph_module> inj;\n  [[maybe_unused]] App app = inj.create<App>();\n#endif\n}")
    path.write_text("\n".join(out) + "\n")
    return path


def explain_cost(checks, runs=3):
    hop_rows, out_rows, time_rows = [], [], []
    sizes = [(10, "100"), (100, "1000")]
    configs = [("ok (bound)", 3, ["-DBOUND"], "-std=c++20"), ("guard (no explain pass)", 9, [], "-std=c++20"),
               ("explain", 3, [], "-std=c++20"), ("msg26", 7, [], None)]
    for depth, n in sizes:
        src = generate_graph(depth)
        exe = BUILD / "bin" / f"graph-explain-{n}"
        rc, out, _, _ = run(["g++-14", "-std=c++20", f"-I{SRC}", "-DPRINT_EXPLAIN", "-o", str(exe), str(src)])
        if rc == 0:
            rc, out, _, _ = run([str(exe)])
        hop_rows.append(f"| {n} | {depth} | {out.strip()} |")
        for comp, cxx in COMPILERS.items():
            for label, variant, defines, std in configs:
                std = std or (STD26[comp] if comp != "gcc-13" else None)
                if std is None:
                    continue
                argv = [cxx, std, *WARNINGS, "-fsyntax-only", f"-I{SRC}", f"-DS3_VARIANT={variant}", *defines, str(src)]
                samples = [run(argv) for _ in range(runs)]
                rc, out = samples[0][0], samples[0][1]
                (BUILD / "bench" / f"graph-{n}-{comp}-{variant}{'-bound' if defines else ''}.txt").write_text(out)
                m = metrics(out)
                wall = statistics.median(s[2] for s in samples)
                rss = statistics.median(s[3] for s in samples)
                expect_ok = label.startswith("ok")
                checks.append(((rc == 0) == expect_ok, f"graph N={n} {comp} {label}: exit {rc}"))
                out_rows.append(f"| {n} | {comp} | {label} | {rc} | {m['errors']} | {m['lines']} | {m['bytes']} "
                                f"| {max((len(l) for l in out.splitlines()), default=0)} |")
                time_rows.append(f"| {n} | {comp} | {label} | {wall:.2f} | {rss:.0f} |")
    return hop_rows, out_rows, time_rows


def main():
    checks = []
    LOGS.mkdir(parents=True, exist_ok=True)
    BUILD.mkdir(parents=True, exist_ok=True)
    print("S3: compiler versions", flush=True)
    version_rows = versions()
    print("S3: sanity (CASE_OK builds and runs)", flush=True)
    sanity_rows = sanity(checks)
    print("S3: diagnostics matrix (240 compiles)", flush=True)
    matrix_rows, summary = matrix(checks)
    print("S3: constexpr type names", flush=True)
    type_rows = type_names(checks)
    print("S3: explain-pass demo", flush=True)
    explain_rows = explain_demo()
    print("S3: normalization rules", flush=True)
    norm_rows = normalization_checks(checks)
    print("S3: explain-pass cost on generated graphs", flush=True)
    hop_rows, out_rows, time_rows = explain_cost(checks)

    summary_rows = []
    for (vname, comp, std), s in summary.items():
        summary_rows.append(f"| {vname} | {comp} | {std} | {'yes' if s['fails'] else 'NO'} | {s['max_errors']} "
                            f"| {s['warnings']} | {s['max_lines']} | {s['max_bytes']} | {'yes' if s['id'] else 'no'} "
                            f"| {'yes' if s['type'] else 'no'} | {'yes' if s['cascade'] else 'no'} |")

    failed = [label for ok, label in checks if not ok]
    doc = ["# Spike S3 results (generated by run.sh; do not edit)", "",
           "Everything here is deterministic and rebuilt by `bash spikes/S3-diagnostics/run.sh`.",
           "Raw compiler output for every cell is in `logs/<compiler>.<std>/<variant>-<case>.txt`.",
           "Timings are machine-dependent and go to `build/spikes/S3-diagnostics/explain-cost.md`.", "",
           "## Compilers", "", "| name | driver | version |", "|---|---|---|", *version_rows, "",
           "## Checks", "", f"{len(checks) - len(failed)} of {len(checks)} checks passed.", ""]
    doc += [f"- FAILED: {label}" for label in failed]
    doc += ["", "## Summary per variant and compiler (6 cases each)", "",
            "Lines and bytes are the raw output with default flags (carets on, no colour), as a user sees it.", "",
            "| variant | compiler | std | all fail | max errors | warnings | max lines | max bytes "
            "| ID in 1st error line | type in 1st error line | cascade |",
            "|---|---|---|---|---|---|---|---|---|---|---|", *summary_rows, "",
            "## Full matrix", "",
            "`id+type line`: first output line that names both the error ID and the failing type. "
            "`cascade`: more than one error, or any warning.", "",
            "| compiler | std | variant | case | exit | errors | warnings | lines | bytes | ID 1st | type 1st "
            "| id+type line | cascade | first error line (normalized) |",
            "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|", *matrix_rows, "",
            "## Sanity: CASE_OK compiles with the NFR-10 warnings, -pedantic-errors and -Werror, and runs", "",
            "| compiler | std | variant | depth | result |", "|---|---|---|---|---|", *sanity_rows, "",
            "## Normalization rules: each variant must normalize to the default run's text", "",
            "| compiler | variant | flag accepted | raw output differs | normalized equal |",
            "|---|---|---|---|---|", *norm_rows, "",
            "## Explain-pass cost: deterministic part", "",
            "Generated layered graphs: width 10, fan-out 3, every node a singleton requested by reference; "
            "the deepest layer needs `std::shared_ptr<IRepository>`, which is unbound unless BOUND.", "",
            "| N | depth | explain pass (gcc-14, PRINT_EXPLAIN) |", "|---|---|---|", *hop_rows, "",
            "Compiler output for the same graphs (`-fsyntax-only`, default flags):", "",
            "| N | compiler | variant | exit | errors | lines | bytes | longest line |",
            "|---|---|---|---|---|---|---|---|", *out_rows, "",
            "## Constexpr type names (type_names.cpp)", "", *type_rows,
            "## Explain pass and C++26 message, printed at run time from a C++20 build", "", *explain_rows]
    RESULTS.write_text("\n".join(doc).rstrip() + "\n")
    timing = ["# Explain-pass cost: timings (machine-dependent)", "",
              f"Median of 3 runs, `-fsyntax-only`, wall seconds and peak RSS of the compiler.", "",
              "| N | compiler | variant | wall s | peak RSS MB |", "|---|---|---|---|---|", *time_rows]
    (BUILD / "explain-cost.md").write_text("\n".join(timing) + "\n")
    print("\n".join(timing))
    print(f"\nS3: {len(checks) - len(failed)} of {len(checks)} checks passed")
    for label in failed:
        print(f"FAILED: {label}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
