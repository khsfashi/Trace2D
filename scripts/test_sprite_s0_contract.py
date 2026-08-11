#!/usr/bin/env python3
from __future__ import annotations

import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
CONTRACT_PATH = ROOT / "docs" / "contracts" / "sprite-s0.json"
ARCH_PATH = ROOT / "docs" / "SPRITE_ARCHITECTURE.md"
ROADMAP_PATH = ROOT / "docs" / "SPRITES.md"


class SpriteS0ContractTests(unittest.TestCase):
    def setUp(self) -> None:
        self.contract = json.loads(CONTRACT_PATH.read_text(encoding="utf-8"))
        self.arch = ARCH_PATH.read_text(encoding="utf-8")
        self.roadmap = ROADMAP_PATH.read_text(encoding="utf-8")

    def test_contract_identity_and_next_stage_are_frozen(self) -> None:
        self.assertEqual(self.contract["schema_version"], 1)
        self.assertEqual(self.contract["contract_id"], "trace2d.sprite.s0")
        self.assertEqual(self.contract["issue"], 119)
        self.assertEqual(self.contract["umbrella_issue"], 59)
        self.assertEqual(self.contract["status"], "frozen")
        self.assertEqual(self.contract["next_stage"], "S1")

    def test_cross_contract_reconciliation_is_explicit_and_paths_exist(self) -> None:
        reconciles = self.contract["reconciles"]
        expected = {
            "sprite_program": "docs/SPRITES.md",
            "production_architecture": "docs/PRODUCTION_ARCHITECTURE_CONTRACTS.md",
            "ai_operated_workflow": "docs/AI_OPERATED_WORKFLOW.md",
            "production_gaps": "docs/PRODUCTION_GAPS.md",
        }
        for key, relative in expected.items():
            self.assertEqual(reconciles[key], relative)
            self.assertTrue((ROOT / relative).is_file(), f"missing reconciled contract: {relative}")
        self.assertIn("does not supersede", reconciles["rule"])

    def test_authority_never_makes_renderer_truth(self) -> None:
        authority = self.contract["authority"]
        self.assertEqual(authority["canonical_asset"], "authored_cpu")
        self.assertEqual(authority["animator_state"], "authoritative_runtime_semantics")
        self.assertEqual(authority["current_fixed_transform"], "authoritative_runtime")
        self.assertEqual(authority["presentation_transform"], "derived_presentation")
        self.assertEqual(authority["normalized_uv"], "derived_presentation")
        self.assertEqual(authority["gpu_handles"], "derived_presentation")
        self.assertEqual(authority["batch_identity"], "derived_presentation")

    def test_source_space_is_exact_pixel_metadata(self) -> None:
        source = self.contract["source_space"]
        self.assertEqual(source["origin"], "top_left")
        self.assertEqual(source["positive_x"], "right")
        self.assertEqual(source["positive_y"], "down")
        self.assertEqual(source["rect_bounds"], "half_open")
        self.assertEqual(source["geometry_units"], "integer_pixels")
        self.assertEqual(source["pivot_space"], "untrimmed_source")
        self.assertTrue(source["trim_preserves_logical_source_space"])
        self.assertTrue(source["packed_rotation_is_storage_only"])

    def test_presentation_history_keeps_current_fixed_authoritative(self) -> None:
        history = self.contract["presentation_history"]
        self.assertEqual(history["authoritative_sample"], "current_fixed")
        self.assertEqual(history["history_sample"], "previous_fixed")
        self.assertEqual(history["rotation_interpolation"], "shortest_arc_2d")
        self.assertEqual(history["hierarchy_policy"], "interpolate_local_then_compose")
        self.assertEqual(history["discontinuity_policy"], "synchronize_previous_to_current")
        self.assertEqual(history["exact_frame_capture"], "authoritative_current")
        self.assertTrue(history["subframe_capture_requires_explicit_alpha"])
        self.assertTrue(history["discrete_fields_are_not_interpolated"])

    def test_batch_compatibility_cannot_override_painter_order(self) -> None:
        seams = self.contract["renderer_seams"]
        self.assertEqual(seams["batching"], "compatible_contiguous_runs_only")
        self.assertFalse(seams["global_resource_sorting_may_change_painter_order"])
        self.assertEqual(seams["resource_identity"], "typed_project_relative_cpu_identity")
        self.assertEqual(seams["view_input"], "backend_independent_resolved_2d_view")
        self.assertEqual(seams["material_identity"], "resolved_material_pipeline_compatibility")

    def test_texture_and_review_boundaries_are_explicit(self) -> None:
        texture = self.contract["texture_contract"]
        review = self.contract["verification_authority"]
        hot = self.contract["hot_path"]
        self.assertTrue(texture["color_space_intent_explicit"])
        self.assertTrue(texture["alpha_conversion_boundary_explicit"])
        self.assertTrue(texture["exact_pixel_metadata_is_canonical"])
        self.assertTrue(texture["gpu_packaging_is_derived"])
        self.assertFalse(texture["single_uncompressed_rgba8_page_assumption_frozen"])
        self.assertEqual(review["objective_sprite_facts"], "deterministic_or_structured")
        self.assertFalse(review["agent_self_report_is_final_truth"])
        self.assertFalse(review["screenshot_may_override_deterministic_failure"])
        self.assertFalse(hot["import_or_repair_per_frame"])
        self.assertFalse(hot["full_agent_snapshot_per_frame"])
        self.assertFalse(hot["capture_readback_per_frame"])
        self.assertFalse(hot["presentation_interpolation_requires_transient_sprite_list"])

    def test_future_handoffs_remain_seams_not_early_implementation(self) -> None:
        self.assertEqual(
            self.contract["future_handoffs"],
            {
                "world_components": 71,
                "resource_lifecycle": 86,
                "camera_viewport": 88,
                "material_shader": 89,
                "mesh2d": 60,
                "spine_license_gate": 61,
            },
        )

    def test_human_docs_reference_the_frozen_contract_and_fixed_order(self) -> None:
        self.assertIn("SPRITE_ARCHITECTURE.md", self.roadmap)
        self.assertIn("contracts/sprite-s0.json", self.roadmap)
        self.assertIn("Current stage: **S0 / #119**", self.roadmap)
        self.assertIn("Exact next stage after S0 merges: **S1**", self.roadmap)
        self.assertIn("compatible contiguous", self.arch)
        self.assertIn("exact-frame", self.arch.lower())
        self.assertIn("#97-#99", self.arch)


if __name__ == "__main__":
    unittest.main()
