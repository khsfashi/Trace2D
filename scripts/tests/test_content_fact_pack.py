from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPTS_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPTS_DIR))
import content_fact_pack


class FactPackTests(unittest.TestCase):
    def setUp(self) -> None:
        self.tempdir = tempfile.TemporaryDirectory()
        self.root = Path(self.tempdir.name)
        self.repo = self.root / "repo"
        self.repo.mkdir()
        self.git("init")
        self.git("config", "user.name", "Trace2D Test")
        self.git("config", "user.email", "trace2d@example.invalid")
        self.git("remote", "add", "origin", "https://github.com/khsfashi/Trace2D.git")
        self.output = self.repo / "content" / "candidates"
        self.registry = self.repo / "content" / "platforms.json"
        self.registry.parent.mkdir(parents=True, exist_ok=True)
        self.write_registry(["alpha", "beta"])

    def tearDown(self) -> None:
        self.tempdir.cleanup()

    def git(self, *args: str) -> str:
        return subprocess.run(
            ["git", "-C", str(self.repo), *args],
            check=True,
            capture_output=True,
            text=True,
        ).stdout.strip()

    def write_registry(self, ids: list[str]) -> None:
        payload = {
            "schema_version": 1,
            "platforms": [
                {
                    "id": platform_id,
                    "enabled": True,
                    "audience": f"{platform_id}-audience",
                    "default_language": "en",
                    "format_class": "test",
                    "publication_mode": "manual",
                }
                for platform_id in ids
            ],
        }
        self.registry.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")

    def commit(self, message: str, filename: str = "fixture.txt", value: str = "value") -> str:
        path = self.repo / filename
        path.write_text(value, encoding="utf-8")
        self.git("add", filename)
        message_file = self.root / "message.txt"
        message_file.write_text(message, encoding="utf-8")
        self.git("commit", "-F", str(message_file))
        return self.git("rev-parse", "HEAD")

    def substantive_message(self) -> str:
        return """Add deterministic content evidence

Issue: #108
Area: content-tooling
Decision: Fact Packs are derived from final-main commit evidence
Constraint: Content tooling cannot block the core engine lane
Rejected: merge-triggered prose | evidence and writing remain separate stages
Directive: Preserve Tested and Not-tested as separate facts
Tested: fixture extraction passed
Not-tested: external publication intentionally not exercised
Gate: content-boundary | no prose generation allowed
Reference: docs/DEVELOPMENT_CONTENT_PIPELINE.md | ADOPT | C0 stops at evidence
Related: PR #110
Content: major
Agent: ChatGPT
Model: GPT-5.6 Sol
"""

    def test_substantive_commit_creates_stable_fact_pack_and_is_idempotent(self) -> None:
        sha = self.commit(self.substantive_message())
        status, path, candidate = content_fact_pack.extract_candidate(
            self.repo, sha, "merge", sha, self.output
        )
        self.assertIn(status, {"created", "updated"})
        assert path and candidate
        first_bytes = path.read_bytes()

        self.assertEqual(candidate["candidate_id"], f"trace2d-merge-{sha}")
        self.assertEqual(candidate["significance"], "major")
        self.assertEqual(candidate["platform_records"], [])
        self.assertEqual(len(candidate["facts"]["tested"]), 1)
        self.assertEqual(len(candidate["facts"]["not_tested"]), 1)
        self.assertNotEqual(
            candidate["facts"]["tested"][0]["id"],
            candidate["facts"]["not_tested"][0]["id"],
        )
        evidence = candidate["facts"]["decisions"][0]["evidence"][0]
        self.assertEqual(evidence["commit"], sha)
        self.assertEqual(evidence["trailer"], "Decision")
        self.assertIn(f"/commit/{sha}", evidence["url"])
        self.assertEqual(candidate["sources"]["issues"][0]["id"], "#108")
        self.assertEqual(candidate["sources"]["pull_requests"][0]["number"], 110)

        status2, path2, _ = content_fact_pack.extract_candidate(
            self.repo, sha, "merge", sha, self.output
        )
        self.assertEqual(status2, "unchanged")
        self.assertEqual(path2, path)
        self.assertEqual(first_bytes, path.read_bytes())

    def test_trivial_and_explicit_none_produce_no_candidate(self) -> None:
        trivial = self.commit("Fix typo\n", "a.txt", "1")
        status, path, candidate = content_fact_pack.extract_candidate(
            self.repo, trivial, "merge", trivial, self.output
        )
        self.assertEqual((status, path, candidate), ("none", None, None))

        none_message = """Refine docs

Issue: #108
Area: content-tooling
Decision: Keep this maintenance-only
Tested: docs check
Content: none
"""
        none_sha = self.commit(none_message, "b.txt", "2")
        status, path, candidate = content_fact_pack.extract_candidate(
            self.repo, none_sha, "merge", none_sha, self.output
        )
        self.assertEqual((status, path, candidate), ("none", None, None))

    def test_platform_registry_is_data_driven_and_records_can_select_different_facts(self) -> None:
        sha = self.commit(self.substantive_message())
        _, path, candidate = content_fact_pack.extract_candidate(
            self.repo, sha, "merge", sha, self.output
        )
        assert path and candidate
        decision_id = candidate["facts"]["decisions"][0]["id"]
        tested_id = candidate["facts"]["tested"][0]["id"]

        content_fact_pack.set_platform_record(
            path,
            self.registry,
            "alpha",
            "selected",
            None,
            None,
            ["architecture"],
            [decision_id],
            None,
            None,
            None,
        )
        content_fact_pack.set_platform_record(
            path,
            self.registry,
            "beta",
            "considered",
            "ko",
            None,
            ["verification"],
            [tested_id],
            None,
            None,
            None,
        )
        updated = content_fact_pack.load_json(path)
        records = {record["platform_id"]: record for record in updated["platform_records"]}
        self.assertEqual(set(records), {"alpha", "beta"})
        self.assertEqual(records["alpha"]["source_fact_ids"], [decision_id])
        self.assertEqual(records["beta"]["source_fact_ids"], [tested_id])
        self.assertNotEqual(records["alpha"]["angle_tags"], records["beta"]["angle_tags"])

        # A new platform is introduced by registry data only; no Fact Pack schema/code change.
        self.write_registry(["alpha", "beta", "gamma"])
        content_fact_pack.set_platform_record(
            path,
            self.registry,
            "gamma",
            "considered",
            None,
            None,
            [],
            [],
            None,
            None,
            None,
        )
        updated = content_fact_pack.load_json(path)
        self.assertEqual(
            {record["platform_id"] for record in updated["platform_records"]},
            {"alpha", "beta", "gamma"},
        )

    def test_manual_review_metadata_survives_reconciliation(self) -> None:
        sha = self.commit(self.substantive_message())
        _, path, _ = content_fact_pack.extract_candidate(self.repo, sha, "merge", sha, self.output)
        assert path
        content_fact_pack.review_candidate(path, "reviewed", "candidate", False)
        before = content_fact_pack.load_json(path)
        self.assertEqual(before["lifecycle"], "reviewed")
        self.assertEqual(before["significance_override"], "candidate")

        status, _, reconciled = content_fact_pack.extract_candidate(
            self.repo, sha, "merge", sha, self.output
        )
        self.assertEqual(status, "unchanged")
        assert reconciled
        self.assertEqual(reconciled["lifecycle"], "reviewed")
        self.assertEqual(reconciled["significance_override"], "candidate")
        self.assertEqual(reconciled["significance"], "major")

    def test_rebuild_recreates_candidates_from_durable_history(self) -> None:
        sha = self.commit(self.substantive_message())
        content_fact_pack.extract_candidate(self.repo, sha, "merge", sha, self.output)
        expected = next(self.output.glob("*.json")).read_text(encoding="utf-8")
        for path in self.output.glob("*.json"):
            path.unlink()

        counts = content_fact_pack.rebuild_candidates(
            self.repo, "HEAD", self.output, "merge"
        )
        self.assertEqual(counts["created_or_updated"], 1)
        rebuilt = next(self.output.glob("*.json")).read_text(encoding="utf-8")
        self.assertEqual(rebuilt, expected)

    def test_fact_pack_contains_no_authored_prose_surface(self) -> None:
        sha = self.commit(self.substantive_message())
        _, _, candidate = content_fact_pack.extract_candidate(
            self.repo, sha, "merge", sha, self.output
        )
        assert candidate
        forbidden = {"draft", "article_body", "social_copy", "generated_title"}
        self.assertTrue(forbidden.isdisjoint(candidate.keys()))


if __name__ == "__main__":
    unittest.main()
