#!/usr/bin/env python3
"""Manage docs/tasks.json, the Trellis feature list.

  scripts/tasks.py next [--brief]    the task to work on now
  scripts/tasks.py show ID           everything about one task
  scripts/tasks.py list [--phase N]  status of every task, or of one phase
  scripts/tasks.py verify ID         run the task's verify commands, keep the evidence
  scripts/tasks.py pass ID           mark a task passing (needs a fresh, green verify)
  scripts/tasks.py add FILE.json     append tasks (a JSON list of task objects)

Tasks are never removed or reworded; only `passes` changes. This script is the
supported way to change the file (a hook blocks direct edits). `pass` refuses
unless `verify` succeeded on exactly the current working tree, so the evidence
always matches what gets committed.
"""
import argparse
import datetime
import hashlib
import json
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TASKS = ROOT / "docs" / "tasks.json"
RECORDS = ROOT / ".verify"
LOGS = ROOT / "build" / "logs" / "verify"
REQUIRED = ("id", "phase", "title", "description", "acceptance", "verify", "depends_on")
# Files that `pass` and the progress log change; excluded from the tree fingerprint.
BOOKKEEPING = ("docs/tasks.json", "docs/PROGRESS.md")


def load():
    return json.loads(TASKS.read_text())


def save(data):
    TASKS.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n")


def git(*args):
    return subprocess.run(["git", *args], cwd=ROOT, capture_output=True, check=True).stdout


def tree_fingerprint():
    """Hash of HEAD plus every uncommitted change outside the bookkeeping files."""
    excludes = [f":(exclude){path}" for path in BOOKKEEPING]
    h = hashlib.sha256()
    h.update(git("rev-parse", "HEAD"))
    h.update(git("diff", "HEAD", "--binary", "--", ".", *excludes))
    untracked = git("ls-files", "--others", "--exclude-standard", "-z").decode().split("\0")
    for name in sorted(n for n in untracked if n and n not in BOOKKEEPING):
        h.update(name.encode())
        path = ROOT / name
        if path.is_file():
            h.update(path.read_bytes())
    return h.hexdigest()


def passed_ids(tasks):
    return {t["id"] for t in tasks if t.get("passes")}


def ready(task, done):
    return not task.get("passes") and all(dep in done for dep in task["depends_on"])


def next_task(tasks):
    done = passed_ids(tasks)
    order = sorted(range(len(tasks)), key=lambda i: (tasks[i]["phase"], i))
    return next((tasks[i] for i in order if ready(tasks[i], done)), None)


def find(tasks, task_id):
    for task in tasks:
        if task["id"] == task_id:
            return task
    sys.exit(f"no task {task_id}")


def show(task):
    status = "PASS" if task.get("passes") else "todo"
    print(f"{task['id']} (phase {task['phase']}) {task['title']}  [{status}]")
    print(f"\n{task['description']}\n")
    if task.get("plan_refs"):
        print("Plan refs: " + ", ".join(task["plan_refs"]))
    if task.get("interfaces"):
        print("Interfaces:")
        for item in task["interfaces"]:
            print(f"  - {item}")
    print("Acceptance:")
    for item in task["acceptance"]:
        print(f"  - {item}")
    print("Verify:")
    for command in task["verify"]:
        print(f"  $ {command}")
    if task["depends_on"]:
        print("Depends on: " + ", ".join(task["depends_on"]))
    if task.get("review") == "owner":
        print("Needs owner sign-off before the result is final.")


def first_error(text):
    for line in text.splitlines():
        if "error" in line.lower() or "FAIL" in line:
            return line.strip()[:200]
    lines = [line for line in text.splitlines() if line.strip()]
    return lines[-1].strip()[:200] if lines else ""


