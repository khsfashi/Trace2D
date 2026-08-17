from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from scripts import benchmark_b2_acceptance as acceptance


class B2AcceptanceContractTests(unittest.TestCase):
    def test_contract_is_frozen_non_scored_and_prompt_hash_matches(self) -> None:
        contract = acceptance.validate_contract()
        self.assertFalse(contract["scored"])
        self.assertEqual(contract["state"], "frozen_pre_acceptance")
        self.assertEqual(contract["initial_runs"], 2)
        self.assertTrue(contract["isolation"]["scored_record_write_forbidden"])

    def test_acceptance_root_rejects_scored_and_repository_paths(self) -> None:
        with self.assertRaises(acceptance.AcceptanceError):
            acceptance.require_acceptance_root(
                str(Path(tempfile.gettempdir()) / "benchmark-b2-scored-v1")
            )
        with self.assertRaises(acceptance.AcceptanceError):
            acceptance.require_acceptance_root(str(acceptance.REPO_ROOT / ".acceptance"))

    def test_review_payload_is_bound_to_exact_capture(self) -> None:
        summary = {
            "review_target": {
                "trial_id": "accept-initial-01-trace2d-agent",
                "captures": [{"sha256": "abc123", "path": "preview.png"}],
            }
        }
        payload = {
            "reviewer_agent": "ChatGPT",
            "model": "GPT-5.6 Sol",
            "target_trial_id": "accept-initial-01-trace2d-agent",
            "capture_sha256": "abc123",
            "findings": ["HUD is readable."],
            "recommendation": "Increase hit-effect separation.",
        }
        self.assertIs(acceptance.validate_review_payload(payload, summary), payload)
        payload["capture_sha256"] = "wrong"
        with self.assertRaises(acceptance.AcceptanceError):
            acceptance.validate_review_payload(payload, summary)

    def test_revision_prompt_preserves_exact_human_feedback(self) -> None:
        feedback = "피격 이펙트를 더 눈에 띄게 해줘."
        prompt = acceptance.build_revision_prompt(
            feedback, {"recommendation": "Keep the HUD clear."}
        )
        self.assertIn(feedback, prompt)
        self.assertIn("Preserve every deterministic gameplay semantic", prompt)

    def test_owner_workflow_is_acceptance_only_and_action_archive_free(self) -> None:
        workflow = (
            acceptance.REPO_ROOT
            / ".github/workflows/benchmark-b2-owner-acceptance.yml"
        ).read_text(encoding="utf-8")
        self.assertIn("benchmark-b2-acceptance-v1", workflow)
        self.assertNotIn("benchmark-b2-scored-v1", workflow)
        self.assertIn("/b2 accept-start", workflow)
        self.assertIn("/b2 accept-review ", workflow)
        self.assertIn("/b2 accept-feedback ", workflow)
        self.assertNotIn("uses:", workflow)
        self.assertIn("git -c protocol.version=2 fetch", workflow)
        self.assertIn("Node.js 24", workflow)

    def test_diagnostic_workflow_is_non_authoritative_and_isolates_hosted_image_publication(self) -> None:
        workflow = (
            acceptance.REPO_ROOT
            / ".github/workflows/benchmark-b2-owner-diagnostic.yml"
        ).read_text(encoding="utf-8")
        self.assertIn("/b2 accept-diagnostic", workflow)
        self.assertIn("/b2 accept-diagnostic-publish", workflow)
        self.assertIn("benchmark-b2-acceptance-v1", workflow)
        self.assertEqual(workflow.count("benchmark-b2-scored-v1"), 1)
        self.assertIn("Contains('benchmark-b2-scored-v1')", workflow)
        self.assertIn("Diagnostic export must never read the scored durable root.", workflow)
        self.assertIn("diagnostic_only = $true", workflow)
        self.assertIn("acceptance_authority = $false", workflow)
        self.assertIn("scored = $false", workflow)
        self.assertIn("Get-FileHash", workflow)
        self.assertIn("ReadAllBytes", workflow)
        self.assertIn("contents: write", workflow)
        self.assertIn("issues: write", workflow)
        self.assertIn("hosted-image-publish", workflow)
        self.assertIn("runs-on: ubuntu-latest", workflow)
        self.assertIn("TRACE2D_B2_DIAGNOSTIC_METADATA=", workflow)
        self.assertIn("base64.b64decode", workflow)
        self.assertIn("hashlib.sha256", workflow)
        self.assertIn("b2-diagnostic-captures", workflow)
        self.assertIn("/git/blobs", workflow)
        self.assertIn("/git/trees", workflow)
        self.assertIn("/git/refs", workflow)
        self.assertIn("raw.githubusercontent.com", workflow)
        self.assertIn("TRACE2D_B2_DIAGNOSTIC_IMAGES", workflow)
        self.assertNotIn("uses:", workflow)
        self.assertNotIn("Set-Content", workflow)
        self.assertNotIn("Out-File", workflow)


if __name__ == "__main__":
    unittest.main()
