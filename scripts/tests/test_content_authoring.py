from __future__ import annotations

import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

MODULE_PATH = Path(__file__).resolve().parents[1] / "content_authoring.py"
spec = importlib.util.spec_from_file_location("content_authoring", MODULE_PATH)
module = importlib.util.module_from_spec(spec)
assert spec and spec.loader
spec.loader.exec_module(module)


class ContentAuthoringTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        (self.root / "docs").mkdir()
        (self.root / "content" / "candidates").mkdir(parents=True)
        (self.root / "docs" / "CONTENT_AUTHOR_STYLE.md").write_text(
            "# Maintainer author style profile\n\n"
            "Approved public reference corpus:\n\n"
            "- https://woodroot.tistory.com/\n\n"
            "Use direct Korean technical prose. Never invent autobiographical anecdotes.\n",
            encoding="utf-8",
        )
        self.platforms = {
            "schema_version": 1,
            "platforms": [
                {
                    "id": "tistory",
                    "enabled": True,
                    "audience": "ko-technical-longform",
                    "default_language": "ko",
                    "format_class": "longform",
                    "publication_mode": "manual",
                },
                {
                    "id": "show-hn",
                    "enabled": True,
                    "audience": "en-technical-builders",
                    "default_language": "en",
                    "format_class": "project-showcase",
                    "publication_mode": "manual",
                },
            ],
        }
        self.write_json(self.root / "content" / "platforms.json", self.platforms)
        self.first = self.make_candidate(
            "trace2d-merge-aaa",
            {
                "decisions": [
                    {"id": "decisions-001", "text": "CPU remains the semantic oracle", "evidence": []}
                ],
                "tested": [
                    {"id": "tested-001", "text": "Hosted CI passed", "evidence": []}
                ],
                "not_tested": [
                    {"id": "not-tested-001", "text": "Real GPU smoke not run", "evidence": []}
                ],
                "gates": [
                    {"id": "gates-001", "text": "Real GPU gate remains", "evidence": []}
                ],
                "limitations": [
                    {
                        "id": "limitations-001",
                        "text": "No universal bit-identical GPU claim",
                        "evidence": [],
                    }
                ],
                "benchmark_metrics": [
                    {"id": "benchmark-001", "text": "One lane lost 2/5 trials", "evidence": []}
                ],
            },
            platform_records=[
                {
                    "platform_id": "tistory",
                    "state": "selected",
                    "language": "ko",
                    "audience": "engine-programmers",
                    "angle_tags": ["architecture"],
                    "source_fact_ids": ["decisions-001"],
                }
            ],
        )
        self.second = self.make_candidate(
            "trace2d-merge-bbb",
            {
                "decisions": [
                    {
                        "id": "decisions-001",
                        "text": "GPU backend selection is explicit",
                        "evidence": [],
                    }
                ],
                "tested": [
                    {"id": "tested-001", "text": "Compiler fixtures passed", "evidence": []}
                ],
            },
        )

    def tearDown(self) -> None:
        self.temp.cleanup()

    def write_json(self, path: Path, value: dict) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    def make_candidate(self, candidate_id: str, facts: dict, platform_records=None) -> Path:
        path = self.root / "content" / "candidates" / f"{candidate_id}.json"
        self.write_json(
            path,
            {
                "schema_version": 1,
                "candidate_id": candidate_id,
                "source_event": "merge",
                "significance": "major",
                "areas": ["particles"],
                "topics": [],
                "sources": {"issues": [], "pull_requests": [], "commits": [], "tags": []},
                "facts": facts,
                "artifacts": {},
                "references": [],
                "platform_records": platform_records or [],
            },
        )
        return path

    def prepare(self, **overrides):
        params = dict(
            repo_root=self.root,
            candidate_values=[str(self.first)],
            maintainer_request="GPU 파티클 설계 과정을 개발로그로 써줘",
            platform_id="tistory",
            mode="development-log",
        )
        params.update(overrides)
        return module.prepare_request(**params)

    def test_requires_explicit_maintainer_request(self):
        with self.assertRaises(module.AuthoringError):
            self.prepare(maintainer_request="   ")

    def test_editorial_brief_controls_output_and_combines_candidates(self):
        packet = self.prepare(
            candidate_values=[str(self.first), str(self.second)],
            goal="CPU oracle 선택 이유 설명",
            audiences=["game-client-programmer"],
            angles=["architecture-decision"],
            emphasize=["rejected alternatives"],
            language="ko",
            length="long",
            mode="engineering-thesis",
        )
        self.assertEqual(packet["editorial_brief"]["goal"], "CPU oracle 선택 이유 설명")
        self.assertEqual(packet["editorial_brief"]["audience"], ["game-client-programmer"])
        self.assertEqual(packet["editorial_brief"]["mode"], "engineering-thesis")
        self.assertEqual(len(packet["candidate_sources"]), 2)
        refs = {fact["ref"] for fact in packet["factual_authority"]["facts"]}
        self.assertIn("trace2d-merge-aaa#decisions-001", refs)
        self.assertIn("trace2d-merge-bbb#decisions-001", refs)

    def test_style_contract_and_dynamic_platform_registry_are_inputs_not_schema(self):
        self.platforms["platforms"].append(
            {
                "id": "future-platform",
                "enabled": True,
                "audience": "future-builders",
                "default_language": "en",
                "format_class": "custom",
                "publication_mode": "manual",
            }
        )
        self.write_json(self.root / "content" / "platforms.json", self.platforms)
        packet = self.prepare(platform_id="future-platform")
        self.assertEqual(packet["platform_profile"]["id"], "future-platform")
        self.assertEqual(packet["editorial_brief"]["language"], "en")
        self.assertIn("https://woodroot.tistory.com/", packet["style"]["approved_reference_corpus"])
        self.assertEqual(packet["style"]["contract_path"], "docs/CONTENT_AUTHOR_STYLE.md")
        self.assertTrue(packet["style"]["contract_sha256"])

    def test_same_evidence_can_produce_different_requested_pieces(self):
        tistory = self.prepare(
            platform_id="tistory",
            goal="설계 의사결정 설명",
            language="ko",
            mode="development-log",
        )
        show_hn = self.prepare(
            platform_id="show-hn",
            goal="Explain reproducible architecture evidence",
            language="en",
            mode="engineering-thesis",
        )
        self.assertNotEqual(tistory["request_id"], show_hn["request_id"])
        self.assertNotEqual(tistory["platform_profile"], show_hn["platform_profile"])
        self.assertEqual(
            [x["candidate_id"] for x in tistory["candidate_sources"]],
            [x["candidate_id"] for x in show_hn["candidate_sources"]],
        )

    def test_truth_boundaries_and_style_safety_are_explicit(self):
        packet = self.prepare()
        boundaries = set(packet["factual_authority"]["mandatory_truth_boundary_refs"])
        self.assertEqual(
            boundaries,
            {
                "trace2d-merge-aaa#not-tested-001",
                "trace2d-merge-aaa#gates-001",
                "trace2d-merge-aaa#limitations-001",
                "trace2d-merge-aaa#benchmark-001",
            },
        )
        rules = "\n".join(packet["authoring_rules"])
        self.assertIn("never fabricate personal anecdotes", rules)
        self.assertIn("clearly marked editable DRAFT", rules)
        self.assertIn("Not-tested", rules)

    def test_wrap_draft_requires_explicit_boundary_dispositions_and_does_not_mutate_fact_pack(self):
        packet = self.prepare()
        before = self.first.read_bytes()
        with self.assertRaises(module.AuthoringError):
            module.wrap_draft(request_packet=packet, draft_body="본문")

        boundaries = packet["factual_authority"]["mandatory_truth_boundary_refs"]
        wrapped, metadata = module.wrap_draft(
            request_packet=packet,
            draft_body="# GPU 파티클 설계\n\n실제 GPU 검증은 아직 남아 있다.",
            used_fact_refs=["trace2d-merge-aaa#decisions-001"],
            acknowledged_boundaries=boundaries[:2],
            not_material_boundaries={
                ref: "이 글의 비교 결론과 직접 관련 없음" for ref in boundaries[2:]
            },
        )
        self.assertTrue(
            wrapped.startswith("> **DRAFT — maintainer review required. Not published.**")
        )
        self.assertEqual(metadata["status"], "draft")
        self.assertEqual(metadata["publication_mode"], "manual")
        self.assertEqual(before, self.first.read_bytes())
        self.assertEqual(len(metadata["truth_boundary_dispositions"]), len(boundaries))

    def test_cli_surface_has_no_publish_or_automatic_generate_command(self):
        parser = module.build_parser()
        subparsers = [
            action for action in parser._actions if action.__class__.__name__ == "_SubParsersAction"
        ]
        self.assertEqual(len(subparsers), 1)
        commands = set(subparsers[0].choices)
        self.assertEqual(commands, {"prepare", "wrap-draft"})
        self.assertNotIn("publish", commands)
        self.assertNotIn("generate", commands)


if __name__ == "__main__":
    unittest.main()
