#!/usr/bin/env python3
from __future__ import annotations

import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

SCRIPTS = Path(__file__).resolve().parent
if str(SCRIPTS) not in sys.path:
    sys.path.insert(0, str(SCRIPTS))

import benchmark_b1_codex_windows_acl_wrapper as wrapper


class BenchmarkB1CodexWindowsAclWrapperTests(unittest.TestCase):
    def test_godot_agent_config_uses_loopback_http_mcp(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            with mock.patch.dict(
                os.environ,
                {"TRACE2D_B1_GODOT_AI_ENDPOINT": "http://127.0.0.1:54321/mcp"},
                clear=False,
            ):
                path = wrapper.write_b1_external_acl_config(
                    codex_home=root / "codex-home",
                    workspace=root / "workspace",
                    read_only_roots=[],
                    lane="godot.agent",
                    godot_mcp_server=None,
                    trace2d_mcp_server=None,
                )
            text = path.read_text(encoding="utf-8")
            self.assertIn("[mcp_servers.godot]", text)
            self.assertIn('url = "http://127.0.0.1:54321/mcp"', text)
            self.assertNotIn("command =", text)
            self.assertIn('sandbox = "elevated"', text)
            self.assertIn("network_access = false", text)

    def test_godot_agent_rejects_non_loopback_or_non_mcp_endpoint(self) -> None:
        for endpoint in (
            "https://example.com/mcp",
            "http://127.0.0.1:8000/other",
            "",
        ):
            with self.subTest(endpoint=endpoint), tempfile.TemporaryDirectory() as temp:
                with mock.patch.dict(
                    os.environ,
                    {"TRACE2D_B1_GODOT_AI_ENDPOINT": endpoint},
                    clear=False,
                ):
                    with self.assertRaises(wrapper.B1WrapperError):
                        wrapper.write_b1_external_acl_config(
                            codex_home=Path(temp) / "codex-home",
                            workspace=Path(temp) / "workspace",
                            read_only_roots=[],
                            lane="godot.agent",
                            godot_mcp_server=None,
                            trace2d_mcp_server=None,
                        )

    def test_trace2d_agent_does_not_inject_benchmark_only_scene_mcp(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            path = wrapper.write_b1_external_acl_config(
                codex_home=root / "codex-home",
                workspace=root / "workspace",
                read_only_roots=[],
                lane="trace2d.agent",
                godot_mcp_server=None,
                trace2d_mcp_server=None,
            )
            text = path.read_text(encoding="utf-8")
            self.assertNotIn("[mcp_servers.trace2d]", text)
            self.assertNotIn("scene.trace2d.toml", text)

    def test_plugin_injection_is_exact_and_reversible(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            project = Path(temp) / "project.godot"
            original = '[application]\nconfig/name="B1"\n'
            project.write_text(original, encoding="utf-8")
            self.assertTrue(wrapper.enable_godot_ai_plugin(project))
            self.assertIn(wrapper.GODOT_AI_PLUGIN, project.read_text(encoding="utf-8"))
            wrapper.remove_injected_godot_ai_plugin(project)
            self.assertEqual(original, project.read_text(encoding="utf-8"))

    def test_plugin_injection_refuses_ambiguous_existing_editor_plugins(self) -> None:
        with tempfile.TemporaryDirectory() as temp:
            project = Path(temp) / "project.godot"
            project.write_text(
                '[application]\nconfig/name="B1"\n\n[editor_plugins]\nenabled=PackedStringArray("res://other/plugin.cfg")\n',
                encoding="utf-8",
            )
            with self.assertRaisesRegex(wrapper.B1WrapperError, "ambiguous harness injection"):
                wrapper.enable_godot_ai_plugin(project)

    def test_completed_mcp_tool_names_ignore_shell_items(self) -> None:
        events = [
            {
                "type": "item.completed",
                "item": {"type": "mcp_tool_call", "tool": "godot.editor_state"},
            },
            {
                "type": "item.completed",
                "item": {"type": "command_execution", "command": "echo nope"},
            },
        ]
        self.assertEqual(["godot.editor_state"], wrapper.completed_mcp_tool_names(events))


if __name__ == "__main__":
    unittest.main()
