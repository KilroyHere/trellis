#!/usr/bin/env python3
"""Spike S1 on MSVC (task T1.13): build and run forms.cpp, limits.cpp, stub_test.cpp and the
compile-fail cases with cl, unchanged. Run it from a Visual Studio developer prompt (vcvars64.bat);
.github/workflows/msvc-probe.yml does that on GitHub's windows-latest runner, the only MSVC this
project can reach.

  python msvc.py version                  one notice: compiler, toolset and runner image
  python msvc.py forms OUT                forms.cpp: 8 operator sets x 35 parameter forms
  python msvc.py report OUT FIRST-LAST    one annotation per form FIRST..LAST (1-based, as in the
                                          write-up's table), from OUT/forms.json
  python msvc.py prototype OUT            limits.cpp, stub_test.cpp, compile_fail/*.cpp, then
                                          limits.cpp and stub_test.cpp again with each fallback
                                          operator set (min3, refq) in deduce.hpp's prototype

Each result is reported two ways. Against GCC 14 and Clang 20 (the snapshot in results/), for the
write-up. And against the recorded MSVC results (results/msvc.json, with results/forms-msvc.txt
for the form cells), for the exit status: the record of this cl version (major.minor), or the
newest record if this version has none. Exit status: 0 when the results are unchanged, 1 when
MSVC's behaviour changed. Results are GitHub annotations (::notice and ::warning lines), at most
ten per step, because the checks API serves annotations even where job logs can't be downloaded.
Full outputs go to the job log and to OUT, OUT/*-summary.json in the record's format.

Flags: /std:c++20 /permissive- /W4 /EHsc, no /WX (warnings are counted instead). MSVC_PROBE_CL
overrides the compiler and MSVC_PROBE_RECORD the record, for testing this script elsewhere.
"""
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
import uuid
from collections import Counter
from pathlib import Path

sys.dont_write_bytecode = True  # keep the source tree free of __pycache__

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
RESULTS = HERE / "results"
BASELINES = {"gcc-14": RESULTS / "forms-gcc14.txt", "clang-20": RESULTS / "forms-clang20.txt"}
RECORD = Path(os.environ.get("MSVC_PROBE_RECORD") or RESULTS / "msvc.json")
FLAGS = ["/nologo", "/std:c++20", "/permissive-", "/W4", "/EHsc"]
TIMEOUT_S = 600
# forms.cpp's sets, in its order; the fallbacks S1 measured are min3 and refq.
SETS = ["min", "min3", "min_rr", "refq", "boost", "plan", "rref_first", "value"]
KEY_SETS = ("min", "min3", "refq")
FORMS = 35


# ---- running cl and reporting ---------------------------------------------------------------

def compiler():
    return os.environ.get("MSVC_PROBE_CL") or shutil.which("cl") or "cl"


def run(cmd, cwd, timeout=TIMEOUT_S):
    """Run cmd; returns (exit code, combined output, seconds, timed out)."""
    start = time.monotonic()
    try:
        p = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           text=True, errors="replace", timeout=timeout)
        return p.returncode, p.stdout, time.monotonic() - start, False
    except subprocess.TimeoutExpired as e:
        out = e.stdout or ""
        if isinstance(out, bytes):
            out = out.decode(errors="replace")
        return -1, out, time.monotonic() - start, True


def cl(args, cwd, timeout=TIMEOUT_S):
    return run([compiler(), *FLAGS, *args], cwd, timeout)


def cl_version():
    """(full version, architecture) from cl's banner, e.g. ("19.51.36260", "x64")."""
    _, banner, _, _ = run([compiler()], Path(tempfile.gettempdir()), timeout=60)
    m = re.search(r"Version (\d[\d.]*) for (\S+)", banner)
    return (m.group(1), m.group(2)) if m else ("unknown", "?")


def shorten(text):
    """Drop the repository's path prefix, in either separator style."""
    for root in {str(ROOT), str(ROOT).replace("\\", "/")}:
        text = re.sub(re.escape(root) + r"[\\/]", "", text, flags=re.IGNORECASE)
    return text


# cl's "error C2338:", "fatal error C1202:", link's "error LNK2019:", or a plain "error:".
ERROR_LINE = re.compile(r"\b(?:fatal )?error(?: [A-Z]+\d+)?\s*:")


def first_error(output):
    """The first error line of a compiler's output, without the path prefix."""
    for line in output.splitlines():
        if ERROR_LINE.search(line):
            return shorten(line.strip())
    return ""


