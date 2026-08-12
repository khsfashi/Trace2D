#!/usr/bin/env python3
"""Validate Benchmark B1 baseline selection and scored-suite freeze gate."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

REQUIRED_TASK_CLASSES = (
    "sprite_import_or_normalize",
    "trim_pivot_alignment_repair",
    "deterministic_animation_exact_event",
    "particle_structural_performance_budget",
    "exact_frame_headless_and_presentation_evidence",
    "seeded_content_defect_diagnosis_and_repair",
)
REQUIRED_CANDIDATES = (
    "satelliteoflove/godot-mcp",
    "hi-godot/godot-ai",
    "Erodenn/godot-mcp-runtime",
)
SELECTED_ID = "hi-godot/godot-ai"
SELECTED_PIN = "v3.0.6@f3d99dfbd38c9e095edf1467f85bee507ace2c3a"
QUALIFICATION_RUN = 31622618958


class ContractError(ValueError):
    pass


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise ContractError(message)


def validate_contract(data: dict[str, Any], repo_root: Path) -> None:
    _require(data.get("schema_version") == 1, "schema_version must be 1")
    _require(data.get("benchmark_id") == "trace2d-b1", "benchmark_id must be trace2d-b1")
    _require(data.get("state") == "baseline_selected", "B1 strongest baseline must be selected before scored task freeze")

    inherits = data.get("inherits")
    _require(isinstance(inherits, dict), "inherits must be an object")
    _require(inherits.get("godot_version") == "4.7.1-stable", "B1 must reuse frozen Godot 4.7.1-stable")
    for key in ("b0_suite", "agent_profile"):
        value = inherits.get(key)
        _require(isinstance(value, str) and value, f"inherits.{key} must be a repository path")
        _require((repo_root / value).is_file(), f"inherited file does not exist: {value}")

    _require(
        data.get("required_task_classes") == list(REQUIRED_TASK_CLASSES),
        "required_task_classes must preserve the #103 preregistered order and membership",
    )

    selection_rule = data.get("selection_rule")
    _require(isinstance(selection_rule, dict), "selection_rule must be an object")
    for key in (
        "must_precede_scored_task_freeze",
        "non_scored_fixture_required",
        "ordinary_external_user_workflow_only",
        "task_specific_solution_logic_forbidden",
    ):
        _require(selection_rule.get(key) is True, f"selection_rule.{key} must be true")

    candidates = data.get("candidates")
    _require(isinstance(candidates, list), "candidates must be an array")
    ids = [candidate.get("id") for candidate in candidates if isinstance(candidate, dict)]
    _require(tuple(ids) == REQUIRED_CANDIDATES, "candidate membership/order must match the reviewed B1 baseline set")
    _require(len(set(ids)) == len(ids), "candidate ids must be unique")
    for candidate in candidates:
        _require(isinstance(candidate, dict), "candidate entry must be an object")
        _require(isinstance(candidate.get("pin"), str) and candidate["pin"], f"{candidate.get('id')} must have an exact pin")
        source = candidate.get("source")
        _require(isinstance(source, str) and source.startswith("https://github.com/"), f"{candidate.get('id')} must record a primary GitHub source")
        notes = candidate.get("capability_notes")
        _require(isinstance(notes, list) and notes, f"{candidate.get('id')} must record reviewed capability notes")

    satellite = candidates[0]
    satellite_result = satellite.get("qualification_result")
    _require(isinstance(satellite_result, dict), "satellite qualification result must be recorded")
    _require(satellite_result.get("qualified") is False, "satellite must retain the observed B1 qualification failure")
    _require(satellite_result.get("workflow_run_id") == QUALIFICATION_RUN, "satellite qualification run must remain pinned")
    _require(
        satellite_result.get("reason_code") == "godot_4_7_method_track_api_incompatible",
        "satellite failure reason must remain explicit",
    )

    hi_godot = candidates[1]
    _require(hi_godot.get("pin") == SELECTED_PIN, "selected hi-godot pin changed")
    hi_result = hi_godot.get("qualification_result")
    _require(isinstance(hi_result, dict), "selected candidate qualification result must be recorded")
    _require(hi_result.get("qualified") is True, "selected candidate must have passed qualification")
    _require(hi_result.get("workflow_run_id") == QUALIFICATION_RUN, "selected qualification run must remain pinned")
    for key in ("known_good_accepted", "known_bad_rejected", "presentation_capture_handoff"):
        _require(hi_result.get(key) is True, f"selected candidate evidence missing {key}")

    qualification = data.get("qualification")
    _require(isinstance(qualification, dict), "qualification must be an object")
    _require(qualification.get("status") == "passed", "qualification must be passed before scored freeze")
    _require(qualification.get("workflow_run_id") == QUALIFICATION_RUN, "qualification workflow run must remain pinned")
    _require(qualification.get("selected_candidate_id") == SELECTED_ID, "selected B1 Godot Agent changed")
    _require(qualification.get("selected_pin") == SELECTED_PIN, "selected B1 Godot Agent pin changed")
    selection_document = qualification.get("selection_document")
    _require(isinstance(selection_document, str) and selection_document, "selection document path is required")
    _require((repo_root / selection_document).is_file(), "selection document does not exist")
    evidence = qualification.get("required_evidence")
    _require(isinstance(evidence, list) and len(evidence) >= 8, "qualification.required_evidence is incomplete")

    freeze_gate = data.get("freeze_gate")
    _require(isinstance(freeze_gate, dict), "freeze_gate must be an object")
    _require(freeze_gate.get("scored_suite_allowed") is True, "scored suite may open only after selected qualification evidence")


def load_and_validate(path: Path, repo_root: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    _require(isinstance(data, dict), "B1 contract root must be an object")
    validate_contract(data, repo_root)
    return data


def main() -> int:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)
    validate = subparsers.add_parser("validate-contract")
    validate.add_argument(
        "--contract",
        default="benchmarks/b1/baseline-qualification.json",
        help="repository-relative B1 qualification contract",
    )
    args = parser.parse_args()

    if args.command == "validate-contract":
        repo_root = Path(__file__).resolve().parents[1]
        path = repo_root / args.contract
        load_and_validate(path, repo_root)
        print("Benchmark B1 baseline selection contract: OK")
        return 0
    raise AssertionError("unreachable")


if __name__ == "__main__":
    raise SystemExit(main())
