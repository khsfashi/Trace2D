#!/usr/bin/env python3
from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

import benchmark_b0_codex_chatgpt_wrapper as chatgpt_wrapper
import benchmark_b0_codex_wrapper as core


class ChatGptCodexAclWrapperTests(unittest.TestCase):
    def test_frozen_identity_and_backend_are_exact(self) -> None:
        self.assertEqual(chatgpt_wrapper.MODEL_ID, "gpt-5.5")
        self.assertEqual(chatgpt_wrapper.MODEL_REVISION, "gpt-5.5")
        self.assertEqual(chatgpt_wrapper.PERMISSION_PROFILE, ":workspace")
        self.assertEqual(chatgpt_wrapper.ISOLATION_BACKEND, "windows_ntfs_acl_v1")
        self.assertEqual(
            chatgpt_wrapper.PROVIDER_REVISION_POLICY,
            "chatgpt_codex_cli_selector_no_dated_snapshot",
        )

    def test_configure_replaces_rejected_native_profile_path(self) -> None:
        original_id = core.MODEL_ID
        original_revision = core.MODEL_REVISION
        original_profile = core.PERMISSION_PROFILE
        original_writer = core.write_isolated_config
        original_run = core.run_codex
        try:
            chatgpt_wrapper.configure()
            self.assertEqual(core.MODEL_ID, "gpt-5.5")
            self.assertEqual(core.MODEL_REVISION, "gpt-5.5")
            self.assertEqual(core.PERMISSION_PROFILE, ":workspace+windows_ntfs_acl_v1")
            self.assertIs(core.write_isolated_config, chatgpt_wrapper.write_external_acl_config)
            self.assertIs(core.run_codex, chatgpt_wrapper.guarded_run_codex)
        finally:
            core.MODEL_ID = original_id
            core.MODEL_REVISION = original_revision
            core.PERMISSION_PROFILE = original_profile
            core.write_isolated_config = original_writer
            core.run_codex = original_run

    def test_config_uses_builtin_workspace_and_disables_shell_network(self) -> None:
        with tempfile.TemporaryDirectory() as root_text:
            root = Path(root_text)
            config = chatgpt_wrapper.write_external_acl_config(
                codex_home=root / "codex-home",
                workspace=root / "workspace",
                read_only_roots=[],
                lane="godot.generic",
                godot_mcp_server=None,
                trace2d_mcp_server=None,
            )
            text = config.read_text(encoding="utf-8")
        self.assertIn('default_permissions = ":workspace"', text)
        self.assertIn("[sandbox_workspace_write]", text)
        self.assertIn("network_access = false", text)
        self.assertIn('web_search = "disabled"', text)
        self.assertNotIn("[permissions.", text)
        self.assertNotIn("trace2d_b0_isolated", text)

    def test_repository_root_is_always_protected(self) -> None:
        roots = chatgpt_wrapper.protected_roots()
        self.assertIn(Path(chatgpt_wrapper.__file__).resolve().parents[1], roots)


if __name__ == "__main__":
    unittest.main()
