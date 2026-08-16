#!/usr/bin/env python3
"""Validate Benchmark B2 P1 strongest-baseline and scored-schedule freeze.

P1 freezes the selected Godot Agent identity and all nine scored slots while
keeping scoring blocked until lane-specific independent verifiers are qualified.
"""
from __future__ import annotations

import argparse
from pathlib import Path
from typing import Any

from scripts import benchmark_b2_preregistration as p0

EXPECTED_STATE = "baseline_frozen_verifier_pending"
EXPECTED_CANDIDATE_STATE = "selected_frozen"
EXPECTED_SELECTED_ID = "hi-godot/godot-ai"
EXPECTED_SELECTED_PIN = {
    "version": "3.1.5",
    "source_commit": "09a1e3311015153d967710fbe6502ac519585a9b",
    "package_identity": "sha256:51863ba177c66299808aa19ef6cd9069768915b2434d7787b9300e40c3620b04",
}
EXPECTED_QUALIFICATION = {
    "workflow_run_id": 31930609551,
    "artifact_id": 9259154101,
    "artifact_sha256": "7cc3439f506bc28ca064885c41b4c8432260e2c43e7e73f98650612be4d6a811",
}
EXPECTED_LANE_ORDERS = [
    ["godot.generic", "godot.agent", "trace2d.agent"],
    ["godot.agent", "trace2d.agent", "godot.generic"],
    ["trace2d.agent", "godot.generic", "godot.agent"],
]


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise p0.PreregistrationError(message)


def _load(path: Path) -> dict[str, Any]:
    return p0._load_json(path)


