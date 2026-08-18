from __future__ import annotations

import argparse
import tempfile
import unittest
from pathlib import Path

from scripts import benchmark_b2_acceptance_v3 as v3


class BenchmarkB2AcceptanceV3Tests(unittest.TestCase):
    def test_contract_is_frozen_and_keeps_v2_gate_budget(self) -> None:
        contract = v3.validate_contract()
        self.assertEqual(contract["acceptance_version"], 3)
        self.assertEqual(contract["initial_runs"], 2)
        self.assertEqual(contract["budget"]["max_input_tokens"], 100000)
        self.assertEqual(contract["presentation_gate"]["image"]["maximum_dark_pixel_ratio"], 0.9)
        self.assertEqual(contract["isolation"]["durable_root_name"], "benchmark-b2-acceptance-v3")

    def test_root_accepts_only_v3_and_rejects_historical_roots(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            accepted = v3.require_acceptance_root(str(base / "benchmark-b2-acceptance-v3"))
            self.assertTrue(str(accepted).endswith("benchmark-b2-acceptance-v3"))
            for historical in (
                "benchmark-b2-scored-v1",
                "benchmark-b2-acceptance-v1",
                "benchmark-b2-acceptance-v2",
            ):
                with self.assertRaises(v3.AcceptanceV3Error):
                    v3.require_acceptance_root(str(base / historical / "benchmark-b2-acceptance-v3"))

    def test_consumed_v3_root_is_rejected_before_agent_execution(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "benchmark-b2-acceptance-v3"
            root.mkdir()
            (root / "sentinel.txt").write_text("consumed\n", encoding="utf-8")
            args = argparse.Namespace(runs_root=str(root))
            with self.assertRaisesRegex(v3.AcceptanceV3Error, "already consumed"):
                v3.start(args)

    def test_initial_status_never_promotes_machine_or_integrity_failure(self) -> None:
        process = {"timed_out": False}
        passing_gate = {"passed": True}
        failing_gate = {"passed": False}
        passing_verifier = {"verdict": {"status": "pass"}}
        self.assertEqual(
            v3._initial_status(process, False, None, passing_gate, True),
            "agent_identity_or_result_failure",
        )
        self.assertEqual(
            v3._initial_status(process, True, None, passing_gate, True),
            "deterministic_failure",
        )
        self.assertEqual(
            v3._initial_status(process, True, passing_verifier, failing_gate, True),
            "presentation_gate_failure",
        )
        self.assertEqual(
            v3._initial_status(process, True, passing_verifier, passing_gate, False),
            "integrity_failure",
        )
        self.assertEqual(
            v3._initial_status(process, True, passing_verifier, passing_gate, True),
            "accepted_for_perceptual_review",
        )

    def test_human_revision_prompt_preserves_machine_authorities(self) -> None:
        prompt = v3.build_revision_prompt("Make the HUD easier to read.", {"recommendation": "Increase contrast."})
        self.assertIn("Preserve every deterministic gameplay semantic", prompt)
        self.assertIn("Do not change benchmark/verifier/harness files", prompt)
        self.assertIn("Make the HUD easier to read.", prompt)
        self.assertIn("ember-hall-death.png", prompt)

    def test_runner_has_exactly_two_initial_attempts_and_no_retry_loop(self) -> None:
        source = Path(v3.__file__).read_text(encoding="utf-8")
        self.assertIn("for index in range(1, 3)", source)
        self.assertIn('"automatic_retries": 0', source)
        self.assertIn('"replacement_trials": 0', source)
        self.assertNotIn("while retry", source.casefold())
        self.assertNotIn("best_of", source.casefold())


if __name__ == "__main__":
    unittest.main()