def error_code(line):
    """The code of an error line ('...: error C2259: ...') or of an error_message() ('C2259: ...'),
    or '' if it has none."""
    m = re.search(r"\berror ([A-Z]+\d+)", line) or re.match(r"([A-Z]+\d+): ", line)
    return m.group(1) if m else ""


def error_message(line, width=160):
    """'Cnnnn: message' from an MSVC error line (or the line itself), without namespace
    qualifiers and "aka" clauses, at most `width` characters."""
    m = re.search(r"error ([A-Z]+\d+)\s*: (.*)$", line)
    text = f"{m.group(1)}: {m.group(2)}" if m else line
    text = re.sub(r"\b(?:std|forms|s1|s2|lim|graph)::", "", re.sub(r" \(aka '[^']*'\)", "", text))
    return text if len(text) <= width else text[:width - 3] + "..."


def error_lines(output):
    return [ln for ln in output.splitlines() if re.search(r"\b(?:fatal )?error [A-Z]+\d+", ln)]


def warnings(output):
    """Counter of warning codes in a compiler's output."""
    return Counter(re.findall(r"\bwarning ([A-Z]+\d+)", output))


def describe_warnings(counter):
    if not counter:
        return "0 warnings"
    codes = ", ".join(f"{code} x{n}" for code, n in counter.most_common(4))
    total = sum(counter.values())
    return f"{total} warning{'s' if total != 1 else ''} ({codes})"


def escape_data(text):
    return text.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")


def escape_property(text):
    return escape_data(text).replace(":", "%3A").replace(",", "%2C")


def annotate(level, title, message):
    """One GitHub annotation; level is notice, warning or error."""
    print(f"::{level} title={escape_property(title)}::{escape_data(message)}", flush=True)


def dump(title, text):
    """Raw output into a collapsed group of the job log, with workflow commands switched off."""
    token = uuid.uuid4().hex
    print(f"::group::{title}")
    print(f"::stop-commands::{token}")
    print(shorten(text.rstrip()))
    print(f"::{token}::")
    print("::endgroup::", flush=True)


def build(sources, work, name, includes=(), link=True, timeout=TIMEOUT_S):
    """Compile (and link) sources into work/name.exe. Returns a dict with ok, seconds, warnings,
    first_error, the failing lines and the output."""
    work.mkdir(parents=True, exist_ok=True)
    args = [f"/I{d}" for d in includes] + [str(s) for s in sources]
    if link:
        args += [f"/Fe{work / (name + '.exe')}", f"/Fo{work / (name + '.obj')}"]
    else:
        args += ["/c", f"/Fo{work / (name + '.obj')}"]
    code, out, secs, timed_out = cl(args, work, timeout)
    return {"ok": code == 0, "exit": code, "timed_out": timed_out, "seconds": round(secs, 1),
            "warnings": dict(warnings(out)), "first_error": first_error(out),
            "errors": len(error_lines(out)), "lines": len(out.splitlines()), "output": out}


def execute(exe, work):
    code, out, secs, timed_out = run([str(exe)], work, timeout=120)
    lines = [ln for ln in out.splitlines() if ln.strip()]
    return {"exit": code, "timed_out": timed_out, "output": out, "last": lines[-1] if lines else ""}


# ---- the record --------------------------------------------------------------------------------

def record(path=None):
    """(the record's cl version, its data) from a record file shaped {"versions": {"19.51": {...}}}:
    this cl's major.minor, or else the newest recorded version. (None, {}) without a record."""
    path = path or RECORD
    if not path.exists():
        return None, {}
    versions = json.loads(path.read_text()).get("versions", {})
    if not versions:
        return None, {}
    mine = ".".join(cl_version()[0].split(".")[:2])
    key = mine if mine in versions else max(versions, key=lambda v: [int(x) for x in v.split(".")])
    return key, versions[key]


def compare(recorded, measured, path=""):
    """Differences between a recorded and a measured summary, as 'path: recorded X, now Y', key by
    key through nested dicts. A key on one side only is a difference too."""
    if isinstance(recorded, dict) and isinstance(measured, dict):
        diffs = []
        for key in list(recorded) + [k for k in measured if k not in recorded]:
            diffs += compare(recorded.get(key, "(not recorded)"), measured.get(key, "(missing)"),
                             f"{path}{key}/")
        return diffs
    return [] if recorded == measured else [f"{path.rstrip('/')}: recorded {recorded!r}, now {measured!r}"]


