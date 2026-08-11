#!/usr/bin/env python3
from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

import run_benchmark_b0_codex_chatgpt_calibration_safe as safe_runner


class SafeCalibrationCleanupTests(unittest.TestCase):
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


if __name__ == "__main__":
    unittest.main()
