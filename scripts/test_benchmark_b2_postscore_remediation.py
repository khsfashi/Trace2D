#!/usr/bin/env python3
from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from scripts import benchmark_b2_postscore_remediation as remediation


class BenchmarkB2PostscoreRemediationTests(unittest.TestCase):
    def test_godot_ai_cleanup_removes_dangling_autoload_and_preserves_authored_entries(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            project = Path(temporary) / "project.godot"
            project.write_text(
                '[application]\nconfig/name="B2"\n\n'
                '[autoload]\n'
                'Other="*res://scripts/other.gd"\n'
                'GameHelper="*res://addons/godot_ai/runtime/game_helper.gd"\n\n'
                '[editor_plugins]\n'
                'enabled=PackedStringArray("res://addons/godot_ai/plugin.cfg", "res://addons/other/plugin.cfg")\n',
                encoding="utf-8",
            )
            remediation.remove_injected_godot_ai_plugin(project)
            cleaned = project.read_text(encoding="utf-8")
            self.assertNotIn("res://addons/godot_ai/", cleaned)
            self.assertIn('Other="*res://scripts/other.gd"', cleaned)
            self.assertIn('enabled=PackedStringArray("res://addons/other/plugin.cfg")', cleaned)

    def test_verifier_proven_candidate_failure_is_diagnosed_as_implementation(self) -> None:
        raw = {
            "status": "agent_setup_failure",
            "failure_domain": "infrastructure",
            "deterministic_verifier": {
                "verdict": {"status": "fail", "code": "candidate_compile_or_link_failed"}
            },
        }
        result = remediation.diagnostic_classification(raw)
        self.assertEqual(result["diagnostic_status"], "deterministic_failure")
        self.assertEqual(result["diagnostic_failure_domain"], "implementation")
        self.assertTrue(result["raw_record_unchanged"])
        self.assertEqual(raw["status"], "agent_setup_failure")

    def test_unproven_setup_failure_remains_infrastructure(self) -> None:
        raw = {
            "status": "agent_setup_failure",
            "failure_domain": "infrastructure",
            "deterministic_verifier": {"verdict": {"status": "error", "code": "verifier_crashed"}},
        }
        result = remediation.diagnostic_classification(raw)
        self.assertEqual(result["diagnostic_status"], "agent_setup_failure")
        self.assertEqual(result["diagnostic_failure_domain"], "infrastructure")

    def test_budget_diagnostics_exposes_overshoot_ratios(self) -> None:
        record = {
            "agent_result": {
                "budget": {
                    "limits": {"max_tool_calls": 120, "max_input_tokens": 100000, "max_output_tokens": 20000},
                    "observed": {"tool_calls": 40, "input_tokens": 663195, "output_tokens": 21110},
                    "exceeded": ["input_tokens", "output_tokens"],
                }
            }
        }
        result = remediation.budget_diagnostics(record)
        assert result is not None
        self.assertAlmostEqual(result["observed_to_limit_ratio"]["input_tokens"], 6.63195)
        self.assertAlmostEqual(result["observed_to_limit_ratio"]["output_tokens"], 1.0555)
        self.assertLess(result["observed_to_limit_ratio"]["tool_calls"], 1.0)

    def test_public_game_header_is_explicit(self) -> None:
        self.assertEqual(remediation.TRACE2D_GAME_PUBLIC_HEADER, "trace2d/application/Application.hpp")


if __name__ == "__main__":
    unittest.main()
