#!/usr/bin/env python3
from __future__ import annotations

import unittest

import run_benchmark_b0_codex_chatgpt_calibration_safe as safe_runner


class RetiredCalibrationTests(unittest.TestCase):
    def test_frozen_model_identity_is_preserved(self) -> None:
        self.assertEqual(safe_runner.FROZEN_MODEL_ID, "gpt-5.5")
        self.assertEqual(
            safe_runner.FROZEN_PROVIDER_REVISION_POLICY,
            "chatgpt_codex_cli_selector_no_dated_snapshot",
        )

    def test_replacement_probe_is_named(self) -> None:
        self.assertEqual(
            safe_runner.REPLACEMENT_PROBE.as_posix(),
            "scripts/qualify_benchmark_b0_windows_acl_isolation.py",
        )

    def test_main_is_disabled(self) -> None:
        with self.assertRaises(safe_runner.calibration.CalibrationError):
            safe_runner.main()


if __name__ == "__main__":
    unittest.main()
