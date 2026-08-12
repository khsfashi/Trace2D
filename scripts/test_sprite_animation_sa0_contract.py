#!/usr/bin/env python3
from __future__ import annotations

import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
CONTRACT_PATH = ROOT / "docs" / "contracts" / "sprite-animation-sa0.json"
TIMING_PATH = ROOT / "docs" / "SPRITE_ANIMATION_TIMING_SA0.md"
SPRITES_PATH = ROOT / "docs" / "SPRITES.md"
STATUS_PATH = ROOT / "PROJECT_STATUS.md"
RUNTIME_PATH = ROOT / "engine" / "runtime" / "include" / "trace2d" / "runtime" / "FixedStepRuntime.hpp"


class SpriteAnimationSa0ContractTests(unittest.TestCase):
    def setUp(self) -> None:
        self.contract = json.loads(CONTRACT_PATH.read_text(encoding="utf-8"))
        self.timing = TIMING_PATH.read_text(encoding="utf-8")
        self.sprites = SPRITES_PATH.read_text(encoding="utf-8")
        self.status = STATUS_PATH.read_text(encoding="utf-8")
        self.runtime = RUNTIME_PATH.read_text(encoding="utf-8")

    def test_contract_identity_and_successor_are_frozen(self) -> None:
        self.assertEqual(self.contract["schema_version"], 1)
        self.assertEqual(self.contract["contract_id"], "trace2d.sprite.animation.sa0")
        self.assertEqual(self.contract["issue"], 144)
        self.assertEqual(self.contract["umbrella_issue"], 59)
        self.assertEqual(self.contract["status"], "frozen")
        self.assertEqual(self.contract["next_stage"], "SA1")

    def test_animation_time_matches_existing_integer_runtime_domain(self) -> None:
        time_domain = self.contract["time_domain"]
        self.assertEqual(time_domain["unit"], "integer_nanoseconds")
        self.assertIn("std::chrono::nanoseconds", self.runtime)
        self.assertTrue(time_domain["frame_duration_must_be_positive"])
        self.assertEqual(time_domain["clip_duration"], "checked_exact_sum_of_frame_durations")
        self.assertFalse(time_domain["floating_cursor_is_authoritative"])
        self.assertFalse(time_domain["wall_clock_advances_authoritative_animation"])
        self.assertFalse(time_domain["presentation_alpha_advances_authoritative_animation"])
        self.assertEqual(
            time_domain["future_speed_scaling"],
            "exact_integer_time_with_retained_remainder",
        )

    def test_frame_boundaries_are_half_open_and_terminal_completion_is_explicit(self) -> None:
        frames = self.contract["frame_boundaries"]
        self.assertEqual(frames["ownership"], "half_open")
        self.assertEqual(frames["frame_interval"], "[boundary_i,boundary_i_plus_1)")
        self.assertEqual(frames["internal_boundary_selects"], "following_frame")
        self.assertEqual(frames["normal_lookup_domain"], "0 <= t < duration")
        self.assertEqual(frames["non_loop_terminal_time"], "t == duration")
        self.assertEqual(frames["non_loop_terminal_presentation"], "last_authored_frame")
        self.assertTrue(frames["completion_is_explicit_state"])
        self.assertFalse(frames["synthetic_boundary_frame_allowed"])

    def test_event_crossings_are_directional_stable_and_lossless(self) -> None:
        events = self.contract["events"]
        self.assertEqual(events["equal_offset_order"], "authored_ordinal")
        self.assertEqual(events["forward_crossing"], "a < event_time <= b")
        self.assertEqual(events["forward_order"], "increasing_time_then_authored_ordinal")
        self.assertEqual(events["reverse_crossing"], "b <= event_time < a")
        self.assertEqual(events["reverse_order"], "decreasing_time_then_authored_ordinal")
        self.assertTrue(events["emit_once_per_actual_crossing"])
        self.assertTrue(events["large_advance_preserves_all_crossings"])
        self.assertFalse(events["seek_reset_inspection_replays_history"])
        self.assertFalse(events["structural_markers_are_authored_events"])
        self.assertFalse(events["silent_event_loss_allowed"])

    def test_loop_and_pingpong_preserve_boundary_ownership(self) -> None:
        traversal = self.contract["traversal"]
        self.assertEqual(traversal["linear_loop"], "ordered_segments_across_terminal_and_start")
        self.assertEqual(
            traversal["forward_wrap_offset_zero"],
            "emit_once_after_loop_marker_before_positive_offset_segment",
        )
        self.assertTrue(traversal["reverse_wrap_uses_same_timeline"])
        self.assertEqual(
            traversal["pingpong_endpoint"],
            "reverse_without_duplicate_endpoint_frame",
        )
        self.assertFalse(traversal["reverse_uses_separate_reversed_clip"])
        self.assertEqual(traversal["multi_wrap"], "compose_segments_and_preserve_crossing_order")

    def test_renderer_and_observation_never_become_animation_truth(self) -> None:
        authority = self.contract["authority"]
        self.assertEqual(authority["animation_time"], "authoritative_runtime")
        self.assertEqual(authority["event_crossings"], "authoritative_runtime_output")
        self.assertEqual(authority["renderer_selection"], "derived_presentation_consumer")
        self.assertEqual(authority["gpu_state"], "derived_presentation")
        self.assertEqual(authority["capture_pixels"], "derived_evidence")
        self.assertEqual(authority["agent_snapshot"], "explicit_observation")

    def test_hot_path_forbids_reporting_allocation_and_silent_event_loss(self) -> None:
        hot = self.contract["hot_path"]
        self.assertFalse(hot["filesystem_per_tick"])
        self.assertFalse(hot["json_per_tick"])
        self.assertFalse(hot["string_formatting_per_tick"])
        self.assertFalse(hot["semantic_name_lookup_per_tick"])
        self.assertFalse(hot["renderer_or_gpu_required"])
        self.assertFalse(hot["mandatory_heap_allocation_per_tick"])
        self.assertTrue(hot["precomputed_cumulative_offsets_allowed"])
        self.assertEqual(hot["work_scales_with"], "crossed_timeline_boundaries")
        self.assertEqual(hot["event_output"], "caller_owned_or_reused_bounded_storage")
        self.assertEqual(hot["capacity_exhaustion"], "explicit_failure_not_silent_drop")

    def test_sa0_document_contains_boundary_examples_and_external_decisions(self) -> None:
        self.assertIn("a < event_time <= b", self.timing)
        self.assertIn("b <= event_time < a", self.timing)
        self.assertIn("offset-zero events once", self.timing)
        self.assertIn("ADOPT / ADAPT", self.timing)
        self.assertIn("REJECT", self.timing)
        self.assertIn("Do **not** implement SA1", self.timing)

    def test_program_handoff_keeps_sa0_frozen_and_marks_sa1_active(self) -> None:
        self.assertIn("SR8 [complete]", self.sprites)
        self.assertIn("SA0 [complete]", self.sprites)
        self.assertIn("SA1 [active #146/#147]", self.sprites)
        self.assertIn("#144", self.status)
        self.assertIn("#146", self.status)
        self.assertIn("SA0", self.status)
        self.assertIn("SA1", self.status)
        self.assertIn("SA2", self.status)


if __name__ == "__main__":
    unittest.main()
