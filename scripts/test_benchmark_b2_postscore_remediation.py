#!/usr/bin/env python3
from __future__ import annotations

import json
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

    def test_terminal_evidence_separates_process_result_and_verifier_failure(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            process = root / "agent-process.json"
            result = root / "agent-result.json"
            process.write_text(
                json.dumps({"return_code": 2, "timed_out": False, "duration_ms": 887000.0}),
                encoding="utf-8",
            )
            result.write_text("{not-json", encoding="utf-8")
            record = {
                "status": "agent_setup_failure",
                "agent_identity_ok": False,
                "agent_result": None,
                "metrics": {"wall_ms": 887000.0},
                "artifacts": {"agent_process": str(process), "agent_result": str(result)},
                "deterministic_verifier": {
                    "verdict": {
                        "status": "fail",
                        "code": "candidate_rejected",
                        "process": {
                            "return_code": 1,
                            "timed_out": False,
                            "stderr": "Parse Error: generated candidate failed",
                        },
                    }
                },
            }
            evidence = remediation.terminal_evidence(record)
            self.assertEqual(evidence["agent_process"]["return_code"], 2)
            self.assertFalse(evidence["agent_process"]["timed_out"])
            self.assertTrue(evidence["structured_agent_result"]["artifact_present"])
            self.assertEqual(evidence["structured_agent_result"]["artifact_parse_status"], "invalid_json")
            self.assertEqual(evidence["verifier"]["code"], "candidate_rejected")
            self.assertIn("Parse Error", evidence["verifier"]["stderr"])

    def test_budget_diagnostics_exposes_overshoot_and_cached_token_accounting(self) -> None:
        record = {
            "agent_result": {
                "metrics": {
                    "tool_calls": 40,
                    "input_tokens": 663195,
                    "cached_input_tokens": 574464,
                    "output_tokens": 21110,
                    "reasoning_output_tokens": 9413,
                },
                "budget": {
                    "limits": {"max_tool_calls": 120, "max_input_tokens": 100000, "max_output_tokens": 20000},
                    "observed": {"tool_calls": 40, "input_tokens": 663195, "output_tokens": 21110},
                    "exceeded": ["input_tokens", "output_tokens"],
                },
            }
        }
        result = remediation.budget_diagnostics(record)
        assert result is not None
        self.assertAlmostEqual(result["observed_to_limit_ratio"]["input_tokens"], 6.63195)
        self.assertAlmostEqual(result["observed_to_limit_ratio"]["output_tokens"], 1.0555)
        self.assertEqual(result["token_accounting"]["cached_input_tokens"], 574464)
        self.assertEqual(result["token_accounting"]["uncached_input_tokens"], 88731)
        self.assertEqual(result["token_accounting"]["reasoning_output_tokens"], 9413)

    def test_integrity_diagnostics_enforces_zero_retry_replacement_and_freeze(self) -> None:
        valid = {
            "integrity": {
                "schedule_prefix_valid": True,
                "automatic_retries": 0,
                "replacement_trials": 0,
                "repo_freeze_unchanged_after_agent": True,
            }
        }
        self.assertTrue(remediation.integrity_diagnostics(valid)["valid"])
        broken = {
            "integrity": {
                "schedule_prefix_valid": False,
                "automatic_retries": 1,
                "replacement_trials": 1,
                "repo_freeze_unchanged_after_agent": False,
            }
        }
        result = remediation.integrity_diagnostics(broken)
        self.assertFalse(result["valid"])
        self.assertEqual(len(result["failed"]), 4)

    def test_public_api_index_points_game_to_real_header_and_canonical_example(self) -> None:
        symbol = remediation.public_api_symbol("trace2d::application::Game")
        self.assertEqual(symbol["include"], "trace2d/application/Application.hpp")
        self.assertNotEqual(symbol["include"], "trace2d/application/Game.hpp")
        source = remediation.REPO_ROOT / symbol["source"]
        example = remediation.REPO_ROOT / symbol["canonical_example"]
        self.assertTrue(source.is_file())
        self.assertTrue(example.is_file())
        example_text = example.read_text(encoding="utf-8")
        self.assertIn("#include <trace2d/application/Application.hpp>", example_text)
        self.assertIn("public trace2d::application::Game", example_text)


if __name__ == "__main__":
    unittest.main()
