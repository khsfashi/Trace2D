import copy
import importlib.util
import json
from pathlib import Path
import unittest

MODULE_PATH = Path(__file__).with_name("benchmark_b1.py")
SPEC = importlib.util.spec_from_file_location("benchmark_b1", MODULE_PATH)
benchmark_b1 = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(benchmark_b1)


class BenchmarkB1ContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.repo_root = Path(__file__).resolve().parents[1]
        with (cls.repo_root / "benchmarks/b1/baseline-qualification.json").open("r", encoding="utf-8") as handle:
            cls.contract = json.load(handle)

    def test_committed_contract_is_valid(self):
        benchmark_b1.validate_contract(copy.deepcopy(self.contract), self.repo_root)

    def test_candidate_membership_cannot_silently_change(self):
        mutated = copy.deepcopy(self.contract)
        mutated["candidates"].pop()
        with self.assertRaises(benchmark_b1.ContractError):
            benchmark_b1.validate_contract(mutated, self.repo_root)

    def test_scored_suite_stays_blocked_before_qualification(self):
        mutated = copy.deepcopy(self.contract)
        mutated["freeze_gate"]["scored_suite_allowed"] = True
        with self.assertRaises(benchmark_b1.ContractError):
            benchmark_b1.validate_contract(mutated, self.repo_root)

    def test_selection_cannot_precede_evidence(self):
        mutated = copy.deepcopy(self.contract)
        mutated["qualification"]["selected_candidate_id"] = "hi-godot/godot-ai"
        with self.assertRaises(benchmark_b1.ContractError):
            benchmark_b1.validate_contract(mutated, self.repo_root)

    def test_required_task_taxonomy_cannot_be_trimmed(self):
        mutated = copy.deepcopy(self.contract)
        mutated["required_task_classes"].pop()
        with self.assertRaises(benchmark_b1.ContractError):
            benchmark_b1.validate_contract(mutated, self.repo_root)


if __name__ == "__main__":
    unittest.main()
