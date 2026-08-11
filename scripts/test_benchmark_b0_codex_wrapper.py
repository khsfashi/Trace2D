#!/usr/bin/env python3
from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

import benchmark_b0_codex_wrapper as wrapper


class CodexWrapperTests(unittest.TestCase):
    def test_provider_usage_uses_last_turn_completed_record(self) -> None:
        events = [
            {"type": "turn.completed", "usage": {"input_tokens": 5, "output_tokens": 2}},
            {
                "type": "turn.completed",
                "usage": {
                    "input_tokens": 101,
                    "cached_input_tokens": 17,
                    "output_tokens": 23,
                    "reasoning_output_tokens": 11,
                },
            },
        ]
        self.assertEqual(
            wrapper.token_usage(events),
            {
                "input_tokens": 101,
                "cached_input_tokens": 17,
                "output_tokens": 23,
                "reasoning_output_tokens": 11,
            },
        )

    def test_jsonl_and_tool_metrics_preserve_native_shape(self) -> None:
        text = "\n".join(
            [
                json.dumps({"type": "thread.started", "thread_id": "t"}),
                json.dumps(
                    {
                        "type": "item.completed",
                        "item": {"type": "command_execution", "command": "dir"},
                    }
                ),
                json.dumps(
                    {
                        "type": "item.completed",
                        "item": {"type": "file_change", "path": "main.tscn"},
                    }
                ),
                json.dumps(
                    {
                        "type": "item.completed",
                        "item": {"type": "mcp_tool_call", "tool": "godot_runtime_state"},
                    }
                ),
                json.dumps({"type": "turn.completed", "usage": {"input_tokens": 7, "output_tokens": 3}}),
            ]
        )
        events = wrapper.parse_jsonl(text)
        metrics = wrapper.tool_metrics(events)
        self.assertEqual(metrics["tool_calls"], 3)
        self.assertEqual(metrics["revisions"], 1)
        self.assertEqual(metrics["normalized_operations"]["shell"], 1)
        self.assertEqual(metrics["normalized_operations"]["file_write"], 1)
        self.assertEqual(metrics["normalized_operations"]["runtime_inspect"], 1)
        self.assertGreaterEqual(metrics["engine_native_operations"]["mcp_tool_call:godot_runtime_state"], 1)
        self.assertTrue(wrapper.saw_turn_completed(events))

    def test_permission_config_is_workspace_write_and_external_read_is_explicit_only(self) -> None:
        with tempfile.TemporaryDirectory() as root_text:
            root = Path(root_text)
            workspace = root / "candidate"
            codex_home = root / "codex-home"
            tool_root = root / "tool-root"
            forbidden = root / "held-out-verifier"
            workspace.mkdir()
            tool_root.mkdir()
            forbidden.mkdir()

            config = wrapper.write_isolated_config(
                codex_home=codex_home,
                workspace=workspace,
                read_only_roots=[tool_root],
                lane="godot.generic",
                godot_mcp_server=None,
                trace2d_mcp_server=None,
            ).read_text(encoding="utf-8")

            self.assertIn('default_permissions = "trace2d_b0_isolated"', config)
            self.assertIn('[permissions.trace2d_b0_isolated.filesystem]', config)
            self.assertIn('\":minimal\" = \"read\"', config)
            self.assertIn('[permissions.trace2d_b0_isolated.filesystem.\":workspace_roots\"]', config)
            self.assertIn('\".\" = \"write\"', config)
            self.assertIn(json.dumps(str(tool_root.resolve())) + ' = "read"', config)
            self.assertNotIn(str(forbidden.resolve()), config)
            self.assertIn('[permissions.trace2d_b0_isolated.network]', config)
            self.assertIn('enabled = false', config)
            self.assertNotIn("sandbox_mode", config)

    def test_godot_mcp_plugin_injection_round_trips(self) -> None:
        with tempfile.TemporaryDirectory() as root_text:
            project = Path(root_text) / "project.godot"
            original = 'config_version=5\n\n[application]\nconfig/name="B0"\n'
            project.write_text(original, encoding="utf-8")
            self.assertTrue(wrapper.enable_godot_mcp_plugin(project))
            injected = project.read_text(encoding="utf-8")
            self.assertIn("[editor_plugins]", injected)
            self.assertIn('res://addons/godot_mcp/plugin.cfg', injected)
            wrapper.remove_injected_godot_mcp_plugin(project)
            self.assertEqual(project.read_text(encoding="utf-8"), original)

    def test_frozen_identity_is_exact(self) -> None:
        self.assertEqual(wrapper.EXPECTED_CODEX_VERSION, "0.144.6")
        self.assertEqual(wrapper.AGENT_ID, "openai-codex-cli@0.144.6")
        self.assertEqual(wrapper.MODEL_ID, "gpt-5.5")
        self.assertEqual(wrapper.MODEL_REVISION, "gpt-5.5-2026-04-23")
        self.assertEqual(wrapper.REASONING_EFFORT, "high")


if __name__ == "__main__":
    unittest.main()
