#!/usr/bin/env python3
from __future__ import annotations

import ast
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
WRAPPER = REPO_ROOT / "scripts" / "benchmark_b2_codex_windows_acl_wrapper.py"


class BenchmarkB2CodexWindowsAclWrapperTests(unittest.TestCase):
    def setUp(self) -> None:
        self.source = WRAPPER.read_text(encoding="utf-8")
        self.tree = ast.parse(self.source)

    def test_probe_has_dedicated_engine_independent_tool_roots(self) -> None:
        self.assertIn("def _probe_tool_roots(", self.source)
        self.assertIn('os.environ.get("TRACE2D_BENCH_CODEX_READ_ROOTS", "")', self.source)
        self.assertIn('"godot_bin": None', self.source)
        self.assertIn('"trace2d_bin": None', self.source)
        probe_function = next(
            node for node in self.tree.body if isinstance(node, ast.FunctionDef) and node.name == "_probe_tool_roots"
        )
        probe_source = ast.get_source_segment(self.source, probe_function) or ""
        self.assertNotIn("TRACE2D_BENCH_GODOT_BIN", probe_source)
        self.assertNotIn("TRACE2D_BENCH_TRACE2D_BIN", probe_source)
        referenced_attributes = {
            node.attr for node in ast.walk(probe_function) if isinstance(node, ast.Attribute)
        }
        self.assertNotIn("tool_roots_for_b1", referenced_attributes)

    def test_normal_run_keeps_b1_lane_specific_tool_requirements(self) -> None:
        configure_function = next(
            node for node in self.tree.body if isinstance(node, ast.FunctionDef) and node.name == "configure_b2"
        )
        configure_source = ast.get_source_segment(self.source, configure_function) or ""
        self.assertIn("base.configure()", configure_source)
        self.assertIn("if probe_only:", configure_source)
        self.assertIn("core.tool_roots_for_lane = _probe_tool_roots", configure_source)
        self.assertNotIn("else:", configure_source)

    def test_command_is_parsed_before_probe_specific_configuration(self) -> None:
        main_function = next(
            node for node in self.tree.body if isinstance(node, ast.FunctionDef) and node.name == "main"
        )
        main_source = ast.get_source_segment(self.source, main_function) or ""
        parse_index = main_source.index("args = build_parser().parse_args()")
        configure_index = main_source.index('configure_b2(probe_only=args.command == "probe-isolation")')
        handler_index = main_source.index("return int(args.handler(args))")
        self.assertLess(parse_index, configure_index)
        self.assertLess(configure_index, handler_index)

    def test_probe_and_run_remain_separate_subcommands(self) -> None:
        self.assertIn('subparsers.add_parser("run"', self.source)
        self.assertIn('subparsers.add_parser("probe-isolation"', self.source)
        self.assertIn("run.set_defaults(handler=run_b2_trial)", self.source)
        self.assertIn("probe.set_defaults(handler=core.run_isolation_probe)", self.source)


if __name__ == "__main__":
    unittest.main()