def verdict(title, version, recorded, measured):
    """Annotate whether measured matches the record; returns 0 when it does, 1 otherwise."""
    if version is None or recorded is None:
        annotate("warning", title, "no recorded MSVC results to compare with")
        return 1
    diffs = compare(recorded, measured)
    mine = cl_version()[0]
    against = f"the record for cl {version}" + ("" if mine.startswith(version + ".") else f" (this is cl {mine})")
    if not diffs:
        annotate("notice", title, f"unchanged: equal to {against}")
        return 0
    text = "; ".join(diffs[:8]) + (f"; and {len(diffs) - 8} more" if len(diffs) > 8 else "")
    annotate("warning", title, f"MSVC behaviour changed against {against}: {text}")
    return 1


def save_summary(out, name, summary):
    (out / f"{name}-summary.json").write_text(json.dumps(summary, indent=1, sort_keys=True) + "\n")
    dump(f"{name}: summary in the record's format", json.dumps(summary, indent=1, sort_keys=True))


# ---- version -----------------------------------------------------------------------------------

VERSION_PROGRAM = r"""#include <cstdio>
#include <version>
int main() {
#ifdef _MSC_FULL_VER
  std::printf("_MSC_FULL_VER %ld\n", static_cast<long>(_MSC_FULL_VER));
#endif
#ifdef _MSVC_LANG
  std::printf("_MSVC_LANG %ld\n", static_cast<long>(_MSVC_LANG));
#endif
#ifdef _MSVC_STL_UPDATE
  std::printf("_MSVC_STL_UPDATE %ld\n", static_cast<long>(_MSVC_STL_UPDATE));
#endif
#ifdef __cpp_concepts
  std::printf("__cpp_concepts %ld\n", static_cast<long>(__cpp_concepts));
#endif
  return 0;
}
"""


def version():
    """The compiler's banner and macros, the toolset and the runner image, as one notice. Fails if
    the toolset the workflow asked for (MSVC_PROBE_TOOLSET, e.g. 14.44) is not the one on PATH."""
    work = Path(tempfile.mkdtemp(prefix="msvc-version-"))
    full, arch = cl_version()
    src = work / "version.cpp"
    src.write_text(VERSION_PROGRAM)
    b = build([src], work, "version")
    macros = {}
    if b["ok"]:
        r = execute(work / "version.exe", work)
        macros = dict(ln.split(" ", 1) for ln in r["output"].splitlines() if " " in ln)
    toolset = os.environ.get("VCToolsVersion", "unknown")
    parts = [f"cl {full} {arch}", f"_MSC_FULL_VER {macros.get('_MSC_FULL_VER', '?')}",
             f"toolset {toolset}", f"Visual Studio {os.environ.get('VS_DISPLAY_VERSION', '?')}",
             f"STL {macros.get('_MSVC_STL_UPDATE', '?')}",
             f"__cpp_concepts {macros.get('__cpp_concepts', '?')}",
             f"image {os.environ.get('ImageOS', '?')} {os.environ.get('ImageVersion', '?')}",
             f"toolsets on the image: {os.environ.get('MSVC_TOOLSETS', '?')}"]
    dump("version program", b["output"])
    wanted = os.environ.get("MSVC_PROBE_TOOLSET", "latest")
    if wanted != "latest" and not toolset.startswith(wanted):
        annotate("error", "MSVC version", f"asked for toolset {wanted}, got {toolset}: " + ", ".join(parts))
        return 1
    annotate("notice", f"MSVC version ({wanted})", ", ".join(parts))
    return 0


# ---- forms.cpp -----------------------------------------------------------------------------

def parse_rows(text):
    """forms.cpp's ROW lines, as {(set, form index): cell}."""
    rows = {}
    for line in text.splitlines():
        if not line.startswith("ROW|"):
            continue
        _, set_name, form, status, oper, request, verdict_text, si, fi = line.split("|")
        rows[(set_name, int(fi))] = {"set": set_name, "set_index": int(si), "form": form,
                                     "form_index": int(fi), "status": status, "operator": oper,
                                     "request": request,
                                     "verdict": verdict_text.replace(" UNEXPECTED", ""),
                                     "unexpected": verdict_text.endswith("UNEXPECTED")}
    return rows


def same(a, b):
    keys = ("status", "operator", "request", "verdict")
    return a is not None and b is not None and all(a[k] == b[k] for k in keys)


def cell_key(c):
    """A cell as one comparable string: status|operator|request|verdict."""
    return "|".join(c[k] for k in ("status", "operator", "request", "verdict"))


def cell_text(c):
    if c is None:
        return "not measured"
    if c["status"] == "resolves":
        return f"{c['operator'].replace('operator ', '')} -> {c['request']} {c['verdict']}"
    if c["status"] == "not-constructible":
        return "not constructible"
    return c["status"]


