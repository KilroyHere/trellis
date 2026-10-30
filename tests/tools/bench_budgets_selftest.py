#!/usr/bin/env python3
"""Self-test of the budget checker of benchmarks/compile/run.py, on synthetic records.

It loads the real benchmarks/compile/budgets.json, so a malformed budgets file fails here, and
checks how run.py judges variants against the hand-written baseline: values are compared as they
are printed (2 decimals), a size without a checkpoint isn't checked, a failed program or a median
of too few compiles misses its budget, and only the programs a budget covers get every compile.
CTest runs this as tools.bench_budgets_selftest.
"""
import copy
import importlib.util
import sys
import unittest
from pathlib import Path

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parent.parent.parent
_spec = importlib.util.spec_from_file_location("bench_run", ROOT / "benchmarks" / "compile" / "run.py")
run = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(run)

BUDGETS = run.load_budgets()


def record(variant, seconds, rss=100.0, program="graph", n=100, fan_in="1-3", compiler="gcc-13",
           runs=3):
    r = {"program": program, "variant": variant, "compiler": compiler,
         "compile_s": {"median": seconds, "min": seconds, "max": seconds, "runs": runs},
         "peak_rss_mb": rss, "output_ok": True}
    if program == "graph":
        r.update(n=n, fan_in=fan_in, forms="shared_ptr", seed=12345)
    return r


def checks_of(*records, budget_id=None):
    found = run.check_budgets(list(records), BUDGETS)
    return [c for c in found if budget_id is None or c["budget"] == budget_id]


class BudgetsFileTest(unittest.TestCase):
    def test_the_file_names_the_three_targets(self):
        self.assertEqual([b["id"] for b in BUDGETS["budgets"]], ["NFR-03", "NFR-04", "NFR-05"])
        self.assertEqual(BUDGETS["baseline"], "manual")

    def test_malformed_budgets_are_rejected(self):
        broken = []
        data = copy.deepcopy(BUDGETS)
        del data["runs"]
        broken.append(data)
        data = copy.deepcopy(BUDGETS)
        data["budgets"][1]["checkpoints"][0] = {"n": "100", "max_ratio": 1.3}
        broken.append(data)
        data = copy.deepcopy(BUDGETS)
        data["budgets"][2]["measure"] = "seconds"
        broken.append(data)
        data = copy.deepcopy(BUDGETS)
        del data["budgets"][0]["max_extra"]
        broken.append(data)
        for data in broken:
            with self.assertRaises(ValueError):
                run.parse_budgets(data)


class CheckBudgetsTest(unittest.TestCase):
    def test_a_ratio_is_compared_as_printed(self):
        # 9.88 / 7.6 is 1.3000000000000003 in binary floating point; printed, it is 1.30.
        [check] = checks_of(record("manual", 7.6), record("trellis", 9.88), budget_id="NFR-04")
        self.assertEqual(check["value"], 1.3)
        self.assertTrue(check["ok"])
        self.assertIn("1.30x (max 1.3x)", run.describe_check(check))
        self.assertTrue(run.describe_check(check).startswith("ok"))

    def test_a_ratio_above_the_limit_misses(self):
        [check] = checks_of(record("manual", 10.0), record("trellis", 13.1), budget_id="NFR-04")
        self.assertEqual(check["value"], 1.31)
        self.assertFalse(check["ok"])
        self.assertTrue(run.describe_check(check).startswith("MISS"))

    def test_the_scenario_budget_is_a_difference(self):
        # 1.50 - 1.20 is 0.30000000000000004 in binary floating point.
        [check] = checks_of(record("manual", 1.20, program="scenario"),
                            record("trellis", 1.50, program="scenario"))
        self.assertEqual((check["budget"], check["kind"], check["value"]), ("NFR-03", "extra", 0.3))
        self.assertTrue(check["ok"])
        [check] = checks_of(record("manual", 1.20, program="scenario"),
                            record("trellis", 1.51, program="scenario"))
        self.assertFalse(check["ok"])

    def test_memory_is_checked_at_100_components(self):
        [check] = checks_of(record("manual", 7.0, rss=200.0), record("trellis", 7.0, rss=320.0),
                            budget_id="NFR-05")
        self.assertEqual(check["value"], 1.6)
        self.assertFalse(check["ok"])

    def test_the_1000_component_checkpoint_has_its_own_limit(self):
        [check] = checks_of(record("manual", 100.0, n=1000), record("trellis", 149.0, n=1000))
        self.assertEqual((check["budget"], check["limit"], check["ok"]), ("NFR-04", 1.5, True))

    def test_sizes_without_a_checkpoint_are_not_checked(self):
        self.assertEqual(checks_of(record("manual", 3.0, n=50), record("trellis", 9.0, n=50)), [])

    def test_every_fan_in_profile_is_checked_against_its_own_baseline(self):
        found = checks_of(record("manual", 10.0), record("manual", 20.0, fan_in="10"),
                          record("trellis", 12.0), record("trellis", 30.0, fan_in="10"),
                          budget_id="NFR-04")
        self.assertEqual(sorted((c["fan_in"], c["value"], c["ok"]) for c in found),
                         [("1-3", 1.2, True), ("10", 1.5, False)])

    def test_each_compiler_is_checked_on_its_own(self):
        found = checks_of(record("manual", 10.0), record("manual", 5.0, compiler="clang-18"),
                          record("trellis", 12.0), record("trellis", 7.0, compiler="clang-18"),
                          budget_id="NFR-04")
        self.assertEqual(sorted((c["compiler"], c["value"]) for c in found),
                         [("clang-18", 1.4), ("gcc-13", 1.2)])

    def test_a_failed_program_misses_its_budgets(self):
        failed = {"program": "graph", "variant": "trellis", "compiler": "gcc-13", "n": 100,
                  "fan_in": "1-3", "forms": "shared_ptr", "seed": 12345,
                  "error": "compile: timed out after 300 s"}
        found = checks_of(record("manual", 10.0), failed)
        self.assertEqual(sorted(c["budget"] for c in found), ["NFR-04", "NFR-05"])
        self.assertTrue(all(not c["ok"] and "not measured" in c["note"] for c in found))

    def test_a_missing_baseline_misses(self):
        [check] = checks_of(record("trellis", 10.0), budget_id="NFR-04")
        self.assertFalse(check["ok"])

    def test_a_median_of_fewer_compiles_than_runs_misses(self):
        [check] = checks_of(record("manual", 10.0), record("trellis", 10.0, runs=1),
                            budget_id="NFR-04")
        self.assertFalse(check["ok"])
        self.assertIn("median of 3", check["note"])
        # Memory is the largest over the compiles, so one compile measures it.
        [check] = checks_of(record("manual", 10.0), record("trellis", 10.0, runs=1),
                            budget_id="NFR-05")
        self.assertTrue(check["ok"])

    def test_the_baseline_alone_has_nothing_to_check(self):
        self.assertEqual(checks_of(record("manual", 10.0), record("manual", 1.0, program="scenario")),
                         [])


class CoversTest(unittest.TestCase):
    def test_only_programs_under_a_budget_get_every_compile(self):
        self.assertTrue(run.covers(BUDGETS, {"program": "scenario"}))
        self.assertTrue(run.covers(BUDGETS, {"program": "graph", "n": 100}))
        self.assertTrue(run.covers(BUDGETS, {"program": "graph", "n": 1000}))
        self.assertFalse(run.covers(BUDGETS, {"program": "graph", "n": 500}))


if __name__ == "__main__":
    unittest.main(verbosity=2)