def validate_data(
    policy: dict[str, Any],
    candidates: dict[str, Any],
    cohort: dict[str, Any],
    *,
    agent_profile: dict[str, Any],
) -> None:
    _require(policy.get("schema_version") == 1, "policy schema_version must be 1")
    _require(policy.get("kind") == p0.EXPECTED_KIND, "policy kind changed")
    _require(policy.get("benchmark_id") == p0.EXPECTED_BENCHMARK, "benchmark id changed")
    _require(policy.get("state") == EXPECTED_STATE, f"P1 state must be {EXPECTED_STATE}")
    _require(policy.get("preregistration_base_commit") == p0.EXPECTED_BASE, "P0 base commit changed")
    _require(policy.get("budget") == p0.EXPECTED_BUDGET, "B2 budget drifted")
    _require(p0._canonical_sha256(agent_profile) == p0.EXPECTED_AGENT_PROFILE_HASH, "Agent profile drifted")

    b1 = policy.get("historical_b1_anchor", {})
    _require(b1.get("benchmark_tree_git_sha1") == p0.EXPECTED_B1_TREE, "B1 tree anchor changed")
    _require(b1.get("immutable") is True, "B1 must remain immutable")

    task = policy.get("task_freeze", {})
    _require(task.get("state") == "frozen", "B2 task is not frozen")
    _require(task.get("task_ids") == [p0.EXPECTED_TASK], "B2 task membership changed")
    _require(task.get("membership_or_semantic_change_after_this_commit_forbidden") is True, "task mutation guard changed")

    baseline = policy.get("baseline_selection", {})
    _require(baseline.get("selection_state") == "frozen", "selected baseline is not frozen")
    _require(baseline.get("selected_godot_agent_candidate") == EXPECTED_SELECTED_ID, "selected Godot Agent changed")
    selected_pin = baseline.get("selected_pin", {})
    for key, value in EXPECTED_SELECTED_PIN.items():
        _require(selected_pin.get(key) == value, f"selected baseline {key} changed")
    _require(selected_pin.get("qualification_artifact_sha256") == EXPECTED_QUALIFICATION["artifact_sha256"],
             "selected baseline qualification artifact changed")

    _require(candidates.get("schema_version") == 1, "candidate schema_version changed")
    _require(candidates.get("benchmark_id") == p0.EXPECTED_BENCHMARK, "candidate benchmark id changed")
    _require(candidates.get("state") == EXPECTED_CANDIDATE_STATE, "candidate set is not frozen")
    actual_pins: dict[str, tuple[str, str]] = {}
    for item in candidates.get("candidates", []):
        _require(isinstance(item, dict), "candidate entry must be object")
        actual_pins[str(item.get("id", ""))] = (str(item.get("version", "")), str(item.get("source_commit", "")))
    _require(actual_pins == p0.EXPECTED_CANDIDATE_PINS, "candidate identities drifted from P0")

    qualification = candidates.get("qualification", {})
    _require(qualification.get("state") == "passed_selection_frozen", "baseline qualification not frozen")
    _require(qualification.get("scored_task_prompt_visible_to_qualification_agent") is False,
             "qualification exposed scored prompt")
    _require(qualification.get("selected_candidate_id") == EXPECTED_SELECTED_ID, "candidate selection changed")
    candidate_pin = qualification.get("selected_pin", {})
    for key, value in EXPECTED_SELECTED_PIN.items():
        _require(candidate_pin.get(key) == value, f"candidate selected pin {key} changed")
    evidence = qualification.get("evidence", {})
    _require(evidence.get("godot_ai_workflow_run_id") == EXPECTED_QUALIFICATION["workflow_run_id"],
             "Godot AI qualification run changed")
    _require(evidence.get("godot_ai_artifact_id") == EXPECTED_QUALIFICATION["artifact_id"],
             "Godot AI qualification artifact id changed")
    _require(evidence.get("godot_ai_artifact_sha256") == EXPECTED_QUALIFICATION["artifact_sha256"],
             "Godot AI qualification digest changed")

    planned = policy.get("planned_scored_cohort", {})
    _require(planned.get("repetitions_per_task_lane") == 3, "B2 repetitions changed")
    _require(planned.get("lanes") == p0.EXPECTED_LANES, "B2 lanes changed")
    _require(planned.get("planned_total_trials") == 9, "B2 total trials changed")
    _require(planned.get("schedule_file") == "benchmarks/b2/scored-cohort-v1.json", "scored schedule path changed")
    _require(planned.get("schedule_created_after_baseline_selection") is True, "schedule ordering guard missing")

    _require(cohort.get("schema_version") == 1, "cohort schema_version must be 1")
    _require(cohort.get("kind") == "trace2d_b2_scored_cohort_policy", "cohort kind changed")
    _require(cohort.get("state") == "frozen_verifier_pending", "cohort must remain verifier-pending")
    _require(cohort.get("task_id") == p0.EXPECTED_TASK, "cohort task changed")
    _require(cohort.get("agent_profile_canonical_sha256") == p0.EXPECTED_AGENT_PROFILE_HASH, "cohort Agent changed")
    _require(cohort.get("lane_order_by_repetition") == EXPECTED_LANE_ORDERS, "lane rotation changed")
    _require(cohort.get("budget") == p0.EXPECTED_BUDGET, "cohort budget changed")

    selected = cohort.get("selected_godot_agent", {})
    _require(selected.get("id") == EXPECTED_SELECTED_ID, "cohort selected agent changed")
    for key, value in EXPECTED_SELECTED_PIN.items():
        _require(selected.get(key) == value, f"cohort selected agent {key} changed")
    _require(selected.get("qualification_workflow_run_id") == EXPECTED_QUALIFICATION["workflow_run_id"],
             "cohort qualification run changed")
    _require(selected.get("qualification_artifact_id") == EXPECTED_QUALIFICATION["artifact_id"],
             "cohort qualification artifact id changed")
    _require(selected.get("qualification_artifact_sha256") == EXPECTED_QUALIFICATION["artifact_sha256"],
             "cohort qualification digest changed")

    slots = cohort.get("slots")
    _require(isinstance(slots, list) and len(slots) == 9, "cohort must contain exactly nine slots")
    expected_slots = []
    slot_id = 1
    for repetition, lane_order in enumerate(EXPECTED_LANE_ORDERS, start=1):
        for lane in lane_order:
            expected_slots.append({
                "slot": slot_id,
                "repetition": repetition,
                "task": p0.EXPECTED_TASK,
                "lane": lane,
            })
            slot_id += 1
    _require(slots == expected_slots, "explicit nine-slot schedule changed")

    scoring = policy.get("scoring_gate", {})
    _require(scoring.get("allowed") is False, "scoring opened before verifier qualification")
    _require(scoring.get("scored_results_observed") is False, "scored results observed before verifier qualification")
    _require(scoring.get("remaining_requirements_before_opening") == [
        "lane-specific independent verifiers qualified on committed known-good and meaningful known-bad fixtures"
    ], "unexpected scoring blocker set")
    _require(cohort.get("scoring_gate", {}).get("allowed") is False, "cohort scoring gate opened early")
    _require(candidates.get("freeze_gate", {}).get("scored_suite_allowed") is False, "candidate gate opened early")


def validate_repository(repo_root: Path) -> None:
    policy = _load(repo_root / "benchmarks/b2/preregistration-v1.json")
    candidates = _load(repo_root / "benchmarks/b2/baseline-candidates.json")
    cohort = _load(repo_root / "benchmarks/b2/scored-cohort-v1.json")
    agent_profile = _load(repo_root / "benchmarks/b0/agent-profile.codex-0.144.6.json")
    validate_data(policy, candidates, cohort, agent_profile=agent_profile)
    p0._validate_repo_b1_tree(repo_root)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    try:
        validate_repository(args.repo_root.resolve())
    except p0.PreregistrationError as exc:
        print(f"B2 P1 freeze invalid: {exc}")
        return 1
    print("B2 P1 freeze valid: baseline and nine-slot schedule frozen; scoring remains blocked on verifier qualification.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
