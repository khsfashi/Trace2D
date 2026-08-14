#!/usr/bin/env python3
from __future__ import annotations

import json
import unittest
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REPORT = ROOT / "benchmarks/b1/postmortem-v1.json"
DOC = ROOT / "docs/BENCHMARK_B1_POSTMORTEM.md"


class BenchmarkB1PostmortemTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.report = json.loads(REPORT.read_text(encoding="utf-8"))
        cls.doc = DOC.read_text(encoding="utf-8")

    def test_authority_is_exact_frozen_scored_cohort(self) -> None:
        authority = self.report["authority"]
        self.assertEqual(authority["scored_head"], "6d6904e99ad7060341861cb3823e04591a579bf7")
        self.assertEqual(authority["workflow_run"], 31763107941)
        self.assertEqual(authority["artifact_id"], 9206626314)
        self.assertEqual(
            authority["artifact_sha256"],
            "74ab53220927f557621c96ee7b8df7395010e60c191d3959705ab7ba09f8d4d6",
        )

    def test_trace2d_failure_taxonomy_matches_frozen_evidence(self) -> None:
        trials = self.report["trials"]
        self.assertEqual(len(trials), 9)
        failed = [trial for trial in trials if trial["scored_status"] != "success"]
        succeeded = [trial for trial in trials if trial["scored_status"] == "success"]
        self.assertEqual(len(failed), 6)
        self.assertEqual(len(succeeded), 3)
        self.assertTrue(all(trial["verifier_status"] == "pass" for trial in trials))
        self.assertTrue(all(trial["primary_category"] == "input_token_budget_exhaustion" for trial in failed))
        self.assertTrue(all(trial["metrics"]["input_tokens"] > 100000 for trial in failed))
        self.assertTrue(all(trial["metrics"]["input_tokens"] <= 100000 for trial in succeeded))
        self.assertTrue(all(trial["metrics"]["output_tokens"] <= 20000 for trial in trials))
        self.assertTrue(all(trial["metrics"]["tool_calls"] <= 80 for trial in trials))
        self.assertTrue(all(trial["metrics"]["human_interventions"] == 0 for trial in trials))

    def test_trace2d_task_scores_are_preserved(self) -> None:
        counts: Counter[str] = Counter()
        totals: Counter[str] = Counter()
        for trial in self.report["trials"]:
            task = trial["task_id"]
            totals[task] += 1
            if trial["scored_status"] == "success":
                counts[task] += 1
        self.assertEqual(totals, Counter({
            "b1-sprite-normalize-repair": 3,
            "b1-animation-exact-event": 3,
            "b1-particle-budget-repair": 3,
        }))
        self.assertEqual(counts["b1-sprite-normalize-repair"], 1)
        self.assertEqual(counts["b1-animation-exact-event"], 2)
        self.assertEqual(counts["b1-particle-budget-repair"], 0)

    def test_no_engine_native_authoring_operation_is_claimed(self) -> None:
        summary = self.report["trace2d_summary"]
        self.assertEqual(summary["engine_native_authoring_calls"], 0)
        for trial in self.report["trials"]:
            native = set(trial["engine_native_operations"])
            self.assertFalse(any(name.startswith("mcp_tool_call") or name.startswith("dynamic_tool_call") for name in native))

    def test_single_resource_complexity_budget_is_explicit(self) -> None:
        budget = self.report["single_resource_authoring_complexity_budget"]
        self.assertEqual(budget["hard_model_budget"]["max_input_tokens"], 100000)
        self.assertEqual(budget["hard_model_budget"]["max_output_tokens"], 20000)
        self.assertEqual(budget["hard_model_budget"]["max_tool_calls"], 80)
        self.assertEqual(budget["hard_model_budget"]["max_human_interventions"], 0)
        surface = budget["surface_requirements"]
        self.assertFalse(surface["raw_text_edit_required"])
        self.assertFalse(surface["git_metadata_required"])
        self.assertEqual(surface["primary_semantic_mutations_max"], 1)
        self.assertEqual(surface["deterministic_validation_calls_max"], 1)
        self.assertEqual(surface["expected_revisions_max"], 1)
        self.assertFalse(surface["visual_feedback_required_for_deterministic_acceptance"])

    def test_document_preserves_b1_and_b2_boundaries(self) -> None:
        self.assertIn("immutable pre-improvement baseline", self.doc)
        self.assertIn("9/9 Trace2D trials", self.doc)
        self.assertIn("6/6", self.doc)
        self.assertIn("input tokens", self.doc)
        self.assertIn("zero engine-native authoring operations", self.doc)
        self.assertIn("B2 entry gate", self.doc)
        self.assertIn("new held-out tasks", self.doc)
        self.assertIn("#178", self.doc)
        self.assertIn("#179", self.doc)

    def test_follow_up_issues_are_exact_b2_blockers(self) -> None:
        followups = self.report["follow_up_issues"]
        self.assertEqual([item["issue"] for item in followups], [178, 179])
        self.assertTrue(all(item["required_before_b2"] for item in followups))


if __name__ == "__main__":
    unittest.main()
