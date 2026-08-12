#!/usr/bin/env python3
from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent
sys.path.insert(0, str(SCRIPT_DIR))

import project_state  # noqa: E402


class ProjectStateContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.config = json.loads(
            (REPO_ROOT / "config" / "trace2d.core-lane.json").read_text(encoding="utf-8")
        )
        cls.fixture_dir = REPO_ROOT / "tests" / "fixtures" / "project_state"

    def load_fixture(self, name: str) -> dict:
        return json.loads((self.fixture_dir / name).read_text(encoding="utf-8"))

    def test_active_draft_owner_detour_outranks_stale_core_prose(self) -> None:
        state = project_state.derive_state(self.config, self.load_fixture("active_draft.json"))

        self.assertTrue(state["live"]["available"])
        self.assertEqual(state["core"]["current"]["kind"], "owner_detour")
        self.assertEqual(state["core"]["current"]["issue"], 158)
        self.assertEqual(state["core"]["current"]["state"], "active_draft")
        self.assertEqual(state["next_action"]["kind"], "continue_pull_request")
        self.assertEqual(state["next_action"]["pull_request"], 160)

    def test_completed_detours_and_spp1_resolve_spp2_as_next_ready_stage(self) -> None:
        state = project_state.derive_state(self.config, self.load_fixture("spp2_ready.json"))

        self.assertEqual(state["core"]["current"]["kind"], "core_stage")
        self.assertEqual(state["core"]["current"]["id"], "SPP2")
        self.assertEqual(state["core"]["current"]["state"], "ready")
        self.assertIsNone(state["core"]["current"]["issue"])
        self.assertEqual(state["core"]["previous_core_stage"]["id"], "SPP1")
        self.assertEqual(state["next_action"]["kind"], "create_issue")
        self.assertEqual(state["next_action"]["id"], "SPP2")

    def test_unavailable_live_state_never_guesses_from_committed_markdown(self) -> None:
        state = project_state.unavailable_state(self.config, "offline")

        self.assertFalse(state["live"]["available"])
        self.assertIsNone(state["core"]["current"])
        self.assertEqual(state["next_action"]["kind"], "inspect_live_github")
        self.assertIn("live_github_unavailable", state["blockers"])

    def test_sa2_hardening_detour_blocks_spp2_after_first_detour_completes(self) -> None:
        snapshot = self.load_fixture("sa2_detour_ready.json")
        state = project_state.derive_state(self.config, snapshot)

        self.assertEqual(state["core"]["current"]["kind"], "owner_detour")
        self.assertEqual(state["core"]["current"]["issue"], 159)
        self.assertEqual(state["core"]["current"]["state"], "ready")
        self.assertEqual(state["next_action"]["kind"], "implement_issue")
        self.assertEqual(state["next_action"]["issue"], 159)


if __name__ == "__main__":
    unittest.main()