def isolated_rows(work):
    """When forms.cpp as a whole does not build (a hard error in some cell): build and run each
    form on its own, all 8 sets, from a copy of forms.cpp's namespace (everything before its
    main), and each cell on its own for a form that still fails, so the hard error is pinned to
    its cells and hides nothing else. Returns (rows, {"set|form index": first error})."""
    src = (HERE / "forms.cpp").read_text()
    marker = "#if defined(S1_DIAG_SET)"
    if marker not in src:
        return {}, {}
    work.mkdir(parents=True, exist_ok=True)
    (work / "forms_prefix.hpp").write_text(src.split(marker)[0])

    def program(name, cells):
        tu = work / f"{name}.cpp"
        tu.write_text('#include "forms_prefix.hpp"\nint main() {\n' + "".join(
            f"  forms::run_cell<std::tuple_element_t<{si}, forms::sets>, "
            f"std::tuple_element_t<{fi}, forms::forms_list>, {si}, {fi}>();\n" for si, fi in cells)
            + "  return 0;\n}\n")
        b = build([tu], work, name)
        return parse_rows(execute(work / f"{name}.exe", work)["output"]) if b["ok"] else b

    rows, hard = {}, {}
    for fi in range(FORMS):
        got = program(f"form-{fi}", [(si, fi) for si in range(len(SETS))])
        if "ok" not in got:
            rows.update(got)
            continue
        for si, set_name in enumerate(SETS):
            cell = program(f"cell-{si}-{fi}", [(si, fi)])
            if "ok" in cell:
                hard[f"{set_name}|{fi}"] = error_message(cell["first_error"])
            else:
                rows.update(cell)
    return rows, hard


def recorded_forms(data):
    """The forms part of a record, with its cells loaded from its rows file."""
    forms_record = dict(data.get("forms", {}))
    rows_file = forms_record.pop("rows_file", None)
    if rows_file:
        forms_record["cells"] = {f"{s}|{f}": cell_key(c)
                                 for (s, f), c in parse_rows((RECORD.parent / rows_file).read_text()).items()}
    return forms_record


def forms(out):
    out.mkdir(parents=True, exist_ok=True)
    b = build([HERE / "forms.cpp"], out, "forms")
    dump("forms.cpp: build output", b["output"])
    result = {"build": {k: v for k, v in b.items() if k != "output"}, "mode": "full", "diag": {},
              "hard_errors": {}}
    if b["ok"]:
        r = execute(out / "forms.exe", out)
        dump("forms.cpp: program output", r["output"])
        result["run"] = {"exit": r["exit"], "last": r["last"]}
        rows = parse_rows(r["output"])
        annotate("notice", "S1 forms.cpp",
                 f"built in {b['seconds']:.0f} s, {describe_warnings(warnings(b['output']))}; "
                 f"ran with exit code {r['exit']}: {r['last']}")
    else:
        rows, result["hard_errors"] = isolated_rows(out / "cells")
        result["mode"] = "isolated"
        hard = result["hard_errors"]
        annotate("notice", "S1 forms.cpp",
                 f"does not build ({describe_warnings(warnings(b['output']))}): "
                 f"{error_message(b['first_error'], 160)}. Built form by form instead: "
                 f"{len(hard)} cell(s) hard-error on their own"
                 + (": " + "; ".join(f"{k.split('|')[0]} x form {int(k.split('|')[1]) + 1}" for k in hard)
                    if hard else ""))
    baselines = {name: parse_rows(path.read_text()) for name, path in BASELINES.items()}
    # Why each cell that does not build fails: every not-constructible cell of the key sets, and
    # every cell whose status differs from both GCC and Clang (at most 30 more compiles).
    todo = [k for k, c in rows.items() if c["status"] == "not-constructible" and k[0] in KEY_SETS]
    todo += [k for k, c in rows.items()
             if k[0] not in KEY_SETS and c["status"] == "not-constructible"
             and all(base.get(k, {}).get("status") != c["status"] for base in baselines.values())][:30]
    for set_name, fi in todo:
        d = cl(["/Zs", f"/DS1_DIAG_SET={SETS.index(set_name)}", f"/DS1_DIAG_FORM={fi}",
                str(HERE / "forms.cpp")], out)
        result["diag"][f"{set_name}|{fi}"] = error_message(first_error(d[1]))
    ordered = sorted(rows.items(), key=lambda kv: (SETS.index(kv[0][0]), kv[0][1]))
    result["rows"] = {f"{s}|{f}": c for (s, f), c in ordered}
    (out / "forms.json").write_text(json.dumps(result, indent=1) + "\n")
    # One line for all sets: how many cells match each compiler.
    parts = []
    for set_name in SETS:
        cells = [(rows.get((set_name, fi)), {n: b.get((set_name, fi)) for n, b in baselines.items()})
                 for fi in range(FORMS)]
        measured = sum(c is not None for c, _ in cells)
        matches = ", ".join(str(sum(same(c, base[n]) for c, base in cells)) for n in BASELINES)
        parts.append(f"{set_name} ({matches})" + (f" of {measured} measured" if measured != FORMS else ""))
    mins = Counter(c["verdict"] if c and c["status"] == "resolves" else cell_text(c)
                   for c in (rows.get(("min", fi)) for fi in range(FORMS)))
    annotate("notice", "S1 sets vs " + " and ".join(BASELINES),
             f"of the {FORMS} cells of each set, how many equal ({', '.join(BASELINES)}): " +
             ", ".join(parts) + ". min on MSVC: " + ", ".join(f"{n} {k}" for k, n in mins.most_common()))
    summary = {"builds": b["ok"], "first_error": error_code(b["first_error"]),
               "build_warnings": dict(warnings(b["output"])),
               "hard_errors": {k: error_code(v) for k, v in result["hard_errors"].items()},
               "cells": {key: cell_key(c) for key, c in result["rows"].items()}}
    save_summary(out, "forms", summary)
    version_key, data = record()
    return verdict("S1 forms.cpp against the MSVC record", version_key,
                   recorded_forms(data) if version_key else None, summary)


