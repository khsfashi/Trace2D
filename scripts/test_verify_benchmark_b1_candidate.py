#!/usr/bin/env python3
from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path

SCRIPTS = Path(__file__).resolve().parent
if str(SCRIPTS) not in sys.path:
    sys.path.insert(0, str(SCRIPTS))

import benchmark_b1
import verify_benchmark_b1_candidate as candidate


class BenchmarkB1CandidateVerifierTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.repo_root = SCRIPTS.parent
        cls.suite = benchmark_b1.load_and_validate_suite(
            cls.repo_root / "benchmarks/b1/suite.json", cls.repo_root
        )
        cls.registry = json.loads(
            (cls.repo_root / "benchmarks/b1/verifiers.json").read_text(encoding="utf-8")
        )

    def test_frozen_task_lane_pairs_resolve_expected_verifier_engine(self) -> None:
        registry_by_id = {
            entry["id"]: entry
            for entry in self.registry["verifiers"]
        }
        for task_id in benchmark_b1.EXPECTED_TASKS:
            for lane_id in benchmark_b1.EXPECTED_LANES:
                verifier_id = candidate._verifier_id_for(self.suite, task_id, lane_id)
                entry = registry_by_id[verifier_id]
                expected_engine = "godot" if lane_id.startswith("godot.") else "trace2d"
                self.assertEqual(task_id, entry["task_id"])
                self.assertEqual(expected_engine, entry["engine"])
                candidate._validate_registry(self.repo_root, verifier_id, task_id, lane_id)

    def test_trace2d_file_dispatch_is_only_sprite_and_particle(self) -> None:
        self.assertEqual(
            ("sprite", Path("hero.sprite.toml")),
            candidate.TRACE2D_FILE_BY_TASK["b1-sprite-normalize-repair"],
        )
        self.assertEqual(
            ("particle", Path("hit_spark.trace2d.particle.toml")),
            candidate.TRACE2D_FILE_BY_TASK["b1-particle-budget-repair"],
        )
        self.assertNotIn(candidate.TRACE2D_ANIMATION_TASK, candidate.TRACE2D_FILE_BY_TASK)

    def test_process_exit_zero_is_pass(self) -> None:
        result = candidate._classify_process(
            {
                "timed_out": False,
                "return_code": 0,
                "argv": ["verifier"],
                "duration_ms": 1.0,
                "stdout": "accepted\n",
                "stderr": "",
            },
            "verifier-id",
        )
        self.assertEqual("pass", result["status"])

    def test_process_exit_one_is_deterministic_rejection(self) -> None:
        result = candidate._classify_process(
            {
                "timed_out": False,
                "return_code": 1,
                "argv": ["verifier"],
                "duration_ms": 1.0,
                "stdout": "rejected\n",
                "stderr": "",
            },
            "verifier-id",
        )
        self.assertEqual("fail", result["status"])
        self.assertEqual("candidate_rejected", result["code"])

    def test_timeout_and_unexpected_exit_are_infrastructure_errors(self) -> None:
        timeout = candidate._classify_process(
            {
                "timed_out": True,
                "return_code": None,
                "argv": ["verifier"],
                "duration_ms": 100.0,
                "stdout": "",
                "stderr": "",
            },
            "verifier-id",
        )
        self.assertEqual("error", timeout["status"])
        self.assertEqual("verifier_timeout", timeout["code"])

        unexpected = candidate._classify_process(
            {
                "timed_out": False,
                "return_code": 2,
                "argv": ["verifier"],
                "duration_ms": 1.0,
                "stdout": "",
                "stderr": "usage",
            },
            "verifier-id",
        )
        self.assertEqual("error", unexpected["status"])
        self.assertEqual("verifier_process_error", unexpected["code"])


if __name__ == "__main__":
    unittest.main()
