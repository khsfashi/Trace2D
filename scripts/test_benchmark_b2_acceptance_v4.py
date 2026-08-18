from __future__ import annotations

import argparse
import tempfile
import unittest
from pathlib import Path

from scripts import benchmark_b2_acceptance_v4 as v4


class BenchmarkB2AcceptanceV4Tests(unittest.TestCase):
    def test_contract_is_frozen_and_keeps_v3_gate_budget(self) -> None:
        contract = v4.validate_contract()
        self.assertEqual(contract["acceptance_version"], 4)
        self.assertEqual(contract["initial_runs"], 2)
        self.assertEqual(contract["budget"]["max_input_tokens"], 100000)
        self.assertEqual(contract["presentation_gate"]["image"]["maximum_dark_pixel_ratio"], 0.9)
        self.assertEqual(contract["isolation"]["durable_root_name"], "benchmark-b2-acceptance-v4")

    def test_root_accepts_only_v4_and_rejects_all_historical_roots(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            accepted = v4.require_acceptance_root(str(base / "benchmark-b2-acceptance-v4"))
            self.assertTrue(str(accepted).endswith("benchmark-b2-acceptance-v4"))
            for historical in (
                "benchmark-b2-scored-v1",
                "benchmark-b2-acceptance-v1",
                "benchmark-b2-acceptance-v2",
                "benchmark-b2-acceptance-v3",
            ):
                with self.assertRaises(v4.AcceptanceV4Error):
                    v4.require_acceptance_root(str(base / historical / "benchmark-b2-acceptance-v4"))

    def test_consumed_v4_root_is_rejected_before_agent_execution(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "benchmark-b2-acceptance-v4"
            root.mkdir()
            (root / "sentinel.txt").write_text("consumed\n", encoding="utf-8")
            args = argparse.Namespace(runs_root=str(root))
            with self.assertRaisesRegex(v4.AcceptanceV4Error, "already consumed"):
                v4.start(args)

    def test_initial_status_never_promotes_machine_or_integrity_failure(self) -> None:
        process = {"timed_out": False}
        passing_gate = {"passed": True}
        failing_gate = {"passed": False}
        passing_verifier = {"verdict": {"status": "pass"}}
        self.assertEqual(v4._initial_status(process, False, None, passing_gate, True), "agent_identity_or_result_failure")
        self.assertEqual(v4._initial_status(process, True, None, passing_gate, True), "deterministic_failure")
        self.assertEqual(v4._initial_status(process, True, passing_verifier, failing_gate, True), "presentation_gate_failure")
        self.assertEqual(v4._initial_status(process, True, passing_verifier, passing_gate, False), "integrity_failure")
        self.assertEqual(v4._initial_status(process, True, passing_verifier, passing_gate, True), "accepted_for_perceptual_review")

    def test_human_revision_prompt_preserves_machine_authorities(self) -> None:
        prompt = v4.build_revision_prompt("Make the HUD easier to read.", {"recommendation": "Increase contrast."})
        self.assertIn("Preserve every deterministic gameplay semantic", prompt)
        self.assertIn("Do not change benchmark/verifier/harness files", prompt)
        self.assertIn("Make the HUD easier to read.", prompt)
        self.assertIn("ember-hall-death.png", prompt)

    def test_runner_has_exactly_two_initial_attempts_and_no_retry_loop(self) -> None:
        source = Path(v4.__file__).read_text(encoding="utf-8")
        self.assertIn("for index in range(1, 3)", source)
        self.assertIn('"automatic_retries": 0', source)
        self.assertIn('"replacement_trials": 0', source)
        self.assertIn('"acceptance_v3_write_forbidden": True', source)
        self.assertNotIn("while retry", source.casefold())
        self.assertNotIn("best_of", source.casefold())

    def test_owner_loop_is_main_owner_only_and_has_no_retry_entrypoint(self) -> None:
        workflow = (
            Path(v4.__file__).resolve().parent.parent
            / ".github"
            / "workflows"
            / "benchmark-b2-owner-acceptance-v4.yml"
        ).read_text(encoding="utf-8")
        for required in (
            "github.repository == 'khsfashi/Trace2D'",
            "github.actor == 'khsfashi'",
            "github.run_attempt == 1",
            "github.event.issue.number == 104",
            "github.ref == 'refs/heads/main'",
            "github.event.comment.body == '/b2 accept-v4-start'",
            "startsWith(github.event.comment.body, '/b2 accept-v4-review ')",
            "startsWith(github.event.comment.body, '/b2 accept-v4-feedback ')",
            "startsWith(github.event.comment.body, '/b2 accept-v4-final-review ')",
            "benchmark-b2-acceptance-v4",
            "benchmark_b2_acceptance_v4.py start",
            "cancel-in-progress: false",
        ):
            self.assertIn(required, workflow, required)
        self.assertNotIn("workflow_dispatch:", workflow)
        self.assertNotIn("rerun", workflow.casefold())
        self.assertNotIn("retry", workflow.casefold())

    def test_candidate_free_qualification_cannot_start_or_mutate_v4(self) -> None:
        workflow = (
            Path(v4.__file__).resolve().parent.parent
            / ".github"
            / "workflows"
            / "benchmark-b2-owner-acceptance-v4-qualification.yml"
        ).read_text(encoding="utf-8")
        for required in (
            "github.repository == 'khsfashi/Trace2D'",
            "github.actor == 'khsfashi'",
            "github.run_attempt == 1",
            "github.event.issue.number == 104",
            "github.ref == 'refs/heads/main'",
            "github.event.comment.body == '/b2 accept-v4-qualify'",
            "benchmark-b2-acceptance-v4",
            "benchmark_b2_acceptance_v4.py preflight",
            "Candidate-free V4 preflight unexpectedly created durable state.",
        ):
            self.assertIn(required, workflow, required)
        for forbidden in (
            "benchmark_b2_acceptance_v4.py start",
            "record-review --runs-root",
            "feedback --runs-root",
            "record-final-review",
            "workflow_dispatch:",
        ):
            self.assertNotIn(forbidden, workflow, forbidden)

    def test_read_only_diagnostic_workflow_cannot_execute_or_mutate_acceptance(self) -> None:
        workflow_path = (
            Path(v4.__file__).resolve().parent.parent
            / ".github"
            / "workflows"
            / "benchmark-b2-owner-acceptance-v4-diagnostics.yml"
        )
        workflow = workflow_path.read_text(encoding="utf-8")
        for required in (
            "github.repository == 'khsfashi/Trace2D'",
            "github.actor == 'khsfashi'",
            "github.run_attempt == 1",
            "github.event.issue.number == 104",
            "github.ref == 'refs/heads/main'",
            "github.event.comment.body == '/b2 accept-v4-diagnose'",
            "runs-on: [self-hosted, windows, x64, trace2d-gpu]",
            "benchmark-b2-acceptance-v4",
            "benchmark-b2-scored-v1",
            "benchmark-b2-acceptance-v1",
            "benchmark-b2-acceptance-v2",
            "benchmark-b2-acceptance-v3",
            "Expected exactly two consumed acceptance-v4 initial records",
            "trace2d_b2_nonscored_acceptance_v4_initial",
            "codex-events.jsonl",
            "TRACE2D_B2_ACCEPT_V4_DIAGNOSTIC_RECORD_BEGIN",
            "TRACE2D_B2_ACCEPT_V4_TRIAL_LOG_BEGIN",
            "TRACE2D_B2_ACCEPT_V4_SOURCE_BEGIN",
            "$logText = [IO.File]::ReadAllText($logPath)",
            "$text = [IO.File]::ReadAllText($file.FullName)",
        ):
            self.assertIn(required, workflow, required)
        for forbidden in (
            "benchmark_b2_acceptance_v4.py start",
            "/b2 accept-v4-start",
            "record-review --runs-root",
            "feedback --runs-root",
            "record-final-review --runs-root",
            "workflow_dispatch:",
        ):
            self.assertNotIn(forbidden, workflow, forbidden)


if __name__ == "__main__":
    unittest.main()