def report(out, span):
    """One annotation per form in span ('1-10'), from OUT/forms.json: what MSVC reports, how that
    compares with GCC and Clang, and whether it matches the MSVC record."""
    path = out / "forms.json"
    if not path.exists():
        annotate("warning", f"S1 forms {span}", "no forms.json: the forms step did not finish")
        return 1
    data = json.loads(path.read_text())
    rows = {(c["set"], c["form_index"]): c for c in data["rows"].values()}
    baselines = {name: parse_rows(p.read_text()) for name, p in BASELINES.items()}
    version_key, record_data = record()
    recorded = recorded_forms(record_data) if version_key else {}
    first, last = (int(x) for x in span.split("-"))
    status = 0
    for number in range(first, min(last, FORMS) + 1):
        fi = number - 1
        label = next((c["form"] for c in (rows.get(("min", fi)), baselines["clang-20"].get(("min", fi))) if c), "?")
        mine = rows.get(("min", fi))
        base = {n: b.get(("min", fi)) for n, b in baselines.items()}
        hard = data["hard_errors"].get(f"min|{fi}")
        as_others = mine is not None and all(same(mine, c) for c in base.values())
        text = f"min: {cell_text(mine) if not hard else 'hard error: ' + hard}"
        if not as_others:
            texts = sorted({cell_text(c) for c in base.values()})
            text += f" (gcc-14 and clang-20: {texts[0]})" if len(texts) == 1 else \
                    " (" + "; ".join(f"{n}: {cell_text(c)}" for n, c in base.items()) + ")"
        if mine and mine["status"] == "not-constructible" and f"min|{fi}" in data["diag"]:
            text += f"; cl says {data['diag'][f'min|{fi}']}"
        groups = {"as both": [], "as gcc-14 only": [], "as clang-20 only": [], "as neither": [],
                  "hard error": [], "not measured": []}
        for set_name in SETS:
            c = rows.get((set_name, fi))
            g, k = (same(c, baselines[n].get((set_name, fi))) for n in BASELINES)
            key = ("hard error" if f"{set_name}|{fi}" in data["hard_errors"] else "not measured") \
                if c is None else "as both" if g and k else \
                "as gcc-14 only" if g else "as clang-20 only" if k else "as neither"
            groups[key].append(set_name)
        if len(groups["as both"]) == len(SETS):
            text += "; all 8 sets as gcc-14 and clang-20"
        else:
            text += "; " + "; ".join(f"{k}: {' '.join(v)}" for k, v in groups.items() if v)
        for set_name in groups["hard error"][:1]:  # what the first hard error says
            text += f" ({set_name}: {data['hard_errors'][f'{set_name}|{fi}']})"
        for set_name in groups["as neither"][:2]:  # what MSVC reports where both compilers differ
            text += f"; {set_name} on cl: {cell_text(rows.get((set_name, fi)))}"
        # Against the MSVC record: every set's cell, and the hard errors, of this form.
        changed = []
        for set_name in SETS:
            key = f"{set_name}|{fi}"
            now = cell_key(rows[(set_name, fi)]) if (set_name, fi) in rows else \
                f"hard error {error_code(data['hard_errors'][key])}" \
                if key in data["hard_errors"] else "not measured"
            then = recorded.get("cells", {}).get(key) or \
                (f"hard error {recorded.get('hard_errors', {}).get(key)}" if key in recorded.get("hard_errors", {})
                 else "not recorded")
            if now != then:
                changed.append(f"{set_name}: recorded {then}, now {now}")
        if changed:
            text += "; CHANGED against the MSVC record: " + "; ".join(changed[:3])
            status = 1
        else:
            text += "; unchanged against the MSVC record"
        annotate("warning" if changed else "notice", f"S1 form {number:02d} {label}", text)
    return status


