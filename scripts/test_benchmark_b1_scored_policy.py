#!/usr/bin/env python3
from __future__ import annotations

import copy
import json
import sys
import unittest
from pathlib import Path

SCRIPTS = Path(__file__).resolve().parent
if str(SCRIPTS) not in sys.path:
    sys.path.insert(0, str(SCRIPTS))

import benchmark_b1
import benchmark_b1_scored_policy as scored


class BenchmarkB1ScoredPolicyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.repo_root = SCRIPTS.parent
        cls.policy = json.loads(
            (cls.repo_root / "benchmarks/b1/scored-cohort-v1.json").read_text(encoding="utf-8")
        )
        cls.suite = benchmark_b1.load_and_validate_suite(
            cls.repo_root / "benchmarks/b1/suite.json", cls.repo_root
        )
        cls.qualification = json.loads(
            (cls.repo_root / "benchmarks/b1/fixture-qualification.json").read_text(encoding="utf-8")
        )
        cls.profile = json.loads(
            (cls.repo_root / "benchmarks/b0/agent-profile.codex-0.144.6.json").read_text(encoding="utf-8")
        )
        cls.b0_policy = json.loads(
            (cls.repo_root / "benchmarks/b0/scored-cohort-v1.json").read_text(encoding="utf-8")
        )

    def validate(self, policy: dict) -> list[dict]:
        return scored.validate_policy_data(
            policy,
            suite=self.suite,
            qualification=self.qualification,
            profile=self.profile,
            b0_policy=self.b0_policy,
        )

    def test_committed_policy_expands_to_balanced_27_slot_schedule(self) -> None:
        schedule = self.validate(copy.deepcopy(self.policy))
        self.assertEqual(27, len(schedule))
        self.assertEqual(
            {
                "slot": 1,
                "repetition": 1,
                "task_id": "b1-sprite-normalize-repair",
                "lane_id": "godot.generic",
            },
            schedule[0],
        )
        counts: dict[tuple[str, str], int] = {}
        for item in schedule:
            key = (item["task_id"], item["lane_id"])
            counts[key] = counts.get(key, 0) + 1
        self.assertEqual(9, len(counts))
        self.assertTrue(all(value == 3 for value in counts.values()))

    def test_task_membership_cannot_change_after_preregistration(self) -> None:
        policy = copy.deepcopy(self.policy)
        policy["task_order_by_repetition"][1][0] = "b1-sprite-normalize-repair"
        with self.assertRaisesRegex(scored.ScoredPolicyError, "every frozen B1 task exactly once"):
            self.validate(policy)

    def test_lane_rotation_must_remain_identical_to_b0(self) -> None:
        policy = copy.deepcopy(self.policy)
        policy["lane_order_by_repetition"][0] = list(reversed(policy["lane_order_by_repetition"][0]))
        with self.assertRaisesRegex(scored.ScoredPolicyError, "exactly reuse"):
            self.validate(policy)

    def test_retries_and_replacements_remain_zero(self) -> None:
        policy = copy.deepcopy(self.policy)
        policy["retry_policy"]["automatic_retries_per_trial"] = 1
        with self.assertRaisesRegex(scored.ScoredPolicyError, "automatic retries"):
            self.validate(policy)

        policy = copy.deepcopy(self.policy)
        policy["retry_policy"]["replacement_trials_for_infrastructure_failure"] = 1
        with self.assertRaisesRegex(scored.ScoredPolicyError, "replacement trials"):
            self.validate(policy)

    def test_policy_refuses_post_result_preregistration_state(self) -> None:
        qualification = copy.deepcopy(self.qualification)
        qualification["scored_runs_observed"] = True
        with self.assertRaisesRegex(scored.ScoredPolicyError, "before scored B1 results"):
            scored.validate_policy_data(
                copy.deepcopy(self.policy),
                suite=self.suite,
                qualification=qualification,
                profile=self.profile,
                b0_policy=self.b0_policy,
            )

    def test_agent_profile_hash_and_budget_are_frozen(self) -> None:
        profile = copy.deepcopy(self.profile)
        profile["budget"]["max_tool_calls"] += 1
        with self.assertRaisesRegex(scored.ScoredPolicyError, "SHA-256 changed"):
            scored.validate_policy_data(
                copy.deepcopy(self.policy),
                suite=self.suite,
                qualification=self.qualification,
                profile=profile,
                b0_policy=self.b0_policy,
            )


if __name__ == "__main__":
    unittest.main()
