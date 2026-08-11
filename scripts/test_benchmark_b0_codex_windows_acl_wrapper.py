#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

import benchmark_b0_codex_chatgpt_wrapper as base
import benchmark_b0_codex_windows_acl_wrapper as wrapper
import benchmark_b0_codex_wrapper as core


class WindowsAclCodexWrapperTests(unittest.TestCase):
    def tearDown(self) -> None:
        wrapper._PREPARED_IDENTITIES.clear()

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

    def test_identity_is_discovered_during_codex_home_setup(self) -> None:
        with tempfile.TemporaryDirectory() as root_text:
            root = Path(root_text)
            workspace = root / "workspace"
            workspace.mkdir()
            home = root / "result" / "codex-home"
            host = (
                "S-1-5-21-1",
                subprocess.CompletedProcess(args=[], returncode=0, stdout="host", stderr=""),
            )
            sandbox = (
                "S-1-5-21-2",
                subprocess.CompletedProcess(args=[], returncode=0, stdout="sandbox", stderr=""),
            )

            def fake_setup(**_kwargs):
                home.mkdir(parents=True)
                return home

            with mock.patch.object(wrapper, "_ORIGINAL_SETUP_CODEX_HOME", side_effect=fake_setup), mock.patch.object(
                base, "_host_sid", return_value=host
            ), mock.patch.object(base, "_sandbox_sid", return_value=sandbox):
                result = wrapper.setup_codex_home_with_identity(
                    codex="codex",
                    workspace=workspace,
                    lane="godot.agent",
                    result_root=home.parent,
                    read_roots=[],
                    godot_mcp=None,
                    trace2d_mcp=None,
                )

            self.assertEqual(result, home)
            self.assertEqual(
                wrapper._PREPARED_IDENTITIES[wrapper._identity_key(home)],
                (host, sandbox),
            )

    def test_guarded_turn_reuses_prepared_identity_without_second_discovery(self) -> None:
        with tempfile.TemporaryDirectory() as root_text:
            root = Path(root_text)
            workspace = root / "workspace"
            codex_home = root / "result" / "codex-home"
            workspace.mkdir()
            codex_home.mkdir(parents=True)
            host = (
                "S-1-5-21-1",
                subprocess.CompletedProcess(args=[], returncode=0, stdout="host", stderr=""),
            )
            sandbox = (
                "S-1-5-21-2",
                subprocess.CompletedProcess(args=[], returncode=0, stdout="sandbox", stderr=""),
            )
            wrapper._PREPARED_IDENTITIES[wrapper._identity_key(codex_home)] = (host, sandbox)

            def fake_guard(**_kwargs):
                self.assertEqual(base._host_sid(workspace)[0], host[0])
                self.assertEqual(
                    base._sandbox_sid(
                        codex="codex",
                        workspace=workspace,
                        codex_home=codex_home,
                        read_roots=[],
                    )[0],
                    sandbox[0],
                )
                return subprocess.CompletedProcess(args=[], returncode=0, stdout="", stderr=""), []

            with mock.patch.object(base, "guarded_run_codex", side_effect=fake_guard):
                wrapper.guarded_run_codex(
                    codex="codex",
                    workspace=workspace,
                    codex_home=codex_home,
                    read_roots=[],
                    prompt="trial",
                    timeout=1.0,
                )

            self.assertNotIn(wrapper._identity_key(codex_home), wrapper._PREPARED_IDENTITIES)

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

    def test_completed_provider_turn_over_budget_is_not_transport_failure(self) -> None:
        with tempfile.TemporaryDirectory() as root_text:
            result_path = Path(root_text) / "agent-result.json"

            def fake_trial(_args):
                result_path.write_text(
                    json.dumps(
                        {
                            "status": "tool_transport_failure",
                            "metrics": {
                                "tool_calls": 11,
                                "input_tokens": 149255,
                                "output_tokens": 2716,
                            },
                            "wrapper": {
                                "process_return_code": 0,
                                "turn_completed": True,
                                "budget_ok": False,
                            },
                        }
                    ),
                    encoding="utf-8",
                )
                return 1

            with mock.patch.object(wrapper, "_ORIGINAL_RUN_TRIAL", side_effect=fake_trial), mock.patch.dict(
                "os.environ",
                {
                    "TRACE2D_BENCH_MAX_TOOL_CALLS": "80",
                    "TRACE2D_BENCH_MAX_INPUT_TOKENS": "100000",
                    "TRACE2D_BENCH_MAX_OUTPUT_TOKENS": "20000",
                },
                clear=False,
            ):
                return_code = wrapper.run_trial_with_budget_classification(
                    SimpleNamespace(result_file=str(result_path))
                )

            result = json.loads(result_path.read_text(encoding="utf-8"))
            self.assertEqual(return_code, 1)
            self.assertEqual(result["status"], "budget_exceeded")
            self.assertEqual(result["budget"]["exceeded"], ["input_tokens"])
            self.assertTrue(result["budget"]["within"]["tool_calls"])
            self.assertTrue(result["budget"]["within"]["output_tokens"])

    def test_configure_preserves_final_integration_hooks(self) -> None:
        original_id = core.MODEL_ID
        original_revision = core.MODEL_REVISION
        original_profile = core.PERMISSION_PROFILE
        original_writer = core.write_isolated_config
        original_setup = core.setup_codex_home
        original_run = core.run_codex
        original_run_trial = core.run_trial
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
            self.assertIs(core.setup_codex_home, wrapper.setup_codex_home_with_identity)
            self.assertIs(core.run_codex, wrapper.guarded_run_codex)
            self.assertIs(core.run_trial, wrapper.run_trial_with_budget_classification)
            self.assertIs(core.command_texts, wrapper.normalized_command_texts)
            self.assertEqual(base.ISOLATION_BACKEND, wrapper.ISOLATION_BACKEND)
        finally:
            core.MODEL_ID = original_id
            core.MODEL_REVISION = original_revision
            core.PERMISSION_PROFILE = original_profile
            core.write_isolated_config = original_writer
            core.setup_codex_home = original_setup
            core.run_codex = original_run
            core.run_trial = original_run_trial
            core.command_texts = original_command_texts
            base.ISOLATION_BACKEND = original_backend


if __name__ == "__main__":
    unittest.main()
