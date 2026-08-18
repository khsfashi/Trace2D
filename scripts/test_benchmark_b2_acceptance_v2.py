from __future__ import annotations

import json
import struct
import tempfile
import unittest
import zlib
from pathlib import Path

from scripts import benchmark_b2_acceptance_v2 as v2


def write_png(path: Path, width: int, height: int, state: int = 0, *, game_like: bool = True) -> None:
    pixels = bytearray()
    for y in range(height):
        pixels.append(0)
        for x in range(width):
            if not game_like:
                r = g = b = 24
            else:
                r = 22 + ((x // 48) % 5) * 9
                g = 26 + ((y // 36) % 5) * 10
                b = 38 + (((x // 72) + (y // 54)) % 6) * 12
                if x < 14 or x >= width - 14 or y < 14 or y >= height - 14:
                    r, g, b = 88, 96, 112
                if y < height // 5:
                    r, g, b = 18, 22, 30
                    if 30 < x < 180 and 24 < y < 42:
                        r, g, b = 220, 230, 240
                    if width // 2 - 85 < x < width // 2 + 85 and 28 < y < 45:
                        r, g, b = 205, 215, 225
                    if width - 190 < x < width - 34 and 24 < y < 42:
                        r, g, b = 220, 230, 240
                if 170 <= x < 210 and height // 2 <= y < height // 2 + 40:
                    r, g, b = 45, 200, 235
                if width - 230 <= x < width - 190 and height // 2 <= y < height // 2 + 40:
                    r, g, b = 220, 55, 170
                if (80 <= x < 115 and 120 <= y < 155) or (width - 120 <= x < width - 85 and height - 90 <= y < height - 55):
                    r, g, b = 235, 120, 40
                if state == 1 and 230 <= x < 310 and height // 2 + 5 <= y < height // 2 + 25:
                    r, g, b = 255, 180, 55
                if state == 2 and width - 255 <= x < width - 165 and height // 2 - 15 <= y < height // 2 + 55:
                    r, g, b = 250, 235, 180
                if state == 3 and width - 285 <= x < width - 145 and height // 2 - 55 <= y < height // 2 + 85:
                    r, g, b = 245, 105, 35
            pixels.extend((r, g, b))

    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    encoded = v2.PNG_SIGNATURE + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(bytes(pixels), level=0)) + chunk(b"IEND", b"")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(encoded)


class B2AcceptanceV2Tests(unittest.TestCase):
    def test_contract_is_additive_frozen_and_non_scored(self) -> None:
        contract = v2.validate_contract()
        self.assertEqual(contract["acceptance_version"], 2)
        self.assertFalse(contract["scored"])
        self.assertEqual(contract["state"], "frozen_pre_acceptance")
        self.assertEqual(contract["supersedes_acceptance_version"], 1)
        self.assertEqual(contract["does_not_reinterpret"], ["benchmark-b2-scored-v1", "benchmark-b2-acceptance-v1"])
        self.assertTrue(contract["isolation"]["previous_acceptance_write_forbidden"])
        self.assertTrue(contract["isolation"]["scored_record_write_forbidden"])

    def test_v2_root_rejects_scored_v1_and_repository_paths(self) -> None:
        with self.assertRaises(v2.AcceptanceV2Error):
            v2.require_acceptance_root(str(Path(tempfile.gettempdir()) / "benchmark-b2-scored-v1"))
        with self.assertRaises(v2.AcceptanceV2Error):
            v2.require_acceptance_root(str(Path(tempfile.gettempdir()) / "benchmark-b2-acceptance-v1"))
        with self.assertRaises(v2.AcceptanceV2Error):
            v2.require_acceptance_root(str(v2.REPO_ROOT / "benchmark-b2-acceptance-v2"))
        accepted = v2.require_acceptance_root(str(Path(tempfile.gettempdir()) / "benchmark-b2-acceptance-v2"))
        self.assertIn("benchmark-b2-acceptance-v2", str(accepted))

    def test_machine_presentation_gate_accepts_game_like_four_state_capture_set(self) -> None:
        contract = v2.validate_contract()
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            roles = contract["presentation_gate"]["required_capture_roles"]
            for state, role in enumerate(("overview", "attack", "hit", "death")):
                write_png(workspace / roles[role], 640, 360, state)
            gate = v2.presentation_gate(workspace, contract)
            self.assertTrue(gate["passed"], gate["failures"])
            self.assertEqual(set(gate["captures"]), {"overview", "attack", "hit", "death"})
            self.assertTrue(all(value >= 0.003 for value in gate["state_change_sample_difference_ratio"].values()))

    def test_machine_presentation_gate_rejects_debug_like_flat_capture_set(self) -> None:
        contract = v2.validate_contract()
        with tempfile.TemporaryDirectory() as directory:
            workspace = Path(directory)
            roles = contract["presentation_gate"]["required_capture_roles"]
            for state, role in enumerate(("overview", "attack", "hit", "death")):
                write_png(workspace / roles[role], 640, 360, state, game_like=False)
            gate = v2.presentation_gate(workspace, contract)
            self.assertFalse(gate["passed"])
            self.assertTrue(any("overview_visual_family" in item or "dominant_color_ratio" in item for item in gate["failures"]))

    def test_perceptual_review_requires_exact_capture_set_and_all_boolean_rubric(self) -> None:
        contract = v2.validate_contract()
        captures = {role: {"sha256": f"sha-{role}", "path": f"{role}.png"} for role in ("overview", "attack", "hit", "death")}
        summary = {"review_target": {"trial_id": "accept-v2-initial-01-trace2d-agent", "captures": captures}}
        rubric = {name: True for name in contract["perceptual_review"]["rubric"]}
        payload = {
            "reviewer_agent": "ChatGPT",
            "model": "GPT-5.6 Sol",
            "target_trial_id": "accept-v2-initial-01-trace2d-agent",
            "capture_sha256s": {role: value["sha256"] for role, value in captures.items()},
            "rubric": rubric,
            "passed": True,
            "findings": ["The room reads as a playable combat screen."],
            "recommendation": "Increase attack anticipation slightly."
        }
        self.assertIs(v2.validate_review_payload(payload, summary, contract), payload)
        bad = json.loads(json.dumps(payload))
        bad["rubric"]["reads_as_game_screen"] = False
        bad["passed"] = False
        with self.assertRaises(v2.AcceptanceV2Error):
            v2.validate_review_payload(bad, summary, contract, final=True)

    def test_owner_v2_workflow_is_separate_from_v1_and_scored_roots(self) -> None:
        workflow = (v2.REPO_ROOT / ".github/workflows/benchmark-b2-owner-acceptance-v2.yml").read_text(encoding="utf-8")
        self.assertIn("/b2 accept-v2-start", workflow)
        self.assertIn("/b2 accept-v2-review ", workflow)
        self.assertIn("/b2 accept-v2-feedback ", workflow)
        self.assertIn("/b2 accept-v2-final-review ", workflow)
        self.assertIn("benchmark-b2-acceptance-v2", workflow)
        self.assertNotIn("/b2 accept-start", workflow)
        self.assertNotIn("benchmark-b2-scored-v1", workflow)
        self.assertNotIn("benchmark-b2-acceptance-v1", workflow)
        self.assertNotIn("uses:", workflow)


if __name__ == "__main__":
    unittest.main()
