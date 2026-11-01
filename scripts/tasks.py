#!/usr/bin/env python3
"""Manage docs/tasks.json, the Trellis feature list.

  scripts/tasks.py next [--brief]    the task to work on now
  scripts/tasks.py show ID           everything about one task
  scripts/tasks.py list [--phase N]  status of every task, or of one phase
  scripts/tasks.py verify ID         run the task's verify commands, keep the evidence
  scripts/tasks.py pass ID           mark a task passing (needs a fresh, green verify)
  scripts/tasks.py add FILE.json     append tasks (a JSON list of task objects)
  scripts/tasks.py amend ID FIELD VALUE_JSON --reason TEXT --approved-by TEXT
                                     correct a task field, with the owner's approval

Tasks are never removed, and only `passes` changes in normal work. This script is
the supported way to change the file (a hook blocks direct edits). `pass` refuses
unless `verify` succeeded on exactly the current working tree, so the evidence
always matches what gets committed. `amend` exists for owner-approved
corrections only: it records every change, with the old and new value, the
reason and the approval, in docs/task-amendments.md, and an amended task must be
verified and passed again.
"""
import argparse
import datetime
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TASKS = ROOT / "docs" / "tasks.json"
RECORDS = ROOT / ".verify"
CACHE = RECORDS / "cache"
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
    """The git tree of the working copy's content outside the bookkeeping files: the tracked files
    as they are on disk, and the untracked files that aren't ignored.

    It names content, not commits: committing a change keeps the fingerprint, and so does a commit
    that changes only docs/tasks.json or docs/PROGRESS.md (a pass, a progress entry).
    """
    with tempfile.TemporaryDirectory() as scratch:
        index = Path(scratch) / "index"
        env = {**os.environ, "GIT_INDEX_FILE": str(index)}

        def run(*args):
            return subprocess.run(["git", *args], cwd=ROOT, env=env, capture_output=True,
                                  check=True).stdout

        # A copy of the real index keeps its file stats, so `add` hashes only changed files.
        real_index = ROOT / git("rev-parse", "--git-path", "index").decode().strip()
        if real_index.is_file():
            shutil.copyfile(real_index, index)
        else:
            run("read-tree", "HEAD")
        run("add", "--all", "--", ".", *(f":(exclude){path}" for path in BOOKKEEPING))
        run("rm", "--cached", "--quiet", "--ignore-unmatch", "--", *BOOKKEEPING)
        return run("write-tree").decode().strip()


def passed_ids(tasks):
    return {t["id"] for t in tasks if t.get("passes")}


def ready(task, done):
    return not task.get("passes") and all(dep in done for dep in task["depends_on"])


def ready_tasks(tasks):
    """Every task that doesn't pass yet and whose dependencies all pass, in phase order."""
    done = passed_ids(tasks)
    order = sorted(range(len(tasks)), key=lambda i: (tasks[i]["phase"], i))
    return [tasks[i] for i in order if ready(tasks[i], done)]


def next_task(tasks):
    return next(iter(ready_tasks(tasks)), None)


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


def cache_entry(fingerprint, command):
    """Where verify keeps the result of COMMAND on the tree FINGERPRINT."""
    key = hashlib.sha256(f"{fingerprint}\0{command}".encode()).hexdigest()
    return CACHE / f"{key}.json"


def verify(task, fresh=False):
    LOGS.mkdir(parents=True, exist_ok=True)
    RECORDS.mkdir(exist_ok=True)
    CACHE.mkdir(exist_ok=True)
    fingerprint = tree_fingerprint()
    results = []
    for index, command in enumerate(task["verify"], start=1):
        # A command that already passed on this exact tree, for any task, passes again:
        # tasks often share commands such as scripts/check-full.sh, and the tree
        # fingerprint covers every file that could change the outcome. --fresh re-runs.
        cached = cache_entry(fingerprint, command)
        if not fresh and cached.exists():
            earlier = json.loads(cached.read_text())
            results.append({"command": command, "exit": 0, "seconds": earlier["seconds"],
                            "log": earlier["log"], "reused_from": earlier["task"]})
            print(f"[PASS] {command}  (passed on this tree for {earlier['task']} at {earlier['time']},"
                  f" log {earlier['log']}; --fresh re-runs it)")
            continue
        log = LOGS / f"{task['id']}-{index}.log"
        started = time.monotonic()
        proc = subprocess.run(["bash", "-c", command], cwd=ROOT, capture_output=True, text=True)
        seconds = time.monotonic() - started
        log.write_text(f"$ {command}\n{proc.stdout}{proc.stderr}")
        ok = proc.returncode == 0
        results.append({"command": command, "exit": proc.returncode, "seconds": round(seconds, 1),
                        "log": str(log.relative_to(ROOT))})
        if ok and tree_fingerprint() == fingerprint:
            cached.write_text(json.dumps({"task": task["id"], "seconds": round(seconds, 1),
                                          "log": str(log.relative_to(ROOT)),
                                          "time": datetime.datetime.now(datetime.timezone.utc)
                                          .isoformat(timespec="seconds")}) + "\n")
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


