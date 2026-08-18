from __future__ import annotations

import argparse
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from scripts import benchmark_b2_acceptance_v5 as v5
from scripts import benchmark_b2_agent_outcome as outcome


class BenchmarkB2AcceptanceV5Tests(unittest.TestCase):
    def test_contract_is_frozen_and_keeps_v4_gate_budget(self) -> None:
        contract = v5.validate_contract()
        self.assertEqual(contract["acceptance_version"], 5)
        self.assertEqual(contract["initial_runs"], 2)
        self.assertEqual(contract["budget"]["max_input_tokens"], 100000)
        self.assertTrue(contract["budget"]["budget_is_recorded_not_deterministic_gameplay_authority"])
        self.assertEqual(contract["presentation_gate"]["image"]["maximum_dark_pixel_ratio"], 0.9)
        self.assertEqual(contract["isolation"]["durable_root_name"], "benchmark-b2-acceptance-v5")
        self.assertTrue(
            contract["qualification_policy"][
                "agent_transport_failure_must_precede_deterministic_verifier_classification"
            ]
        )

    def test_root_accepts_only_v5_and_rejects_all_historical_roots(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            accepted = v5.require_acceptance_root(str(base / "benchmark-b2-acceptance-v5"))
            self.assertTrue(str(accepted).endswith("benchmark-b2-acceptance-v5"))
            for historical in (
                "benchmark-b2-scored-v1",
                "benchmark-b2-acceptance-v1",
                "benchmark-b2-acceptance-v2",
                "benchmark-b2-acceptance-v3",
                "benchmark-b2-acceptance-v4",
            ):
                with self.assertRaises(v5.AcceptanceV5Error):
                    v5.require_acceptance_root(str(base / historical / "benchmark-b2-acceptance-v5"))

    def test_consumed_v5_root_is_rejected_before_agent_execution(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "benchmark-b2-acceptance-v5"
            root.mkdir()
            (root / "sentinel.txt").write_text("consumed\n", encoding="utf-8")
            args = argparse.Namespace(runs_root=str(root))
            with self.assertRaisesRegex(v5.AcceptanceV5Error, "already consumed"):
                v5.start(args)

    def test_true_transport_failure_is_non_authoritative(self) -> None:
        result = {
            "status": "tool_transport_failure",
            "wrapper": {
                "process_return_code": 1,
                "turn_completed": False,
                "budget_ok": True,
            },
        }
        classified = v5._classify_agent_execution({"timed_out": False}, result, True)
        self.assertEqual(classified["status"], outcome.TRANSPORT_FAILURE)
        self.assertFalse(classified["deterministic_verifier_authoritative"])
        self.assertEqual(
            v5._initial_status(classified, None, v5._skipped_gate("transport"), True),
            "agent_transport_failure",
        )

    def test_timeout_is_non_authoritative_even_if_partial_result_exists(self) -> None:
        result = {
            "status": "completed",
            "wrapper": {"process_return_code": 0, "turn_completed": True, "budget_ok": True},
        }
        classified = v5._classify_agent_execution({"timed_out": True}, result, True)
        self.assertEqual(classified["status"], outcome.TIMEOUT)
        self.assertFalse(classified["deterministic_verifier_authoritative"])

    def test_completed_budget_only_overage_preserves_v4_gameplay_authority(self) -> None:
        result = {
            "status": "tool_transport_failure",
            "wrapper": {
                "process_return_code": 0,
                "turn_completed": True,
                "budget_ok": False,
            },
        }
        classified = v5._classify_agent_execution({"timed_out": False}, result, True)
        self.assertEqual(classified["status"], v5.COMPLETED_OVER_BUDGET)
        self.assertTrue(classified["deterministic_verifier_authoritative"])
        self.assertEqual(classified["failure_domain"], "budget")
        self.assertEqual(classified["base_classifier_status"], outcome.TRANSPORT_FAILURE)

    def test_budget_override_requires_explicit_completed_turn(self) -> None:
        for wrapper in (
            {"process_return_code": 1, "turn_completed": True, "budget_ok": False},
            {"process_return_code": 0, "turn_completed": False, "budget_ok": False},
            {"process_return_code": 0, "turn_completed": None, "budget_ok": False},
        ):
            result = {"status": "tool_transport_failure", "wrapper": wrapper}
            classified = v5._classify_agent_execution({"timed_out": False}, result, True)
            self.assertFalse(classified["deterministic_verifier_authoritative"])
            self.assertNotEqual(classified["status"], v5.COMPLETED_OVER_BUDGET)

    def test_identity_failure_remains_non_authoritative(self) -> None:
        result = {
            "status": "completed",
            "wrapper": {"process_return_code": 0, "turn_completed": True, "budget_ok": True},
        }
        classified = v5._classify_agent_execution({"timed_out": False}, result, False)
        self.assertEqual(classified["status"], outcome.IDENTITY_FAILURE)
        self.assertFalse(classified["deterministic_verifier_authoritative"])

    def test_machine_evidence_does_not_invoke_verifier_after_transport_failure(self) -> None:
        classified = {
            "status": outcome.TRANSPORT_FAILURE,
            "deterministic_verifier_authoritative": False,
        }
        with tempfile.TemporaryDirectory() as temporary, mock.patch.object(
            v5.v1, "run_verifier", side_effect=AssertionError("verifier must stay skipped")
        ), mock.patch.object(
            v5.v2, "presentation_gate", side_effect=AssertionError("presentation gate must stay skipped")
        ):
            process, verifier, gate = v5._machine_evidence(
                workspace=Path(temporary),
                verifier_result_path=Path(temporary) / "verifier.json",
                build_slot=999,
                contract=v5.validate_contract(),
                agent_execution=classified,
            )
        self.assertTrue(process["skipped"])
        self.assertIsNone(verifier)
        self.assertTrue(gate["skipped"])
        self.assertFalse(gate["passed"])

    def test_completed_budget_overage_does_invoke_machine_evidence(self) -> None:
        classified = {
            "status": v5.COMPLETED_OVER_BUDGET,
            "deterministic_verifier_authoritative": True,
        }
        fake_process = {"duration_ms": 1.0, "stdout": "", "stderr": ""}
        fake_verifier = {"verdict": {"status": "pass"}}
        fake_gate = {"passed": True, "failures": [], "captures": {}}
        with tempfile.TemporaryDirectory() as temporary, mock.patch.object(
            v5.v1, "run_verifier", return_value=(fake_process, fake_verifier)
        ) as verifier_call, mock.patch.object(
            v5.v2, "presentation_gate", return_value=fake_gate
        ) as gate_call:
            process, verifier, gate = v5._machine_evidence(
                workspace=Path(temporary),
                verifier_result_path=Path(temporary) / "verifier.json",
                build_slot=998,
                contract=v5.validate_contract(),
                agent_execution=classified,
            )
        verifier_call.assert_called_once()
        gate_call.assert_called_once()
        self.assertIs(process, fake_process)
        self.assertIs(verifier, fake_verifier)
        self.assertIs(gate, fake_gate)

    def test_initial_status_never_promotes_machine_or_integrity_failure(self) -> None:
        completed = {
            "status": outcome.COMPLETED,
            "deterministic_verifier_authoritative": True,
        }
        passing_gate = {"passed": True}
        failing_gate = {"passed": False}
        passing_verifier = {"verdict": {"status": "pass"}}
        self.assertEqual(v5._initial_status(completed, None, passing_gate, True), "deterministic_failure")
        self.assertEqual(
            v5._initial_status(completed, passing_verifier, failing_gate, True),
            "presentation_gate_failure",
        )
        self.assertEqual(
            v5._initial_status(completed, passing_verifier, passing_gate, False),
            "integrity_failure",
        )
        self.assertEqual(
            v5._initial_status(completed, passing_verifier, passing_gate, True),
            "accepted_for_perceptual_review",
        )

    def test_human_revision_prompt_preserves_machine_authorities(self) -> None:
        prompt = v5.build_revision_prompt("Make the HUD easier to read.", {"recommendation": "Increase contrast."})
        self.assertIn("Preserve every deterministic gameplay semantic", prompt)
        self.assertIn("Do not change benchmark/verifier/harness files", prompt)
        self.assertIn("Make the HUD easier to read.", prompt)
        self.assertIn("ember-hall-death.png", prompt)

    def test_runner_has_exactly_two_initial_attempts_and_no_retry_loop(self) -> None:
        source = Path(v5.__file__).read_text(encoding="utf-8")
        self.assertIn("for index in range(1, 3)", source)
        self.assertIn('"automatic_retries": 0', source)
        self.assertIn('"replacement_trials": 0', source)
        self.assertIn('"acceptance_v4_write_forbidden": True', source)
        self.assertIn("agent_transport_failure_precedes_verifier", source)
        self.assertIn('"downstream_verifier_skipped_when_agent_incomplete": True', source)
        self.assertNotIn("while retry", source.casefold())
        self.assertNotIn("best_of", source.casefold())

    def test_owner_loop_is_main_owner_only_and_has_no_retry_entrypoint(self) -> None:
        workflow = (
            Path(v5.__file__).resolve().parent.parent
            / ".github"
            / "workflows"
            / "benchmark-b2-owner-acceptance-v5.yml"
        ).read_text(encoding="utf-8")
        for required in (
            "github.repository == 'khsfashi/Trace2D'",
            "github.actor == 'khsfashi'",
            "github.run_attempt == 1",
            "github.event.issue.number == 104",
            "github.ref == 'refs/heads/main'",
            "github.event.comment.body == '/b2 accept-v5-start'",
            "startsWith(github.event.comment.body, '/b2 accept-v5-review ')",
            "startsWith(github.event.comment.body, '/b2 accept-v5-feedback ')",
            "startsWith(github.event.comment.body, '/b2 accept-v5-final-review ')",
            "benchmark-b2-acceptance-v5",
            "benchmark_b2_acceptance_v5.py start",
            "cancel-in-progress: false",
        ):
            self.assertIn(required, workflow, required)
        self.assertNotIn("workflow_dispatch:", workflow)
        self.assertNotIn("rerun", workflow.casefold())
        self.assertNotIn("retry", workflow.casefold())

    def test_candidate_free_qualification_cannot_start_or_mutate_v5(self) -> None:
        workflow = (
            Path(v5.__file__).resolve().parent.parent
            / ".github"
            / "workflows"
            / "benchmark-b2-owner-acceptance-v5-qualification.yml"
        ).read_text(encoding="utf-8")
        for required in (
            "github.repository == 'khsfashi/Trace2D'",
            "github.actor == 'khsfashi'",
            "github.run_attempt == 1",
            "github.event.issue.number == 104",
            "github.ref == 'refs/heads/main'",
            "github.event.comment.body == '/b2 accept-v5-qualify'",
            "benchmark-b2-acceptance-v5",
            "benchmark_b2_acceptance_v5.py preflight",
            "Candidate-free V5 preflight unexpectedly created durable state.",
        ):
            self.assertIn(required, workflow, required)
        for forbidden in (
            "benchmark_b2_acceptance_v5.py start",
            "record-review --runs-root",
            "feedback --runs-root",
            "record-final-review",
            "workflow_dispatch:",
        ):
            self.assertNotIn(forbidden, workflow, forbidden)


if __name__ == "__main__":
    unittest.main()
