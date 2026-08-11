#!/usr/bin/env python3
from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

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

    def test_configure_preserves_acl_guard_and_replaces_writer(self) -> None:
        original_id = core.MODEL_ID
        original_revision = core.MODEL_REVISION
        original_profile = core.PERMISSION_PROFILE
        original_writer = core.write_isolated_config
        original_run = core.run_codex
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
            self.assertIs(core.run_codex, base.guarded_run_codex)
            self.assertEqual(base.ISOLATION_BACKEND, wrapper.ISOLATION_BACKEND)
        finally:
            core.MODEL_ID = original_id
            core.MODEL_REVISION = original_revision
            core.PERMISSION_PROFILE = original_profile
            core.write_isolated_config = original_writer
            core.run_codex = original_run
            base.ISOLATION_BACKEND = original_backend


if __name__ == "__main__":
    unittest.main()
