#!/usr/bin/env python3
from __future__ import annotations

import copy
import unittest

from scripts import benchmark_b2_acceptance_v5_freeze as freeze


class BenchmarkB2AcceptanceV5FreezeTests(unittest.TestCase):
    def setUp(self) -> None:
        self.v4 = freeze.load_json(freeze.REPO_ROOT / freeze.V4_CONTRACT_PATH)
        self.v5 = freeze.load_json(freeze.REPO_ROOT / freeze.V5_CONTRACT_PATH)

    def test_repository_freeze_is_valid(self) -> None:
        freeze.validate_repository()

    def test_budget_cannot_be_relaxed(self) -> None:
        candidate = copy.deepcopy(self.v5)
        candidate["budget"]["max_input_tokens"] += 1
        with self.assertRaisesRegex(freeze.AcceptanceV5FreezeError, "budget"):
            freeze.validate_contract_data(candidate, self.v4)

    def test_presentation_threshold_cannot_be_relaxed(self) -> None:
        candidate = copy.deepcopy(self.v5)
        candidate["presentation_gate"]["image"]["maximum_dark_pixel_ratio"] = 0.95
        with self.assertRaisesRegex(freeze.AcceptanceV5FreezeError, "presentation_gate"):
            freeze.validate_contract_data(candidate, self.v4)

    def test_task_prompt_cannot_be_changed(self) -> None:
        candidate = copy.deepcopy(self.v5)
        candidate["task_prompt"] = "benchmarks/b2/acceptance/tasks/other/PROMPT.md"
        with self.assertRaisesRegex(freeze.AcceptanceV5FreezeError, "task_prompt"):
            freeze.validate_contract_data(candidate, self.v4)

    def test_v4_historical_boundary_cannot_be_dropped(self) -> None:
        candidate = copy.deepcopy(self.v5)
        candidate["does_not_reinterpret"].pop()
        with self.assertRaisesRegex(freeze.AcceptanceV5FreezeError, "historical evidence"):
            freeze.validate_contract_data(candidate, self.v4)

    def test_readiness_commit_and_run_are_frozen(self) -> None:
        candidate = copy.deepcopy(self.v5)
        candidate["agent_readiness"]["workflow_run_id"] += 1
        with self.assertRaisesRegex(freeze.AcceptanceV5FreezeError, "readiness run"):
            freeze.validate_contract_data(candidate, self.v4)

    def test_readiness_must_be_candidate_free(self) -> None:
        candidate = copy.deepcopy(self.v5)
        candidate["agent_readiness"]["acceptance_candidate_created"] = True
        with self.assertRaisesRegex(freeze.AcceptanceV5FreezeError, "safety guard"):
            freeze.validate_contract_data(candidate, self.v4)

    def test_external_canary_denial_cannot_be_dropped(self) -> None:
        candidate = copy.deepcopy(self.v5)
        candidate["agent_readiness"]["external_read_denied"] = False
        with self.assertRaisesRegex(freeze.AcceptanceV5FreezeError, "readiness proof"):
            freeze.validate_contract_data(candidate, self.v4)

    def test_transport_precedence_guard_cannot_be_dropped(self) -> None:
        candidate = copy.deepcopy(self.v5)
        candidate["qualification_policy"]["agent_transport_failure_must_precede_deterministic_verifier_classification"] = False
        with self.assertRaisesRegex(freeze.AcceptanceV5FreezeError, "qualification guard"):
            freeze.validate_contract_data(candidate, self.v4)

    def test_v4_immutability_cannot_be_dropped(self) -> None:
        candidate = copy.deepcopy(self.v5)
        candidate["acceptance"]["acceptance_v4_must_remain_immutable"] = False
        with self.assertRaisesRegex(freeze.AcceptanceV5FreezeError, "historical immutability"):
            freeze.validate_contract_data(candidate, self.v4)

    def test_provenance_files_are_forced_to_lf_on_all_platforms(self) -> None:
        attributes = (freeze.REPO_ROOT / ".gitattributes").read_text(encoding="utf-8")
        for required in (
            ".github/workflows/*.yml text eol=lf",
            ".github/workflows/*.yaml text eol=lf",
            "scripts/*.py text eol=lf",
        ):
            self.assertIn(required, attributes, required)


if __name__ == "__main__":
    unittest.main()