AMENDMENTS = ROOT / "docs" / "task-amendments.md"
AMENDABLE = ("title", "description", "plan_refs", "interfaces", "acceptance", "verify",
             "depends_on", "review", "phase")
LIST_FIELDS = ("plan_refs", "interfaces", "acceptance", "verify", "depends_on")


def has_cycle(tasks):
    """True if depends_on edges form a cycle (depth-first search with colours)."""
    deps = {t["id"]: t["depends_on"] for t in tasks}
    state = {}

    def visit(task_id):
        state[task_id] = "open"
        for dep in deps.get(task_id, []):
            if state.get(dep) == "open" or (dep not in state and visit(dep)):
                return True
        state[task_id] = "done"
        return False

    return any(task_id not in state and visit(task_id) for task_id in deps)


def amend(data, task_id, field, value_json, reason, approved_by):
    tasks = data["tasks"]
    task = find(tasks, task_id)
    if field not in AMENDABLE:
        sys.exit(f"'{field}' can't be amended; amendable fields: {', '.join(AMENDABLE)}")
    if not reason.strip() or not approved_by.strip():
        sys.exit("--reason and --approved-by must both be non-empty")
    try:
        value = json.loads(value_json)
    except ValueError as error:
        sys.exit(f"VALUE_JSON is not valid JSON: {error}")
    if field in LIST_FIELDS:
        if not isinstance(value, list) or not all(isinstance(item, str) for item in value):
            sys.exit(f"{field} must be a JSON list of strings")
    elif field == "phase":
        if not isinstance(value, int) or value < 0:
            sys.exit("phase must be a non-negative integer")
    elif not isinstance(value, str):
        sys.exit(f"{field} must be a JSON string")
    if field == "depends_on":
        known = {t["id"] for t in tasks}
        unknown = [dep for dep in value if dep not in known]
        if unknown or task_id in value:
            sys.exit(f"bad depends_on: unknown {unknown}" if unknown else "a task can't depend on itself")
    old = task.get(field)
    if old == value:
        sys.exit(f"{task_id}.{field} already has that value")
    task[field] = value
    if field == "depends_on" and has_cycle(tasks):
        sys.exit("that change would create a dependency cycle")
    was_passing = bool(task.get("passes"))
    task["passes"] = False
    save(data)
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d %H:%M UTC")
    if not AMENDMENTS.exists():
        AMENDMENTS.write_text("# Task amendments\n\nOwner-approved corrections to docs/tasks.json, "
                              "recorded by `scripts/tasks.py amend`. Newest last.\n")
    with AMENDMENTS.open("a") as log:
        log.write(f"\n## {stamp}: {task_id}.{field}\n\n"
                  f"- **Reason:** {reason.strip()}\n"
                  f"- **Approved by:** {approved_by.strip()}\n"
                  f"- **Old:** `{json.dumps(old, ensure_ascii=False)}`\n"
                  f"- **New:** `{json.dumps(value, ensure_ascii=False)}`\n")
    note = " It was passing and must be verified and passed again." if was_passing else ""
    print(f"{task_id}.{field} amended and recorded in docs/task-amendments.md.{note}")
    return 0


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="cmd", required=True)
    next_parser = sub.add_parser("next")
    next_parser.add_argument("--brief", action="store_true")
    next_parser.add_argument("--all", action="store_true",
                             help="list every ready task, one line each, for a parallel team")
    sub.add_parser("show").add_argument("id")
    sub.add_parser("list").add_argument("--phase", type=int)
    verify_parser = sub.add_parser("verify")
    verify_parser.add_argument("id")
    verify_parser.add_argument("--fresh", action="store_true",
                               help="re-run commands that already passed on this tree")
    sub.add_parser("pass").add_argument("id")
    sub.add_parser("add").add_argument("file")
    amend_parser = sub.add_parser("amend")
    amend_parser.add_argument("id")
    amend_parser.add_argument("field")
    amend_parser.add_argument("value_json")
    amend_parser.add_argument("--reason", required=True)
    amend_parser.add_argument("--approved-by", required=True)
    args = parser.parse_args(argv)

    data = load()
    tasks = data["tasks"]
    if args.cmd == "next":
        task = next_task(tasks)
        if task is not None and args.all:
            for each in ready_tasks(tasks):
                print(f"{each['id']} {each['title']}")
        elif task is None:
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
        return verify(find(tasks, args.id), fresh=args.fresh)
    if args.cmd == "pass":
        return mark_passing(data, find(tasks, args.id))
    if args.cmd == "amend":
        return amend(data, args.id, args.field, args.value_json, args.reason, args.approved_by)
    return add(data, args.file)


if __name__ == "__main__":
    sys.exit(main())
