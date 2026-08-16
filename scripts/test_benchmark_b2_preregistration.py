from __future__ import annotations

import copy
import json
from pathlib import Path
import unittest

from scripts import benchmark_b2_preregistration as b2


ROOT = Path(__file__).resolve().parents[1]


def load(path: str) -> dict:
    return json.loads((ROOT / path).read_text(encoding="utf-8"))


class BenchmarkB2PreregistrationTests(unittest.TestCase):
    def setUp(self) -> None:
        self.policy = load("benchmarks/b2/preregistration-v1.json")
        self.candidates = load("benchmarks/b2/baseline-candidates.json")
        self.profile = load("benchmarks/b0/agent-profile.codex-0.144.6.json")

    def validate(self, policy=None, candidates=None) -> None:
        b2.validate_data(
            self.policy if policy is None else policy,
            self.candidates if candidates is None else candidates,
            agent_profile=self.profile,
        )

    def test_frozen_contract_is_valid(self) -> None:
        self.validate()

    def test_scoring_cannot_open_before_qualification(self) -> None:
        policy = copy.deepcopy(self.policy)
        policy["scoring_gate"]["allowed"] = True
        with self.assertRaises(b2.PreregistrationError):
            self.validate(policy=policy)

    def test_input_token_budget_cannot_be_inflated(self) -> None:
        policy = copy.deepcopy(self.policy)
        policy["budget"]["max_input_tokens"] = 200000
        with self.assertRaises(b2.PreregistrationError):
            self.validate(policy=policy)

    def test_candidate_pin_drift_is_rejected(self) -> None:
        candidates = copy.deepcopy(self.candidates)
        candidates["candidates"][0]["source_commit"] = "0" * 40
        with self.assertRaises(b2.PreregistrationError):
            self.validate(candidates=candidates)

    def test_lane_specific_human_feedback_is_rejected(self) -> None:
        policy = copy.deepcopy(self.policy)
        policy["verifier_authority"]["human_review"]["same_feedback_applied_to_every_eligible_lane"] = False
        with self.assertRaises(b2.PreregistrationError):
            self.validate(policy=policy)


if __name__ == "__main__":
    unittest.main()
