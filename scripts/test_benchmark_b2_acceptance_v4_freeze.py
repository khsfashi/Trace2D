#!/usr/bin/env python3
from __future__ import annotations

import copy
import unittest

from scripts import benchmark_b2_acceptance_v4_freeze as freeze


class BenchmarkB2AcceptanceV4FreezeTests(unittest.TestCase):
    def setUp(self) -> None:
        self.v3 = freeze.load_json(freeze.REPO_ROOT / freeze.V3_CONTRACT_PATH)
        self.v4 = freeze.load_json(freeze.REPO_ROOT / freeze.V4_CONTRACT_PATH)

    def test_repository_freeze_is_valid(self) -> None:
        freeze.validate_repository()

    def test_budget_cannot_be_relaxed(self) -> None:
        candidate = copy.deepcopy(self.v4)
        candidate["budget"]["max_input_tokens"] += 1
        with self.assertRaisesRegex(freeze.AcceptanceV4FreezeError, "budget"):
            freeze.validate_contract_data(candidate, self.v3)

    def test_presentation_threshold_cannot_be_relaxed(self) -> None:
        candidate = copy.deepcopy(self.v4)
        candidate["presentation_gate"]["image"]["maximum_dark_pixel_ratio"] = 0.95
        with self.assertRaisesRegex(freeze.AcceptanceV4FreezeError, "presentation_gate"):
            freeze.validate_contract_data(candidate, self.v3)

    def test_task_prompt_cannot_be_changed(self) -> None:
        candidate = copy.deepcopy(self.v4)
        candidate["task_prompt"] = "benchmarks/b2/acceptance/tasks/other/PROMPT.md"
        with self.assertRaisesRegex(freeze.AcceptanceV4FreezeError, "task_prompt"):
            freeze.validate_contract_data(candidate, self.v3)

    def test_required_game_callback_cannot_be_dropped(self) -> None:
        candidate = copy.deepcopy(self.v4)
        candidate["remediation"]["required_public_api_symbols"].remove(
            "trace2d::application::Game::OnFixedUpdate"
        )
        with self.assertRaisesRegex(freeze.AcceptanceV4FreezeError, "public API"):
            freeze.validate_contract_data(candidate, self.v3)

    def test_component_registry_discovery_cannot_be_dropped(self) -> None:
        candidate = copy.deepcopy(self.v4)
        candidate["remediation"]["required_public_api_symbols"].remove("trace2d::scene::ComponentRegistry")
        with self.assertRaisesRegex(freeze.AcceptanceV4FreezeError, "public API"):
            freeze.validate_contract_data(candidate, self.v3)

    def test_historical_v3_boundary_cannot_be_dropped(self) -> None:
        candidate = copy.deepcopy(self.v4)
        candidate["does_not_reinterpret"].pop()
        with self.assertRaisesRegex(freeze.AcceptanceV4FreezeError, "historical evidence"):
            freeze.validate_contract_data(candidate, self.v3)

    def test_bridge_mechanics_remain_visible(self) -> None:
        candidate = copy.deepcopy(self.v4)
        candidate["remediation"]["bridge_handoff_file"] = "main.cpp"
        with self.assertRaisesRegex(freeze.AcceptanceV4FreezeError, "bridge filename"):
            freeze.validate_contract_data(candidate, self.v3)


if __name__ == "__main__":
    unittest.main()
