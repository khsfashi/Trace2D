#!/usr/bin/env python3
from __future__ import annotations

import copy
import unittest

from scripts import benchmark_b2_acceptance_v3_freeze as freeze


class BenchmarkB2AcceptanceV3FreezeTests(unittest.TestCase):
    def setUp(self) -> None:
        self.v2 = freeze.load_json(freeze.REPO_ROOT / freeze.V2_CONTRACT_PATH)
        self.v3 = freeze.load_json(freeze.REPO_ROOT / freeze.V3_CONTRACT_PATH)

    def test_repository_freeze_is_valid(self) -> None:
        freeze.validate_repository()

    def test_budget_cannot_be_relaxed(self) -> None:
        candidate = copy.deepcopy(self.v3)
        candidate["budget"]["max_input_tokens"] += 1
        with self.assertRaisesRegex(freeze.AcceptanceV3FreezeError, "budget"):
            freeze.validate_contract_data(candidate, self.v2)

    def test_presentation_threshold_cannot_be_relaxed(self) -> None:
        candidate = copy.deepcopy(self.v3)
        candidate["presentation_gate"]["image"]["maximum_dark_pixel_ratio"] = 0.95
        with self.assertRaisesRegex(freeze.AcceptanceV3FreezeError, "presentation_gate"):
            freeze.validate_contract_data(candidate, self.v2)

    def test_task_prompt_cannot_be_changed(self) -> None:
        candidate = copy.deepcopy(self.v3)
        candidate["task_prompt"] = "benchmarks/b2/acceptance/tasks/other/PROMPT.md"
        with self.assertRaisesRegex(freeze.AcceptanceV3FreezeError, "task_prompt"):
            freeze.validate_contract_data(candidate, self.v2)

    def test_bridge_mechanics_cannot_be_hidden_again(self) -> None:
        candidate = copy.deepcopy(self.v3)
        candidate["remediation"]["bridge_handoff_file"] = "main.cpp"
        with self.assertRaisesRegex(freeze.AcceptanceV3FreezeError, "bridge filename"):
            freeze.validate_contract_data(candidate, self.v2)

    def test_historical_v2_boundary_cannot_be_dropped(self) -> None:
        candidate = copy.deepcopy(self.v3)
        candidate["does_not_reinterpret"].pop()
        with self.assertRaisesRegex(freeze.AcceptanceV3FreezeError, "historical evidence"):
            freeze.validate_contract_data(candidate, self.v2)


if __name__ == "__main__":
    unittest.main()
