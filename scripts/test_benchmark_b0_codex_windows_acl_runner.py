#!/usr/bin/env python3
from __future__ import annotations

import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import run_benchmark_b0_codex_windows_acl_calibration as runner


class WindowsAclCalibrationRunnerTests(unittest.TestCase):
    def test_frozen_contract(self) -> None:
        self.assertEqual(runner.FROZEN_MODEL_ID, "gpt-5.5")
        self.assertEqual(runner.ISOLATION_BACKEND, "windows_ntfs_acl_v1_elevated")
        self.assertEqual(runner.WRAPPER_MODULE, "benchmark_b0_codex_windows_acl_wrapper")
        self.assertEqual(runner.ISOLATION_TIMEOUT_SECONDS, 285.0)
        self.assertEqual(runner.STABLE_HARNESS.name, "benchmark_b0_stable_harness.py")

    def test_isolation_timeout_and_wrapper_match_final_backend(self) -> None:
        completed = subprocess.CompletedProcess(args=[], returncode=0, stdout="", stderr="")
        with mock.patch.object(runner, "_BASE_RUN", return_value=completed) as base_run:
            runner.run_with_matched_isolation_timeout(
                ["python", "-m", "benchmark_b0_codex_chatgpt_wrapper", "probe-isolation"],
                cwd=Path.cwd(),
                check=False,
                capture=True,
            )
        argv = base_run.call_args.args[0]
        self.assertIn("benchmark_b0_codex_windows_acl_wrapper", argv)
        self.assertNotIn("benchmark_b0_codex_chatgpt_wrapper", argv)
        self.assertEqual(argv[-2:], ["--timeout", "285.0"])

    def test_owner_local_harness_is_routed_through_stable_entrypoint(self) -> None:
        completed = subprocess.CompletedProcess(args=[], returncode=0, stdout="", stderr="")
        with mock.patch.object(runner, "_BASE_RUN", return_value=completed) as base_run:
            runner.run_with_matched_isolation_timeout(
                [
                    "python",
                    str(Path("D:/Trace2D-pr118/scripts/benchmark_b0.py")),
                    "run-trial",
                    "--task",
                    "b0-semantic-scene-authoring",
                ],
                cwd=Path.cwd(),
                check=False,
                capture=True,
            )
        argv = base_run.call_args.args[0]
        self.assertEqual(Path(argv[1]), runner.STABLE_HARNESS)

    def test_model_preflight_uses_stdin_dash(self) -> None:
        with tempfile.TemporaryDirectory() as root_text:
            root = Path(root_text)
            workspace = root / "workspace"
            evidence = root / "evidence"
            auth = root / "auth.json"
            auth.write_text("credential", encoding="utf-8")
            completed = subprocess.CompletedProcess(
                args=[],
                returncode=0,
                stdout='{"type":"turn.completed","usage":{"input_tokens":7,"output_tokens":2}}\n',
                stderr="",
            )
            original_model = runner.calibration.MODEL_ID
            try:
                runner.calibration.MODEL_ID = runner.FROZEN_MODEL_ID
                with mock.patch.object(
                    runner.calibration.core,
                    "capture",
                    return_value=completed,
                ) as capture:
                    result = runner.stdin_model_preflight(
                        codex="codex.cmd",
                        auth_file=auth,
                        workspace=workspace,
                        evidence_root=evidence,
                    )
            finally:
                runner.calibration.MODEL_ID = original_model
        self.assertTrue(result["passed"])
        self.assertEqual(capture.call_args.args[1][-1], "-")
        self.assertEqual(
            capture.call_args.kwargs["stdin_text"],
            "Reply exactly MODEL_OK. Do not use tools.",
        )

    def test_scrub_removes_codex_home_without_descending(self) -> None:
        with tempfile.TemporaryDirectory() as root_text:
            root = Path(root_text)
            codex_home = root / "trial" / "codex-home"
            (codex_home / "plugins" / "volatile").mkdir(parents=True)
            (codex_home / "auth.json").write_text("credential", encoding="utf-8")
            runner.scrub_transient_codex_state(root)
            self.assertFalse(codex_home.exists())


if __name__ == "__main__":
    unittest.main()