# ---- limits.cpp, stub_test.cpp, compile_fail, fallbacks ------------------------------------

def strip_static_asserts(text):
    """text without its namespace-scope static_assert declarations (those starting a line), with
    the line count kept, so that a file whose assertions fail can still be run for its output."""
    out, i = [], 0
    for m in re.finditer(r"^static_assert\(", text, flags=re.MULTILINE):
        if m.start() < i:
            continue
        depth, j, in_string = 0, m.end() - 1, False
        while j < len(text):
            ch = text[j]
            if in_string:
                if ch == "\\":
                    j += 1
                elif ch == '"':
                    in_string = False
            elif ch == '"':
                in_string = True
            elif ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
                if depth == 0:
                    break
            j += 1
        end = text.index(";", j) + 1
        out.append(text[i:m.start()])
        out.append("\n" * text.count("\n", m.start(), end))
        i = end
    out.append(text[i:])
    return "".join(out)


def failed_asserts(output, source):
    """[(line, source text)] for each line of `source` (a file name) with an error: in limits.cpp
    and stub_test.cpp, the static_asserts that failed."""
    lines = (HERE / source).read_text().splitlines()
    found = []
    for line_no in re.findall(re.escape(source) + r"\((\d+)(?:,\d+)?\)\s*: (?:fatal )?error", output):
        n = int(line_no)
        entry = (n, lines[n - 1].strip().rstrip(";")[:70] if 0 < n <= len(lines) else "")
        if entry not in found:
            found.append(entry)
    return found


def check_program(name, src_dir, work, expected_last):
    """Build and run src_dir/name.cpp. If it does not build, report which static_asserts failed,
    then build a copy without them, so its output still says what MSVC computes."""
    b = build([src_dir / f"{name}.cpp"], work, name)
    dump(f"{name}.cpp ({src_dir.name}): build output", b["output"])
    res = {"built": b["ok"], "seconds": b["seconds"], "warnings": b["warnings"],
           "first_error": b["first_error"], "failed_asserts": []}
    if b["ok"]:
        r = execute(work / f"{name}.exe", work)
        dump(f"{name}.cpp ({src_dir.name}): program output", r["output"])
        res.update(exit=r["exit"], last=r["last"], output=r["output"])
        res["ok"] = r["exit"] == 0 and r["last"].startswith(expected_last)
        return res
    res["failed_asserts"] = failed_asserts(b["output"], f"{name}.cpp")
    res["ok"] = False
    stripped = work / "stripped"
    stripped.mkdir(parents=True, exist_ok=True)
    (stripped / f"{name}.cpp").write_text(strip_static_asserts((src_dir / f"{name}.cpp").read_text()))
    s = build([stripped / f"{name}.cpp"], stripped, name, includes=[src_dir])
    if s["ok"]:
        r = execute(stripped / f"{name}.exe", stripped)
        dump(f"{name}.cpp without its static_asserts: program output", r["output"])
        res.update(stripped_exit=r["exit"], stripped_last=r["last"], output=r["output"])
    else:
        res["stripped_error"] = error_message(s["first_error"])
    return res


def limit_rows(text):
    return {ln.split("|")[1]: ln for ln in text.splitlines() if ln.startswith("LIMIT|")}


def limits_differing(res):
    """{case: 'status|arity|shorter viable'} for each LIMIT row that differs from GCC and Clang
    (identical in results/), from the run, or from the run without assertions."""
    if not res.get("output"):
        return {"(no output)": ""}
    mine, base = limit_rows(res["output"]), limit_rows((RESULTS / "limits-gcc14.txt").read_text())
    return {label: "|".join(mine.get(label, "missing").split("|")[2:])
            for label in base if mine.get(label) != base[label]}


def program_summary(name, res):
    """The parts of a check_program result that the record holds."""
    summary = {"built": res["built"], "warnings": res["warnings"],
               "failed_asserts": [n for n, _ in res["failed_asserts"]]}
    if name == "limits":
        summary["limit_rows_unlike_gcc_clang"] = limits_differing(res)
    else:
        lines = (res.get("output") or "").splitlines()
        summary["passes"] = sum(ln.startswith("PASS ") for ln in lines)
        summary["fails"] = [ln[5:] for ln in lines if ln.startswith("FAIL ")]
        summary["last"] = res.get("last") or res.get("stripped_last", "")
    return summary


