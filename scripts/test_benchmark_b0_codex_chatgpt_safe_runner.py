#!/usr/bin/env python3
from __future__ import annotations

import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import run_benchmark_b0_codex_chatgpt_calibration_safe as safe_runner


class SafeCalibrationCleanupTests(unittest.TestCase):
    def test_recovery_freezes_gpt55_before_scoring(self) -> None:
        self.assertEqual(safe_runner.FROZEN_MODEL_ID, "gpt-5.5")
        self.assertEqual(
            safe_runner.FROZEN_PROVIDER_REVISION_POLICY,
            "chatgpt_codex_cli_selector_no_dated_snapshot",
        )

    def test_scrub_removes_codex_home_without_walking_volatile_cache(self) -> None:
        with tempfile.TemporaryDirectory() as root_text:
            root = Path(root_text)
            volatile = (
                root
                / "isolation-probe"
                / "workspace"
                / ".probe-artifacts"
                / "codex-home"
                / "plugins"
                / "cache"
                / "openai-curated-remote"
                / "openai-templates"
                / "0.1.1"
                / "skills"
                / "artifact-template-analytics-dashboard"
            )
            volatile.mkdir(parents=True)
            (volatile / "auth.json").write_text("secret", encoding="utf-8")

            safe_runner.scrub_transient_codex_state(root)

            self.assertFalse(
                (
                    root
                    / "isolation-probe"
                    / "workspace"
                    / ".probe-artifacts"
                    / "codex-home"
                ).exists()
            )

    def test_scrub_removes_unexpected_auth_outside_codex_home(self) -> None:
        with tempfile.TemporaryDirectory() as root_text:
            root = Path(root_text)
            nested = root / "calibration" / "unexpected"
            nested.mkdir(parents=True)
            auth = nested / "auth.json"
            auth.write_text("secret", encoding="utf-8")

            safe_runner.scrub_transient_codex_state(root)

            self.assertFalse(auth.exists())

    def test_model_preflight_uses_stdin_dash_not_free_form_argv(self) -> None:
        with tempfile.TemporaryDirectory() as root_text:
            root = Path(root_text)
            workspace = root / "workspace"
            evidence = root / "evidence"
            auth = root / "auth.json"
            auth.write_text("credential", encoding="utf-8")
            completed = subprocess.CompletedProcess(
                args=[],
                returncode=0,
                stdout=(
                    '{"type":"turn.completed","usage":'
                    '{"input_tokens":7,"output_tokens":2}}\n'
                ),
                stderr="",
            )

            original_model = safe_runner.calibration.MODEL_ID
            try:
                safe_runner.calibration.MODEL_ID = safe_runner.FROZEN_MODEL_ID
                with mock.patch.object(
                    safe_runner.calibration.core,
                    "capture",
                    return_value=completed,
                ) as capture:
                    result = safe_runner.stdin_model_preflight(
                        codex="codex.cmd",
                        auth_file=auth,
                        workspace=workspace,
                        evidence_root=evidence,
                    )
            finally:
                safe_runner.calibration.MODEL_ID = original_model

            self.assertTrue(result["passed"])
            self.assertEqual(result["model_selector"], "gpt-5.5")
            self.assertEqual(result["prompt_transport"], "stdin_dash")
            args = capture.call_args.args[1]
            self.assertEqual(args[-1], "-")
            self.assertIn("gpt-5.5", args)
            self.assertNotIn("Reply exactly MODEL_OK. Do not use tools.", args)
            self.assertEqual(
                capture.call_args.kwargs["stdin_text"],
                "Reply exactly MODEL_OK. Do not use tools.",
            )
            self.assertFalse((evidence / "codex-home").exists())


if __name__ == "__main__":
    unittest.main()
