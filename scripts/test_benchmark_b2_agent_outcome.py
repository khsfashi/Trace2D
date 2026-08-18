#!/usr/bin/env python3
from __future__ import annotations

import copy
import unittest

from scripts import benchmark_b2_agent_outcome as outcome


class BenchmarkB2AgentOutcomeTests(unittest.TestCase):
    def test_v4_transport_failure_precedes_downstream_verifier_error(self) -> None:
        record = {
            "status": "deterministic_failure",
            "agent_identity_ok": True,
            "agent_result": {
                "status": "tool_transport_failure",
                "wrapper": {
                    "process_return_code": 1,
                    "turn_completed": False,
                    "budget_ok": True,
                },
            },
            "deterministic_verifier": {
                "verdict": {
                    "status": "error",
                    "code": "verifier_setup_error",
                    "message": "B2Candidate.cpp not found",
                }
            },
        }
        original = copy.deepcopy(record)
        result = outcome.classify_retained_record(record)
        self.assertEqual(result["status"], outcome.TRANSPORT_FAILURE)
        self.assertEqual(result["failure_domain"], "infrastructure")
        self.assertFalse(result["deterministic_verifier_authoritative"])
        self.assertEqual(result["reason"], "agent_turn_did_not_complete")
        self.assertTrue(result["raw_record_unchanged"])
        self.assertEqual(record, original)

    def test_explicit_process_failure_is_transport_even_if_status_drifted(self) -> None:
        result = outcome.classify_agent_execution(
            agent_result={
                "status": "completed",
                "wrapper": {"process_return_code": 1, "turn_completed": True, "budget_ok": True},
            },
            agent_identity_ok=True,
        )
        self.assertEqual(result["status"], outcome.TRANSPORT_FAILURE)
        self.assertFalse(result["deterministic_verifier_authoritative"])

    def test_missing_turn_completion_is_not_invented_as_failure_for_legacy_completed_result(self) -> None:
        result = outcome.classify_agent_execution(
            agent_result={"status": "completed", "wrapper": {}},
            agent_identity_ok=True,
        )
        self.assertEqual(result["status"], outcome.COMPLETED)
        self.assertTrue(result["deterministic_verifier_authoritative"])

    def test_budget_failure_is_separate_from_transport(self) -> None:
        result = outcome.classify_agent_execution(
            agent_result={
                "status": "budget_exceeded",
                "wrapper": {"process_return_code": 0, "turn_completed": True, "budget_ok": False},
            },
            agent_identity_ok=True,
        )
        self.assertEqual(result["status"], outcome.BUDGET_FAILURE)
        self.assertEqual(result["failure_domain"], "budget")
        self.assertFalse(result["deterministic_verifier_authoritative"])

    def test_identity_failure_precedes_candidate_verification(self) -> None:
        result = outcome.classify_agent_execution(
            agent_result={
                "status": "completed",
                "wrapper": {"process_return_code": 0, "turn_completed": True, "budget_ok": True},
            },
            agent_identity_ok=False,
        )
        self.assertEqual(result["status"], outcome.IDENTITY_FAILURE)
        self.assertFalse(result["deterministic_verifier_authoritative"])

    def test_timeout_has_highest_precedence(self) -> None:
        result = outcome.classify_agent_execution(
            agent_result={
                "status": "completed",
                "wrapper": {"process_return_code": 0, "turn_completed": True, "budget_ok": True},
            },
            agent_identity_ok=True,
            process_timed_out=True,
        )
        self.assertEqual(result["status"], outcome.TIMEOUT)
        self.assertFalse(result["deterministic_verifier_authoritative"])


if __name__ == "__main__":
    unittest.main()
