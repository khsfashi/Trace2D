from __future__ import annotations

import argparse
import tempfile
import unittest
from pathlib import Path

from scripts import benchmark_b2_acceptance_v5 as v5
from scripts import benchmark_b2_agent_outcome as outcome


class BenchmarkB2AcceptanceV5Tests(unittest.TestCase):
    def test_contract_is_frozen_and_keeps_v4_gate_budget(self) -> None:
        contract = v5.validate_contract()
        self.assertEqual(contract["acceptance_version"], 5)
        self.assertEqual(contract["initial_runs"], 2)
        self.assertEqual(contract["budget"]["max_input_tokens"], 100000)
        self.assertEqual(contract["presentation_gate"]["image"]["maximum_dark_pixel_ratio"], 0.9)
        self.assertEqual(contract["isolation"]["durable_root_name"], "benchmark-b2-acceptance-v5")

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

    def test_agent_transport_failure_precedes_deterministic_verifier(self) -> None:
        execution = v5._agent_execution(
            {"timed_out": False},
            {
                "status": "tool_transport_failure",
                "wrapper": {"process_return_code": 1, "turn_completed": False, "budget_ok": True},
            },
            True,
        )
        self.assertEqual(execution["status"], outcome.TRANSPORT_FAILURE)
        self.assertFalse(execution["deterministic_verifier_authoritative"])
        passing_gate = {"passed": True}
        passing_verifier = {"verdict": {"status": "pass"}}
        self.assertEqual(
            v5._initial_status(execution, passing_verifier, passing_gate, True),
            outcome.TRANSPORT_FAILURE,
        )

    def test_completed_agent_still_requires_deterministic_and_presentation_pass(self) -> None:
        execution = {
            "status": outcome.COMPLETED,
            "deterministic_verifier_authoritative": True,
        }
        passing_gate = {"passed": True}
        failing_gate = {"passed": False}
        passing_verifier = {"verdict": {"status": "pass"}}
        self.assertEqual(v5._initial_status(execution, None, passing_gate, True), "deterministic_failure")
        self.assertEqual(v5._initial_status(execution, passing_verifier, failing_gate, True), "presentation_gate_failure")
        self.assertEqual(v5._initial_status(execution, passing_verifier, passing_gate, False), "integrity_failure")
        self.assertEqual(v5._initial_status(execution, passing_verifier, passing_gate, True), "accepted_for_perceptual_review")

    def test_incomplete_agent_skips_downstream_verifier_and_presentation_work(self) -> None:
        source = Path(v5.__file__).read_text(encoding="utf-8")
        self.assertIn('if agent_execution["deterministic_verifier_authoritative"]:', source)
        self.assertIn('gate = _not_run_presentation_gate("not_run_agent_execution_incomplete")', source)
        self.assertIn('"downstream_verifier_skipped_when_agent_incomplete": True', source)
        classify_index = source.index("agent_execution = _agent_execution(process, agent_result, identity_ok)")
        verifier_index = source.index("v1.run_verifier(workspace, verifier_result_path")
        self.assertLess(classify_index, verifier_index)

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
