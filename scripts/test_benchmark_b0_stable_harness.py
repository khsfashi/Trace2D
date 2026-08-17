#!/usr/bin/env python3
from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

import benchmark_b0_stable_harness as stable


class StableHarnessTests(unittest.TestCase):
    def test_workspace_hash_ignores_godot_cache(self) -> None:
        with tempfile.TemporaryDirectory() as root_text:
            root = Path(root_text)
            (root / "main.tscn").write_text("authored", encoding="utf-8")
            cache = root / ".godot" / "shader_cache" / "shader.bin"
            cache.parent.mkdir(parents=True)
            cache.write_text("first", encoding="utf-8")
            first = stable.stable_tree_hash(root)
            cache.write_text("second", encoding="utf-8")
            second = stable.stable_tree_hash(root)
            self.assertEqual(first, second)

    def test_workspace_hash_ignores_top_level_harness_godot_user_cache(self) -> None:
        with tempfile.TemporaryDirectory() as root_text:
            root = Path(root_text)
            (root / "main.tscn").write_text("authored", encoding="utf-8")
            cache = (
                root
                / ".godot-user"
                / ".appdata"
                / "Godot"
                / "app_userdata"
                / "Trace2D B2 Scored Starter"
                / "shader_cache"
                / "shader.bin"
            )
            cache.parent.mkdir(parents=True)
            cache.write_text("first", encoding="utf-8")
            first = stable.stable_tree_hash(root)
            cache.write_text("second", encoding="utf-8")
            second = stable.stable_tree_hash(root)
            self.assertEqual(first, second)

    def test_workspace_hash_keeps_nested_godot_user_directory_authored(self) -> None:
        with tempfile.TemporaryDirectory() as root_text:
            root = Path(root_text)
            nested = root / "content" / ".godot-user" / "data.txt"
            nested.parent.mkdir(parents=True)
            nested.write_text("one", encoding="utf-8")
            first = stable.stable_tree_hash(root)
            nested.write_text("two", encoding="utf-8")
            second = stable.stable_tree_hash(root)
            self.assertNotEqual(first, second)

    def test_workspace_hash_changes_for_authored_file(self) -> None:
        with tempfile.TemporaryDirectory() as root_text:
            root = Path(root_text)
            authored = root / "main.tscn"
            authored.write_text("one", encoding="utf-8")
            first = stable.stable_tree_hash(root)
            authored.write_text("two", encoding="utf-8")
            second = stable.stable_tree_hash(root)
            self.assertNotEqual(first, second)

    def test_budget_exceeded_is_implementation_outcome(self) -> None:
        stable.configure()
        status, domain = stable.classify_agent_result(
            process={"timed_out": False, "return_code": 1},
            agent_result={"status": "budget_exceeded", "human_interventions": 0},
            verifier={"status": "pass"},
            integrity_ok=True,
        )
        self.assertEqual(status, "budget_exceeded")
        self.assertEqual(domain, "implementation")

    def test_integrity_failure_still_precedes_budget_status(self) -> None:
        stable.configure()
        status, domain = stable.classify_agent_result(
            process={"timed_out": False, "return_code": 1},
            agent_result={"status": "budget_exceeded", "human_interventions": 0},
            verifier={"status": "pass"},
            integrity_ok=False,
        )
        self.assertEqual(status, "benchmark_integrity_failure")
        self.assertEqual(domain, "integrity")


if __name__ == "__main__":
    unittest.main()
