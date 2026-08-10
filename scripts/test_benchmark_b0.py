from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

import benchmark_b0


class BenchmarkB0Tests(unittest.TestCase):
    def test_committed_suite_contract_is_structurally_valid(self) -> None:
        suite = benchmark_b0.validate_suite(
            benchmark_b0.repository_root() / "benchmarks/b0/suite.json"
        )
        self.assertEqual(suite["suite_id"], "trace2d-b0")
        self.assertEqual(
            {lane["id"] for lane in suite["lanes"]},
            set(benchmark_b0.EXPECTED_LANES),
        )
        self.assertEqual(suite["state"], "qualification_required")

    def test_hash_chained_jsonl_detects_tampering(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "raw.jsonl"
            first = benchmark_b0.append_hash_chained_jsonl(path, {"value": 1})
            second = benchmark_b0.append_hash_chained_jsonl(path, {"value": 2})
            records = benchmark_b0.verify_jsonl_chain(path)
            self.assertEqual([record["value"] for record in records], [1, 2])
            self.assertEqual(second["previous_record_sha256"], first["record_sha256"])

            lines = path.read_text(encoding="utf-8").splitlines()
            tampered = json.loads(lines[0])
            tampered["value"] = 99
            lines[0] = benchmark_b0.canonical_json(tampered)
            path.write_text("\n".join(lines) + "\n", encoding="utf-8")
            with self.assertRaises(benchmark_b0.HarnessError):
                benchmark_b0.verify_jsonl_chain(path)

    def test_report_flags_mixed_agent_profiles(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "raw.jsonl"
            base = {
                "task_id": "task",
                "lane_id": "godot.generic",
                "scored": True,
                "result": {"status": "success"},
                "metrics": {
                    "revisions": 1,
                    "tool_calls": 2,
                    "input_tokens": 3,
                    "output_tokens": 4,
                    "wall_ms": 5,
                    "verifier_ms": 6,
                    "human_interventions": 0,
                },
            }
            benchmark_b0.append_hash_chained_jsonl(
                path, {**base, "agent_profile_sha256": "profile-a"}
            )
            benchmark_b0.append_hash_chained_jsonl(
                path,
                {
                    **base,
                    "lane_id": "trace2d.agent",
                    "agent_profile_sha256": "profile-b",
                },
            )
            report = benchmark_b0.report_records(path, include_unscored=False)
            self.assertFalse(report["integrity"]["same_agent_profile_per_task"])
            self.assertIn("task", report["integrity"]["mixed_agent_profile_hashes"])

    def test_unresolved_agent_command_placeholder_is_rejected(self) -> None:
        with self.assertRaises(benchmark_b0.HarnessError):
            benchmark_b0.substitute_command(["agent", "{unknown}"], {})

    def test_all_committed_environment_qualifications_are_positive(self) -> None:
        suite = benchmark_b0.validate_suite(
            benchmark_b0.repository_root() / "benchmarks/b0/suite.json"
        )
        for lane_id in benchmark_b0.EXPECTED_LANES:
            evidence = benchmark_b0.validate_qualification(suite, lane_id)
            self.assertTrue(evidence["qualified"])

        godot_agent = benchmark_b0.validate_qualification(suite, "godot.agent")
        self.assertEqual(godot_agent["bridge"]["id"], "satelliteoflove/godot-mcp")
        self.assertEqual(godot_agent["bridge"]["version"], "4.1.0")
        self.assertTrue(godot_agent["checks"]["authoring"])
        self.assertTrue(godot_agent["checks"]["runtime_inspection"])
        self.assertTrue(godot_agent["checks"]["timed_input"])
        self.assertTrue(godot_agent["checks"]["deterministic_step"])


if __name__ == "__main__":
    unittest.main()