def verify(task):
    LOGS.mkdir(parents=True, exist_ok=True)
    RECORDS.mkdir(exist_ok=True)
    fingerprint = tree_fingerprint()
    results = []
    for index, command in enumerate(task["verify"], start=1):
        log = LOGS / f"{task['id']}-{index}.log"
        started = time.monotonic()
        proc = subprocess.run(["bash", "-c", command], cwd=ROOT, capture_output=True, text=True)
        seconds = time.monotonic() - started
        log.write_text(f"$ {command}\n{proc.stdout}{proc.stderr}")
        ok = proc.returncode == 0
        results.append({"command": command, "exit": proc.returncode, "seconds": round(seconds, 1),
                        "log": str(log.relative_to(ROOT))})
        line = f"[{'PASS' if ok else 'FAIL'}] {command}  ({seconds:.1f} s, log {log.relative_to(ROOT)})"
        if not ok:
            line += f"\n       exit {proc.returncode}: {first_error(proc.stdout + proc.stderr)}"
        print(line)
    all_passed = all(r["exit"] == 0 for r in results)
    if tree_fingerprint() != fingerprint:
        print("note: the verify commands changed the working tree; run verify again before `pass`.")
        all_passed = False
    record = {"task": task["id"], "fingerprint": fingerprint, "all_passed": all_passed,
              "time": datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds"),
              "results": results}
    (RECORDS / f"{task['id']}.json").write_text(json.dumps(record, indent=2) + "\n")
    print(f"{task['id']}: {'all verify commands passed' if all_passed else 'verification FAILED'}")
    return 0 if all_passed else 1


def mark_passing(data, task):
    tasks = data["tasks"]
    missing = [dep for dep in task["depends_on"] if dep not in passed_ids(tasks)]
    if missing:
        sys.exit(f"{task['id']} depends on tasks that don't pass yet: {', '.join(missing)}")
    record_file = RECORDS / f"{task['id']}.json"
    if not record_file.exists():
        sys.exit(f"run `scripts/tasks.py verify {task['id']}` first")
    record = json.loads(record_file.read_text())
    if not record.get("all_passed"):
        sys.exit(f"the last verify of {task['id']} failed; fix it and verify again")
    if record.get("fingerprint") != tree_fingerprint():
        sys.exit(f"files changed since the last verify of {task['id']}; verify again")
    task["passes"] = True
    save(data)
    print(f"{task['id']} marked passing (verified {record['time']})")
    if task.get("review") == "owner":
        print("Reminder: this task needs owner sign-off; note it under 'Waiting for owner' in docs/PROGRESS.md.")
    return 0


def add(data, path):
    new_tasks = json.loads(Path(path).read_text())
    tasks = data["tasks"]
    ids = {t["id"] for t in tasks}
    for task in new_tasks:
        for field in REQUIRED:
            if field not in task:
                sys.exit(f"task {task.get('id', '?')} lacks '{field}'")
        if task["id"] in ids:
            sys.exit(f"duplicate task id {task['id']}")
        unknown = [dep for dep in task["depends_on"] if dep not in ids]
        if unknown:
            sys.exit(f"task {task['id']} depends on unknown tasks: {', '.join(unknown)}")
        task["passes"] = False
        tasks.append(task)
        ids.add(task["id"])
    save(data)
    print(f"added {len(new_tasks)} task(s)")
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="cmd", required=True)
    sub.add_parser("next").add_argument("--brief", action="store_true")
    sub.add_parser("show").add_argument("id")
    sub.add_parser("list").add_argument("--phase", type=int)
    sub.add_parser("verify").add_argument("id")
    sub.add_parser("pass").add_argument("id")
    sub.add_parser("add").add_argument("file")
    args = parser.parse_args(argv)

    data = load()
    tasks = data["tasks"]
    if args.cmd == "next":
        task = next_task(tasks)
        if task is None:
            print("All tasks pass." if all(t.get("passes") for t in tasks) else "No task is ready; check depends_on.")
        elif args.brief:
            print(f"{task['id']} {task['title']}")
        else:
            show(task)
        return 0
    if args.cmd == "show":
        show(find(tasks, args.id))
        return 0
    if args.cmd == "list":
        for task in tasks:
            if args.phase is None or task["phase"] == args.phase:
                status = "PASS" if task.get("passes") else "todo"
                owner = "  (owner review)" if task.get("review") == "owner" else ""
                print(f"{status}  {task['id']:<6} {task['title']}{owner}")
        return 0
    if args.cmd == "verify":
        return verify(find(tasks, args.id))
    if args.cmd == "pass":
        return mark_passing(data, find(tasks, args.id))
    return add(data, args.file)


if __name__ == "__main__":
    sys.exit(main())
