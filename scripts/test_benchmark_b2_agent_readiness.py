#!/usr/bin/env python3
from __future__ import annotations

import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
WORKFLOW = REPO_ROOT / ".github" / "workflows" / "benchmark-b2-owner-agent-readiness.yml"


class BenchmarkB2AgentReadinessTests(unittest.TestCase):
    def test_readiness_is_owner_main_only_and_uses_real_model_probe(self) -> None:
        workflow = WORKFLOW.read_text(encoding="utf-8")
        for required in (
            "github.repository == 'khsfashi/Trace2D'",
            "github.actor == 'khsfashi'",
            "github.run_attempt == 1",
            "github.event.issue.number == 104",
            "github.ref == 'refs/heads/main'",
            "github.event.comment.body == '/b2 agent-readiness'",
            "runs-on: [self-hosted, windows, x64, trace2d-gpu]",
            "benchmark_b2_codex_windows_acl_wrapper.py probe-isolation",
            "workspace_write_proved",
            "external_read_denied",
            "canary_secret_leaked",
            "No benchmark prompt or acceptance candidate was used.",
        ):
            self.assertIn(required, workflow, required)

    def test_readiness_cannot_run_or_mutate_acceptance(self) -> None:
        workflow = WORKFLOW.read_text(encoding="utf-8")
        for forbidden in (
            "benchmark_b2_acceptance_v1.py",
            "benchmark_b2_acceptance_v2.py",
            "benchmark_b2_acceptance_v3.py",
            "benchmark_b2_acceptance_v4.py",
            "accept-v1-start",
            "accept-v2-start",
            "accept-v3-start",
            "accept-v4-start",
            "record-review",
            "record-final-review",
            "feedback --runs-root",
            "benchmarks/b2/acceptance/tasks/",
            "workflow_dispatch:",
        ):
            self.assertNotIn(forbidden, workflow, forbidden)

    def test_readiness_scratch_state_uses_runner_temp_and_is_cleaned(self) -> None:
        workflow = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn("$env:RUNNER_TEMP", workflow)
        self.assertIn("Trace2D\\b2-agent-readiness", workflow)
        self.assertIn("Remove readiness scratch state", workflow)
        self.assertNotIn("benchmark-b2-acceptance-v4", workflow)
        self.assertNotIn("benchmark-b2-acceptance-v5", workflow)


if __name__ == "__main__":
    unittest.main()
