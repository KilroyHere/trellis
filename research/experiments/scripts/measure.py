#!/usr/bin/env python3
"""Build and measure the DI experiments. See ../README.md.

Usage:
  python3 -I measure.py --deps DIR --build DIR --results DIR
                        [--runs 5] [--scaling-runs 3] [--scaling-n 10,25,50,100]
                        [--bench-iters 200000] [--bench-repeats 7]
                        [--only matrix,diagnostics,bench,scaling]

Everything is compiled from outside the downloaded trees: include paths and
sources are passed as arguments, objects go to --build, and nothing shipped
in the downloads is executed. Results (JSON + Markdown + compiler logs) go to
--results.
"""
import argparse
import importlib.util
import json
import os
import platform
import re
import shutil
import signal
import statistics
import subprocess
import tempfile
import threading
import time
import sys
from pathlib import Path

sys.dont_write_bytecode = True  # keep scripts/ free of __pycache__

HERE = Path(__file__).resolve().parent
EXP = HERE.parent
COMMON = EXP / "common"

FRAMEWORKS = ["manual", "boost_di", "fruit", "kangaru", "dingo", "hypodermic", "qlibs_di"]
COMPILERS = {"gcc": "g++", "clang": "clang++"}
STDS = ["17", "20"]
OPT = ["-O2"]
WARN = ["-Wall", "-Wextra"]
SCALING_FRAMEWORKS = ["manual", "boost_di", "fruit", "fruit_flat", "kangaru", "dingo",
                      "hypodermic", "qlibs_di"]
SCALING_FRAMEWORKS_ALL = list(SCALING_FRAMEWORKS)
# Default caps for graphs that blow up (override with --scaling-cap fw=N,...):
#  * fruit_flat (one Fruit component with ~2N chained bindings): on 2026-10-30
#    N=25 was OOM-killed at ~13.9 GB RSS with both GCC 13.3 and Clang 18.1.
#  * qlibs_di: N=25 took 518 s and 10.8 GB with GCC 13.3 (2.75 s at N=10).
SCALING_MAX_N = {"fruit_flat": 10, "qlibs_di": 10}
SCALING_CAP_REASON = {
    "fruit_flat": "N=25 was OOM-killed at ~13.9 GB RSS with GCC and Clang (2026-10-30)",
    "qlibs_di": "N=25 took 518 s and 10.8 GB RSS with GCC (2026-10-30)",
}
PERSIST = None  # set by main(): saves partial scaling results after each record
MEM_ERRORS = ("out of memory", "memory exhausted", "Cannot allocate memory", "bad_alloc",
              "Killed signal", "LLVM ERROR")


