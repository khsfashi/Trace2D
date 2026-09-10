import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

try:
    import tomllib
except ModuleNotFoundError:  # pragma: no cover
    tomllib = None


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts" / "trace2d_agent_skill.py"
REGISTRY = ROOT / "docs" / "agent" / "skills" / "registry-v1.json"
AGENT_INDEX_TEMPLATE = ROOT / "cmake" / "Trace2DAgentIndex.json.in"
CAPABILITIES = ROOT / "config" / "trace2d.capabilities.toml"


def load_module():
    spec = importlib.util.spec_from_file_location("trace2d_agent_skill", SCRIPT)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


class AgentSkillContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.module = load_module()
        cls.registry = cls.module.validate_registry(REGISTRY)

    def test_registry_is_descriptor_only_and_bounded(self):
        self.assertEqual(self.registry["kind"], "trace2d-agent-skill-registry")
        self.assertEqual(len(self.registry["skills"]), 3)
        self.assertLess(len(REGISTRY.read_bytes()), 8 * 1024)
        for skill in self.registry["skills"]:
            self.assertNotIn("body_content", skill)
            self.assertLess(len(json.dumps(skill, sort_keys=True).encode("utf-8")), 2048)

    def test_capability_and_surface_references_are_authoritative(self):
        if tomllib is None:
            self.skipTest("tomllib unavailable")
        capability_doc = tomllib.loads(CAPABILITIES.read_text(encoding="utf-8"))
        capability_ids = {entry["id"] for entry in capability_doc["capabilities"]}
        index = json.loads(AGENT_INDEX_TEMPLATE.read_text(encoding="utf-8").replace("@PROJECT_VERSION@", "test"))
        surface_ids = {entry["id"] for entry in index["surfaces"]}
        for skill in self.registry["skills"]:
            for capability in skill["required_capabilities"] + skill["related_capabilities"]:
                self.assertIn(capability, capability_ids)
            for surface in skill["surfaces"]:
                self.assertIn(surface, surface_ids)

    def test_frozen_dogfood_queries_select_expected_skill_first(self):
        cases = {
            "move player continuously while the key is held": "held-input-movement",
            "start menu button transitions into the game": "semantic-menu-flow",
            "apply damage then play impact sound and hit feedback": "combat-hit-feedback",
        }
        for query, expected in cases.items():
            with self.subTest(query=query):
                matches = self.module.search_skills(self.registry, query)
                self.assertGreater(len(matches), 0)
                self.assertEqual(matches[0]["id"], expected)

    def test_list_and_search_do_not_return_skill_bodies(self):
        for command in (["list"], ["search", "held player movement"]):
            completed = subprocess.run(
                [sys.executable, str(SCRIPT), "--registry", str(REGISTRY), *command],
                check=True,
                capture_output=True,
                text=True,
            )
            serialized = json.dumps(json.loads(completed.stdout))
            self.assertNotIn("# Held input movement", serialized)
            self.assertNotIn("# Semantic menu flow", serialized)
            self.assertNotIn("# Combat hit feedback", serialized)

    def test_get_loads_only_selected_body(self):
        completed = subprocess.run(
            [sys.executable, str(SCRIPT), "--registry", str(REGISTRY), "get", "held-input-movement"],
            check=True,
            capture_output=True,
            text=True,
        )
        body = json.loads(completed.stdout)["skill"]["body"]
        self.assertIn("# Held input movement", body)
        self.assertNotIn("# Semantic menu flow", body)
        self.assertNotIn("# Combat hit feedback", body)

    def test_duplicate_id_and_escaping_body_fail_closed(self):
        with tempfile.TemporaryDirectory() as temp:
            temp_root = Path(temp)
            shutil.copytree(REGISTRY.parent, temp_root / "skills")
            candidate = temp_root / "skills" / "registry-v1.json"
            duplicate = json.loads(candidate.read_text(encoding="utf-8"))
            duplicate["skills"].append(dict(duplicate["skills"][0]))
            candidate.write_text(json.dumps(duplicate), encoding="utf-8")
            with self.assertRaises(self.module.SkillContractError):
                self.module.validate_registry(candidate)

            escaping = json.loads(REGISTRY.read_text(encoding="utf-8"))
            escaping["skills"][0]["body"] = "../outside.md"
            candidate.write_text(json.dumps(escaping), encoding="utf-8")
            with self.assertRaises(self.module.SkillContractError):
                self.module.validate_registry(candidate)


if __name__ == "__main__":
    unittest.main()
