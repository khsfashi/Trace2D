#!/usr/bin/env python3
from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

SCRIPTS = Path(__file__).resolve().parent
if str(SCRIPTS) not in sys.path:
    sys.path.insert(0, str(SCRIPTS))

import benchmark_b1_scored_harness as harness


class BenchmarkB1ScoredHarnessTests(unittest.TestCase):
    def process(self, *, return_code: int | None = 0, timed_out: bool = False) -> dict:
        return {
            "return_code": return_code,
            "timed_out": timed_out,
            "duration_ms": 1.0,
            "stdout": "",
            "stderr": "",
            "argv": [],
        }

    def agent(self, status: str = "completed", humans: int = 0) -> dict:
        return {
            "status": status,
            "human_interventions": humans,
            "metrics": {},
        }

    def verifier(self, status: str = "pass") -> dict:
        return {"verdict": {"status": status}}

    def test_success_requires_integrity_identity_agent_and_verifier(self) -> None:
        status, domain = harness._classify(
            process=self.process(),
            agent_result=self.agent(),
            verifier=self.verifier(),
            integrity_ok=True,
            identity_ok=True,
        )
        self.assertEqual(("success", "success"), (status, domain))

    def test_budget_exceeded_precedes_candidate_verdict(self) -> None:
        status, domain = harness._classify(
            process=self.process(return_code=1),
            agent_result=self.agent("budget_exceeded"),
            verifier=self.verifier("pass"),
            integrity_ok=True,
            identity_ok=True,
        )
        self.assertEqual(("budget_exceeded", "implementation"), (status, domain))

    def test_verifier_rejection_is_implementation_failure(self) -> None:
        status, domain = harness._classify(
            process=self.process(),
            agent_result=self.agent(),
            verifier=self.verifier("fail"),
            integrity_ok=True,
            identity_ok=True,
        )
        self.assertEqual(("engine_build_test_failure", "implementation"), (status, domain))

    def test_verifier_error_is_infrastructure_failure(self) -> None:
        status, domain = harness._classify(
            process=self.process(),
            agent_result=self.agent(),
            verifier=self.verifier("error"),
            integrity_ok=True,
            identity_ok=True,
        )
        self.assertEqual(("verifier_failure", "infrastructure"), (status, domain))

    def test_integrity_failure_has_highest_precedence(self) -> None:
        status, domain = harness._classify(
            process=self.process(timed_out=True, return_code=None),
            agent_result=None,
            verifier=None,
            integrity_ok=False,
            identity_ok=False,
        )
        self.assertEqual(("benchmark_integrity_failure", "integrity"), (status, domain))

    def test_agent_command_records_b1_adapter_not_task_specific_answer_api(self) -> None:
        command = harness._agent_command(
            workspace=Path("workspace"),
            prompt=Path("prompt.md"),
            lane_id="godot.agent",
            result_file=Path("agent-result.json"),
        )
        self.assertEqual("-m", command[1])
        self.assertEqual(harness.EXECUTION_ADAPTER_MODULE, command[2])
        self.assertIn("godot.agent", command)
        joined = " ".join(command)
        self.assertNotIn("known_good", joined)
        self.assertNotIn("known_bad", joined)
        self.assertNotIn("verifier", joined.lower())

    def test_protected_snapshot_covers_frozen_manifest_and_scored_infrastructure(self) -> None:
        snapshot = harness._protected_snapshot(SCRIPTS.parent)
        names = {Path(path).name for path in snapshot}
        self.assertIn("freeze-manifest.json", names)
        self.assertIn("scored-cohort-v1.json", names)
        self.assertIn("godot-ai-python-freeze.txt", names)
        self.assertIn("benchmark_b1_scored_harness.py", names)
        self.assertIn("benchmark_b1_codex_windows_acl_wrapper.py", names)
        self.assertIn("verify_benchmark_b1_candidate.py", names)
        self.assertTrue(harness._protected_unchanged(snapshot))


if __name__ == "__main__":
    unittest.main()
