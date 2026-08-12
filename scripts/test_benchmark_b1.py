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

    def test_selected_candidate_cannot_change_after_qualification(self):
        mutated = copy.deepcopy(self.contract)
        mutated["qualification"]["selected_candidate_id"] = "satelliteoflove/godot-mcp"
        with self.assertRaises(benchmark_b1.ContractError):
            benchmark_b1.validate_contract(mutated, self.repo_root)

    def test_scored_gate_requires_passed_qualification(self):
        mutated = copy.deepcopy(self.contract)
        mutated["qualification"]["status"] = "not_run"
        with self.assertRaises(benchmark_b1.ContractError):
            benchmark_b1.validate_contract(mutated, self.repo_root)

    def test_selected_candidate_must_retain_independent_evidence(self):
        mutated = copy.deepcopy(self.contract)
        mutated["candidates"][1]["qualification_result"]["known_bad_rejected"] = False
        with self.assertRaises(benchmark_b1.ContractError):
            benchmark_b1.validate_contract(mutated, self.repo_root)

    def test_satellite_failure_reason_cannot_be_erased(self):
        mutated = copy.deepcopy(self.contract)
        mutated["candidates"][0]["qualification_result"]["reason_code"] = "unknown"
        with self.assertRaises(benchmark_b1.ContractError):
            benchmark_b1.validate_contract(mutated, self.repo_root)

    def test_required_task_taxonomy_cannot_be_trimmed(self):
        mutated = copy.deepcopy(self.contract)
        mutated["required_task_classes"].pop()
        with self.assertRaises(benchmark_b1.ContractError):
            benchmark_b1.validate_contract(mutated, self.repo_root)


if __name__ == "__main__":
    unittest.main()
