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
        cls.contract = json.loads((cls.repo_root / "benchmarks/b1/baseline-qualification.json").read_text(encoding="utf-8"))
        cls.suite = json.loads((cls.repo_root / "benchmarks/b1/suite.json").read_text(encoding="utf-8"))
        cls.verifiers = json.loads((cls.repo_root / "benchmarks/b1/verifiers.json").read_text(encoding="utf-8"))

    def test_committed_contract_is_valid(self):
        benchmark_b1.validate_contract(copy.deepcopy(self.contract), self.repo_root)

    def test_committed_frozen_suite_is_valid(self):
        benchmark_b1.validate_suite(
            copy.deepcopy(self.suite),
            copy.deepcopy(self.contract),
            copy.deepcopy(self.verifiers),
            self.repo_root,
        )

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

    def test_suite_task_membership_and_order_are_frozen(self):
        mutated = copy.deepcopy(self.suite)
        mutated["tasks"].reverse()
        with self.assertRaises(benchmark_b1.ContractError):
            benchmark_b1.validate_suite(mutated, self.contract, self.verifiers, self.repo_root)

    def test_suite_budget_cannot_drift_from_frozen_agent_profile(self):
        mutated = copy.deepcopy(self.suite)
        mutated["tasks"][0]["budget"]["max_tool_calls"] += 1
        with self.assertRaises(benchmark_b1.ContractError):
            benchmark_b1.validate_suite(mutated, self.contract, self.verifiers, self.repo_root)

    def test_godot_generic_and_agent_receive_identical_fixture(self):
        mutated = copy.deepcopy(self.suite)
        mutated["tasks"][0]["lanes"]["godot.agent"]["known_good"] = mutated["tasks"][0]["lanes"]["trace2d.agent"]["known_good"]
        with self.assertRaises(benchmark_b1.ContractError):
            benchmark_b1.validate_suite(mutated, self.contract, self.verifiers, self.repo_root)

    def test_selected_godot_agent_pin_cannot_move_after_suite_freeze(self):
        mutated = copy.deepcopy(self.suite)
        mutated["frozen_source"]["godot_agent"]["pin"] = "v3.0.7@different"
        with self.assertRaises(benchmark_b1.ContractError):
            benchmark_b1.validate_suite(mutated, self.contract, self.verifiers, self.repo_root)

    def test_fixture_qualification_gate_path_is_frozen(self):
        mutated = copy.deepcopy(self.suite)
        mutated["scoring_gate"]["qualification_evidence"] = "benchmarks/b1/other.json"
        with self.assertRaises(benchmark_b1.ContractError):
            benchmark_b1.validate_suite(mutated, self.contract, self.verifiers, self.repo_root)

    def test_task_class_coverage_cannot_be_reordered(self):
        mutated = copy.deepcopy(self.suite)
        mutated["tasks"][1]["task_classes"].reverse()
        with self.assertRaises(benchmark_b1.ContractError):
            benchmark_b1.validate_suite(mutated, self.contract, self.verifiers, self.repo_root)

    def test_verifier_membership_is_frozen(self):
        mutated = copy.deepcopy(self.verifiers)
        mutated["verifiers"].pop()
        with self.assertRaises(benchmark_b1.ContractError):
            benchmark_b1.validate_verifier_registry(mutated)

    def test_verifier_qualification_requirement_cannot_be_removed(self):
        mutated = copy.deepcopy(self.verifiers)
        mutated["verifiers"][0]["qualification_required"] = False
        with self.assertRaises(benchmark_b1.ContractError):
            benchmark_b1.validate_verifier_registry(mutated)


if __name__ == "__main__":
    unittest.main()