def describe_program(name, res):
    """One line about a check_program result."""
    head = f"{name}.cpp: "
    if res["built"]:
        head += f"built, {describe_warnings(Counter(res['warnings']))}; exit {res['exit']}: {res['last']}"
    else:
        asserts = [f"{n}: {text}" for n, text in res["failed_asserts"]]
        head += (f"does not build: {len(asserts)} static_assert(s) failed, at " + "; ".join(asserts[:4])
                 if asserts else f"does not build: {error_message(res['first_error'])}")
        if "stripped_last" in res:
            head += f". Without its static_asserts it runs: {res['stripped_last']}"
        elif "stripped_error" in res:
            head += f". Without its static_asserts: {res['stripped_error']}"
    if name == "limits" and res.get("output"):
        differ = limits_differing(res)
        total = len(limit_rows((RESULTS / "limits-gcc14.txt").read_text()))
        head += f"; LIMIT rows equal to gcc-14 and clang-20: {total - len(differ)} of {total}"
        if differ:  # label: MSVC's status|arity|shorter-viable
            head += " (differ: " + "; ".join(f"{d}: {v}" for d, v in list(differ.items())[:4]) + ")"
    if name == "stub_test" and res.get("output"):
        s = program_summary(name, res)
        head += f"; {s['passes']} PASS, {len(s['fails'])} FAIL"
        head += f": {'; '.join(s['fails'][:3])}" if s["fails"] else ""
    return head


def compile_fail_cases(work):
    """{case: {"ok": the first error has the intended ID, "errors": error lines, "lines": lines}}"""
    cases = {}
    for f in sorted((HERE / "compile_fail").glob("*.cpp")):
        want = re.search(r"^// expect: (\S+)", f.read_text(), flags=re.MULTILINE).group(1)
        b = build([f], work, f.stem, link=False)
        dump(f"compile_fail/{f.name}: output", b["output"])
        cases[f.stem] = {"ok": not b["ok"] and want in b["first_error"], "errors": b["errors"],
                         "lines": b["lines"]}
        if not cases[f.stem]["ok"]:
            cases[f.stem]["first_error"] = error_message(b["first_error"], 90) or "compiled"
    return cases


# Each fallback, as replacements in deduce.hpp: (text, replacement), each text found exactly once.
FALLBACKS = {
    # min3: min plus operator const T&() const, so that const U& no longer needs operator T& to
    # deduce T = const U (S1 section 3.3).
    "min3": [
        ("    requires not_self<Consumer, T>\n  operator T&() const;\n",
         "    requires not_self<Consumer, T>\n  operator T&() const;\n"
         "  template <class T>\n    requires not_self<Consumer, T>\n  operator const T&() const;\n"),
        ("    requires not_self<Consumer, T> && (!can_resolve<Ctx, T&>)\n  operator T&() const = delete;\n",
         "    requires not_self<Consumer, T> && (!can_resolve<Ctx, T&>)\n  operator T&() const = delete;\n"
         "\n  template <class T>\n    requires not_self<Consumer, T> && can_resolve<Ctx, const T&>\n"
         "  operator const T&() const {\n    return ctx.template resolve<const T&>();\n  }\n"
         "  template <class T>\n    requires not_self<Consumer, T> && (!can_resolve<Ctx, const T&>)\n"
         "  operator const T&() const = delete;\n"),
    ],
    # refq: the same ranking from ref-qualifiers (&& beats const&) instead of const.
    "refq": [
        ("    requires not_self<Consumer, T>\n  operator T();\n",
         "    requires not_self<Consumer, T>\n  operator T() &&;\n"),
        ("    requires not_self<Consumer, T>\n  operator T&() const;\n",
         "    requires not_self<Consumer, T>\n  operator T&() const&;\n"),
        ("  operator T() {\n", "  operator T() && {\n"),
        ("  operator T() = delete;\n", "  operator T() && = delete;\n"),
        ("  operator T&() const {\n", "  operator T&() const& {\n"),
        ("  operator T&() const = delete;\n", "  operator T&() const& = delete;\n"),
    ],
}


