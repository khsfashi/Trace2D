from __future__ import annotations

import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent
WORKFLOW = REPO_ROOT / ".github" / "workflows" / "benchmark-b2-owner-acceptance-v5-diagnostics.yml"


class BenchmarkB2AcceptanceV5DiagnosticsTests(unittest.TestCase):
    def test_workflow_is_owner_main_issue_104_only(self) -> None:
        text = WORKFLOW.read_text(encoding="utf-8")
        for required in (
            "github.repository == 'khsfashi/Trace2D'",
            "github.actor == 'khsfashi'",
            "github.run_attempt == 1",
            "github.event.issue.number == 104",
            "github.ref == 'refs/heads/main'",
            "github.event.comment.body == '/b2 accept-v5-diagnose'",
            "runs-on: [self-hosted, windows, x64, trace2d-gpu]",
            "benchmark-b2-acceptance-v5",
            "trace2d_b2_nonscored_acceptance_v5_initial",
            "codex-events.jsonl",
            "agent-result.json",
            "diagnostic_only = $true",
            "acceptance_authority = $false",
            "scored = $false",
        ):
            self.assertIn(required, text, required)

    def test_workflow_cannot_execute_or_mutate_an_acceptance_candidate(self) -> None:
        text = WORKFLOW.read_text(encoding="utf-8").casefold()
        for forbidden in (
            "benchmark_b2_acceptance_v5.py start",
            "accept-v5-start",
            "accept-v5-review",
            "accept-v5-feedback",
            "accept-v5-final-review",
            "record-review --runs-root",
            "feedback --runs-root",
            "record-final-review",
            "accept-v6-start",
            "workflow_dispatch:",
        ):
            self.assertNotIn(forbidden.casefold(), text, forbidden)

    def test_historical_and_scored_roots_are_explicitly_forbidden(self) -> None:
        text = WORKFLOW.read_text(encoding="utf-8")
        for forbidden_root in (
            "benchmark-b2-scored-v1",
            "benchmark-b2-acceptance-v1",
            "benchmark-b2-acceptance-v2",
            "benchmark-b2-acceptance-v3",
            "benchmark-b2-acceptance-v4",
        ):
            self.assertIn(forbidden_root, text, forbidden_root)

    def test_diagnostic_output_is_bounded(self) -> None:
        text = WORKFLOW.read_text(encoding="utf-8")
        for required in (
            "$maxFileChars = 65536",
            "$maxTrialLogChars = 196608",
            "$maxWorkspaceChars = 262144",
            "TRIAL_LOG_OMITTED",
        ):
            self.assertIn(required, text, required)


if __name__ == "__main__":
    unittest.main()
