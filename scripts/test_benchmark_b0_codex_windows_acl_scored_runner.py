#!/usr/bin/env python3
from __future__ import annotations

import ast
import json
import tempfile
import unittest
from pathlib import Path

import benchmark_b0
import run_benchmark_b0_codex_windows_acl_scored_cohort as runner


class WindowsAclScoredCohortRunnerTests(unittest.TestCase):
    def test_preregistered_schedule_is_exactly_nine_without_retry(self) -> None:
        repo = Path(__file__).resolve().parent.parent
        policy = json.loads((repo / runner.POLICY_RELATIVE).read_text(encoding="utf-8"))
        schedule = runner.policy_schedule(policy)
        self.assertEqual(
            schedule,
            [
                (1, "godot.generic"),
                (1, "godot.agent"),
                (1, "trace2d.agent"),
                (2, "godot.agent"),
                (2, "trace2d.agent"),
                (2, "godot.generic"),
                (3, "trace2d.agent"),
                (3, "godot.generic"),
                (3, "godot.agent"),
            ],
        )
        self.assertEqual(policy["state"], "ready")
        self.assertEqual(policy["total_planned_trials"], 9)
        self.assertEqual(policy["retry_policy"]["automatic_retries_per_trial"], 0)
        self.assertEqual(policy["retry_policy"]["replacement_trials_for_infrastructure_failure"], 0)
        self.assertFalse(policy["retry_policy"]["early_stopping"])
        self.assertFalse(policy["reporting_policy"]["best_of_n"])

    def test_suite_task_and_profile_are_eligible_and_frozen(self) -> None:
        repo = Path(__file__).resolve().parent.parent
        suite = benchmark_b0.validate_suite(repo / "benchmarks/b0/suite.json")
        task = benchmark_b0.find_task(suite, runner.TASK_ID)
        profile = json.loads((repo / runner.PROFILE_RELATIVE).read_text(encoding="utf-8"))
        policy = json.loads((repo / runner.POLICY_RELATIVE).read_text(encoding="utf-8"))
        acceptance = json.loads((repo / runner.ACCEPTANCE_RELATIVE).read_text(encoding="utf-8"))
        schedule = runner.validate_frozen_contract(
            repo_root=repo,
            suite=suite,
            policy=policy,
            acceptance=acceptance,
            profile=profile,
        )
        self.assertEqual(suite["state"], "eligible")
        self.assertEqual(task["state"], "eligible")
        self.assertEqual(len(schedule), 9)
        self.assertEqual(
            benchmark_b0.sha256_json(profile),
            "2407c4feccc334ab92f871fc5a870ae745713e37ccb3f4406fe4dca9d4f11708",
        )
        self.assertEqual(profile["budget"]["max_input_tokens"], 100000)

    def test_policy_rejects_replacement_retry(self) -> None:
        repo = Path(__file__).resolve().parent.parent
        policy = json.loads((repo / runner.POLICY_RELATIVE).read_text(encoding="utf-8"))
        policy["retry_policy"]["replacement_trials_for_infrastructure_failure"] = 1
        with self.assertRaises(runner.ScoredCohortError):
            runner.policy_schedule(policy)

    def test_accepted_run_is_resolved_from_committed_archive_name(self) -> None:
        with tempfile.TemporaryDirectory() as root_text:
            root = Path(root_text)
            acceptance = {"source_archive": "codex-chatgpt-calibration-example.zip"}
            expected = root / "codex-chatgpt-calibration-example"
            expected.mkdir()
            self.assertEqual(runner.accepted_local_run_root(root, acceptance), expected)

    def test_runner_contains_no_json_style_boolean_or_null_identifiers(self) -> None:
        source_path = Path(runner.__file__).resolve()
        tree = ast.parse(source_path.read_text(encoding="utf-8"), filename=str(source_path))
        forbidden = sorted(
            {
                node.id
                for node in ast.walk(tree)
                if isinstance(node, ast.Name) and node.id in {"true", "false", "null"}
            }
        )
        self.assertEqual(forbidden, [])


if __name__ == "__main__":
    unittest.main()