def load_generator():
    spec = importlib.util.spec_from_file_location("gen_scaling", HERE / "gen_scaling.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def run(cmd, timeout=None, cwd=None, mem_limit_gb=None):
    """Run cmd; return wall time, exit code, peak RSS (KiB, incl. children) and output.
    mem_limit_gb caps the address space of the child (RLIMIT_AS), so a runaway
    compile fails with "out of memory" instead of triggering the OOM killer."""
    out = tempfile.TemporaryFile()
    err = tempfile.TemporaryFile()
    t0 = time.perf_counter()
    preexec = None
    if mem_limit_gb:
        import resource
        limit = int(mem_limit_gb * 1024 ** 3)

        def preexec():
            resource.setrlimit(resource.RLIMIT_AS, (limit, limit))
    # Own session/process group, so a timeout also kills cc1plus / clang -cc1.
    proc = subprocess.Popen([str(c) for c in cmd], stdout=out, stderr=err, cwd=cwd,
                            preexec_fn=preexec, start_new_session=True)
    timed_out = []
    timer = None
    if timeout:
        def kill():
            timed_out.append(True)
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
        timer = threading.Timer(timeout, kill)
        timer.start()
    _, status, usage = os.wait4(proc.pid, 0)
    wall = time.perf_counter() - t0
    if timer:
        timer.cancel()
    proc.returncode = os.waitstatus_to_exitcode(status)
    out.seek(0)
    err.seek(0)
    return {
        "rc": proc.returncode,
        "wall": wall,
        "maxrss_kb": usage.ru_maxrss,
        "timed_out": bool(timed_out),
        "stdout": out.read().decode("utf-8", "replace"),
        "stderr": err.read().decode("utf-8", "replace"),
    }


def includes(fw, deps, build, cc, std):
    return {
        "manual": [],
        "boost_di": [deps / "boost_di/include"],
        "fruit": [deps / "fruit/include", build / f"fruit-{cc}-{std}/include"],
        "fruit_flat": [deps / "fruit/include", build / f"fruit-{cc}-{std}/include"],
        "kangaru": [deps / "kangaru/include", build / "kangaru-config/generated/include"],
        "dingo": [deps / "dingo/include"],
        "hypodermic": [deps / "hypodermic"],  # + Boost from /usr/include
        "qlibs_di": [deps / "qlibs_di"],
    }[fw]


def libs(fw, build, cc, std):
    return [build / f"fruit-{cc}-{std}/src/libfruit.a"] if fw.startswith("fruit") else []


WARN_RE = re.compile(r"^(?P<file>[^:\s]+):\d+:\d+: warning: (?P<msg>.*?)(?: \[(?P<flag>-W[^\]]+)\])?$")


def parse_warnings(stderr, deps):
    found = []
    for line in stderr.splitlines():
        m = WARN_RE.match(line)
        if not m:
            continue
        f = m.group("file")
        origin = "framework" if f.startswith(str(deps)) else "own code"
        short = f.replace(str(deps) + "/", "<deps>/").replace(str(EXP) + "/", "")
        found.append({"file": short, "origin": origin, "msg": m.group("msg"),
                      "flag": m.group("flag") or ""})
    unique = sorted({(w["file"], w["flag"], w["msg"]) for w in found})
    return {"count": len(found), "unique": [{"file": a, "flag": b, "msg": c} for a, b, c in unique]}


def file_size(p):
    return p.stat().st_size if p.exists() else None


def env_info(deps):
    def first_line(cmd):
        try:
            return subprocess.run(cmd, capture_output=True, text=True).stdout.splitlines()[0]
        except Exception as e:  # noqa: BLE001
            return f"unavailable ({e})"
    model = ""
    try:
        for line in Path("/proc/cpuinfo").read_text().splitlines():
            if line.startswith("model name"):
                model = line.split(":", 1)[1].strip()
                break
    except OSError:
        pass
    boost = ""
    vh = Path("/usr/include/boost/version.hpp")
    if vh.exists():
        m = re.search(r'#define BOOST_LIB_VERSION "([^"]+)"', vh.read_text())
        boost = m.group(1) if m else ""
    versions = (deps / "VERSIONS.txt").read_text() if (deps / "VERSIONS.txt").exists() else ""
    return {
        "date_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "uname": " ".join(platform.uname()),
        "nproc": os.cpu_count(),
        "cpu_model": model,
        "gcc": first_line(["g++", "--version"]),
        "clang": first_line(["clang++", "--version"]),
        "cmake": first_line(["cmake", "--version"]),
        "python": platform.python_version(),
        "boost_headers": boost,
        "flags": {"opt": OPT, "warnings": WARN},
        "frameworks": versions,
        "loadavg": os.getloadavg(),
    }


def header_stats(deps):
    trees = {
        "boost_di": [deps / "boost_di/include"],
        "fruit": [deps / "fruit/include"],
        "fruit (library sources)": [deps / "fruit/src"],
        "kangaru": [deps / "kangaru/include"],
        "dingo": [deps / "dingo/include"],
        "hypodermic": [deps / "hypodermic/Hypodermic"],
        "qlibs_di": [deps / "qlibs_di/di"],
    }
    stats = {}
    for name, roots in trees.items():
        files, lines = 0, 0
        for root in roots:
            paths = [root] if root.is_file() else [p for p in root.rglob("*")
                                                   if p.is_file() and p.suffix in (".h", ".hpp", ".cpp", "")]
            for p in paths:
                try:
                    lines += p.read_text(errors="replace").count("\n")
                    files += 1
                except OSError:
                    pass
        stats[name] = {"files": files, "lines": lines}
    return stats


def compile_cmd(cxx, std, fw, deps, build, cc, src, obj, extra=()):
    inc = [f"-I{COMMON}"] + [f"-I{p}" for p in includes(fw, deps, build, cc, std)]
    return [cxx, f"-std=c++{std}", *OPT, *WARN, *inc, *extra, "-c", src, "-o", obj]


def matrix(args, deps, build, results, log):
    out = []
    logs = results / "logs" / "compile"
    logs.mkdir(parents=True, exist_ok=True)
    work = build / "matrix"
    work.mkdir(parents=True, exist_ok=True)
    for fw in FRAMEWORKS:
        src = EXP / fw / "main.cpp"
        for cc, cxx in COMPILERS.items():
            for std in STDS:
                tag = f"{fw}-{cc}-c++{std}"
                obj, exe = work / f"{tag}.o", work / tag
                cmd = compile_cmd(cxx, std, fw, deps, build, cc, src, obj)
                first = run(cmd, timeout=600)
                rec = {"framework": fw, "compiler": cc, "std": std, "compiles": first["rc"] == 0,
                       "rc": first["rc"], "stderr_bytes": len(first["stderr"].encode()),
                       "stderr_lines": first["stderr"].count("\n"),
                       "warnings": parse_warnings(first["stderr"], deps),
                       "command": " ".join(str(c) for c in cmd).replace(str(deps), "<deps>")
                                  .replace(str(build), "<build>").replace(str(EXP), "<experiments>")}
                if first["stderr"]:
                    (logs / f"{tag}.txt").write_text(first["stderr"])
                if first["rc"] == 0:
                    times, rss = [], []
                    for _ in range(args.runs):
                        r = run(cmd, timeout=600)
                        times.append(r["wall"])
                        rss.append(r["maxrss_kb"])
                    rec["compile_s"] = {"median": statistics.median(times), "min": min(times),
                                        "max": max(times), "runs": len(times)}
                    rec["compile_maxrss_mb"] = round(max(rss) / 1024, 1)
                    link = run([cxx, *OPT, obj, *libs(fw, build, cc, std), "-o", exe])
                    rec["links"] = link["rc"] == 0
                    if link["rc"] == 0:
                        stripped = work / f"{tag}.stripped"
                        shutil.copyfile(exe, stripped)
                        run(["strip", "--strip-all", stripped])
                        rec["binary_bytes"] = file_size(exe)
                        rec["stripped_bytes"] = file_size(stripped)
                        r = run([exe], timeout=60)
                        rec["run_rc"] = r["rc"]
                        rec["run_stdout"] = r["stdout"].strip()
                    else:
                        rec["link_stderr"] = link["stderr"][-2000:]
                    pp = run([c for c in cmd if c not in ("-c", obj, "-o")] + ["-E", "-P"])
                    rec["preprocessed_lines"] = pp["stdout"].count("\n") if pp["rc"] == 0 else None
                else:
                    rec["first_errors"] = [l for l in first["stderr"].splitlines() if "error" in l][:5]
                log(f"[matrix] {tag}: compiles={rec['compiles']} "
                    f"t={rec.get('compile_s', {}).get('median', float('nan')):.2f}s "
                    f"warn={rec['warnings']['count']} size={rec.get('stripped_bytes')}")
                out.append(rec)
    return out


KEY_PATTERNS = [r"did you forget", r"not bound", r"NoBindingFound", r"abstract", r"di::error",
                r"static assertion failed", r"static_assert", r"no matching", r"was not declared",
                r"undeclared identifier", r"what\(\)", r"terminate called", r"exception",
                r"constraints not satisfied", r"Segmentation", r"nullptr", r"IRepository"]


def diagnostics(args, deps, build, results, log):
    out = []
    logs = results / "logs" / "diagnostics"
    logs.mkdir(parents=True, exist_ok=True)
    work = build / "diagnostics"
    work.mkdir(parents=True, exist_ok=True)
    std = "20"
    for fw in FRAMEWORKS:
        src = EXP / fw / "main.cpp"
        for cc, cxx in COMPILERS.items():
            tag = f"{fw}-{cc}-c++{std}-missing"
            obj, exe = work / f"{tag}.o", work / tag
            cmd = compile_cmd(cxx, std, fw, deps, build, cc, src, obj, extra=["-DDI_MISSING_BINDING"])
            r = run(cmd, timeout=600)
            err = r["stderr"]
            (logs / f"{tag}.compile.txt").write_text(err)
            rec = {"framework": fw, "compiler": cc, "std": std, "compile_rc": r["rc"],
                   "compile_s": r["wall"], "stderr_bytes": len(err.encode()),
                   "stderr_lines": err.count("\n"),
                   "error_lines": sum(1 for l in err.splitlines() if re.search(r"\berror\b", l)),
                   "first_error": next((l for l in err.splitlines() if "error" in l), ""),
                   "key_lines": [l[:400] for l in err.splitlines()
                                 if any(re.search(p, l) for p in KEY_PATTERNS)][:12]}
            if r["rc"] == 0:
                rec["detected_at"] = "runtime (compiles)"
                if rec["stderr_lines"]:
                    rec["detected_at"] = "runtime (compiles, with warnings)"
                    # Boost.DI's README compiles with -Werror; check what that changes.
                    # (separate object file: a failing compile deletes its output)
                    we = run(cmd[:-4] + ["-Werror"] + cmd[-4:-1] + [work / f"{tag}.werror.o"],
                             timeout=600)
                    rec.update({"werror_rc": we["rc"], "werror_stderr_bytes": len(we["stderr"].encode()),
                                "werror_stderr_lines": we["stderr"].count("\n")})
                    (logs / f"{tag}.werror.txt").write_text(we["stderr"])
                link = run([cxx, *OPT, obj, *libs(fw, build, cc, std), "-o", exe])
                rec["link_rc"] = link["rc"]
                if link["rc"] != 0:
                    lerr = link["stderr"]
                    (logs / f"{tag}.link.txt").write_text(lerr)
                    rec.update({"detected_at": "link time (compile only warned)",
                                "link_stderr_bytes": len(lerr.encode()),
                                "link_stderr_lines": lerr.count("\n"),
                                "link_first_error": next((l for l in lerr.splitlines()
                                                          if "undefined reference" in l or "error" in l), "")})
                if link["rc"] == 0:
                    rr = run([exe], timeout=60)
                    (logs / f"{tag}.run.txt").write_text(
                        f"exit code: {rr['rc']}\n--- stdout ---\n{rr['stdout']}--- stderr ---\n{rr['stderr']}")
                    rec.update({"run_rc": rr["rc"], "run_stdout": rr["stdout"][-1000:],
                                "run_stderr": rr["stderr"][-2000:],
                                "run_stderr_bytes": len(rr["stderr"].encode())})
                    if rr["rc"] == 0:
                        rec["detected_at"] = "not detected (program ran)"
            else:
                rec["detected_at"] = "compile time"
            log(f"[diag] {tag}: {rec['detected_at']} rc={r['rc']} lines={rec['stderr_lines']} "
                f"bytes={rec['stderr_bytes']}")
            out.append(rec)
    return out


def bench(args, deps, build, results, log):
    out = []
    work = build / "bench"
    work.mkdir(parents=True, exist_ok=True)
    pin = ["taskset", "-c", str((os.cpu_count() or 1) - 1)] if shutil.which("taskset") else []
    variants = [(fw, fw, []) for fw in FRAMEWORKS]
    if "boost_di" in FRAMEWORKS:
        variants.append(("boost_di_shared_ext", "boost_di",
                         ["-DDI_BOOST_SHARED_EXT", f"-I{deps / 'boost_di/extension/include'}"]))
    std = "20"
    for name, fw, extra in variants:
        src = EXP / fw / "main.cpp"
        for cc, cxx in COMPILERS.items():
            tag = f"{name}-{cc}-bench"
            obj, exe = work / f"{tag}.o", work / tag
            cmd = compile_cmd(cxx, std, fw, deps, build, cc, src, obj,
                              extra=["-DNDEBUG", "-DDI_BENCH", *extra])
            c = run(cmd, timeout=600)
            if c["rc"] != 0:
                out.append({"framework": name, "compiler": cc, "error": c["stderr"][-1500:]})
                log(f"[bench] {tag}: COMPILE FAILED")
                continue
            l = run([cxx, *OPT, obj, *libs(fw, build, cc, std), "-o", exe])
            if l["rc"] != 0:
                out.append({"framework": name, "compiler": cc, "error": l["stderr"][-1500:]})
                continue
            load_before = os.getloadavg()
            r = run(pin + [exe, str(args.bench_iters), str(args.bench_repeats)], timeout=1800)
            load_after = os.getloadavg()
            for line in r["stdout"].splitlines():
                if line.startswith("{"):
                    rec = json.loads(line)
                    rec.update({"framework": name, "compiler": cc, "pinned_cpu": " ".join(pin[2:]),
                                "loadavg_before": load_before[0], "loadavg_after": load_after[0]})
                    out.append(rec)
                    log(f"[bench] {name} {cc} {rec['case']}: {rec['median_ns']:.1f} ns/op "
                        f"(min {rec['min_ns']:.1f}, max {rec['max_ns']:.1f})")
            if r["rc"] != 0:
                out.append({"framework": name, "compiler": cc, "error": f"rc={r['rc']} {r['stderr'][-800:]}"})
    return out


def scaling(args, deps, build, results, log):
    gen = load_generator()
    out = []
    work = build / "scaling"
    work.mkdir(parents=True, exist_ok=True)
    logs = results / "logs" / "scaling"
    logs.mkdir(parents=True, exist_ok=True)
    std = "20"
    ns = [int(x) for x in args.scaling_n.split(",")]
    for n in ns:
        expected = gen.expected_checksum(n)
        for fw in SCALING_FRAMEWORKS:
            if n > SCALING_MAX_N.get(fw, 10 ** 9):
                rec = {"framework": fw, "n": n, "skipped": "above --scaling-cap: "
                       + SCALING_CAP_REASON.get(fw, "")}
                out.append(rec)
                if PERSIST:
                    PERSIST(rec)
                continue
            src = work / f"{fw}_{n}.cpp"
            src.write_text(gen.generate(fw, n))
            for cc, cxx in COMPILERS.items():
                tag = f"{fw}-{n}-{cc}"
                obj, exe = work / f"{tag}.o", work / tag
                inc = [f"-I{p}" for p in includes(fw, deps, build, cc, std)]
                cmd = [cxx, f"-std=c++{std}", *OPT, *inc, "-c", src, "-o", obj]
                times, rss = [], []
                rec = {"framework": fw, "n": n, "compiler": cc}
                for i in range(args.scaling_runs):
                    r = run(cmd, timeout=args.scaling_timeout, mem_limit_gb=args.mem_limit_gb)
                    if r["rc"] != 0:
                        rec["error"] = (f"timeout ({args.scaling_timeout} s)" if r["timed_out"]
                                        else f"rc={r['rc']}")
                        if any(m in r["stderr"] for m in MEM_ERRORS):
                            rec["error"] += f" (memory cap {args.mem_limit_gb} GiB address space)"
                        rec["failed_after_s"] = round(r["wall"], 1)
                        rec["peak_rss_mb_at_failure"] = round(r["maxrss_kb"] / 1024, 1)
                        (logs / f"{tag}.txt").write_text(r["stderr"])
                        break
                    times.append(r["wall"])
                    rss.append(r["maxrss_kb"])
                    if r["wall"] > 60:  # long compiles: one sample is enough
                        break
                if times:
                    rec["compile_s"] = {"median": statistics.median(times), "min": min(times),
                                        "max": max(times), "runs": len(times)}
                    rec["compile_maxrss_mb"] = round(max(rss) / 1024, 1)
                    link = run([cxx, *OPT, obj, *libs(fw, build, cc, std), "-o", exe])
                    if link["rc"] == 0:
                        rr = run([exe], timeout=60)
                        rec["checksum_ok"] = rr["stdout"].strip() == f"checksum={expected}"
                        rec["stripped_bytes"] = None
                        stripped = work / f"{tag}.stripped"
                        shutil.copyfile(exe, stripped)
                        run(["strip", "--strip-all", stripped])
                        rec["stripped_bytes"] = file_size(stripped)
                    else:
                        rec["error"] = "link failed"
                rec["caps"] = {"timeout_s": args.scaling_timeout, "mem_limit_gib": args.mem_limit_gb}
                log(f"[scaling] {tag}: {rec.get('compile_s', {}).get('median', float('nan')):.2f}s "
                    f"rss={rec.get('compile_maxrss_mb')}MB ok={rec.get('checksum_ok')} {rec.get('error', '')}")
                out.append(rec)
                if PERSIST:
                    PERSIST(rec)
    return out


def multitu(args, deps, build, results, log):
    """Same graph as `scaling`, one translation unit per component, compiled
    in parallel (like `make -j<nproc>`). Only manual and Fruit."""
    import concurrent.futures
    gen = load_generator()
    out = []
    std = "20"
    jobs = os.cpu_count() or 1
    for n in [int(x) for x in args.multitu_n.split(",")]:
        expected = gen.expected_checksum(n)
        for fw in gen.MULTI_TU_FRAMEWORKS:
            if fw not in FRAMEWORKS:
                continue
            d = build / "multitu" / f"{fw}_{n}"
            if d.exists():
                shutil.rmtree(d)
            d.mkdir(parents=True)
            for name, text in gen.generate_multi_tu(fw, n).items():
                (d / name).write_text(text)
            srcs = sorted(d.glob("*.cpp"))
            for cc, cxx in COMPILERS.items():
                inc = [f"-I{p}" for p in includes(fw, deps, build, cc, std)] + [f"-I{d}"]
                objs = [d / f"{s.stem}-{cc}.o" for s in srcs]
                cmds = [[cxx, f"-std=c++{std}", *OPT, *inc, "-c", s, "-o", o]
                        for s, o in zip(srcs, objs)]
                t0 = time.perf_counter()
                with concurrent.futures.ThreadPoolExecutor(jobs) as pool:
                    res = list(pool.map(lambda c: run(c, timeout=args.scaling_timeout), cmds))
                wall = time.perf_counter() - t0
                main_res = res[[s.name for s in srcs].index("main.cpp")]
                rec = {"framework": fw, "n": n, "compiler": cc, "tus": len(srcs), "jobs": jobs,
                       "parallel_wall_s": wall, "sum_tu_s": sum(r["wall"] for r in res),
                       "max_tu_s": max(r["wall"] for r in res), "main_tu_s": main_res["wall"],
                       "max_rss_mb": round(max(r["maxrss_kb"] for r in res) / 1024, 1),
                       "ok": all(r["rc"] == 0 for r in res)}
                if rec["ok"]:
                    exe = d / f"app-{cc}"
                    t1 = time.perf_counter()
                    link = run([cxx, *OPT, *objs, *libs(fw, build, cc, std), "-o", exe])
                    rec["link_s"] = time.perf_counter() - t1
                    if link["rc"] == 0:
                        rr = run([exe], timeout=60)
                        rec["checksum_ok"] = rr["stdout"].strip() == f"checksum={expected}"
                        stripped = d / f"app-{cc}.stripped"
                        shutil.copyfile(exe, stripped)
                        run(["strip", "--strip-all", stripped])
                        rec["stripped_bytes"] = file_size(stripped)
                else:
                    bad = next(r for r in res if r["rc"] != 0)
                    rec["error"] = bad["stderr"][-1500:]
                log(f"[multitu] {fw} N={n} {cc}: wall={wall:.2f}s (-j{jobs}) sum={rec['sum_tu_s']:.2f}s "
                    f"main={rec['main_tu_s']:.2f}s ok={rec.get('checksum_ok')}")
                out.append(rec)
    return out


def sizes(args, deps, build, results, log):
    """Adds text/data/bss (from `size`) to the matrix records. Stripped file
    sizes are quantised by 4 KiB segment alignment; text size is finer."""
    data = json.loads((results / "results.json").read_text())
    recs = data.get("matrix", [])
    for r in recs:
        exe = build / "matrix" / f"{r['framework']}-{r['compiler']}-c++{r['std']}"
        if not exe.exists() or not r.get("compiles"):
            continue
        out = run(["size", "--format=berkeley", exe])
        lines = out["stdout"].splitlines()
        if out["rc"] == 0 and len(lines) >= 2:
            text, dat, bss = (int(x) for x in lines[1].split()[:3])
            r.update({"text_bytes": text, "data_bytes": dat, "bss_bytes": bss})
            log(f"[sizes] {exe.name}: text={text} data={dat} bss={bss}")
    return recs


def extras(args, deps, build, results, log):
    """Two attribution checks:
    * boost_headers: a TU that includes only the Boost headers Hypodermic uses,
      to see how much of Hypodermic's compile time is Boost.
    * fruit_sources: Fruit's 10 library sources compiled directly (outside its
      CMake, which forces -std=c++11) in C++17/C++20 with -Wall -Wextra."""
    out = {"boost_headers": [], "fruit_sources": []}
    work = build / "extras"
    work.mkdir(parents=True, exist_ok=True)
    src = work / "boost_headers_only.cpp"
    src.write_text("#include <boost/algorithm/string.hpp>\n#include <boost/range/adaptor/reversed.hpp>\n"
                   "#include <boost/range/sub_range.hpp>\n#include <boost/signals2.hpp>\nint main() {}\n")
    for cc, cxx in COMPILERS.items():
        for std in STDS:
            cmd = [cxx, f"-std=c++{std}", *OPT, "-c", src, "-o", work / f"boost-{cc}-{std}.o"]
            times = []
            for _ in range(3):
                r = run(cmd, timeout=600)
                if r["rc"] != 0:
                    break
                times.append(r["wall"])
            rec = {"compiler": cc, "std": std, "ok": bool(times),
                   "compile_s_median": statistics.median(times) if times else None}
            out["boost_headers"].append(rec)
            log(f"[extras] boost headers only {cc} c++{std}: {rec['compile_s_median']}")
    fruit_src = sorted((deps / "fruit/src").glob("*.cpp"))
    for cc, cxx in COMPILERS.items():
        for std in STDS:
            warnings, failures, total = [], [], 0.0
            for s in fruit_src:
                cmd = [cxx, f"-std=c++{std}", *OPT, *WARN, f"-I{deps / 'fruit/include'}",
                       f"-I{build / f'fruit-{cc}-{std}/include'}", "-c", s,
                       "-o", work / f"fruit-{s.stem}-{cc}-{std}.o"]
                r = run(cmd, timeout=600)
                total += r["wall"]
                if r["rc"] != 0:
                    failures.append({"file": s.name, "stderr": r["stderr"][-1500:]})
                warnings += parse_warnings(r["stderr"], deps)["unique"]
            rec = {"compiler": cc, "std": std, "files": len(fruit_src), "failures": failures,
                   "warnings": warnings, "serial_compile_s": total}
            out["fruit_sources"].append(rec)
            log(f"[extras] fruit sources {cc} c++{std}: {len(fruit_src)} files, "
                f"{len(failures)} failed, {len(warnings)} distinct warnings, {total:.1f}s serial")
    return out


def fmt_s(x):
    return f"{x:.2f}" if isinstance(x, (int, float)) else "-"


def summary_md(data):
    L = []
    e = data["env"]
    L.append("# DI experiment results\n")
    L.append(f"- Date (UTC): {e['date_utc']}")
    L.append(f"- Host: {e['uname']}")
    L.append(f"- CPU: {e['cpu_model']} ({e['nproc']} logical CPUs)")
    L.append(f"- Compilers: {e['gcc']} / {e['clang']}; {e['cmake']}; Boost headers {e['boost_headers']}")
    L.append(f"- Flags: {' '.join(e['flags']['opt'] + e['flags']['warnings'])}, single translation unit")
    L.append("- Framework versions:\n\n```\n" + e["frameworks"].strip() + "\n```\n")
    if data.get("matrix"):
        L.append("## Compile matrix (scenario program)\n")
        L.append("| framework | compiler | std | compiles | warnings (-Wall -Wextra) | compile s (median) | min-max | runs | peak RSS MB | stripped bytes | .text bytes | preprocessed lines | run output |")
        L.append("|---|---|---|---|---|---|---|---|---|---|---|---|---|")
        for r in data["matrix"]:
            c = r.get("compile_s", {})
            L.append(f"| {r['framework']} | {r['compiler']} | c++{r['std']} | {'yes' if r['compiles'] else 'NO'} | "
                     f"{r['warnings']['count']} | {fmt_s(c.get('median'))} | {fmt_s(c.get('min'))}-{fmt_s(c.get('max'))} | "
                     f"{c.get('runs', '-')} | {r.get('compile_maxrss_mb', '-')} | {r.get('stripped_bytes', '-')} | {r.get('text_bytes', '-')} | "
                     f"{r.get('preprocessed_lines', '-')} | {r.get('run_stdout', '; '.join(r.get('first_errors', [])[:1]))[:120]} |")
        L.append("\n### Distinct warnings\n")
        for r in data["matrix"]:
            for w in r["warnings"]["unique"]:
                L.append(f"- {r['framework']} {r['compiler']} c++{r['std']}: `{w['file']}`: {w['msg']} [{w['flag']}]")
        L.append("")
    if data.get("diagnostics"):
        L.append("## Missing binding (IRepository binding removed, C++20)\n")
        L.append("| framework | compiler | detected | compile rc | stderr lines | stderr bytes | error lines | link/run stderr bytes | runtime rc | first error / runtime message |")
        L.append("|---|---|---|---|---|---|---|---|---|---|")
        for r in data["diagnostics"]:
            if r["compile_rc"] != 0:
                msg = r["first_error"]
            elif r.get("link_rc"):
                msg = "link: " + r.get("link_first_error", "")
            else:
                tail = (r.get("run_stderr") or r.get("run_stdout") or "").strip().splitlines()
                msg = tail[-1] if tail else ""
            msg = msg.replace("|", "\\|")[:220]
            L.append(f"| {r['framework']} | {r['compiler']} | {r['detected_at']} | {r['compile_rc']} | {r['stderr_lines']} | "
                     f"{r['stderr_bytes']} | {r['error_lines']} | "
                     f"{r.get('link_stderr_bytes', r.get('run_stderr_bytes', '-'))} | {r.get('run_rc', '-')} | {msg} |")
        L.append("")
    if data.get("bench"):
        L.append("## Runtime (C++20, -O2 -DNDEBUG, ns per operation)\n")
        L.append("`graph` = create container/injector + register bindings + resolve App + destroy everything; "
                 "`resolve` = resolve App from an existing container (shared objects already built). "
                 "Fruit `graph_normalized` = injector from a pre-built NormalizedComponent.\n")
        L.append("| framework | compiler | case | median ns | min ns | max ns | iters x repeats |")
        L.append("|---|---|---|---|---|---|---|")
        for r in data["bench"]:
            if "error" in r:
                L.append(f"| {r['framework']} | {r['compiler']} | ERROR | {r['error'][:100]} | | | |")
                continue
            L.append(f"| {r['framework']} | {r['compiler']} | {r['case']} | {r['median_ns']:.1f} | {r['min_ns']:.1f} | "
                     f"{r['max_ns']:.1f} | {r['iters']} x {r['repeats']} |")
        L.append("")
    if data.get("scaling"):
        L.append("## Scaling (generated DAG of N shared components, C++20 -O2, compile only)\n")
        L.append("| framework | N | compiler | compile s (median) | runs | peak RSS MB | stripped bytes | checksum ok | note |")
        L.append("|---|---|---|---|---|---|---|---|---|")
        order = {f: i for i, f in enumerate(SCALING_FRAMEWORKS_ALL)}
        rows = sorted(data["scaling"], key=lambda r: (order.get(r["framework"], 99), r["n"],
                                                      r.get("compiler", "")))
        for r in rows:
            if "skipped" in r:
                L.append(f"| {r['framework']} | {r['n']} | - | - | - | - | - | - | skipped: {r['skipped']} |")
                continue
            c = r.get("compile_s", {})
            note = r.get("error", "")
            if r.get("source"):
                note = (note + " " if note else "") + f"[{r['source']}]"
            L.append(f"| {r['framework']} | {r['n']} | {r['compiler']} | {fmt_s(c.get('median'))} | {c.get('runs', '-')} | "
                     f"{r.get('compile_maxrss_mb', '-')} | {r.get('stripped_bytes', '-')} | {r.get('checksum_ok', '-')} | {note} |")
        L.append("")
    if data.get("multitu"):
        L.append("## Scaling, one translation unit per component (parallel build)\n")
        L.append("| framework | N | compiler | TUs | jobs | wall s | sum of TU s | main.cpp s | slowest TU s | link s | peak RSS MB | stripped bytes | checksum ok |")
        L.append("|---|---|---|---|---|---|---|---|---|---|---|---|---|")
        for r in data["multitu"]:
            L.append(f"| {r['framework']} | {r['n']} | {r['compiler']} | {r['tus']} | {r['jobs']} | {fmt_s(r['parallel_wall_s'])} | "
                     f"{fmt_s(r['sum_tu_s'])} | {fmt_s(r['main_tu_s'])} | {fmt_s(r['max_tu_s'])} | {fmt_s(r.get('link_s'))} | "
                     f"{r['max_rss_mb']} | {r.get('stripped_bytes', '-')} | {r.get('checksum_ok', '-')} |")
        L.append("")
    if data.get("extras"):
        x = data["extras"]
        L.append("## Extra checks\n")
        L.append("Boost headers used by Hypodermic, alone in one TU (`-O2 -c`, median of 3):\n")
        L.append("| compiler | std | compile s |")
        L.append("|---|---|---|")
        for r in x.get("boost_headers", []):
            L.append(f"| {r['compiler']} | c++{r['std']} | {fmt_s(r['compile_s_median'])} |")
        L.append("\nFruit library sources compiled directly with `-O2 -Wall -Wextra` (outside Fruit's CMake):\n")
        L.append("| compiler | std | files | failed | distinct warnings | serial compile s |")
        L.append("|---|---|---|---|---|---|")
        for r in x.get("fruit_sources", []):
            L.append(f"| {r['compiler']} | c++{r['std']} | {r['files']} | {len(r['failures'])} | {len(r['warnings'])} | {fmt_s(r['serial_compile_s'])} |")
        for r in x.get("fruit_sources", []):
            for w in r["warnings"]:
                L.append(f"- fruit sources {r['compiler']} c++{r['std']}: `{w['file']}`: {w['msg']} [{w['flag']}]")
        L.append("")
    L.append("## Header sizes (files / lines in the pinned source trees)\n")
    L.append("| tree | files | lines |")
    L.append("|---|---|---|")
    for k, v in data["headers"].items():
        L.append(f"| {k} | {v['files']} | {v['lines']} |")
    return "\n".join(L) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--deps", required=True, type=Path)
    ap.add_argument("--build", required=True, type=Path)
    ap.add_argument("--results", required=True, type=Path)
    ap.add_argument("--runs", type=int, default=5)
    ap.add_argument("--scaling-runs", type=int, default=3)
    ap.add_argument("--scaling-n", default="10,25,50,100")
    ap.add_argument("--scaling-timeout", type=int, default=300)
    ap.add_argument("--scaling-cap", default="fruit_flat=10,qlibs_di=10",
                    help="largest N per framework in the scaling phase, e.g. qlibs_di=25")
    ap.add_argument("--mem-limit-gb", type=float, default=6.0,
                    help="address-space limit for scaling compiles (0 = none)")
    ap.add_argument("--bench-iters", type=int, default=200000)
    ap.add_argument("--bench-repeats", type=int, default=7)
    ap.add_argument("--multitu-n", default="50,100")
    ap.add_argument("--only", default="matrix,sizes,diagnostics,bench,scaling",
                    help="phases to run; 'multitu' and 'extras' exist but were not exercised "
                         "in the 2026-10-30 run")
    ap.add_argument("--frameworks", default="",
                    help="comma-separated subset of frameworks (default: all)")
    args = ap.parse_args()
    deps, build, results = args.deps.resolve(), args.build.resolve(), args.results.resolve()
    SCALING_MAX_N.clear()
    for item in filter(None, args.scaling_cap.split(",")):
        fw, n = item.split("=")
        SCALING_MAX_N[fw] = int(n)
    if args.frameworks:
        keep = set(args.frameworks.split(","))
        FRAMEWORKS[:] = [f for f in FRAMEWORKS if f in keep]
        SCALING_FRAMEWORKS[:] = [f for f in SCALING_FRAMEWORKS
                                 if f in keep or (f == "fruit_flat" and "fruit" in keep)]
    results.mkdir(parents=True, exist_ok=True)
    build.mkdir(parents=True, exist_ok=True)
    logf = open(results / "measure.log", "a")

    def log(msg):
        print(msg, flush=True)
        logf.write(msg + "\n")
        logf.flush()

    json_path = results / "results.json"
    data = json.loads(json_path.read_text()) if json_path.exists() else {}
    env = env_info(deps)
    data.setdefault("env", env)  # environment of the first (main) run is kept
    data.setdefault("runs", []).append({"argv": sys.argv[1:], "date_utc": env["date_utc"],
                                        "loadavg": env["loadavg"]})
    data["headers"] = header_stats(deps)

    def save():
        json_path.write_text(json.dumps(data, indent=1))
        (results / "summary.md").write_text(summary_md(data))

    def merge_scaling(rec):
        key = (rec["framework"], rec["n"], rec.get("compiler"))
        old = [r for r in data.get("scaling", [])
               if (r["framework"], r["n"], r.get("compiler")) != key]
        data["scaling"] = old + [rec]
        save()

    global PERSIST
    PERSIST = merge_scaling
    steps = {"matrix": matrix, "sizes": sizes, "diagnostics": diagnostics, "bench": bench,
             "scaling": scaling, "multitu": multitu, "extras": extras}
    for name in args.only.split(","):
        log(f"== {name} ==")
        result = steps[name](args, deps, build, results, log)
        if name == "scaling":
            pass  # already merged record by record via PERSIST
        else:
            data["matrix" if name == "sizes" else name] = result
        save()
    log(f"results: {json_path} and {results / 'summary.md'}")


if __name__ == "__main__":
    main()
