#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest import mock

import benchmark_b0_codex_chatgpt_wrapper as base
import benchmark_b0_codex_windows_acl_wrapper as wrapper
import benchmark_b0_codex_wrapper as core


class WindowsAclCodexWrapperTests(unittest.TestCase):
    def test_frozen_windows_backend_is_exact(self) -> None:
        self.assertEqual(wrapper.MODEL_ID, "gpt-5.5")
        self.assertEqual(wrapper.MODEL_REVISION, "gpt-5.5")
        self.assertEqual(wrapper.WINDOWS_SANDBOX_MODE, "elevated")
        self.assertEqual(wrapper.ISOLATION_BACKEND, "windows_ntfs_acl_v1_elevated")

    def test_config_pins_elevated_windows_backend(self) -> None:
        with tempfile.TemporaryDirectory() as root_text:
            root = Path(root_text)
            config = wrapper.write_external_acl_config(
                codex_home=root / "codex-home",
                workspace=root / "workspace",
                read_only_roots=[],
                lane="godot.generic",
                godot_mcp_server=None,
                trace2d_mcp_server=None,
            )
            text = config.read_text(encoding="utf-8")
        self.assertIn('default_permissions = ":workspace"', text)
        self.assertIn("[windows]", text)
        self.assertIn('sandbox = "elevated"', text)
        self.assertIn("[sandbox_workspace_write]", text)
        self.assertIn("network_access = false", text)

    def test_codex_windows_display_path_is_canonicalized_for_attempt_matching(self) -> None:
        events = [
            {
                "type": "item.completed",
                "item": {
                    "type": "command_execution",
                    "command": r"type D:\\Trace2D-pr118\\benchmarks\\b0\\verifiers\\canary.txt",
                },
            }
        ]
        commands = wrapper.normalized_command_texts(events)
        self.assertEqual(
            commands,
            [r"type D:\Trace2D-pr118\benchmarks\b0\verifiers\canary.txt"],
        )
        self.assertIn(
            r"D:\Trace2D-pr118\benchmarks\b0\verifiers\canary.txt",
            commands[0],
        )

    def test_probe_acl_evidence_is_exported_outside_skipped_artifacts(self) -> None:
        with tempfile.TemporaryDirectory() as root_text:
            root = Path(root_text)
            workspace = root / "isolation-probe" / "workspace"
            codex_home = workspace / ".probe-artifacts" / "codex-home"
            codex_home.mkdir(parents=True)

            def fake_guard(**_kwargs):
                evidence = codex_home.parent / "acl-isolation.json"
                evidence.write_text(json.dumps({"passed": True}), encoding="utf-8")
                return subprocess.CompletedProcess(args=[], returncode=0, stdout="", stderr=""), []

            with mock.patch.object(base, "guarded_run_codex", side_effect=fake_guard):
                wrapper.guarded_run_codex(
                    codex="codex",
                    workspace=workspace,
                    codex_home=codex_home,
                    read_roots=[],
                    prompt="probe",
                    timeout=1.0,
                )

            exported = workspace.parent / "acl-isolation.json"
            self.assertTrue(exported.is_file())
            self.assertTrue(json.loads(exported.read_text(encoding="utf-8"))["passed"])

    def test_configure_preserves_acl_guard_and_replaces_writer(self) -> None:
        original_id = core.MODEL_ID
        original_revision = core.MODEL_REVISION
        original_profile = core.PERMISSION_PROFILE
        original_writer = core.write_isolated_config
        original_run = core.run_codex
        original_command_texts = core.command_texts
        original_backend = base.ISOLATION_BACKEND
        try:
            wrapper.configure()
            self.assertEqual(core.MODEL_ID, "gpt-5.5")
            self.assertEqual(core.MODEL_REVISION, "gpt-5.5")
            self.assertEqual(
                core.PERMISSION_PROFILE,
                ":workspace+windows_ntfs_acl_v1_elevated",
            )
            self.assertIs(core.write_isolated_config, wrapper.write_external_acl_config)
            self.assertIs(core.run_codex, wrapper.guarded_run_codex)
            self.assertIs(core.command_texts, wrapper.normalized_command_texts)
            self.assertEqual(base.ISOLATION_BACKEND, wrapper.ISOLATION_BACKEND)
        finally:
            core.MODEL_ID = original_id
            core.MODEL_REVISION = original_revision
            core.PERMISSION_PROFILE = original_profile
            core.write_isolated_config = original_writer
            core.run_codex = original_run
            core.command_texts = original_command_texts
            base.ISOLATION_BACKEND = original_backend


if __name__ == "__main__":
    unittest.main()