def fallback_dir(name, work):
    """A copy of the prototype (deduce.hpp, injector_stub.hpp, limits.cpp, stub_test.cpp) whose
    deduce.hpp uses the fallback operator set `name`. Quoted includes look next to the including
    file first, so the copies must sit together."""
    d = work / f"fallback-{name}"
    d.mkdir(parents=True, exist_ok=True)
    text = (HERE / "deduce.hpp").read_text()
    for old, new in FALLBACKS[name]:
        if text.count(old) != 1:
            raise SystemExit(f"msvc.py: deduce.hpp changed; the {name} patch expects {old!r} once")
        text = text.replace(old, new)
    (d / "deduce.hpp").write_text(text)
    for f in ("injector_stub.hpp", "limits.cpp", "stub_test.cpp"):
        shutil.copy(HERE / f, d / f)
    return d


# Why limits.cpp's aggregate {int[2], X} may count differently: which conversion initializes the
# array member when a placeholder with S1's two operator shapes is the first initializer? GCC and
# Clang elide the braces (one placeholder per array element); a compiler that initializes the
# whole array from one placeholder, through operator T&() with T = int[2], counts one field.
AGGREGATE_ARRAY_PROBE = r"""#include <cstdio>
#include <typeinfo>
struct probe {
  template <class T>
  operator T() {
    std::printf(" T() with T = %s;", typeid(T).name());
    return T{};
  }
  template <class T>
  operator T&() const {
    static T t{};
    std::printf(" T&() with T = %s;", typeid(T).name());
    return t;
  }
};
struct agg {
  int a[2];
  int x;
};
template <class Array>
constexpr bool from_array_lvalue = requires(Array& r) { agg{r, 3}; };
template <class P>
constexpr bool two = requires { agg{P{}, P{}}; };
template <class P>
constexpr bool three = requires { agg{P{}, P{}, P{}}; };
int main() {
  std::printf("agg{int[2] lvalue, 3}: %s; agg{p, p}: %s; agg{p, p, p}: %s; agg{p, p} calls:",
              from_array_lvalue<int[2]> ? "valid" : "invalid", two<probe> ? "valid" : "invalid",
              three<probe> ? "valid" : "invalid");
  agg v{probe{}, probe{}};
  std::printf(" x = %d\n", v.x);
  return 0;
}
"""


def aggregate_array_probe(work):
    work.mkdir(parents=True, exist_ok=True)
    (work / "aggregate_array.cpp").write_text(AGGREGATE_ARRAY_PROBE)
    b = build([work / "aggregate_array.cpp"], work, "aggregate_array")
    if not b["ok"]:
        return f"does not build: {error_message(b['first_error'])}"
    return execute(work / "aggregate_array.exe", work)["last"]


def prototype(out):
    out.mkdir(parents=True, exist_ok=True)
    summary = {}
    for name, last in (("limits", "OK: limits"), ("stub_test", "OK: injector stub")):
        res = check_program(name, HERE, out / name, last)
        summary[name] = program_summary(name, res)
        annotate("notice", f"S1 {name}.cpp", describe_program(name, res))
        if name == "limits":
            summary["aggregate_array"] = aggregate_array_probe(out / "aggregate_array")
            annotate("notice", "S1 aggregate-array probe", summary["aggregate_array"])
    cases = compile_fail_cases(out / "compile_fail")
    summary["compile_fail"] = cases
    annotate("notice", "S1 compile_fail",
             f"{sum(c['ok'] for c in cases.values())} of {len(cases)} give the intended ID as the first error: "
             + "; ".join(f"{name} {'ok' if c['ok'] else 'FAIL'} ({c['errors']} error{'s' if c['errors'] != 1 else ''}, "
                         f"{c['lines']} lines)" + ("" if c["ok"] else f": {c['first_error']}")
                         for name, c in cases.items()))
    summary["fallbacks"] = {}
    for name in FALLBACKS:
        d = fallback_dir(name, out)
        parts, summary["fallbacks"][name] = [], {}
        for prog, last in (("limits", "OK: limits"), ("stub_test", "OK: injector stub")):
            res = check_program(prog, d, out / f"fallback-{name}-build" / prog, last)
            summary["fallbacks"][name][prog] = program_summary(prog, res)
            parts.append(describe_program(prog, res))
        annotate("notice", f"S1 fallback {name}", " | ".join(parts))
    save_summary(out, "prototype", summary)
    version_key, data = record()
    return verdict("S1 prototype against the MSVC record", version_key, data.get("prototype"), summary)


def main(argv):
    if len(argv) >= 1 and argv[0] == "version":
        return version()
    if len(argv) == 2 and argv[0] == "forms":
        return forms(Path(argv[1]).resolve())
    if len(argv) == 3 and argv[0] == "report":
        return report(Path(argv[1]).resolve(), argv[2])
    if len(argv) == 2 and argv[0] == "prototype":
        return prototype(Path(argv[1]).resolve())
    print(__doc__, file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
