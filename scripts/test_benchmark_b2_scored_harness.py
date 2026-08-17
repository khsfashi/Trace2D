#!/usr/bin/env python3
from __future__ import annotations

import argparse
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import benchmark_b2_scored_harness as harness


class B2ScoredHarnessRecoveryTests(unittest.TestCase):
    def _trial_root(self, runs_root: Path) -> Path:
        return runs_root / "trials" / "slot-01-godot-generic-r1"

    def _legacy_consumed_trial(self, runs_root: Path) -> Path:
        trial_root = self._trial_root(runs_root)
        workspace = trial_root / "workspace"
        workspace.mkdir(parents=True)
        (workspace / "main.tscn").write_text("authored candidate\n", encoding="utf-8")
        (trial_root / "adapter.stdout.txt").write_text("", encoding="utf-8")
        (trial_root / "adapter.stderr.txt").write_text("", encoding="utf-8")
        return trial_root

    def test_existing_consumed_trial_is_sealed_without_agent_or_verifier_rerun(self) -> None:
        with tempfile.TemporaryDirectory() as root_text:
            runs_root = Path(root_text)
            trial_root = self._legacy_consumed_trial(runs_root)
            (trial_root / "agent-result.json").write_text(
                '{"status":"completed","do_not_reuse":"candidate outcome"}\n',
                encoding="utf-8",
            )
            (trial_root / "verifier-result.json").write_text(
                '{"verdict":{"status":"pass"},"do_not_reuse":"candidate verdict"}\n',
                encoding="utf-8",
            )

            with (
                mock.patch.object(
                    harness,
                    "preflight_environment",
                    side_effect=AssertionError("recovery must not preflight or prepare a new Agent turn"),
                ),
                mock.patch.object(
                    harness,
                    "run_process",
                    side_effect=AssertionError("recovery must not rerun Agent or verifier"),
                ),
            ):
                record = harness.run_slot(argparse.Namespace(runs_root=str(runs_root), slot=1))

            self.assertEqual(record["status"], "benchmark_integrity_failure")
            self.assertEqual(record["failure_domain"], "integrity")
            self.assertIsNone(record["agent_result"])
            self.assertIsNone(record["deterministic_verifier"])
            self.assertFalse(record["recovery"]["candidate_result_reused"])
            self.assertFalse(record["recovery"]["candidate_verdict_reused"])
            self.assertFalse(record["integrity"]["agent_reexecuted"])
            self.assertFalse(record["integrity"]["verifier_reexecuted"])
            self.assertTrue(record["recovery"]["legacy_process_evidence"])
            self.assertIsNotNone(record["workspace_sha256"])

            execution, _, _ = harness.load_contract()
            records, next_slot = harness.next_frozen_slot(runs_root, execution)
            self.assertEqual(len(records), 1)
            self.assertIsNotNone(next_slot)
            self.assertEqual(next_slot["slot"], 2)
            self.assertEqual(next_slot["lane"], "godot.agent")

    def test_existing_trial_without_started_evidence_is_not_consumed_or_rerun(self) -> None:
        with tempfile.TemporaryDirectory() as root_text:
            runs_root = Path(root_text)
            workspace = self._trial_root(runs_root) / "workspace"
            workspace.mkdir(parents=True)
            (workspace / "main.tscn").write_text("setup only\n", encoding="utf-8")

            with (
                mock.patch.object(
                    harness,
                    "preflight_environment",
                    side_effect=AssertionError("ambiguous trial must not start a new turn"),
                ),
                mock.patch.object(
                    harness,
                    "run_process",
                    side_effect=AssertionError("ambiguous trial must not rerun Agent or verifier"),
                ),
            ):
                with self.assertRaisesRegex(
                    harness.B2HarnessError,
                    "without proof that the Agent attempt started",
                ):
                    harness.run_slot(argparse.Namespace(runs_root=str(runs_root), slot=1))

            self.assertFalse((runs_root / "raw.jsonl").exists())

    def test_checkpointed_interrupted_trial_preserves_process_duration_without_reuse(self) -> None:
        with tempfile.TemporaryDirectory() as root_text:
            runs_root = Path(root_text)
            trial_root = self._trial_root(runs_root)
            workspace = trial_root / "workspace"
            workspace.mkdir(parents=True)
            (workspace / "main.tscn").write_text("authored candidate\n", encoding="utf-8")

            harness.write_json_atomic(
                trial_root / harness.ATTEMPT_START_CHECKPOINT,
                {
                    "kind": "trace2d_b2_attempt_start",
                    "trial_id": "slot-01-godot-generic-r1",
                    "slot": 1,
                    "lane_id": "godot.generic",
                    "started_at": "2026-08-17T01:00:00.000Z",
                },
            )
            harness.write_json_atomic(
                trial_root / harness.AGENT_PROCESS_CHECKPOINT,
                {
                    "kind": "trace2d_b2_agent_process",
                    "trial_id": "slot-01-godot-generic-r1",
                    "slot": 1,
                    "lane_id": "godot.generic",
                    "finished_at": "2026-08-17T01:05:00.000Z",
                    "duration_ms": 300000.0,
                },
            )

            with mock.patch.object(
                harness,
                "run_process",
                side_effect=AssertionError("checkpoint recovery must not rerun Agent or verifier"),
            ):
                record = harness.run_slot(argparse.Namespace(runs_root=str(runs_root), slot=1))

            self.assertEqual(record["started_at"], "2026-08-17T01:00:00.000Z")
            self.assertEqual(record["metrics"]["wall_ms"], 300000.0)
            self.assertTrue(record["recovery"]["start_checkpoint_valid"])
            self.assertTrue(record["recovery"]["process_checkpoint_valid"])
            self.assertFalse(record["recovery"]["legacy_process_evidence"])
            self.assertFalse(record["recovery"]["candidate_result_reused"])
            self.assertFalse(record["recovery"]["candidate_verdict_reused"])


if __name__ == "__main__":
    unittest.main()
