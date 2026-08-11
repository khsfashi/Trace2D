#!/usr/bin/env python3
from __future__ import annotations

import tempfile
import unittest
import zipfile
from pathlib import Path

import package_benchmark_b0_evidence as packager


class EvidencePackagerTests(unittest.TestCase):
    def test_skips_transient_codex_and_runtime_caches(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            run_root = base / "run"
            run_root.mkdir()
            (run_root / "toolchain.json").write_text("{}", encoding="utf-8")
            trial = run_root / "calibration" / "trials" / "trial-1"
            workspace = trial / "workspace"
            workspace.mkdir(parents=True)
            (trial / "codex-events.jsonl").write_text("{}\n", encoding="utf-8")
            (workspace / "scene.trace2d.toml").write_text("format_version = 1\n", encoding="utf-8")
            (trial / "codex-home" / "plugins" / "cache").mkdir(parents=True)
            (trial / "codex-home" / "auth.json").write_text("secret", encoding="utf-8")
            (workspace / ".godot" / "cache").mkdir(parents=True)
            (workspace / ".godot" / "cache" / "x").write_text("cache", encoding="utf-8")
            probe = run_root / "isolation-probe" / "workspace" / ".probe-artifacts" / "codex-home"
            probe.mkdir(parents=True)
            (probe / "auth.json").write_text("secret", encoding="utf-8")

            output = base / "evidence.zip"
            count = packager.package(run_root, output)
            self.assertGreaterEqual(count, 3)
            with zipfile.ZipFile(output) as archive:
                names = set(archive.namelist())
            self.assertIn("toolchain.json", names)
            self.assertIn("calibration/trials/trial-1/codex-events.jsonl", names)
            self.assertIn("calibration/trials/trial-1/workspace/scene.trace2d.toml", names)
            self.assertFalse(any("codex-home" in name for name in names))
            self.assertFalse(any(".probe-artifacts" in name for name in names))
            self.assertFalse(any("/.godot/" in f"/{name}" for name in names))
            self.assertFalse(any(name.endswith("auth.json") for name in names))

    def test_rejects_credential_file_outside_transient_home(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            run_root = base / "run"
            run_root.mkdir()
            (run_root / "auth.json").write_text("secret", encoding="utf-8")
            with self.assertRaises(packager.PackagingError):
                packager.package(run_root, base / "evidence.zip")


if __name__ == "__main__":
    unittest.main()
