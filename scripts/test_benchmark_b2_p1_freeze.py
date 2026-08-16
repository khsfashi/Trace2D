from __future__ import annotations

import copy
import json
from pathlib import Path
import unittest

from scripts import benchmark_b2_p1_freeze as b2


ROOT = Path(__file__).resolve().parents[1]


def load(path: str) -> dict:
    return json.loads((ROOT / path).read_text(encoding="utf-8"))


class BenchmarkB2P1FreezeTests(unittest.TestCase):
    def setUp(self) -> None:
        self.policy = load("benchmarks/b2/preregistration-v1.json")
        self.candidates = load("benchmarks/b2/baseline-candidates.json")
        self.cohort = load("benchmarks/b2/scored-cohort-v1.json")
        self.verifier_qualification = load("benchmarks/b2/verifier-qualification-v1.json")
        self.profile = load("benchmarks/b0/agent-profile.codex-0.144.6.json")

    def validate(self, policy=None, candidates=None, cohort=None, verifier_qualification=None) -> None:
        b2.validate_data(
            self.policy if policy is None else policy,
            self.candidates if candidates is None else candidates,
            self.cohort if cohort is None else cohort,
            self.verifier_qualification if verifier_qualification is None else verifier_qualification,
            agent_profile=self.profile,
        )

    def test_frozen_contract_is_valid_with_scoring_open(self) -> None:
        self.validate()

    def test_scoring_gate_cannot_claim_results_were_already_observed(self) -> None:
        policy = copy.deepcopy(self.policy)
        policy["scoring_gate"]["scored_results_observed"] = True
        with self.assertRaises(ValueError):
            self.validate(policy=policy)

    def test_trace2d_verifier_artifact_digest_cannot_drift(self) -> None:
        evidence = copy.deepcopy(self.verifier_qualification)
        evidence["trace2d"]["artifact_sha256"] = "0" * 64
        with self.assertRaises(ValueError):
            self.validate(verifier_qualification=evidence)

    def test_godot_known_bad_rejection_cannot_be_removed(self) -> None:
        evidence = copy.deepcopy(self.verifier_qualification)
        evidence["godot"]["known_bad"]["rejected"] = False
        with self.assertRaises(ValueError):
            self.validate(verifier_qualification=evidence)

    def test_selected_package_identity_cannot_drift(self) -> None:
        candidates = copy.deepcopy(self.candidates)
        candidates["qualification"]["selected_pin"]["package_identity"] = "sha256:" + "0" * 64
        with self.assertRaises(ValueError):
            self.validate(candidates=candidates)

    def test_lane_rotation_cannot_change(self) -> None:
        cohort = copy.deepcopy(self.cohort)
        cohort["slots"][0], cohort["slots"][1] = cohort["slots"][1], cohort["slots"][0]
        with self.assertRaises(ValueError):
            self.validate(cohort=cohort)

    def test_token_budget_cannot_expand(self) -> None:
        cohort = copy.deepcopy(self.cohort)
        cohort["budget"]["max_input_tokens"] = 200000
        with self.assertRaises(ValueError):
            self.validate(cohort=cohort)


if __name__ == "__main__":
    unittest.main()
